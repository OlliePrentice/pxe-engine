#pragma once

namespace pxe {

// Low-res pixel-art target. The extra pad texels exist so the subpixel
// camera shift can reveal real scene content instead of clamping.
struct RenderConfig {
  static constexpr int kViewW = 640;
  static constexpr int kViewH = 360;
  static constexpr int kPad = 2;
  static constexpr int kRtW = kViewW + kPad * 2;
  static constexpr int kRtH = kViewH + kPad * 2;
  static constexpr int kShadow = 1024;
};

}  // namespace pxe
