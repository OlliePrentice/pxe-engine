#pragma once

#include <string>

namespace pxe {

// Loads, optionally splices common.glsl in place of the marker line (after
// `commonPrefix`, e.g. #defines), and links.
unsigned linkProgram(const std::string& vertPath, const std::string& fragPath,
                     const std::string& commonPath = {}, const std::string& commonPrefix = {});

}  // namespace pxe
