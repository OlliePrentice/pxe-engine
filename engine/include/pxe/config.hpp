#pragma once

namespace pxe {

// Low-res pixel-art target. Two pad texels on each side let the upscale
// shift by the camera's sub-texel snap error without sampling the border.
struct RenderConfig {
  static constexpr int kViewW = 640;
  static constexpr int kViewH = 360;
  static constexpr int kPad = 2;
  static constexpr int kRtW = kViewW + kPad * 2;  // 644
  static constexpr int kRtH = kViewH + kPad * 2;  // 364
  static constexpr int kShadow = 2048;
  static constexpr int kPixelsPerUnit = 16;  // 1 texel = 1/16 m on screen x
};

}  // namespace pxe
