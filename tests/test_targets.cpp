// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// Every SIMD target compiled into the library and supported by this CPU must
// agree with VLFeat, not only the one chosen at run time.

#include <cstdio>
#include <string>
#include <vector>

#include "compare.h"
#include "doctest.h"
#include "hwy/targets.h"
#include "lsift/lsift.hpp"
#include "test_util.h"
#include "vlfeat_ref.h"

namespace {

TEST_CASE("Targets.EveryTargetAgreesWithVLFeat") {
  const auto& img = lsift::test::TestImage("roofs1.jpg");
  const std::vector<float> fimg = img.to_float();
  const lsift::Params params;
  const lsift::Features ref = lsift::support::RunVLFeat(fimg.data(), img.width, img.height, params);

  int tested = 0;
  for (const std::int64_t target : hwy::SupportedAndGeneratedTargets()) {
    hwy::SetSupportedTargetsForTest(target);
    const std::string name = lsift::simd_target_name();
    INFO(name);
    lsift::SerialExecutor serial;
    const lsift::Features test =
        lsift::Sift(params, &serial).extract(lsift::ImageView::gray32f(fimg.data(), img.width, img.height));
    const lsift::support::Agreement a = lsift::support::CompareFeatures(ref, test);
    std::printf("[ %-9s ] %s\n", name.c_str(), a.summary().c_str());
    CHECK(a.recall() >= 0.99);
    CHECK(a.precision() >= 0.99);
    CHECK(a.p99_descriptor_distance <= 0.01);
    ++tested;
  }
  hwy::SetSupportedTargetsForTest(0);  // back to the best target
  CHECK(tested > 0);
}

}  // namespace
