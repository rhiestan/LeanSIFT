// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// lsift_bench: compares LeanSift with VLFeat on speed and agreement.
//
//   lsift_bench [--repeat N] [--threads N] [--first-octave N] [--no-vlfeat] [--no-single] image...
//
// All images are first timed with N threads, then with one thread, then with
// VLFeat: long single-threaded runs can leave the CPU in a state where the
// following multi-threaded runs are slower.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "compare.h"
#include "image_io.h"
#include "lsift/lsift.hpp"
#include "vlfeat_ref.h"

namespace {

struct Timing {
  double min_ms = 0;
  double median_ms = 0;
};

template <class F>
Timing Time(int repeat, F&& f) {
  std::vector<double> t;
  for (int r = 0; r < repeat; ++r) {
    const auto t0 = std::chrono::steady_clock::now();
    f();
    const auto t1 = std::chrono::steady_clock::now();
    t.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(t.begin(), t.end());
  return {t.front(), t[t.size() / 2]};
}

struct Image {
  std::string name;
  int width = 0, height = 0;
  std::vector<float> pixels;
  lsift::Features features;
  Timing multi, single, vlfeat;
  std::string agreement = "-";
};

}  // namespace

int main(int argc, char** argv) {
  int repeat = 5;
  unsigned threads = 0;
  bool skip_vlfeat = false;
  bool skip_single = false;
  lsift::Params params;
  std::vector<std::string> paths;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--repeat" && i + 1 < argc) repeat = std::max(1, std::atoi(argv[++i]));
    else if (a == "--threads" && i + 1 < argc) threads = static_cast<unsigned>(std::atoi(argv[++i]));
    else if (a == "--first-octave" && i + 1 < argc) params.first_octave = std::atoi(argv[++i]);
    else if (a == "--no-vlfeat") skip_vlfeat = true;
    else if (a == "--no-single") skip_single = true;
    else paths.push_back(a);
  }
  if (paths.empty()) {
    std::fprintf(stderr,
                 "usage: lsift_bench [--repeat N] [--threads N] [--first-octave N] [--no-vlfeat] [--no-single] "
                 "image...\n");
    return 2;
  }

  std::vector<Image> images;
  for (const std::string& path : paths) {
    try {
      const lsift::support::GrayImage g = lsift::support::LoadGray(path);
      Image img;
      img.name = path.substr(path.find_last_of("/\\") + 1);
      img.width = g.width;
      img.height = g.height;
      img.pixels = g.to_float();
      images.push_back(std::move(img));
    } catch (const std::exception& e) {
      std::fprintf(stderr, "%s: %s\n", path.c_str(), e.what());
    }
  }

  lsift::ThreadPool pool(threads);
  for (Image& img : images) {
    const auto view = lsift::ImageView::gray32f(img.pixels.data(), img.width, img.height);
    lsift::Sift sift(params, &pool);
    sift.extract(view, img.features);  // warm-up: allocations, thread start
    img.multi = Time(repeat, [&] { sift.extract(view, img.features); });
  }
  if (!skip_single) {
    lsift::SerialExecutor serial;
    for (Image& img : images) {
      const auto view = lsift::ImageView::gray32f(img.pixels.data(), img.width, img.height);
      lsift::Sift sift(params, &serial);
      lsift::Features f;
      sift.extract(view, f);
      img.single = Time(repeat, [&] { sift.extract(view, f); });
    }
  }
  if (!skip_vlfeat) {
    for (Image& img : images) {
      lsift::Features ref;
      img.vlfeat = Time(std::min(repeat, 3), [&] {
        ref = lsift::support::RunVLFeat(img.pixels.data(), img.width, img.height, params);
      });
      const lsift::support::Agreement a = lsift::support::CompareFeatures(ref, img.features);
      char buf[160];
      std::snprintf(buf, sizeof(buf), "recall %.4f, precision %.4f, descriptor p99 %.4f", a.recall(), a.precision(),
                    a.p99_descriptor_distance);
      img.agreement = buf;
    }
  }

  std::printf("SIMD target: %s, threads: %u. Times in ms: median (minimum) of %d runs.\n", lsift::simd_target_name(),
              pool.concurrency(), repeat);
  std::printf("%-18s %10s %8s %9s %17s %17s %8s  agreement with VLFeat\n", "image", "size", "features", "VLFeat",
              "LeanSift 1 thr", "LeanSift N thr", "speedup");
  for (const Image& img : images) {
    char size[32], st[32] = "-", mt[32], ref[32] = "-", speedup[32] = "-";
    std::snprintf(size, sizeof(size), "%dx%d", img.width, img.height);
    if (!skip_single) std::snprintf(st, sizeof(st), "%.1f (%.1f)", img.single.median_ms, img.single.min_ms);
    std::snprintf(mt, sizeof(mt), "%.1f (%.1f)", img.multi.median_ms, img.multi.min_ms);
    if (!skip_vlfeat) {
      std::snprintf(ref, sizeof(ref), "%.0f", img.vlfeat.median_ms);
      std::snprintf(speedup, sizeof(speedup), "%.1fx", img.vlfeat.median_ms / img.multi.median_ms);
    }
    std::printf("%-18s %10s %8zu %9s %17s %17s %8s  %s\n", img.name.c_str(), size, img.features.size(), ref, st, mt,
                speedup, img.agreement.c_str());
  }
  return 0;
}
