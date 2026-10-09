// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include <string>

#include "image_io.h"

namespace lsift::test {

inline std::string DataPath(const std::string& name) { return std::string(LSIFT_TEST_DATA_DIR) + "/" + name; }

inline const support::GrayImage& TestImage(const std::string& name) {
  static std::string cached_name;
  static support::GrayImage cached;
  if (cached_name != name) {
    cached = support::LoadGray(DataPath(name));
    cached_name = name;
  }
  return cached;
}

}  // namespace lsift::test
