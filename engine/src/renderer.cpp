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

constexpr float kTLit = 0.75f;
constexpr float kTMid = 0.585f;
constexpr float kGrassBiasMetres = 0.6f;

constexpr float kLit[3] = {0xc7 / 255.f, 0xe4 / 255.f, 0x87 / 255.f};
constexpr float kMid[3] = {0x83 / 255.f, 0xab / 255.f, 0x5c / 255.f};
constexpr float kShadow[3] = {0x46 / 255.f, 0x76 / 255.f, 0x6e / 255.f};
// Tuft colour per band (shadow, mid, lit): one step darker than lit and mid
// ground, one step lighter and greener than shadow, so tufts read on both.
constexpr float kDetail[9] = {
    0x5b / 255.f, 0x8a / 255.f, 0x6e / 255.f, 0x6b / 255.f, 0x94 / 255.f, 0x4e / 255.f,
    0x9c / 255.f, 0xc2 / 255.f, 0x66 / 255.f,
};
// Blade tip over darker ground. Shadow never overhangs anything darker.
constexpr float kTip[9] = {
    0x46 / 255.f, 0x76 / 255.f, 0x6e / 255.f, 0xa2 / 255.f, 0xc6 / 255.f, 0x6c / 255.f,
    0xe2 / 255.f, 0xf3 / 255.f, 0xa8 / 255.f,
};
constexpr float kWindPx = 1.6f;
constexpr float kTest[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};

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

void bindToon(unsigned prog, const glm::mat4& lightVP, float normalBias, bool testColours) {
  u1f(prog, "uTLit", kTLit);
  u1f(prog, "uTMid", kTMid);
  if (testColours) {
    u3f(prog, "uColLit", kTest[0], kTest[1], kTest[2]);
    u3f(prog, "uColMid", kTest[3], kTest[4], kTest[5]);
    u3f(prog, "uColShadow", kTest[6], kTest[7], kTest[8]);
  } else {
    u3f(prog, "uColLit", kLit[0], kLit[1], kLit[2]);
    u3f(prog, "uColMid", kMid[0], kMid[1], kMid[2]);
    u3f(prog, "uColShadow", kShadow[0], kShadow[1], kShadow[2]);
  }
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
  if (progTerrain_) glDeleteProgram(progTerrain_);
  if (progGrass_) glDeleteProgram(progGrass_);
  if (progShadow_) glDeleteProgram(progShadow_);
  if (progPresent_) glDeleteProgram(progPresent_);
  destroyTargets();
}

void Renderer::destroyTargets() {
  unsigned textures[] = {color_, ground_, depth_, shadow_, normal_};
  glDeleteTextures(5, textures);
  color_ = ground_ = depth_ = shadow_ = normal_ = 0;
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
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
  ground_ = makeColor(RenderConfig::kRtW, RenderConfig::kRtH);
  depth_ = makeDepth(RenderConfig::kRtW, RenderConfig::kRtH, false);
  shadow_ = makeDepth(RenderConfig::kShadow, RenderConfig::kShadow, true);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const unsigned draw = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &draw);
  requireFbo("colour");

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
  progTerrain_ = linkProgram(dir + "/terrain.vert", dir + "/terrain.frag", dir + "/common.glsl");
  progGrass_ = linkProgram(dir + "/grass.vert", dir + "/grass.frag", dir + "/common.glsl");
  progShadow_ = linkProgram(dir + "/shadow.vert", dir + "/shadow.frag");
  progPresent_ = linkProgram(dir + "/present.vert", dir + "/present.frag");
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
  worldMin_ = scene.worldMin;
  worldSize_ = scene.worldSize;
  if (!normal_) glGenTextures(1, &normal_);
  glBindTexture(GL_TEXTURE_2D, normal_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, scene.normalSize, scene.normalSize, 0, GL_RGBA, GL_FLOAT,
               scene.normals.empty() ? nullptr : scene.normals.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

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
  glBindTexture(GL_TEXTURE_2D, color_);
  u1i(progPresent_, "uTex", 0);
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
}

void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {
  const LightFit light = fitLight(scene);
  const float normalBias = 1.5f * light.texel;
  const glm::mat4 viewProj = camera.proj * camera.view;
  const glm::vec3 sun = glm::normalize(scene.sunDir);
  const float biasNdc = kGrassBiasMetres * 2.f / (camera.farPlane - camera.nearPlane);

  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
  glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(progShadow_);
  umat(progShadow_, "uLightViewProj", light.viewProj);
  scene.terrain.draw();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, normal_);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_);

  glUseProgram(progTerrain_);
  umat(progTerrain_, "uViewProj", viewProj);
  u1i(progTerrain_, "uNormalTex", 0);
  u1f(progTerrain_, "uWorldMin", worldMin_);
  u1f(progTerrain_, "uWorldSize", worldSize_);
  u1i(progTerrain_, "uShowNdl", params.showNdl ? 1 : 0);
  bindToon(progTerrain_, light.viewProj, normalBias, params.testColours);
  u3f(progTerrain_, "uSunDir", sun.x, sun.y, sun.z);
  scene.terrain.draw();

  if (params.showGrass && grassCount_ > 0) {
    glUseProgram(progGrass_);
    umat(progGrass_, "uViewProj", viewProj);
    u2f(progGrass_, "uTarget", static_cast<float>(RenderConfig::kRtW),
        static_cast<float>(RenderConfig::kRtH));
    u1f(progGrass_, "uDepthBiasNdc", biasNdc);
    bindToon(progGrass_, light.viewProj, normalBias, params.testColours);
    u3fv(progGrass_, "uDetail", 3, params.testColours ? kTest : kDetail);
    u3fv(progGrass_, "uTip", 3, params.testColours ? kTest : kTip);
    u1i(progGrass_, "uGround", 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ground_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
    u3f(progGrass_, "uSunDir", sun.x, sun.y, sun.z);
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
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glFinish();
  glReadPixels(RenderConfig::kPad, RenderConfig::kPad, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return pixels;
}

}  // namespace pxe
