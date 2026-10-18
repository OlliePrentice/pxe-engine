#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "shader.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace pxe {
namespace {

constexpr float kGrassBiasMetres = 0.6f;
constexpr float kWindPx = 1.6f;

int uloc(unsigned prog, const char* name) { return glGetUniformLocation(prog, name); }

void u1i(unsigned prog, const char* name, int v) { glUniform1i(uloc(prog, name), v); }
void u1f(unsigned prog, const char* name, float v) { glUniform1f(uloc(prog, name), v); }
void u2f(unsigned prog, const char* name, float x, float y) { glUniform2f(uloc(prog, name), x, y); }
void u3f(unsigned prog, const char* name, float x, float y, float z) {
  glUniform3f(uloc(prog, name), x, y, z);
}
void u3fv(unsigned prog, const char* name, int count, const float* v) {
  glUniform3fv(uloc(prog, name), count, v);
}
void umat(unsigned prog, const char* name, const glm::mat4& m) {
  glUniformMatrix4fv(uloc(prog, name), 1, GL_FALSE, &m[0][0]);
}

void requireFbo(const char* what) {
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr, "pxe: framebuffer incomplete (%s)\n", what);
    throw std::runtime_error("incomplete framebuffer");
  }
}

std::string shaderDir() {
#ifdef PXE_SHADER_DIR
  return PXE_SHADER_DIR;
#else
  return "engine/shaders";
#endif
}

struct LightFit {
  glm::mat4 viewProj{1.f};
  float texel = 0.05f;
};

LightFit fitLight(const Scene& scene) {
  LightFit fit;
  const glm::vec3 L = glm::normalize(scene.sunDir);
  const glm::vec3 center = (scene.boundsMin + scene.boundsMax) * 0.5f;
  const glm::vec3 up = std::fabs(L.y) > 0.95f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
  const glm::vec3 eye = center + L * 140.f;
  const glm::mat4 view = glm::lookAt(eye, center, up);

  glm::vec3 mn{1e9f};
  glm::vec3 mx{-1e9f};
  for (int i = 0; i < 8; ++i) {
    const glm::vec3 p{(i & 1) ? scene.boundsMax.x : scene.boundsMin.x,
                      (i & 2) ? scene.boundsMax.y : scene.boundsMin.y,
                      (i & 4) ? scene.boundsMax.z : scene.boundsMin.z};
    const glm::vec3 v = glm::vec3(view * glm::vec4(p, 1.f));
    mn = glm::min(mn, v);
    mx = glm::max(mx, v);
  }
  const float margin = 3.f;
  mn.x -= margin;
  mn.y -= margin;
  mx.x += margin;
  mx.y += margin;
  float nearP = -mx.z - 8.f;
  float farP = -mn.z + 8.f;
  if (nearP < 0.2f) nearP = 0.2f;
  if (farP < nearP + 2.f) farP = nearP + 2.f;

  fit.texel = std::max(mx.x - mn.x, mx.y - mn.y) / static_cast<float>(RenderConfig::kShadow);
  fit.viewProj = glm::ortho(mn.x, mx.x, mn.y, mx.y, nearP, farP) * view;
  return fit;
}

// Flatten one Palette field across all surfaces into shadow/mid/lit
// triples, or pure red/green/blue per band for --test-colours.
std::vector<float> ramp(const Scene& scene, glm::vec3 (Palette::*field)[3], bool testColours) {
  std::vector<float> out;
  out.reserve(kSurfCount * 9);
  for (int s = 0; s < kSurfCount; ++s) {
    for (int b = 0; b < 3; ++b) {
      const glm::vec3 c = testColours ? glm::vec3(b == 2, b == 1, b == 0) : (scene.palette[s].*field)[b];
      out.insert(out.end(), {c.x, c.y, c.z});
    }
  }
  return out;
}

void bindToon(unsigned prog, const Scene& scene, const glm::mat4& lightVP, float normalBias, bool testColours) {
  u1f(prog, "uTLit", scene.tLit);
  u1f(prog, "uTMid", scene.tMid);
  const glm::vec3 sun = glm::normalize(scene.sunDir);
  u3f(prog, "uSunDir", sun.x, sun.y, sun.z);
  u3fv(prog, "uBand", kSurfCount * 3, ramp(scene, &Palette::band, testColours).data());
  u3fv(prog, "uDetail", kSurfCount * 3, ramp(scene, &Palette::detail, testColours).data());
  umat(prog, "uLightViewProj", lightVP);
  u1i(prog, "uShadowMap", 1);
  u1f(prog, "uNormalBias", normalBias);
}

}  // namespace

Renderer::~Renderer() {
  if (grassVao_) glDeleteVertexArrays(1, &grassVao_);
  if (grassCorner_) glDeleteBuffers(1, &grassCorner_);
  if (grassInst_) glDeleteBuffers(1, &grassInst_);
  if (presentVao_) glDeleteVertexArrays(1, &presentVao_);
  if (presentVbo_) glDeleteBuffers(1, &presentVbo_);
  if (progMesh_) glDeleteProgram(progMesh_);
  if (progGrass_) glDeleteProgram(progGrass_);
  if (progShadow_) glDeleteProgram(progShadow_);
  if (progPresent_) glDeleteProgram(progPresent_);
  if (progEdge_) glDeleteProgram(progEdge_);
  destroyTargets();
}

