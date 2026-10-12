// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// lsift_microbench: throughput of individual kernels (development aid).

#include <chrono>
#include <cmath>
#include <string>
#include <cstdio>
#include <vector>

#include "hwy/aligned_allocator.h"
#include "kernels.h"

namespace {

using Clock = std::chrono::steady_clock;

// Streams `rows` output rows of a vertical+horizontal blur through a sliding
// window, like the scale-space pipeline does, and reports ns per pixel.
void BenchBlur(int width, int radius, int rows_out) {
  using namespace lsift::detail;
  const std::ptrdiff_t stride = (width + 15) / 16 * 16 + 16;
  const int rows_in = rows_out + 2 * radius;
  auto in = hwy::AllocateAligned<float>(stride * (rows_in + 2) + 2 * kPlaneGuard);
  auto out = hwy::AllocateAligned<float>(stride * 4 + 2 * kPlaneGuard);
  auto scratch = hwy::AllocateAligned<float>(BlurRowScratchSize(static_cast<int>(stride), radius) + 64);
  for (std::ptrdiff_t i = 0; i < stride * (rows_in + 2) + 2 * kPlaneGuard; ++i) in[i] = float(i % 97) / 97.0f;
  std::vector<float> kernel(radius + 1, 1.0f / (2 * radius + 1));
  std::vector<const float*> ptrs(2 * radius + 1);
  float* base = in.get() + kPlaneGuard;

  double best = 1e300;
  for (int rep = 0; rep < 5; ++rep) {
    const auto t0 = Clock::now();
    for (int y = 0; y < rows_out; ++y) {
      for (int j = 0; j <= 2 * radius; ++j) ptrs[j] = base + (y + j) * stride;
      BlurRow(ptrs.data(), radius, kernel.data(), width, 0, width, out.get() + kPlaneGuard + (y % 4) * stride,
              scratch.get());
    }
    const auto t1 = Clock::now();
    best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
  }
  double best_pair = 1e300;
  std::vector<const float*> pair_ptrs(2 * radius + 2);
  for (int rep = 0; rep < 5; ++rep) {
    const auto t0 = Clock::now();
    for (int y = 0; y + 1 < rows_out; y += 2) {
      for (int j = 0; j <= 2 * radius + 1; ++j) pair_ptrs[j] = base + (y + j) * stride;
      float* o = out.get() + kPlaneGuard;
      BlurRowPair(pair_ptrs.data(), radius, kernel.data(), width, 0, width, o, o + 2 * stride, scratch.get());
    }
    const auto t1 = Clock::now();
    best_pair = std::min(best_pair, std::chrono::duration<double, std::nano>(t1 - t0).count());
  }
  double best_quad = 1e300;
  std::vector<const float*> quad_ptrs(2 * radius + 4);
  for (int rep = 0; rep < 5; ++rep) {
    const auto t0 = Clock::now();
    for (int y = 0; y + 3 < rows_out; y += 4) {
      for (int j = 0; j <= 2 * radius + 3; ++j) quad_ptrs[j] = base + (y + j) * stride;
      float* o = out.get() + kPlaneGuard;
      BlurRowQuad(quad_ptrs.data(), radius, kernel.data(), width, 0, width, o, o + stride, o + 2 * stride,
                  o + 3 * stride, scratch.get());
    }
    const auto t1 = Clock::now();
    best_quad = std::min(best_quad, std::chrono::duration<double, std::nano>(t1 - t0).count());
  }
  std::printf("blur w=%5d r=%2d: %.3f ns/px, pairs %.3f ns/px, quads %.3f ns/px\n", width, radius,
              best / (double(width) * rows_out), best_pair / (double(width) * (rows_out / 2 * 2)),
              best_quad / (double(width) * (rows_out / 4 * 4)));
}

// Descriptors of random keypoints on a smooth random image.
void BenchDescriptor(double sigma) {
  using namespace lsift::detail;
  const int w = 1024, h = 768;
  const std::ptrdiff_t stride = w + 16;
  auto mem = hwy::AllocateAligned<float>(stride * h + 2 * kPlaneGuard);
  float* data = mem.get() + kPlaneGuard;
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      data[y * stride + x] = 0.5f + 0.25f * std::sin(x * 0.11f + y * 0.05f) * std::cos(y * 0.13f - x * 0.03f) +
                             0.1f * std::sin(x * 0.7f) * std::sin(y * 0.9f);
    }
  }
  const Plane g{data, w, h, stride};
  DescriptorWindow win;
  win.bin_size = 3.0 * sigma;
  win.window_size = 2.0;
  win.radius = static_cast<int>(std::floor(std::sqrt(2.0) * win.bin_size * 5 / 2.0 + 0.5));
  auto scratch = hwy::AllocateAligned<float>(DescriptorScratchSize(win.radius) + 64);
  float desc[128];
  const int n = 4000;
  double best = 1e300, sink = 0;
  for (int rep = 0; rep < 5; ++rep) {
    const auto t0 = Clock::now();
    for (int i = 0; i < n; ++i) {
      win.x = 100 + (i * 37) % (w - 200) + 0.3;
      win.y = 100 + (i * 53) % (h - 200) + 0.6;
      win.angle = (i % 64) * 0.098;
      DescriptorHistogram(g, win, desc, scratch.get());
      sink += desc[i % 128];
    }
    const auto t1 = Clock::now();
    best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count());
  }
  // Count the samples that fall into a bin, for the last keypoint.
  double samples = 0;
  {
    const int xi = static_cast<int>(win.x + 0.5), yi = static_cast<int>(win.y + 0.5);
    const double ct = std::cos(win.angle) / win.bin_size, st = std::sin(win.angle) / win.bin_size;
    for (int dy = -win.radius; dy <= win.radius; ++dy) {
      for (int dx = -win.radius; dx <= win.radius; ++dx) {
        const double fx = xi + dx - win.x, fy = yi + dy - win.y;
        const double nx = ct * fx + st * fy, ny = -st * fx + ct * fy;
        if (nx >= -2.5 && nx < 2.5 && ny >= -2.5 && ny < 2.5) samples += 1;
      }
    }
  }
  std::printf("descriptor sigma=%.1f radius=%d: %.2f us, %.2f ns/sample (%g)\n", sigma, win.radius,
              best / n / 1000.0, best / n / samples, sink > 0 ? 0.0 : 1.0);
}

}  // namespace

int main(int argc, char** argv) {
  std::printf("SIMD target: %s\n", lsift::detail::TargetName());
  if (argc > 1 && std::string(argv[1]) == "desc") {
    for (double s : {2.0, 3.0, 4.0}) BenchDescriptor(s);
    return 0;
  }
  for (int r : {5, 8, 13}) {
    for (int w : {256, 512, 1024, 2048, 4096}) BenchBlur(w, r, 4 * 1048576 / w);
  }
  return 0;
}
