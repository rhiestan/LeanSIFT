// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#include "compare.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_map>
#include <vector>

namespace lsift::support {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;

double AngleDistance(double a, double b) {
  double d = std::fmod(std::fabs(a - b), kTwoPi);
  return std::min(d, kTwoPi - d);
}

double DescriptorDistance(const float* a, const float* b) {
  double sum = 0;
  for (int i = 0; i < kDescriptorSize; ++i) {
    const double d = double(a[i]) - double(b[i]);
    sum += d * d;
  }
  return std::sqrt(sum);
}

double Percentile(std::vector<double> v, double p) {
  if (v.empty()) return 0;
  const std::size_t k = std::min(v.size() - 1, static_cast<std::size_t>(p * (v.size() - 1) + 0.5));
  std::nth_element(v.begin(), v.begin() + k, v.end());
  return v[k];
}

}  // namespace

std::string Agreement::summary() const {
  char buf[512];
  std::snprintf(buf, sizeof(buf),
                "ref %zu, test %zu, matched %zu (recall %.4f, precision %.4f); max position error %.4f px, "
                "max angle error %.4f rad; descriptor distance median %.5f, p99 %.5f, max %.5f",
                ref_count, test_count, matched, recall(), precision(), max_position_error, max_angle_error,
                median_descriptor_distance, p99_descriptor_distance, max_descriptor_distance);
  return buf;
}

Agreement CompareFeatures(const Features& ref, const Features& test, const KeypointTolerance& tol) {
  Agreement r;
  r.ref_count = ref.size();
  r.test_count = test.size();

  // Hash the test keypoints by octave and octave-pixel cell.
  auto key = [](int octave, long long cx, long long cy) {
    return (static_cast<long long>(octave + 16) << 56) ^ ((cx & 0xFFFFFFF) << 28) ^ (cy & 0xFFFFFFF);
  };
  std::unordered_map<long long, std::vector<std::size_t>> grid;
  for (std::size_t j = 0; j < test.size(); ++j) {
    const Keypoint& k = test.keypoints[j];
    const double xper = std::ldexp(1.0, k.octave);
    grid[key(k.octave, std::llround(k.x / xper), std::llround(k.y / xper))].push_back(j);
  }

  const bool with_desc = ref.descriptors.size() == ref.size() * kDescriptorSize &&
                         test.descriptors.size() == test.size() * kDescriptorSize;
  std::vector<char> used(test.size(), 0);
  std::vector<double> desc_dist;
  for (std::size_t i = 0; i < ref.size(); ++i) {
    const Keypoint& a = ref.keypoints[i];
    const double xper = std::ldexp(1.0, a.octave);
    const long long cx = std::llround(a.x / xper), cy = std::llround(a.y / xper);
    std::size_t best = test.size();
    double best_cost = std::numeric_limits<double>::infinity();
    double best_pos = 0, best_angle = 0;
    for (long long dy = -1; dy <= 1; ++dy) {
      for (long long dx = -1; dx <= 1; ++dx) {
        auto it = grid.find(key(a.octave, cx + dx, cy + dy));
        if (it == grid.end()) continue;
        for (std::size_t j : it->second) {
          if (used[j]) continue;
          const Keypoint& b = test.keypoints[j];
          const double pos = std::hypot(a.x - b.x, a.y - b.y) / xper;
          const double scale = std::fabs(std::log2(double(a.sigma) / b.sigma));
          const double angle = AngleDistance(a.angle, b.angle);
          if (pos > tol.position || scale > tol.log2_scale || angle > tol.angle) continue;
          const double cost = pos / tol.position + scale / tol.log2_scale + angle / tol.angle;
          if (cost < best_cost) {
            best_cost = cost;
            best = j;
            best_pos = pos;
            best_angle = angle;
          }
        }
      }
    }
    if (best == test.size()) continue;
    used[best] = 1;
    ++r.matched;
    r.max_position_error = std::max(r.max_position_error, best_pos);
    r.max_angle_error = std::max(r.max_angle_error, best_angle);
    if (with_desc) desc_dist.push_back(DescriptorDistance(ref.descriptor(i), test.descriptor(best)));
  }
  if (!desc_dist.empty()) {
    r.median_descriptor_distance = Percentile(desc_dist, 0.5);
    r.p99_descriptor_distance = Percentile(desc_dist, 0.99);
    r.max_descriptor_distance = *std::max_element(desc_dist.begin(), desc_dist.end());
  }
  return r;
}

std::string MatchingResult::summary() const {
  char buf[256];
  std::snprintf(buf, sizeof(buf), "features %zu / %zu, matches %zu, correct %zu (precision %.4f)", features_a,
                features_b, matches, correct, precision());
  return buf;
}

MatchingResult MatchWithHomography(const Features& a, const Features& b, const Homography& H, double ratio,
                                   double max_error) {
  MatchingResult r;
  r.features_a = a.size();
  r.features_b = b.size();
  if (b.size() < 2) return r;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const float* da = a.descriptor(i);
    double best = std::numeric_limits<double>::infinity(), second = best;
    std::size_t best_j = 0;
    for (std::size_t j = 0; j < b.size(); ++j) {
      const float* db = b.descriptor(j);
      double sum = 0;
      for (int k = 0; k < kDescriptorSize && sum < second; ++k) {
        const double d = double(da[k]) - double(db[k]);
        sum += d * d;
      }
      if (sum < best) {
        second = best;
        best = sum;
        best_j = j;
      } else if (sum < second) {
        second = sum;
      }
    }
    if (!(std::sqrt(best) < ratio * std::sqrt(second))) continue;
    ++r.matches;
    const Keypoint& ka = a.keypoints[i];
    const Keypoint& kb = b.keypoints[best_j];
    const double w = H[6] * ka.x + H[7] * ka.y + H[8];
    const double px = (H[0] * ka.x + H[1] * ka.y + H[2]) / w;
    const double py = (H[3] * ka.x + H[4] * ka.y + H[5]) / w;
    if (std::hypot(px - kb.x, py - kb.y) <= max_error) ++r.correct;
  }
  return r;
}

}  // namespace lsift::support
