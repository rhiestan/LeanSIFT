// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace lsift::support {

struct GrayImage {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;  // row-major, tightly packed

  std::vector<float> to_float() const;  // pixels / 255
};

/// Loads PGM, PNG, JPEG, BMP, ... as 8-bit grayscale. Throws on failure.
GrayImage LoadGray(const std::string& path);

}  // namespace lsift::support
