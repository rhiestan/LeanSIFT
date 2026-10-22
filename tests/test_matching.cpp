// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// Matching quality on an image and a synthetically warped copy with a known
// homography. LeanSift must match as well as VLFeat.

#include <cmath>
#include <cstdio>
#include <vector>

#include "compare.h"
#include "doctest.h"
#include "lsift/lsift.hpp"
#include "test_util.h"
#include "vlfeat_ref.h"

namespace {

// Warps `src` with the homography H (src -> dst) by inverse mapping and
// bilinear interpolation; pixels mapping outside the source are 0.
std::vector<float> Warp(const std::vector<float>& src, int w, int h, const lsift::support::Homography& H) {
  // Inverse of H by the adjugate.
  const double a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], hh = H[7], i = H[8];
  const double inv[9] = {e * i - f * hh, c * hh - b * i, b * f - c * e, f * g - d * i, a * i - c * g,
                         c * d - a * f,  d * hh - e * g, b * g - a * hh, a * e - b * d};
  std::vector<float> dst(src.size(), 0.0f);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const double wz = inv[6] * x + inv[7] * y + inv[8];
      const double sx = (inv[0] * x + inv[1] * y + inv[2]) / wz;
      const double sy = (inv[3] * x + inv[4] * y + inv[5]) / wz;
      const int x0 = static_cast<int>(std::floor(sx)), y0 = static_cast<int>(std::floor(sy));
      if (x0 < 0 || y0 < 0 || x0 + 1 >= w || y0 + 1 >= h) continue;
      const double fx = sx - x0, fy = sy - y0;
      const float* p = &src[static_cast<std::size_t>(y0) * w + x0];
      dst[static_cast<std::size_t>(y) * w + x] =
          static_cast<float>((1 - fy) * ((1 - fx) * p[0] + fx * p[1]) + fy * ((1 - fx) * p[w] + fx * p[w + 1]));
    }
  }
  return dst;
}

TEST_CASE("Matching.RotatedAndScaledCopy") {
  const auto& img = lsift::test::TestImage("roofs1.jpg");
  const int w = img.width, h = img.height;
  const std::vector<float> a = img.to_float();

  // Rotate by 25 degrees and scale by 0.75 around the image centre.
  const double angle = 25.0 * 3.14159265358979 / 180.0, s = 0.75;
  const double cx = w / 2.0, cy = h / 2.0;
  const double ca = s * std::cos(angle), sa = s * std::sin(angle);
  const lsift::support::Homography H = {ca, -sa, cx - ca * cx + sa * cy, sa, ca, cy - sa * cx - ca * cy, 0, 0, 1};
  const std::vector<float> b = Warp(a, w, h, H);

  lsift::Params params;
  lsift::Sift sift(params);
  const lsift::Features fa = sift.extract(lsift::ImageView::gray32f(a.data(), w, h));
  const lsift::Features fb = sift.extract(lsift::ImageView::gray32f(b.data(), w, h));
  const lsift::Features ra = lsift::support::RunVLFeat(a.data(), w, h, params);
  const lsift::Features rb = lsift::support::RunVLFeat(b.data(), w, h, params);

  const lsift::support::MatchingResult ours = lsift::support::MatchWithHomography(fa, fb, H);
  const lsift::support::MatchingResult theirs = lsift::support::MatchWithHomography(ra, rb, H);
  std::printf("[ LeanSift ] %s\n[ VLFeat   ] %s\n", ours.summary().c_str(), theirs.summary().c_str());

  REQUIRE(theirs.correct > 100u);
  CHECK(ours.correct >= 0.98 * theirs.correct);
  CHECK(ours.precision() >= theirs.precision() - 0.01);
}

}  // namespace
