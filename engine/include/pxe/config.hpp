#pragma once

namespace pxe {

// Low-res pixel-art target. 960x540 at 24 pixels per metre matches the
// finished frame of t3ssel8r's scene video, where every art pixel is 2x2 on
// a 1920x1080 recording and the view is about 40 m across.
struct RenderConfig {
  static constexpr int kViewW = 960;
  static constexpr int kViewH = 540;
  static constexpr float kPixelsPerUnit = 24.f;
  // Pad texels on each side of the view let the upscale shift by the
  // camera's sub-texel snap error without sampling the border.
  static constexpr int kPad = 2;
  static constexpr int kShadow = 4096;
};

}  // namespace pxe
