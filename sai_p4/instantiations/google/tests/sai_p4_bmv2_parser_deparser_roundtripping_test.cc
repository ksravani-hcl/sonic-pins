#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "devtools/build/runtime/get_runfiles_dir.h"
#include "file/base/helpers.h"
#include "file/base/options.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gutil/proto_matchers.h"
#include "gutil/status.h"
#include "gutil/status_matchers.h"
#include "gutil/testing.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4_pdpi/p4_runtime_session.h"
#include "p4_pdpi/packetlib/packetlib.h"
#include "p4_pdpi/packetlib/packetlib.pb.h"
#include "platforms/networking/p4/p4_infra/bmv2/bmv2.h"

namespace pins {
namespace {

using ::gutil::EqualsProto;
using ::gutil::IsOkAndHolds;
using ::orion::p4::test::Bmv2;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

void PreparePacketOrDie(packetlib::Packet& packet) {
  CHECK_OK(packetlib::PadPacketToMinimumSize(packet).status());  // Crash OK.
  CHECK_OK(
      packetlib::UpdateMissingComputedFields(packet).status());  // Crash OK.
}

packetlib::Packet GetIpv4PacketOrDie() {
  auto packet = gutil::ParseProtoOrDie<packetlib::Packet>(R"pb(
    headers {
      ethernet_header {
        ethernet_destination: "02:03:04:05:06:07"
        ethernet_source: "00:01:02:03:04:05"
        ethertype: "0x0800"  # IPv4
      }
    }
    headers {
      ipv4_header {
        version: "0x4"
        ihl: "0x5"
        dscp: "0x1c"
        ecn: "0x0"
        identification: "0x0000"
        flags: "0x0"
        fragment_offset: "0x0000"
        ttl: "0x20"
        protocol: "0xfe"
        ipv4_source: "192.168.100.2"
        ipv4_destination: "192.168.100.1"
      }
    }
    payload: "Untagged IPv4 packet."
  )pb");
  PreparePacketOrDie(packet);
  return packet;
}

absl::StatusOr<p4::v1::ForwardingPipelineConfig> GetBmv2ForwardingConfig() {
  p4::v1::ForwardingPipelineConfig config;
  ASSIGN_OR_RETURN(*config.mutable_p4info(),
                   file::GetTextProto<p4::config::v1::P4Info>(
                       devtools_build::GetDataDependencyFilepath(
                           "google3/third_party/pins_infra/sai_p4/"
                           "instantiations/google/tests/"
                           "testdata/forward_all.p4info.pb.txt"),
                       file::Defaults()));

  ASSIGN_OR_RETURN(
      *config.mutable_p4_device_config(),
      file::GetContents(devtools_build::GetDataDependencyFilepath(
                            "google3/third_party/pins_infra/sai_p4/"
                            "instantiations/google/tests/"
                            "testdata/forward_all.bmv2.json"),
                        file::Defaults()));
  return config;
}

// Parser/deparser roundtripping is relied upon by BMv2 generally as well as our
TEST(SimpleSaiTest, ParserAndDeparserRoundtrip) {
  constexpr int kIngressPort = 1;

  ASSERT_OK_AND_ASSIGN(Bmv2 bmv2, Bmv2::Create(/*args=*/{}));
  ASSERT_OK_AND_ASSIGN(p4::v1::ForwardingPipelineConfig config,
                       GetBmv2ForwardingConfig());
  ASSERT_OK(pdpi::SetMetadataAndSetForwardingPipelineConfig(
      &bmv2.P4RuntimeSession(),
      p4::v1::SetForwardingPipelineConfigRequest::VERIFY_AND_COMMIT, config));

  packetlib::Packet input_packet = GetIpv4PacketOrDie();
  packetlib::Packets output_packets;
  *output_packets.add_packets() = input_packet;

  EXPECT_THAT(bmv2.SendPacket(kIngressPort, input_packet),
              IsOkAndHolds(UnorderedElementsAre(
                  Pair(kIngressPort, EqualsProto(output_packets)))))
      << "The input packet and output packets are different, but we expect "
         "them to always be the same in this test. There are two possible "
         "likely causes:"
         "\n  1. The SAI P4 parser and deparser may not be in sync."
         "\n  2. Packetlib parsing and deparsing may not correctly roundtrip "
         "for all packets."
}

}  // namespace
}  // namespace pins
