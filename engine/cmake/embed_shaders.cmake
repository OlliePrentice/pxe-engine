# Writes a C++ source that embeds shader files as byte arrays, looked up by
# file name through pxe::embeddedShader (src/shader.hpp). Run at build time:
#
#   cmake -DSHADER_DIR=<dir> -DSHADERS=<name,name,...> -DOUTPUT=<file.cpp>
#         -P embed_shaders.cmake
#
# Byte arrays rather than string literals: MSVC caps a string literal at
# 16 KB, and the bytes go to the GL exactly as they are in the file.

if(NOT SHADER_DIR OR NOT SHADERS OR NOT OUTPUT)
  message(FATAL_ERROR "embed_shaders.cmake needs SHADER_DIR, SHADERS and OUTPUT")
endif()

string(REPLACE "," ";" shader_names "${SHADERS}")

set(arrays "")
set(table "")
set(index 0)
foreach(name IN LISTS shader_names)
  set(path "${SHADER_DIR}/${name}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "embed_shaders.cmake: no shader ${path}")
  endif()
  file(READ "${path}" hex HEX)
  if(hex STREQUAL "")
    message(FATAL_ERROR "embed_shaders.cmake: shader ${path} is empty")
  endif()
  # 16 bytes (32 hex digits) to a line.
  string(LENGTH "${hex}" hex_length)
  set(bytes "")
  set(offset 0)
  while(offset LESS hex_length)
    string(SUBSTRING "${hex}" ${offset} 32 line)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," line "${line}")
    string(APPEND bytes "    ${line}\n")
    math(EXPR offset "${offset} + 32")
  endwhile()
  string(APPEND arrays "// ${name}\nconst unsigned char kShader${index}[] = {\n${bytes}};\n\n")
  string(APPEND table "    {\"${name}\", kShader${index}, sizeof(kShader${index})},\n")
  math(EXPR index "${index} + 1")
endforeach()

set(source "// Generated at build time by engine/cmake/embed_shaders.cmake from
// engine/shaders. Do not edit.

#include \"shader.hpp\"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace pxe {
namespace {

${arrays}struct EmbeddedShader {
  std::string_view name;
  const unsigned char* bytes;
  std::size_t size;
};

const EmbeddedShader kShaders[] = {
${table}};

}  // namespace

std::string embeddedShader(std::string_view name) {
  for (const EmbeddedShader& shader : kShaders)
    if (shader.name == name) return std::string(reinterpret_cast<const char*>(shader.bytes), shader.size);
  throw std::runtime_error(\"no embedded shader \" + std::string(name));
}

}  // namespace pxe
")

file(WRITE "${OUTPUT}" "${source}")
