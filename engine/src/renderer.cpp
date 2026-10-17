#include "pxe/renderer.hpp"

#include "gl.hpp"
#include "pxe/config.hpp"
#include "shader.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace pxe {
namespace {

void setMat4(unsigned prog, const char* name, const glm::mat4& m) {
  glUniformMatrix4fv(glGetUniformLocation(prog, name), 1, GL_FALSE, glm::value_ptr(m));
}
void set1i(unsigned prog, const char* name, int v) {
  glUniform1i(glGetUniformLocation(prog, name), v);
}
void set1f(unsigned prog, const char* name, float v) {
  glUniform1f(glGetUniformLocation(prog, name), v);
}
void set2f(unsigned prog, const char* name, float x, float y) {
  glUniform2f(glGetUniformLocation(prog, name), x, y);
}
void set3f(unsigned prog, const char* name, glm::vec3 v) {
  glUniform3f(glGetUniformLocation(prog, name), v.x, v.y, v.z);
}

void bindTex(int unit, unsigned tex) {
  glActiveTexture(GL_TEXTURE0 + unit);
  glBindTexture(GL_TEXTURE_2D, tex);
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

}  // namespace

Renderer::~Renderer() { destroyTargets(); }

void Renderer::destroyTargets() {
  unsigned textures[] = {albedo_, normal_, linDepth_, depth_, colorA_, colorB_, shadowTex_};
  glDeleteTextures(7, textures);
  unsigned fbos[] = {fboG_, fboLight_, fboComp_, fboShadow_};
  glDeleteFramebuffers(4, fbos);
  if (foliageEbo_) glDeleteBuffers(1, &foliageEbo_);
  if (foliageInst_) glDeleteBuffers(1, &foliageInst_);
  if (foliageVbo_) glDeleteBuffers(1, &foliageVbo_);
  if (foliageVao_) glDeleteVertexArrays(1, &foliageVao_);
  if (presentVbo_) glDeleteBuffers(1, &presentVbo_);
  if (presentVao_) glDeleteVertexArrays(1, &presentVao_);
  if (emptyVao_) glDeleteVertexArrays(1, &emptyVao_);
  unsigned progs[] = {progG_, progFoliage_, progShadow_, progLight_,
                      progOutline_, progWater_, progRays_, progPresent_};
  for (unsigned p : progs) {
    if (p) glDeleteProgram(p);
  }
  ready_ = false;
}

unsigned Renderer::makeTarget(int w, int h, int internal, int format, int type, int filter) {
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, format, type, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

void Renderer::createTargets() {
  constexpr int w = RenderConfig::kRtW;
  constexpr int h = RenderConfig::kRtH;

  albedo_ = makeTarget(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST);
  normal_ = makeTarget(w, h, GL_RGBA16F, GL_RGBA, GL_FLOAT, GL_NEAREST);
  linDepth_ = makeTarget(w, h, GL_R32F, GL_RED, GL_FLOAT, GL_NEAREST);
  colorA_ = makeTarget(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_NEAREST);
  colorB_ = makeTarget(w, h, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);

  glGenTextures(1, &depth_);
  glBindTexture(GL_TEXTURE_2D, depth_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenFramebuffers(1, &fboG_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboG_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, albedo_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normal_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, linDepth_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const GLenum gbufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, gbufs);
  requireFbo("gbuffer");

  glGenFramebuffers(1, &fboLight_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboLight_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorA_, 0);
  requireFbo("light");

  glGenFramebuffers(1, &fboComp_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboComp_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorB_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  requireFbo("composite");

  const int s = RenderConfig::kShadow;
  glGenTextures(1, &shadowTex_);
  glBindTexture(GL_TEXTURE_2D, shadowTex_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, s, s, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const float border[] = {1.f, 1.f, 1.f, 1.f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

  glGenFramebuffers(1, &fboShadow_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  requireFbo("shadow");

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::createPresentQuad() {
  glGenVertexArrays(1, &emptyVao_);

  glGenVertexArrays(1, &presentVao_);
  glGenBuffers(1, &presentVbo_);
  glBindVertexArray(presentVao_);
  glBindBuffer(GL_ARRAY_BUFFER, presentVbo_);
  // 6 vertices, replaced every frame.
  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4,
                        reinterpret_cast<void*>(sizeof(float) * 2));

  // Unit quad for foliage. uv (0,0) is the bottom-left of a billboard.
  const float quad[] = {
      0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f,
  };
  const unsigned idx[] = {0, 1, 2, 0, 2, 3};
  glGenVertexArrays(1, &foliageVao_);
  glGenBuffers(1, &foliageVbo_);
  glGenBuffers(1, &foliageEbo_);
  glGenBuffers(1, &foliageInst_);
  glBindVertexArray(foliageVao_);
  glBindBuffer(GL_ARRAY_BUFFER, foliageVbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, foliageEbo_);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));

  glBindBuffer(GL_ARRAY_BUFFER, foliageInst_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(FoliageInstance), nullptr, GL_STATIC_DRAW);
  const auto stride = static_cast<GLsizei>(sizeof(FoliageInstance));
  auto inst = [&](int loc, int comps, std::size_t offsetFloats) {
    glEnableVertexAttribArray(loc);
    glVertexAttribPointer(loc, comps, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetFloats * sizeof(float)));
    glVertexAttribDivisor(loc, 1);
  };
  inst(4, 3, 0);
  inst(5, 3, 3);
  inst(6, 3, 6);
  inst(7, 4, 9);
  glBindVertexArray(0);
}

void Renderer::init() {
  const std::string dir = shaderDir();
  progG_ = linkProgram(dir + "/gbuffer.vert", dir + "/gbuffer.frag", dir + "/common.glsl");
  progFoliage_ = linkProgram(dir + "/foliage.vert", dir + "/foliage.frag", dir + "/common.glsl");
  progShadow_ = linkProgram(dir + "/shadow.vert", dir + "/shadow.frag");
  progLight_ = linkProgram(dir + "/fullscreen.vert", dir + "/light.frag");
  progOutline_ = linkProgram(dir + "/fullscreen.vert", dir + "/outline.frag");
  progWater_ = linkProgram(dir + "/water.vert", dir + "/water.frag");
  progRays_ = linkProgram(dir + "/fullscreen.vert", dir + "/rays.frag");
  progPresent_ = linkProgram(dir + "/present.vert", dir + "/present.frag");

  createTargets();
  createPresentQuad();

  glDisable(GL_DITHER);
  glDisable(GL_MULTISAMPLE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glFrontFace(GL_CCW);
  ready_ = true;

  const unsigned char* vendor = glGetString(GL_RENDERER);
  const unsigned char* version = glGetString(GL_VERSION);
  std::fprintf(stderr, "pxe: %s — %s\n", vendor ? reinterpret_cast<const char*>(vendor) : "?",
               version ? reinterpret_cast<const char*>(version) : "?");
  std::fprintf(stderr, "pxe: internal target %dx%d (view %dx%d + %dpx pad)\n", RenderConfig::kRtW,
               RenderConfig::kRtH, RenderConfig::kViewW, RenderConfig::kViewH, RenderConfig::kPad);
}

void Renderer::uploadFoliage(const std::vector<FoliageInstance>& instances) {
  foliageCount_ = static_cast<int>(instances.size());
  glBindBuffer(GL_ARRAY_BUFFER, foliageInst_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(instances.size() * sizeof(FoliageInstance)),
               instances.empty() ? nullptr : instances.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::updatePresentQuad(const CameraFrame& camera, const FrameParams& params) {
  const float viewW = static_cast<float>(RenderConfig::kViewW);
  const float viewH = static_cast<float>(RenderConfig::kViewH);
  const float rtW = static_cast<float>(RenderConfig::kRtW);
  const float rtH = static_cast<float>(RenderConfig::kRtH);
  const float pad = static_cast<float>(RenderConfig::kPad);
  const float fbW = static_cast<float>(std::max(params.framebufferW, 1));
  const float fbH = static_cast<float>(std::max(params.framebufferH, 1));
  const float scale = std::min(fbW / viewW, fbH / viewH);
  const float dw = viewW * scale;
  const float dh = viewH * scale;
  const float x0 = (fbW - dw) * 0.5f;
  const float y0 = (fbH - dh) * 0.5f;
  const float x1 = x0 + dw;
  const float y1 = y0 + dh;
  auto nx = [&](float x) { return (x / fbW) * 2.f - 1.f; };
  auto ny = [&](float y) { return (y / fbH) * 2.f - 1.f; };

  const float u0 = (pad + camera.texelError.x) / rtW;
  const float v0 = (pad + camera.texelError.y) / rtH;
  const float u1 = (pad + camera.texelError.x + viewW) / rtW;
  const float v1 = (pad + camera.texelError.y + viewH) / rtH;

  const float data[] = {
      nx(x0), ny(y0), u0, v0, nx(x1), ny(y0), u1, v0, nx(x1), ny(y1), u1, v1,
      nx(x0), ny(y0), u0, v0, nx(x1), ny(y1), u1, v1, nx(x0), ny(y1), u0, v1,
  };
  glBindBuffer(GL_ARRAY_BUFFER, presentVbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {
  const glm::mat4 viewProj = camera.proj * camera.view;
  const glm::vec3 lightDir = glm::normalize(scene.sunDirection);
  const glm::vec3 shadowUp =
      std::fabs(lightDir.y) > 0.92f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
  const float ext = scene.shadowExtent;
  const float dist = ext + 18.f;
  const glm::vec3 lightEye = scene.shadowCenter - lightDir * dist;
  const glm::mat4 lightView = glm::lookAt(lightEye, scene.shadowCenter, shadowUp);
  const glm::mat4 lightProj = glm::ortho(-ext, ext, -ext, ext, 1.f, dist + ext + 20.f);
  const glm::mat4 lightVP = lightProj * lightView;

  glm::vec3 fog = glm::vec3{0.67f, 0.73f, 0.64f};
  float fogNear = 8.f;
  float fogFar = 26.f;
  float fogMax = 0.55f;
  if (params.weather == 1) {
    fog = glm::vec3{0.71f, 0.74f, 0.64f};
    fogNear = 4.f;
    fogFar = 18.f;
    fogMax = 0.62f;
  } else if (params.weather == 2) {
    fog = glm::vec3{0.40f, 0.48f, 0.56f};
    fogNear = 1.5f;
    fogFar = 14.f;
    fogMax = 0.72f;
  }
  const float focusDepth = glm::dot(camera.target - camera.eye, camera.forward);

  // ---- shadow ----
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
  glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.75f, 3.5f);
  glClear(GL_DEPTH_BUFFER_BIT);
  glUseProgram(progShadow_);
  setMat4(progShadow_, "uLightViewProj", lightVP);
  scene.shadowCasters.draw();
  glDisable(GL_POLYGON_OFFSET_FILL);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // ---- g-buffer ----
  glBindFramebuffer(GL_FRAMEBUFFER, fboG_);
  glViewport(0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
  const GLenum gbufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
  glDrawBuffers(3, gbufs);
  const float clearAlb[] = {fog.r, fog.g, fog.b, 0.f};
  const float clearNrm[] = {0.f, 1.f, 0.f, 0.f};
  const float clearDep[] = {999.f, 0.f, 0.f, 1.f};
  glClearBufferfv(GL_COLOR, 0, clearAlb);
  glClearBufferfv(GL_COLOR, 1, clearNrm);
  glClearBufferfv(GL_COLOR, 2, clearDep);
  glClear(GL_DEPTH_BUFFER_BIT);

  glUseProgram(progG_);
  setMat4(progG_, "uViewProj", viewProj);
  setMat4(progG_, "uView", camera.view);
  scene.opaque.draw();

  if (foliageCount_ > 0) {
    glDisable(GL_CULL_FACE);
    glUseProgram(progFoliage_);
    setMat4(progFoliage_, "uViewProj", viewProj);
    setMat4(progFoliage_, "uView", camera.view);
    set3f(progFoliage_, "uCamRight", camera.right);
    set3f(progFoliage_, "uCamUp", camera.up);
    set1f(progFoliage_, "uTime", params.time);
    glBindVertexArray(foliageVao_);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, foliageCount_);
    glEnable(GL_CULL_FACE);
  }

  auto drawFullscreen = [&](unsigned prog) {
    glUseProgram(prog);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  };

  // ---- lighting ----
  glDisable(GL_DEPTH_TEST);
  glBindFramebuffer(GL_FRAMEBUFFER, fboLight_);
  glViewport(0, 0, RenderConfig::kRtW, RenderConfig::kRtH);
  glUseProgram(progLight_);
  bindTex(0, albedo_);
  bindTex(1, normal_);
  bindTex(2, linDepth_);
  bindTex(3, shadowTex_);
  set1i(progLight_, "uAlbedo", 0);
  set1i(progLight_, "uNormal", 1);
  set1i(progLight_, "uDepth", 2);
  set1i(progLight_, "uShadow", 3);
  setMat4(progLight_, "uLightViewProj", lightVP);
  set3f(progLight_, "uEye", camera.eye);
  set3f(progLight_, "uRight", camera.right);
  set3f(progLight_, "uUp", camera.up);
  set3f(progLight_, "uForward", camera.forward);
  set1f(progLight_, "uHalfW", camera.orthoHalfW);
  set1f(progLight_, "uHalfH", camera.orthoHalfH);
  set3f(progLight_, "uLightDir", lightDir);
  set3f(progLight_, "uFogColor", fog);
  set1f(progLight_, "uFocusDepth", focusDepth);
  set1f(progLight_, "uFogNear", fogNear);
  set1f(progLight_, "uFogFar", fogFar);
  set1f(progLight_, "uFogMax", fogMax);
  drawFullscreen(progLight_);

  // ---- outlines into the composite buffer ----
  glBindFramebuffer(GL_FRAMEBUFFER, fboComp_);
  glUseProgram(progOutline_);
  bindTex(0, colorA_);
  bindTex(1, normal_);
  bindTex(2, linDepth_);
  bindTex(3, albedo_);
  set1i(progOutline_, "uColor", 0);
  set1i(progOutline_, "uNormal", 1);
  set1i(progOutline_, "uDepth", 2);
  set1i(progOutline_, "uAlbedo", 3);
  setMat4(progOutline_, "uView", camera.view);
  // Favour faces that point up in view space, so the 1px highlight sits on
  // the top lip of a block and the far side of the crease stays plain.
  set3f(progOutline_, "uBias", glm::normalize(glm::vec3{0.15f, 1.f, 0.45f}));
  set2f(progOutline_, "uTexel", 1.f / RenderConfig::kRtW, 1.f / RenderConfig::kRtH);
  const float groundStep = camera.texelWorld / std::tan(camera.pitchRadians);
  set1f(progOutline_, "uDepthThresh", groundStep * 3.2f);
  set1i(progOutline_, "uEnable", params.outlines ? 1 : 0);
  drawFullscreen(progOutline_);

  // ---- water, depth-tested against the opaque depth ----
  if (!scene.water.empty()) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(progWater_);
    bindTex(0, linDepth_);
    set1i(progWater_, "uDepth", 0);
    setMat4(progWater_, "uViewProj", viewProj);
    setMat4(progWater_, "uView", camera.view);
    set2f(progWater_, "uInvRes", 1.f / RenderConfig::kRtW, 1.f / RenderConfig::kRtH);
    set1f(progWater_, "uShore", groundStep * 14.f);
    set1f(progWater_, "uTime", params.time);
    scene.water.draw();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
  }

  // ---- god rays, hard bands from the sun shadow map ----
  float facing = glm::dot(camera.forward, -lightDir);
  facing = glm::clamp(facing, 0.f, 1.f);
  float rayStrength = 0.f;
  if (params.weather == 0) rayStrength = 0.0f;
  if (params.weather == 1) rayStrength = 0.12f;
  rayStrength *= 0.55f + 0.45f * facing;
  if (rayStrength > 0.001f) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progRays_);
    bindTex(0, linDepth_);
    bindTex(1, shadowTex_);
    set1i(progRays_, "uDepth", 0);
    set1i(progRays_, "uShadow", 1);
    setMat4(progRays_, "uLightViewProj", lightVP);
    set3f(progRays_, "uEye", camera.eye);
    set3f(progRays_, "uRight", camera.right);
    set3f(progRays_, "uUp", camera.up);
    set3f(progRays_, "uForward", camera.forward);
    set1f(progRays_, "uHalfW", camera.orthoHalfW);
    set1f(progRays_, "uHalfH", camera.orthoHalfH);
    set1f(progRays_, "uStrength", rayStrength);
    set1f(progRays_, "uRayLen", 9.f);
    set3f(progRays_, "uRayColor", glm::vec3{1.0f, 0.93f, 0.74f});
    drawFullscreen(progRays_);
    glDisable(GL_BLEND);
  }

  // ---- upscale to the window ----
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, params.framebufferW, params.framebufferH);
  glDisable(GL_DEPTH_TEST);
  glClearColor(0.16f, 0.18f, 0.17f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);
  updatePresentQuad(camera, params);

  unsigned show = colorB_;
  int mode = 0;
  switch (params.debugView) {
    case 1:
      mode = 1;
      break;
    case 2:
      show = albedo_;
      mode = 1;
      break;
    case 3:
      show = normal_;
      mode = 2;
      break;
    case 4:
      show = linDepth_;
      mode = 3;
      break;
    default:
      break;
  }
  glUseProgram(progPresent_);
  bindTex(0, show);
  set1i(progPresent_, "uTex", 0);
  set1i(progPresent_, "uMode", mode);
  set1i(progPresent_, "uWeather", params.weather);
  set1f(progPresent_, "uTime", params.time);
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);
}

std::vector<std::uint8_t> Renderer::readLowRes() {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(RenderConfig::kRtW * RenderConfig::kRtH * 4));
  glBindFramebuffer(GL_FRAMEBUFFER, fboComp_);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, RenderConfig::kRtW, RenderConfig::kRtH, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  return pixels;
}

std::vector<std::uint8_t> Renderer::readPresented(int w, int h) {
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(w * h * 4));
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  return pixels;
}

int Renderer::lowResWidth() const { return RenderConfig::kRtW; }
int Renderer::lowResHeight() const { return RenderConfig::kRtH; }

}  // namespace pxe
