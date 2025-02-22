/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef PINS_P4RT_APP_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_
#define PINS_P4RT_APP_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_

#include <cstdint>
#include <optional>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "p4/v1/p4runtime.grpc.pb.h"
#include "p4/v1/p4runtime.pb.h"
#include "sai_p4/fixed/roles.h"

namespace p4rt_app {

// A connection between a controller and p4rt server.
class SdnConnection {
public:
  SdnConnection(grpc::ServerContext *context,
                grpc::ServerReaderWriter<p4::v1::StreamMessageResponse,
                                         p4::v1::StreamMessageRequest> *stream)
      : initialized_(false), grpc_context_(context), grpc_stream_(stream) {}

  void Initialize() { initialized_ = true; }
  bool IsInitialized() const { return initialized_; }

  void SetElectionId(const std::optional<absl::uint128> &id);
  std::optional<absl::uint128> GetElectionId() const;

  void SetRoleName(const std::optional<std::string> &name);
  std::optional<std::string> GetRoleName() const;

  // Sends back StreamMessageResponse to this controller.
  void SendStreamMessageResponse(const p4::v1::StreamMessageResponse &response);

private:
  // The SDN connection should be initialized through arbitration before it can
  // be used.
  bool initialized_;

  // SDN connections are made to the P4RT gRPC service based on role types. The
  // specified role limits the table a connection can write to, and read from.
  // If no role is specified then the connection is assumed to be root, and has
  // access to all tables.
  std::optional<std::string> role_name_;

  // Multiple connections can be established per role, but only one connection
  // (i.e. the primary connection) is allowed to modify state. The primary
  // connection is determined based on the election ID.
  std::optional<absl::uint128> election_id_;

  // While the gRPC connection is open we keep access to the context & the
  // read/write stream for communication.
  grpc::ServerContext *grpc_context_; // not owned.
  grpc::ServerReaderWriter<p4::v1::StreamMessageResponse,
                           p4::v1::StreamMessageRequest>
      *grpc_stream_; // not owned.
};

class SdnControllerManager {
public:
  grpc::Status
  HandleArbitrationUpdate(const p4::v1::MasterArbitrationUpdate &update,
                          SdnConnection *controller) ABSL_LOCKS_EXCLUDED(lock_);

  void Disconnect(SdnConnection *connection) ABSL_LOCKS_EXCLUDED(lock_);

  absl::Status SetDeviceId(uint64_t device_id) ABSL_LOCKS_EXCLUDED(lock_);
  std::optional<uint64_t> GetDeviceId() const ABSL_LOCKS_EXCLUDED(lock_);

  // Controller requests fall into 2 broad categories: writes & reads. Writes
  // can mutate state, and therefore should only be done by a primary
  // connection. Reads are allowed by any connection (i.e. we don't check for an
  // active stream channel, or validate the election ID) since they do not
  // mutate state.
  grpc::Status
  AllowMutableRequest(const std::optional<uint64_t> &device_id,
                      const std::optional<std::string> &role_name,
                      const std::optional<absl::uint128> &election_id) const
      ABSL_LOCKS_EXCLUDED(lock_);
  grpc::Status
  AllowNonMutableRequest(const std::optional<uint64_t> &device_id) const
      ABSL_LOCKS_EXCLUDED(lock_);

  // Returns whether or not a specific type of request should be accepted. If
  // the request is rejected (e.g. wrong device ID, not the primary, etc.) then
  // the status failure will indicate the reason.
  grpc::Status AllowRequest(const p4::v1::WriteRequest &request) const;
  grpc::Status AllowRequest(const p4::v1::ReadRequest &request) const;
  grpc::Status
  AllowRequest(const p4::v1::SetForwardingPipelineConfigRequest &request) const;
  grpc::Status
  AllowRequest(const p4::v1::GetForwardingPipelineConfigRequest &request) const;

  absl::Status
  SendPacketInToPrimary(const p4::v1::StreamMessageResponse &response);

private:
  // Goes through the current list of active connections, and returns if one of
  // them is currently the primary.
  bool PrimaryConnectionExists(const std::optional<std::string> &role_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  absl::Status
  SendStreamMessageToPrimary(const p4::v1::StreamMessageResponse &response)
      ABSL_LOCKS_EXCLUDED(lock_);

  // Sends an arbitration update to all active connections for a role about the
  // current primary connection.
  void InformConnectionsAboutPrimaryChange(
      const std::optional<std::string> &role_name)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Sends an arbitration update to a specific connection.
  void SendArbitrationResponse(SdnConnection *connection)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Lock for protecting SdnControllerManager member fields.
  mutable absl::Mutex lock_;

  // Device ID is used to ensure all requests are connecting to the intended
  // place.
  std::optional<uint64_t> device_id_ ABSL_GUARDED_BY(lock_);

  // We maintain a list of all active connections. The P4 runtime spec requires
  // a number of edge cases based on values existing or not that makes
  // maintaining these connections any other container difficult. However, we
  // don't expect a large number of connections so performance shouldn't be
  // affected.
  //
  // Requirements for roles:
  //  * Each role can have it's own set of primary & backup connections.
  //  * If no role is specified (NOTE: different than "") the role is assumed to
  //    be 'root', and as such has access to any table in the P4 application.
  //
  // Requirements for election ids:
  //  * The connection with the highest election ID is the primary.
  //  * If no election ID is given (NOTE: different than 0) the connection is
  //    valid, but it cannot ever be primary (i.e. the controller can force a
  //    connection to be a backup).
  std::vector<SdnConnection *> connections_ ABSL_GUARDED_BY(lock_);

  // We maintain a map of the highest election IDs that have been selected for
  // the primary connection of a role. Once an election ID is set all new
  // primary connections for that rolue must use an election ID that is >= in
  // value.
  //
  // key:   role_name   (no value indicaates the default/root role)
  // value: election ID (no value indicates there has never been a primary
  //                     connection)
  absl::flat_hash_map<std::optional<std::string>, std::optional<absl::uint128>>
      election_id_past_by_role_ ABSL_GUARDED_BY(lock_);

  // Placeholder for role_config which ideally would be passed
  // via the MasterArbitration method.
  //
  // Contains the roles that will receive packet in messages.
  // A copy of the packet will be sent to the primary for each role.
  absl::flat_hash_set<std::optional<std::string>> role_receives_packet_in_{
      P4RUNTIME_ROLE_SDN_CONTROLLER,
      std::nullopt, // default role
  };
};

} // namespace p4rt_app

#endif // PINS_P4RT_APP_P4RUNTIME_SDN_CONTROLLER_MANAGER_H_
