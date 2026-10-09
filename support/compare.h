// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// Metrics for comparing two SIFT implementations on the same image, and for
// measuring matching quality on an image pair with a known homography.

#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "lsift/lsift.hpp"

namespace lsift::support {

/// When a keypoint of one set counts as the same as one of the other set.
struct KeypointTolerance {
  double position = 0.05;  ///< in pixels of the keypoint's octave
  double log2_scale = 0.01;
  double angle = 0.05;     ///< radians
};

struct Agreement {
  std::size_t ref_count = 0;
  std::size_t test_count = 0;
  std::size_t matched = 0;  ///< one-to-one pairs within the tolerance

  // Over the matched pairs:
  double max_position_error = 0;  ///< octave pixels
  double max_angle_error = 0;     ///< radians
  double median_descriptor_distance = 0;  ///< L2, descriptors have unit norm
  double p99_descriptor_distance = 0;
  double max_descriptor_distance = 0;

  double recall() const { return ref_count ? double(matched) / double(ref_count) : 1.0; }
  double precision() const { return test_count ? double(matched) / double(test_count) : 1.0; }
  std::string summary() const;
};

/// Pairs every reference keypoint with the closest unused test keypoint of the
/// same octave within the tolerance.
Agreement CompareFeatures(const Features& ref, const Features& test, const KeypointTolerance& tol = {});

using Homography = std::array<double, 9>;  // row-major, maps image A to image B

struct MatchingResult {
  std::size_t features_a = 0;
  std::size_t features_b = 0;
  std::size_t matches = 0;  ///< nearest neighbours passing the ratio test
  std::size_t correct = 0;  ///< matches consistent with the homography
  double precision() const { return matches ? double(correct) / double(matches) : 0.0; }
  std::string summary() const;
};

/// Brute-force nearest-neighbour matching from A to B with Lowe's ratio test.
/// A match is correct when H maps the point of A within `max_error` pixels
/// of the point of B.
MatchingResult MatchWithHomography(const Features& a, const Features& b, const Homography& H, double ratio = 0.8,
                                   double max_error = 3.0);

}  // namespace lsift::support
