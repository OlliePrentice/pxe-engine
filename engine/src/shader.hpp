#pragma once

#include <string>

namespace pxe {

// Loads, optionally splices common.glsl in place of the marker line, and links.
unsigned linkProgram(const std::string& vertPath, const std::string& fragPath,
                     const std::string& commonPath = {});

}  // namespace pxe
