#include "shader.hpp"

#include "gl.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace pxe {
namespace {

// An embedded shader with LF line ends whatever the checkout it was built
// from: a CR before a LF (a Windows checkout with CRLF line ends) is dropped.
std::string shaderSource(const std::string& name) {
  const std::string bytes = embeddedShader(name);
  std::string source;
  source.reserve(bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i)
    if (!(bytes[i] == '\r' && i + 1 < bytes.size() && bytes[i + 1] == '\n')) source += bytes[i];
  return source;
}

std::string compose(const std::string& source, const std::string& common) {
  const std::string needle = "// #include \"common.glsl\"";
  const auto at = source.find(needle);
  if (at == std::string::npos) return source;
  std::string out = source;
  out.replace(at, needle.size(), common);
  return out;
}

// Defines for a shader without a common file go just after its #version line.
std::string withDefines(const std::string& source, const std::string& defines) {
  if (defines.empty()) return source;
  const auto eol = source.find('\n');
  if (eol == std::string::npos) return source;
  return source.substr(0, eol + 1) + defines + source.substr(eol + 1);
}

unsigned compile(unsigned type, const std::string& source, const std::string& label) {
  const char* src = source.c_str();
  const unsigned shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, nullptr);
  glCompileShader(shader);
  int ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    int len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    glDeleteShader(shader);
    throw std::runtime_error(label + " compile failed:\n" + log);
  }
  return shader;
}

}  // namespace

unsigned linkProgram(const std::string& vert, const std::string& frag, const std::string& common,
                     const std::string& commonPrefix) {
  const std::string commonSource = common.empty() ? std::string{} : commonPrefix + shaderSource(common);
  const std::string defines = common.empty() ? commonPrefix : std::string{};
  const unsigned vs = compile(GL_VERTEX_SHADER, withDefines(compose(shaderSource(vert), commonSource), defines), vert);
  const unsigned fs = compile(GL_FRAGMENT_SHADER, withDefines(compose(shaderSource(frag), commonSource), defines), frag);
  const unsigned prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok = 0;
  glGetProgramiv(prog, GL_LINK_STATUS, &ok);
  if (!ok) {
    int len = 0;
    glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
    std::string log(static_cast<std::size_t>(std::max(len, 1)), '\0');
    glGetProgramInfoLog(prog, len, nullptr, log.data());
    glDeleteProgram(prog);
    throw std::runtime_error("link failed (" + vert + ", " + frag + "):\n" + log);
  }
  return prog;
}

}  // namespace pxe