void Renderer::destroyTargets() {
  unsigned textures[] = {color_, normals_, final_, ground_, depth_, shadow_};
  glDeleteTextures(6, textures);
  color_ = normals_ = final_ = ground_ = depth_ = shadow_ = 0;
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
  if (fboFinal_) glDeleteFramebuffers(1, &fboFinal_);
  fboFinal_ = 0;
  if (fboShadow_) glDeleteFramebuffers(1, &fboShadow_);
  fbo_ = fboShadow_ = 0;
}

unsigned Renderer::makeColor(int w, int h) {
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

unsigned Renderer::makeDepth(int w, int h, bool shadowCompare) {
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const float border[4] = {1.f, 1.f, 1.f, 1.f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  if (shadowCompare) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  }
  return tex;
}

void Renderer::createTargets() {
  color_ = makeColor(RenderConfig::kRtW, RenderConfig::kRtH);
  normals_ = makeColor(RenderConfig::kRtW, RenderConfig::kRtH);
  final_ = makeColor(RenderConfig::kRtW, RenderConfig::kRtH);
  ground_ = makeColor(RenderConfig::kRtW, RenderConfig::kRtH);
  depth_ = makeDepth(RenderConfig::kRtW, RenderConfig::kRtH, false);
  shadow_ = makeDepth(RenderConfig::kShadow, RenderConfig::kShadow, true);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normals_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const unsigned draws[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, draws);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  requireFbo("colour");

  glGenFramebuffers(1, &fboFinal_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboFinal_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, final_, 0);
  const unsigned finalDraw = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &finalDraw);
  requireFbo("final");

  glGenFramebuffers(1, &fboShadow_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  requireFbo("shadow");
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::createQuads() {
  const float corners[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f};
  glGenVertexArrays(1, &grassVao_);
  glGenBuffers(1, &grassCorner_);
  glGenBuffers(1, &grassInst_);
  glBindVertexArray(grassVao_);
  glBindBuffer(GL_ARRAY_BUFFER, grassCorner_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));

  glBindBuffer(GL_ARRAY_BUFFER, grassInst_);
  glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
  const auto stride = static_cast<GLsizei>(sizeof(GrassInstance));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
  glVertexAttribDivisor(1, 1);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 3));
  glVertexAttribDivisor(2, 1);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(sizeof(float) * 6));
  glVertexAttribDivisor(3, 1);
  glBindVertexArray(0);

  glGenVertexArrays(1, &presentVao_);
  glGenBuffers(1, &presentVbo_);
  glBindVertexArray(presentVao_);
  glBindBuffer(GL_ARRAY_BUFFER, presentVbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        reinterpret_cast<void*>(sizeof(float) * 2));
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::init() {
  const std::string dir = shaderDir();
  progMesh_ = linkProgram(dir + "/mesh.vert", dir + "/mesh.frag", dir + "/common.glsl");
  progGrass_ = linkProgram(dir + "/grass.vert", dir + "/grass.frag", dir + "/common.glsl");
  progShadow_ = linkProgram(dir + "/shadow.vert", dir + "/shadow.frag");
  progPresent_ = linkProgram(dir + "/present.vert", dir + "/present.frag");
  progEdge_ = linkProgram(dir + "/edge.vert", dir + "/edge.frag");
  createTargets();
  createQuads();

  glDisable(GL_DITHER);
  glDisable(GL_MULTISAMPLE);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_BLEND);
  ready_ = true;

  const unsigned char* vendor = glGetString(GL_RENDERER);
  const unsigned char* version = glGetString(GL_VERSION);
  std::fprintf(stderr, "pxe: %s — %s\n", vendor ? reinterpret_cast<const char*>(vendor) : "?",
               version ? reinterpret_cast<const char*>(version) : "?");
  std::fprintf(stderr, "pxe: target %dx%d (view %dx%d + %dpx pad), shadow %d\n", RenderConfig::kRtW,
               RenderConfig::kRtH, RenderConfig::kViewW, RenderConfig::kViewH, RenderConfig::kPad,
               RenderConfig::kShadow);
}

