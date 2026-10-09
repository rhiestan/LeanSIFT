// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// LeanSift must produce the same features as VLFeat up to floating-point
// differences: the same keypoints (within a small position, scale and angle
// tolerance) and nearly identical descriptors.

#include <algorithm>
#include <ostream>
#include <string>

#include "compare.h"
#include "gtest/gtest.h"
#include "lsift/lsift.hpp"
#include "test_util.h"
#include "vlfeat_ref.h"

namespace {

struct Case {
  std::string image;
  int first_octave;
  double peak_threshold;
  bool upright;
};

std::ostream& operator<<(std::ostream& os, const Case& c) {
  return os << c.image << " first_octave=" << c.first_octave << " peak=" << c.peak_threshold
            << (c.upright ? " upright" : "");
}

class VLFeatAgreement : public ::testing::TestWithParam<Case> {};

TEST_P(VLFeatAgreement, SameFeatures) {
  const Case& c = GetParam();
  lsift::support::GrayImage img = lsift::test::TestImage(c.image);
  if (c.first_octave < -1) {
    // VLFeat mixes up width and height when it doubles the image more than
    // once (vl_sift_process_first_octave, "double more"), which only gives the
    // right result for square images.
    const int n = std::min(img.width, img.height);
    lsift::support::GrayImage square;
    square.width = square.height = n;
    for (int y = 0; y < n; ++y) {
      square.pixels.insert(square.pixels.end(), img.pixels.begin() + static_cast<std::ptrdiff_t>(y) * img.width,
                           img.pixels.begin() + static_cast<std::ptrdiff_t>(y) * img.width + n);
    }
    img = square;
  }
  const std::vector<float> fimg = img.to_float();

  lsift::Params params;
  params.first_octave = c.first_octave;
  params.peak_threshold = c.peak_threshold;
  params.upright = c.upright;

  const lsift::Features ref = lsift::support::RunVLFeat(fimg.data(), img.width, img.height, params);
  lsift::Sift sift(params);
  const lsift::Features test = sift.extract(lsift::ImageView::gray32f(fimg.data(), img.width, img.height));

  const lsift::support::Agreement a = lsift::support::CompareFeatures(ref, test);
  RecordProperty("agreement", a.summary());
  std::printf("[ agreement ] %s\n", a.summary().c_str());
  ASSERT_GT(a.ref_count, 0u);
  // Thresholds are decided on floats near a cut-off (peak threshold, edge
  // ratio, 80% orientation peak), so a few borderline keypoints may differ.
  EXPECT_GE(a.recall(), 0.99);
  EXPECT_GE(a.precision(), 0.99);
  // Descriptors are unit vectors; distances between unrelated SIFT
  // descriptors are typically 0.5 - 1.3.
  EXPECT_LE(a.median_descriptor_distance, 0.001);
  EXPECT_LE(a.p99_descriptor_distance, 0.01);
}

INSTANTIATE_TEST_SUITE_P(
    Images, VLFeatAgreement,
    ::testing::Values(Case{"box.pgm", -1, 0.02 / 3, false}, Case{"box.pgm", 0, 0.02 / 3, false},
                      Case{"box.pgm", 1, 0.02 / 3, false}, Case{"box.pgm", -1, 0.0, false},
                      Case{"box.pgm", -1, 0.02 / 3, true}, Case{"roofs1.jpg", -1, 0.02 / 3, false},
                      Case{"roofs2.jpg", 0, 0.01, false}, Case{"river1.jpg", -1, 0.02 / 3, false},
                      Case{"spots.jpg", -1, 0.0, false}, Case{"box.pgm", -2, 0.02 / 3, false},
                      Case{"roofs1.jpg", 2, 0.0, false}),
    [](const ::testing::TestParamInfo<Case>& info) {
      std::string name = info.param.image.substr(0, info.param.image.find('.')) + "_o" +
                         std::to_string(info.param.first_octave + 2) + "_" + std::to_string(info.index);
      return name;
    });

}  // namespace
