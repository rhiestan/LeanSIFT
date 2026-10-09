// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#include "vlfeat_ref.h"

#include <algorithm>
#include <stdexcept>

extern "C" {
#include "vl/sift.h"
}

namespace lsift::support {

Features RunVLFeat(const float* image, int width, int height, const Params& params) {
  if (params.base_sigma != 1.6) throw std::invalid_argument("VLFeat reference only supports base_sigma = 1.6");

  VlSiftFilt* f = vl_sift_new(width, height, params.num_octaves, params.levels_per_octave, params.first_octave);
  if (f == nullptr) throw std::bad_alloc();
  vl_sift_set_peak_thresh(f, params.peak_threshold);
  vl_sift_set_edge_thresh(f, params.edge_threshold);
  vl_sift_set_norm_thresh(f, params.norm_threshold);
  vl_sift_set_magnif(f, params.magnification);
  vl_sift_set_window_size(f, params.window_size);
  f->sigman = params.nominal_sigma;

  Features out;
  int err = vl_sift_process_first_octave(f, image);
  while (err != VL_ERR_EOF) {
    vl_sift_detect(f);
    const VlSiftKeypoint* keys = vl_sift_get_keypoints(f);
    const int n = vl_sift_get_nkeypoints(f);
    for (int i = 0; i < n; ++i) {
      double angles[4] = {0, 0, 0, 0};
      int num_angles = 1;
      if (!params.upright) num_angles = vl_sift_calc_keypoint_orientations(f, angles, &keys[i]);
      num_angles = std::min(num_angles, params.max_orientations);
      for (int a = 0; a < num_angles; ++a) {
        Keypoint kp;
        kp.x = keys[i].x;
        kp.y = keys[i].y;
        kp.sigma = keys[i].sigma;
        kp.angle = static_cast<float>(angles[a]);
        kp.octave = keys[i].o;
        kp.level = keys[i].s;
        out.keypoints.push_back(kp);
        if (params.compute_descriptors) {
          float desc[kDescriptorSize] = {};
          vl_sift_calc_keypoint_descriptor(f, desc, &keys[i], angles[a]);
          out.descriptors.insert(out.descriptors.end(), desc, desc + kDescriptorSize);
        }
      }
    }
    err = vl_sift_process_next_octave(f);
  }
  vl_sift_delete(f);
  return out;
}

}  // namespace lsift::support
