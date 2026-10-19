#include "shader.hpp"

#include "gl.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace pxe {
namespace {

std::string readAll(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("failed to open shader " + path);
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
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

unsigned linkProgram(const std::string& vertPath, const std::string& fragPath,
                     const std::string& commonPath, const std::string& commonPrefix) {
  const std::string common = commonPath.empty() ? std::string{} : commonPrefix + readAll(commonPath);
  const std::string defines = commonPath.empty() ? commonPrefix : std::string{};
  const unsigned vs = compile(GL_VERTEX_SHADER, withDefines(compose(readAll(vertPath), common), defines), vertPath);
  const unsigned fs = compile(GL_FRAGMENT_SHADER, withDefines(compose(readAll(fragPath), common), defines), fragPath);
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
    throw std::runtime_error("link failed (" + vertPath + ", " + fragPath + "):\n" + log);
  }
  return prog;
}

}  // namespace pxe
