#include "pxe/image.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstring>
#include <filesystem>

namespace pxe {

bool writePng(const std::string& path, int width, int height, const std::vector<std::uint8_t>& bottomUpRgba) {
  if (width <= 0 || height <= 0 || bottomUpRgba.size() < static_cast<std::size_t>(width * height * 4)) return false;
  std::filesystem::path file(path);
  if (file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
  std::vector<std::uint8_t> top(static_cast<std::size_t>(width * height * 4));
  const int stride = width * 4;
  for (int y = 0; y < height; ++y) {
    std::memcpy(top.data() + static_cast<std::size_t>(height - 1 - y) * stride,
                bottomUpRgba.data() + static_cast<std::size_t>(y) * stride, stride);
  }
  return stbi_write_png(path.c_str(), width, height, 4, top.data(), stride) != 0;
}

}  // namespace pxe
