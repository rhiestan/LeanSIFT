// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// lsift_bench: compares LeanSift with VLFeat on speed and agreement.
//
//   lsift_bench [--repeat N] [--threads N] [--first-octave N] [--no-vlfeat] image...

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

}  // namespace

int main(int argc, char** argv) {
  int repeat = 5;
  unsigned threads = 0;
  bool skip_vlfeat = false;
  lsift::Params params;
  std::vector<std::string> images;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--repeat" && i + 1 < argc) repeat = std::max(1, std::atoi(argv[++i]));
    else if (a == "--threads" && i + 1 < argc) threads = static_cast<unsigned>(std::atoi(argv[++i]));
    else if (a == "--first-octave" && i + 1 < argc) params.first_octave = std::atoi(argv[++i]);
    else if (a == "--no-vlfeat") skip_vlfeat = true;
    else images.push_back(a);
  }
  if (images.empty()) {
    std::fprintf(stderr, "usage: lsift_bench [--repeat N] [--threads N] [--first-octave N] [--no-vlfeat] image...\n");
    return 2;
  }

  lsift::ThreadPool pool(threads);
  lsift::SerialExecutor serial;
  std::printf("SIMD target: %s, threads: %u. Times in ms: median (minimum) of %d runs.\n", lsift::simd_target_name(),
              pool.concurrency(), repeat);
  std::printf("%-18s %10s %8s %9s %17s %17s %8s  agreement with VLFeat\n", "image", "size", "features", "VLFeat",
              "LeanSift 1 thr", "LeanSift N thr", "speedup");

  for (const std::string& path : images) {
    try {
      const lsift::support::GrayImage img = lsift::support::LoadGray(path);
      const std::vector<float> fimg = img.to_float();
      const lsift::ImageView view = lsift::ImageView::gray32f(fimg.data(), img.width, img.height);

      lsift::Sift sift_mt(params, &pool);
      lsift::Sift sift_st(params, &serial);
      lsift::Features f_mt, f_st, f_ref;
      sift_mt.extract(view, f_mt);  // warm-up: allocations, thread start
      sift_st.extract(view, f_st);

      const Timing t_st = Time(repeat, [&] { sift_st.extract(view, f_st); });
      const Timing t_mt = Time(repeat, [&] { sift_mt.extract(view, f_mt); });
      Timing t_ref;
      std::string agreement = "-";
      if (!skip_vlfeat) {
        t_ref = Time(std::min(repeat, 3), [&] {
          f_ref = lsift::support::RunVLFeat(fimg.data(), img.width, img.height, params);
        });
        const lsift::support::Agreement a = lsift::support::CompareFeatures(f_ref, f_mt);
        char buf[160];
        std::snprintf(buf, sizeof(buf), "recall %.4f, precision %.4f, descriptor p99 %.4f", a.recall(), a.precision(),
                      a.p99_descriptor_distance);
        agreement = buf;
      }
      const std::string name = path.substr(path.find_last_of("/\\") + 1);
      char size[32], st[32], mt[32], ref[32];
      std::snprintf(size, sizeof(size), "%dx%d", img.width, img.height);
      std::snprintf(st, sizeof(st), "%.1f (%.1f)", t_st.median_ms, t_st.min_ms);
      std::snprintf(mt, sizeof(mt), "%.1f (%.1f)", t_mt.median_ms, t_mt.min_ms);
      std::snprintf(ref, sizeof(ref), skip_vlfeat ? "-" : "%.0f", t_ref.median_ms);
      std::printf("%-18s %10s %8zu %9s %17s %17s %7.1fx  %s\n", name.c_str(), size, f_mt.size(), ref, st, mt,
                  skip_vlfeat ? 0.0 : t_ref.median_ms / t_mt.median_ms, agreement.c_str());
    } catch (const std::exception& e) {
      std::fprintf(stderr, "%s: %s\n", path.c_str(), e.what());
    }
  }
  return 0;
}
