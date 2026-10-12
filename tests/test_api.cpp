// LeanSift - fast SIFT feature extraction
// SPDX-License-Identifier: BSD-2-Clause

#include <cstring>
#include <random>
#include <stdexcept>
#include <vector>

#include "gtest/gtest.h"
#include "lsift/lsift.hpp"
#include "test_util.h"

namespace {

void ExpectIdentical(const lsift::Features& a, const lsift::Features& b) {
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    const lsift::Keypoint& p = a.keypoints[i];
    const lsift::Keypoint& q = b.keypoints[i];
    ASSERT_EQ(p.x, q.x) << i;
    ASSERT_EQ(p.y, q.y) << i;
    ASSERT_EQ(p.sigma, q.sigma) << i;
    ASSERT_EQ(p.angle, q.angle) << i;
    ASSERT_EQ(p.octave, q.octave) << i;
  }
  ASSERT_EQ(a.descriptors, b.descriptors);
}

TEST(Sift, SameResultForAnyThreadCount) {
  const auto& img = lsift::test::TestImage("roofs1.jpg");
  const auto view = lsift::ImageView::gray8(img.pixels.data(), img.width, img.height);
  lsift::SerialExecutor serial;
  lsift::ThreadPool pool2(2), pool8(8);
  const lsift::Features a = lsift::Sift({}, &serial).extract(view);
  const lsift::Features b = lsift::Sift({}, &pool2).extract(view);
  const lsift::Features c = lsift::Sift({}, &pool8).extract(view);
  ASSERT_GT(a.size(), 100u);
  ExpectIdentical(a, b);
  ExpectIdentical(a, c);
}

TEST(Sift, U8AndFloatInputsAgree) {
  const auto& img = lsift::test::TestImage("box.pgm");
  const std::vector<float> f = img.to_float();
  lsift::Sift sift;
  const lsift::Features a = sift.extract(lsift::ImageView::gray8(img.pixels.data(), img.width, img.height));
  const lsift::Features b = sift.extract(lsift::ImageView::gray32f(f.data(), img.width, img.height));
  ExpectIdentical(a, b);
}

TEST(Sift, StridedInput) {
  const auto& img = lsift::test::TestImage("box.pgm");
  const int pad = 37;
  std::vector<std::uint8_t> strided(static_cast<std::size_t>(img.width + pad) * img.height, 255);
  for (int y = 0; y < img.height; ++y)
    std::memcpy(&strided[static_cast<std::size_t>(y) * (img.width + pad)], &img.pixels[static_cast<std::size_t>(y) * img.width],
                img.width);
  lsift::Sift sift;
  const lsift::Features a = sift.extract(lsift::ImageView::gray8(img.pixels.data(), img.width, img.height));
  const lsift::Features b = sift.extract(lsift::ImageView::gray8(strided.data(), img.width, img.height, img.width + pad));
  ExpectIdentical(a, b);
}

TEST(Sift, ReuseAcrossImageSizes) {
  const auto box = lsift::test::TestImage("box.pgm");
  const auto roofs = lsift::test::TestImage("roofs1.jpg");
  const auto box_view = lsift::ImageView::gray8(box.pixels.data(), box.width, box.height);
  const auto roofs_view = lsift::ImageView::gray8(roofs.pixels.data(), roofs.width, roofs.height);
  lsift::Sift reused;
  const lsift::Features a1 = reused.extract(roofs_view);
  const lsift::Features b1 = reused.extract(box_view);
  const lsift::Features a2 = reused.extract(roofs_view);
  ExpectIdentical(a1, a2);
  ExpectIdentical(b1, lsift::Sift().extract(box_view));
}

TEST(Sift, ReusedOutputIsOverwritten) {
  const auto box = lsift::test::TestImage("box.pgm");
  const auto roofs = lsift::test::TestImage("roofs1.jpg");
  const auto box_view = lsift::ImageView::gray8(box.pixels.data(), box.width, box.height);
  const auto roofs_view = lsift::ImageView::gray8(roofs.pixels.data(), roofs.width, roofs.height);
  lsift::Sift sift;
  lsift::Features out;
  sift.extract(roofs_view, out);
  sift.extract(box_view, out);  // fewer features than before
  ExpectIdentical(out, lsift::Sift().extract(box_view));
  sift.extract(roofs_view, out);  // more
  ExpectIdentical(out, lsift::Sift().extract(roofs_view));

  lsift::Params p;
  p.compute_descriptors = false;
  lsift::Sift(p).extract(box_view, out);
  EXPECT_EQ(out.size(), lsift::Sift().extract(box_view).size());
  EXPECT_TRUE(out.descriptors.empty());

  EXPECT_THROW(sift.extract(lsift::ImageView::gray8(nullptr, 0, 0), out), std::invalid_argument);
  EXPECT_EQ(out.size(), 0u);
}

TEST(Sift, KeypointsWithoutDescriptors) {
  const auto& img = lsift::test::TestImage("box.pgm");
  const auto view = lsift::ImageView::gray8(img.pixels.data(), img.width, img.height);
  lsift::Params p;
  p.compute_descriptors = false;
  const lsift::Features a = lsift::Sift(p).extract(view);
  const lsift::Features b = lsift::Sift().extract(view);
  EXPECT_TRUE(a.descriptors.empty());
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) EXPECT_EQ(a.keypoints[i].angle, b.keypoints[i].angle);
}

TEST(Sift, DescriptorsAreNormalized) {
  const auto& img = lsift::test::TestImage("box.pgm");
  const lsift::Features f = lsift::Sift().extract(lsift::ImageView::gray8(img.pixels.data(), img.width, img.height));
  ASSERT_GT(f.size(), 0u);
  for (std::size_t i = 0; i < f.size(); ++i) {
    double n = 0;
    for (int j = 0; j < lsift::kDescriptorSize; ++j) {
      const float v = f.descriptor(i)[j];
      ASSERT_GE(v, 0.0f);
      n += double(v) * v;
    }
    EXPECT_NEAR(n, 1.0, 1e-4);
  }
}

TEST(Sift, TinyAndOddImages) {
  std::mt19937 rng(42);
  std::uniform_int_distribution<int> dist(0, 255);
  for (auto [w, h] : {std::pair{1, 1}, {2, 2}, {3, 3}, {5, 7}, {16, 16}, {17, 33}, {64, 9}, {127, 129}}) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w) * h);
    for (auto& p : pixels) p = static_cast<std::uint8_t>(dist(rng));
    for (int first_octave : {-2, -1, 0, 1}) {
      lsift::Params params;
      params.first_octave = first_octave;
      params.peak_threshold = 0;
      lsift::Sift sift(params);
      const lsift::Features f = sift.extract(lsift::ImageView::gray8(pixels.data(), w, h));
      for (const auto& k : f.keypoints) {
        EXPECT_GE(k.x, 0);
        EXPECT_LE(k.x, w);
        EXPECT_GE(k.y, 0);
        EXPECT_LE(k.y, h);
      }
    }
  }
}

TEST(Sift, InvalidArguments) {
  lsift::Params p;
  p.levels_per_octave = 0;
  EXPECT_THROW(lsift::Sift{p}, std::invalid_argument);
  lsift::Sift sift;
  EXPECT_THROW(sift.extract(lsift::ImageView{}), std::invalid_argument);
}

TEST(Sift, ReportsSimdTarget) { EXPECT_STRNE(lsift::simd_target_name(), ""); }

}  // namespace
