// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// LeanSift must produce the same features as VLFeat up to floating-point
// differences: the same keypoints (within a small position, scale and angle
// tolerance) and nearly identical descriptors.

#include <algorithm>
#include <cstdio>
#include <string>

#include "compare.h"
#include "doctest.h"
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

void ExpectSameFeatures(const Case& c) {
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
  std::printf("[ agreement ] %s\n", a.summary().c_str());
  REQUIRE(a.ref_count > 0u);
  // Thresholds are decided on floats near a cut-off (peak threshold, edge
  // ratio, 80% orientation peak), so a few borderline keypoints may differ.
  CHECK(a.recall() >= 0.99);
  CHECK(a.precision() >= 0.99);
  // Descriptors are unit vectors; distances between unrelated SIFT
  // descriptors are typically 0.5 - 1.3.
  CHECK(a.median_descriptor_distance <= 0.001);
  CHECK(a.p99_descriptor_distance <= 0.01);
}

// One test case each, so that CTest can run them in parallel.
TEST_CASE("VLFeatAgreement.box") { ExpectSameFeatures({"box.pgm", -1, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.box_first_octave_0") { ExpectSameFeatures({"box.pgm", 0, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.box_first_octave_1") { ExpectSameFeatures({"box.pgm", 1, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.box_first_octave_-2") { ExpectSameFeatures({"box.pgm", -2, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.box_peak_0") { ExpectSameFeatures({"box.pgm", -1, 0.0, false}); }
TEST_CASE("VLFeatAgreement.box_upright") { ExpectSameFeatures({"box.pgm", -1, 0.02 / 3, true}); }
TEST_CASE("VLFeatAgreement.roofs1") { ExpectSameFeatures({"roofs1.jpg", -1, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.roofs1_first_octave_2") { ExpectSameFeatures({"roofs1.jpg", 2, 0.0, false}); }
TEST_CASE("VLFeatAgreement.roofs2_first_octave_0") { ExpectSameFeatures({"roofs2.jpg", 0, 0.01, false}); }
TEST_CASE("VLFeatAgreement.river1") { ExpectSameFeatures({"river1.jpg", -1, 0.02 / 3, false}); }
TEST_CASE("VLFeatAgreement.spots") { ExpectSameFeatures({"spots.jpg", -1, 0.0, false}); }

}  // namespace
