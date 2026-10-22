// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// Every SIMD target compiled into the library and supported by this CPU must
// agree with VLFeat, not only the one chosen at run time.
//
// To cover targets this CPU lacks, run the test under an emulator (Intel SDE,
// QEMU; see tests/CMakeLists.txt) and set LSIFT_REQUIRE_TARGETS to a
// comma-separated list of target names (e.g. "AVX3,AVX3_SPR") or to "all": the
// test then fails unless all of these that are compiled into the library were
// tested.

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>
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

  std::set<std::string> tested;
  for (const std::int64_t target : hwy::SupportedAndGeneratedTargets()) {
    hwy::SetSupportedTargetsForTest(target);
    const std::string name = hwy::TargetName(target);
    INFO(name);
    CHECK(std::string(lsift::simd_target_name()) == name);
    lsift::SerialExecutor serial;
    const lsift::Features test =
        lsift::Sift(params, &serial).extract(lsift::ImageView::gray32f(fimg.data(), img.width, img.height));
    const lsift::support::Agreement a = lsift::support::CompareFeatures(ref, test);
    std::printf("[ %-9s ] %s\n", name.c_str(), a.summary().c_str());
    CHECK(a.recall() >= 0.99);
    CHECK(a.precision() >= 0.99);
    CHECK(a.p99_descriptor_distance <= 0.01);
    tested.insert(name);
  }
  hwy::SetSupportedTargetsForTest(0);  // back to the best target
  CHECK(!tested.empty());

  // The targets compiled into the library (one bit each in HWY_TARGETS).
  std::set<std::string> compiled;
  for (std::int64_t bits = HWY_TARGETS; bits != 0; bits &= bits - 1) {
    const std::string name = hwy::TargetName(bits & -bits);
    compiled.insert(name);
    if (tested.count(name) == 0) std::printf("[ %-9s ] not tested: not supported by this CPU\n", name.c_str());
  }

  if (const char* required = std::getenv("LSIFT_REQUIRE_TARGETS")) {
    std::set<std::string> names;
    if (std::string(required) == "all") {
      names = compiled;
    } else {
      std::stringstream list(required);
      std::string name;
      while (std::getline(list, name, ',')) {
        if (compiled.count(name) != 0) names.insert(name);
      }
    }
    for (const std::string& name : names) {
      CHECK_MESSAGE(tested.count(name) == 1, "required target " << name << " was not tested");
    }
  }
}

}  // namespace
