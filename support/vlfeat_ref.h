// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#pragma once

#include "lsift/lsift.hpp"

namespace lsift::support {

/// Runs VLFeat's SIFT (single-threaded) with the same parameters and returns
/// its output in LeanSift's format and order. `image` holds width * height
/// floats in [0, 1]. Keypoint::response is not available from VLFeat and is 0.
Features RunVLFeat(const float* image, int width, int height, const Params& params);

}  // namespace lsift::support
