#ifndef PINS_TESTS_FORWARDING_CPU_BUBBLES_TEST_H_
#define PINS_TESTS_FORWARDING_CPU_BUBBLES_TEST_H_

#include <memory>

#include <optional>

#include "dvaas/dataplane_validation.h"
#include "gtest/gtest.h"
#include "p4/config/v1/p4info.pb.h"
#include "p4_pdpi/p4_runtime_session.h"
#include "sai_p4/instantiations/google/instantiations.h"
#include "thinkit/mirror_testbed_fixture.h"

namespace pins_test {

struct CpuBubblesTestParams {
  std::shared_ptr<thinkit::MirrorTestbedInterface> testbed;
  // If provided, installs the P4Info on the SUT. Otherwise, uses the P4Info
  // already on the SUT.
  std::optional<p4::config::v1::P4Info> sut_p4info;
  std::shared_ptr<dvaas::DataplaneValidator> validator;
  dvaas::DataplaneValidationParams validation_params;
};

class CpuBubblesTestFixture
    : public testing::TestWithParam<CpuBubblesTestParams> {
 public:
  void SetUp() override;
  void TearDown() override;
};

}  // namespace pins_test

#endif  // PINS_TESTS_FORWARDING_CPU_BUBBLES_TEST_H_
