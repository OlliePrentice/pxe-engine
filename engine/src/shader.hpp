#pragma once

#include <string>
#include <string_view>

namespace pxe {

// The source of a shader embedded in the library at build time, by its file
// name in engine/shaders (e.g. "mesh.frag"). Throws if there is no such
// shader. Defined in the generated embedded_shaders.cpp.
std::string embeddedShader(std::string_view name);

// Takes the embedded shaders by file name, optionally splices the common
// file in place of the marker line (after `commonPrefix`, e.g. #defines),
// and links. Without a common file, `commonPrefix` goes just after each
// shader's #version line.
unsigned linkProgram(const std::string& vert, const std::string& frag,
                     const std::string& common = {}, const std::string& commonPrefix = {});

}  // namespace pxe
