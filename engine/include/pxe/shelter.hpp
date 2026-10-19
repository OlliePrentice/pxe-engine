#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <vector>

namespace pxe {

// What roofs the scene over, so rain can be kept off whatever burns under
// it: a grid over the ground holding, per cell, the height of every roof
// surface above it. Built once from the scene's built structure (stone);
// tree crowns let rain through, so they are not part of it.
//
// A point is sheltered when kLines of nine sight lines straight up from it -
// one from the point and eight round it kRadius out, since rain leans in
// the wind - meet a roof between kLow and kHigh above it. Anything at or
// below kLow (a campfire's own ring stones, a candle's dish) cannot roof it.
class ShelterMap {
 public:
  static constexpr float kRadius = 0.5f;
  static constexpr float kLow = 0.6f;
  static constexpr float kHigh = 12.f;
  static constexpr int kLines = 7;

  // `triangles` holds three corners per triangle. Walls seen edge-on from
  // above add nothing.
  void build(const std::vector<glm::vec3>& triangles, float cell = 0.1f);
  // How many of the nine sight lines up from `p` are roofed.
  int coveredLines(glm::vec3 p) const;
  bool sheltered(glm::vec3 p) const { return coveredLines(p) >= kLines; }

 private:
  bool roofed(glm::vec2 xz, float from, float to) const;

  glm::vec2 min_{0.f};
  float cell_ = 0.1f;
  int w_ = 0, h_ = 0;
  std::vector<int> start_;      // per cell, into heights_; w_ * h_ + 1 entries
  std::vector<float> heights_;  // roof heights, grouped by cell
};

}  // namespace pxe