void Renderer::upload(const Scene& scene) {
  grassCount_ = static_cast<int>(scene.grass.size());
  glBindBuffer(GL_ARRAY_BUFFER, grassInst_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scene.grass.size() * sizeof(GrassInstance)),
               scene.grass.empty() ? nullptr : scene.grass.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::drawPresent(const CameraFrame& camera, const FrameParams& params) {
  const int fbW = std::max(params.framebufferW, 1);
  const int fbH = std::max(params.framebufferH, 1);
  int scale = std::min(fbW / RenderConfig::kViewW, fbH / RenderConfig::kViewH);
  if (scale < 1) scale = 1;
  const int dw = RenderConfig::kViewW * scale;
  const int dh = RenderConfig::kViewH * scale;
  const int x0 = (fbW - dw) / 2;
  const int y0 = (fbH - dh) / 2;
  const int x1 = x0 + dw;
  const int y1 = y0 + dh;

  auto nx = [&](int x) { return (static_cast<float>(x) / static_cast<float>(fbW)) * 2.f - 1.f; };
  auto ny = [&](int y) { return (static_cast<float>(y) / static_cast<float>(fbH)) * 2.f - 1.f; };

  const float rtW = static_cast<float>(RenderConfig::kRtW);
  const float rtH = static_cast<float>(RenderConfig::kRtH);
  const float pad = static_cast<float>(RenderConfig::kPad);
  const float viewW = static_cast<float>(RenderConfig::kViewW);
  const float viewH = static_cast<float>(RenderConfig::kViewH);
  const float u0 = (pad + camera.texelError.x) / rtW;
  const float v0 = (pad + camera.texelError.y) / rtH;
  const float u1 = (pad + viewW + camera.texelError.x) / rtW;
  const float v1 = (pad + viewH + camera.texelError.y) / rtH;

  const float data[] = {
      nx(x0), ny(y0), u0, v0, nx(x1), ny(y0), u1, v0, nx(x1), ny(y1), u1, v1,
      nx(x0), ny(y0), u0, v0, nx(x1), ny(y1), u1, v1, nx(x0), ny(y1), u0, v1,
  };
  glBindBuffer(GL_ARRAY_BUFFER, presentVbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbW, fbH);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  glUseProgram(progPresent_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, final_);
  u1i(progPresent_, "uTex", 0);
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {
  const LightFit light = fitLight(scene);
  const float normalBias = 1.5f * light.texel;
  const glm::mat4 viewProj = camera.proj * camera.view;
  const float biasNdc = kGrassBiasMetres * 2.f / (camera.farPlane - camera.nearPlane);

  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  // Both faces: water, foam and the ground are open sheets.
  glDisable(GL_CULL_FACE);
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
  glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(progShadow_);
  umat(progShadow_, "uLightViewProj", light.viewProj);
  for (const Mesh& m : scene.meshes) m.draw();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glUseProgram(progMesh_);
  umat(progMesh_, "uViewProj", viewProj);
  u1i(progMesh_, "uShowNdl", params.showNdl ? 1 : 0);
  u1f(progMesh_, "uTime", params.time);
  bindToon(progMesh_, scene, light.viewProj, normalBias, params.testColours);
  for (const Mesh& m : scene.meshes) m.draw();

  if (params.showGrass && grassCount_ > 0) {
    glUseProgram(progGrass_);
    umat(progGrass_, "uViewProj", viewProj);
    u2f(progGrass_, "uTarget", static_cast<float>(RenderConfig::kRtW),
        static_cast<float>(RenderConfig::kRtH));
    u1f(progGrass_, "uDepthBiasNdc", biasNdc);
    bindToon(progGrass_, scene, light.viewProj, normalBias, params.testColours);
    u3fv(progGrass_, "uTip", kSurfCount * 3, ramp(scene, &Palette::tip, params.testColours).data());
    u1i(progGrass_, "uGround", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ground_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
    u1f(progGrass_, "uTime", params.time);
    u1f(progGrass_, "uWindPx", kWindPx);
    u1i(progGrass_, "uMagenta", params.magentaGrass ? 1 : 0);
    if (params.grassDepthTest) {
      glEnable(GL_DEPTH_TEST);
      glDepthMask(GL_TRUE);
    } else {
      glDisable(GL_DEPTH_TEST);
    }
    glDisable(GL_CULL_FACE);
    glBindVertexArray(grassVao_);
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, grassCount_);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
  }

  // Edge pass into final_, which is what gets presented and captured.
  glBindFramebuffer(GL_FRAMEBUFFER, fboFinal_);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(progEdge_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, color_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, normals_);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, depth_);
  u1i(progEdge_, "uColor", 0);
  u1i(progEdge_, "uNormal", 1);
  u1i(progEdge_, "uDepth", 2);
  u1f(progEdge_, "uDepthMetres", camera.farPlane - camera.nearPlane);
  u1i(progEdge_, "uEnabled", params.edges ? 1 : 0);
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);

  if (params.present) drawPresent(camera, params);
  pxeGlCheck("render");
}

std::vector<std::uint8_t> Renderer::readPresented(int width, int height) {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width * height * 4));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glFinish();
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

std::vector<std::uint8_t> Renderer::readView() {
  const int w = RenderConfig::kViewW;
  const int h = RenderConfig::kViewH;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w * h * 4));
  glBindFramebuffer(GL_FRAMEBUFFER, fboFinal_);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glFinish();
  glReadPixels(RenderConfig::kPad, RenderConfig::kPad, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return pixels;
}

}  // namespace pxe
