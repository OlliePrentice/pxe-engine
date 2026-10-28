#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace pxe {

// Writes `bottomUpRgba` (width * height RGBA8 pixels, bottom row first, as
// OpenGL reads them back) to a PNG at `path`, top row first. Creates the
// parent directories. False if the size is not positive, the buffer is too
// small, or the file can't be written.
bool writePng(const std::string& path, int width, int height, const std::vector<std::uint8_t>& bottomUpRgba);

}  // namespace pxe
