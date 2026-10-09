// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#include "image_io.h"

#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PNM
#include "stb_image.h"

namespace lsift::support {

std::vector<float> GrayImage::to_float() const {
  std::vector<float> out(pixels.size());
  for (std::size_t i = 0; i < pixels.size(); ++i) out[i] = pixels[i] / 255.0f;
  return out;
}

GrayImage LoadGray(const std::string& path) {
  int w = 0, h = 0, channels = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 1);
  if (data == nullptr) {
    throw std::runtime_error("cannot read image " + path + ": " + stbi_failure_reason());
  }
  GrayImage img;
  img.width = w;
  img.height = h;
  img.pixels.assign(data, data + static_cast<std::size_t>(w) * h);
  stbi_image_free(data);
  return img;
}

}  // namespace lsift::support
