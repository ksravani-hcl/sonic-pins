// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef PINS_P4RT_APP_SONIC_VLAN_ENTRY_TRANSLATION_H_
#define PINS_P4RT_APP_SONIC_VLAN_ENTRY_TRANSLATION_H_

#include <vector>

#include "absl/status/statusor.h"
#include "p4/v1/p4runtime.pb.h"
#include "p4_pdpi/ir.pb.h"
#include "p4rt_app/sonic/redis_connections.h"
#include "swss/rediscommand.h"

namespace p4rt_app {
namespace sonic {

// Creates an AppDB update from an IR-level update request.
absl::StatusOr<swss::KeyOpFieldsValuesTuple> CreateAppDbVlanUpdate(
    p4::v1::Update::Type update_type, const pdpi::IrTableEntry& entry);

absl::StatusOr<swss::KeyOpFieldsValuesTuple> CreateAppDbVlanMemberUpdate(
    p4::v1::Update::Type update_type, const pdpi::IrTableEntry& entry);

// Pushes an update to the VLAN table.
absl::StatusOr<pdpi::IrUpdateStatus> PerformAppDbVlanUpdate(
    const VlanTable& vlan_table, const swss::KeyOpFieldsValuesTuple& update);

// Pushes an update to the VLAN_MEMBER table.
absl::StatusOr<pdpi::IrUpdateStatus> PerformAppDbVlanMemberUpdate(
    const VlanMemberTable& vlan_member_table,
    const swss::KeyOpFieldsValuesTuple& update);

// Returns all the VLAN_TABLE_P4 entries currently installed in AppDb. This
// does not include any entries that are currently being handled by the lower
// layers (i.e. keys starting with _).
absl::StatusOr<std::vector<pdpi::IrTableEntry>> GetAllAppDbVlanTableEntries(
    VlanTable& vlan_table);

// Returns all the VLAN_MEMBER_TABLE_P4 entries currently installed in AppDb.
// This does not include any entries that are currently being handled by the
// lower layers (i.e. keys starting with _).
absl::StatusOr<std::vector<pdpi::IrTableEntry>>
GetAllAppDbVlanMemberTableEntries(VlanMemberTable& vlan_member_table);

}  // namespace sonic
}  // namespace p4rt_app

#endif  // PINS_P4RT_APP_SONIC_VLAN_ENTRY_TRANSLATION_H_
