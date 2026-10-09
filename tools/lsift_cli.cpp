// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause
//
// lsift: extract SIFT features from an image.
//
//   lsift [options] image [output]
//
// The output has one line per feature: x y sigma angle followed by the 128
// descriptor values as bytes (VLFeat's 512 * d, clamped to 255).

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "image_io.h"
#include "lsift/lsift.hpp"

namespace {

void Usage() {
  std::fprintf(stderr,
               "usage: lsift [options] image [output.txt]\n"
               "  --first-octave N    first octave (default -1)\n"
               "  --octaves N         number of octaves (default: all)\n"
               "  --levels N          levels per octave (default 3)\n"
               "  --peak-thresh T     peak threshold (default 0.02/3)\n"
               "  --edge-thresh T     edge threshold (default 10)\n"
               "  --upright           do not compute orientations\n"
               "  --no-descriptors    only detect keypoints\n"
               "  --threads N         worker threads (default: all hardware threads)\n"
               "  --repeat N          run N times and report the fastest\n");
}

}  // namespace

int main(int argc, char** argv) {
  lsift::Params params;
  unsigned threads = 0;
  int repeat = 1;
  std::string input, output;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* {
      if (i + 1 >= argc) {
        Usage();
        std::exit(2);
      }
      return argv[++i];
    };
    if (a == "--first-octave") params.first_octave = std::atoi(next());
    else if (a == "--octaves") params.num_octaves = std::atoi(next());
    else if (a == "--levels") params.levels_per_octave = std::atoi(next());
    else if (a == "--peak-thresh") params.peak_threshold = std::atof(next());
    else if (a == "--edge-thresh") params.edge_threshold = std::atof(next());
    else if (a == "--upright") params.upright = true;
    else if (a == "--no-descriptors") params.compute_descriptors = false;
    else if (a == "--threads") threads = static_cast<unsigned>(std::atoi(next()));
    else if (a == "--repeat") repeat = std::max(1, std::atoi(next()));
    else if (a == "-h" || a == "--help") {
      Usage();
      return 0;
    } else if (input.empty()) input = a;
    else if (output.empty()) output = a;
    else {
      Usage();
      return 2;
    }
  }
  if (input.empty()) {
    Usage();
    return 2;
  }

  try {
    const lsift::support::GrayImage img = lsift::support::LoadGray(input);
    lsift::ThreadPool pool(threads);
    lsift::Sift sift(params, &pool);
    lsift::Features features;
    double best_ms = 1e300;
    for (int r = 0; r < repeat; ++r) {
      const auto t0 = std::chrono::steady_clock::now();
      sift.extract(lsift::ImageView::gray8(img.pixels.data(), img.width, img.height), features);
      const auto t1 = std::chrono::steady_clock::now();
      best_ms = std::min(best_ms, std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::fprintf(stderr, "%s: %dx%d, %zu features in %.2f ms (%u threads, %s)\n", input.c_str(), img.width,
                 img.height, features.size(), best_ms, pool.concurrency(), lsift::simd_target_name());

    if (!output.empty()) {
      std::unique_ptr<FILE, int (*)(FILE*)> f(std::fopen(output.c_str(), "w"), &std::fclose);
      if (!f) throw std::runtime_error("cannot write " + output);
      const std::vector<std::uint8_t> desc = features.descriptors_u8();
      for (std::size_t i = 0; i < features.size(); ++i) {
        const lsift::Keypoint& k = features.keypoints[i];
        std::fprintf(f.get(), "%.4f %.4f %.4f %.6f", k.x, k.y, k.sigma, k.angle);
        for (int j = 0; j < lsift::kDescriptorSize && !desc.empty(); ++j)
          std::fprintf(f.get(), " %d", desc[i * lsift::kDescriptorSize + j]);
        std::fputc('\n', f.get());
      }
    }
  } catch (const std::exception& e) {
    std::fprintf(stderr, "lsift: %s\n", e.what());
    return 1;
  }
  return 0;
}
