#include "pxe/renderer.hpp"

#include "gl.hpp"
#include "pxe/config.hpp"
#include "shader.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTau = 6.28318530717958647692f;
}

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

float saturate(float v) { return std::clamp(v, 0.f, 1.f); }

float smooth01(float edge0, float edge1, float x) {
  float t = saturate((x - edge0) / (edge1 - edge0));
  return t * t * (3.f - 2.f * t);
}

// One directional light for the whole day. Azimuth turns steadily so shadows
// sweep. Elevation stays high enough at dawn and dusk that the shadow map
// still covers the casters, while the colour goes warm and dim. At night the
// same orbit is a cool, low moon. The angle is quantized so the shadow edge
// steps by a texel instead of swimming inside one.
struct DayLight {
  glm::vec3 travel{0.f, -1.f, 0.f};  // direction the light travels
  glm::vec3 sunColor{1.f};
  glm::vec3 shadowTint{0.5f};
  glm::vec3 skyColor{0.62f, 0.74f, 0.78f};
};

DayLight evaluateDay(float hour, float azimuth0) {
  hour = std::fmod(hour, 24.f);
  if (hour < 0.f) hour += 24.f;

  const float dayWave = std::sin((hour - 6.f) / 24.f * kTau);  // +1 noon, -1 midnight
  float elev;
  if (dayWave >= 0.f) {
    elev = std::lerp(0.20f, 1.02f, std::pow(dayWave, 0.80f));
  } else {
    elev = std::lerp(0.22f, 0.48f, std::pow(-dayWave, 0.90f));
  }
  float az = azimuth0 + (hour / 24.f) * kTau;

  constexpr float kQuant = 0.45f * kPi / 180.f;
  az = std::round(az / kQuant) * kQuant;
  elev = std::round(elev / kQuant) * kQuant;

  const glm::vec3 toLight{std::cos(az) * std::cos(elev), std::sin(elev), std::sin(az) * std::cos(elev)};

  // Full colour shortly after the sun clears the horizon, so dawn and dusk
  // read as a warm low sun rather than a dim mix with the moon.
  const float sunUp = smooth01(-0.05f, 0.10f, dayWave);
  const float lowSun = saturate(1.f - std::fabs(dayWave) * 2.6f) * smooth01(0.02f, 0.18f, dayWave);

  const glm::vec3 daySun{1.04f, 1.01f, 0.90f};
  const glm::vec3 goldSun{1.34f, 0.72f, 0.36f};
  const glm::vec3 moon{0.42f, 0.52f, 0.90f};
  // Cool, not black. Vertical stone in shade should stay a grey, not a hole.
  const glm::vec3 dayShadow{0.60f, 0.68f, 0.74f};
  const glm::vec3 goldShadow{0.55f, 0.46f, 0.56f};
  const glm::vec3 nightShadow{0.22f, 0.24f, 0.38f};

  const glm::vec3 daySky{0.64f, 0.78f, 0.70f};
  const glm::vec3 goldSky{0.86f, 0.62f, 0.42f};
  const glm::vec3 nightSky{0.08f, 0.10f, 0.16f};

  DayLight dl;
  dl.travel = -glm::normalize(toLight);
  const glm::vec3 dayLit = glm::mix(daySun, goldSun, lowSun);
  const glm::vec3 daySh = glm::mix(dayShadow, goldShadow, lowSun);
  dl.sunColor = glm::mix(moon, dayLit, sunUp);
  dl.shadowTint = glm::mix(nightShadow, daySh, sunUp);
  dl.skyColor = glm::mix(nightSky, glm::mix(daySky, goldSky, lowSun), sunUp);
  return dl;
}

// Lock the shadow projection to texel centres so a slow sun (and a panning
// camera) does not shimmer the edge between shadow texels.
#ifndef PXE_ASSET_DIR
#define PXE_ASSET_DIR "assets"
#endif

unsigned loadSprite(const std::string& path) {
  stbi_set_flip_vertically_on_load(1);
  int w = 0, h = 0, n = 0;
  unsigned char* data = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!data || w <= 0 || h <= 0) {
    std::fprintf(stderr, "pxe: failed to load sprite %s (%s)\n", path.c_str(),
                 stbi_failure_reason());
    throw std::runtime_error("missing sprite");
  }
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  stbi_image_free(data);
  std::fprintf(stderr, "pxe: sprite %s %dx%d\n", path.c_str(), w, h);
  return tex;
}

glm::mat4 snapShadowMatrix(const glm::mat4& lightVP, float shadowSize) {
  glm::vec4 origin = lightVP * glm::vec4(0.f, 0.f, 0.f, 1.f);
  origin *= shadowSize * 0.5f;
  const glm::vec4 rounded = glm::round(origin);
  glm::vec3 offset{(rounded.x - origin.x), (rounded.y - origin.y), 0.f};
  offset *= 2.f / shadowSize;
  return glm::translate(glm::mat4{1.f}, offset) * lightVP;
}

}  // namespace

WindState windPreset(int weather) {
  static const WindState kPresets[] = {
      {0.80f, 0.60f, 0.30f, 0.10f, 0.20f, 4.f, 0.f},
      {0.80f, 0.60f, 1.50f, 0.40f, 0.50f, 8.f, 0.f},
      {0.80f, 0.60f, 4.00f, 0.90f, 0.80f, 12.f, 1.f},
      {0.80f, 0.60f, 1.00f, 0.30f, 0.40f, 6.f, 0.6f},
  };
  const int i = std::clamp(weather, 0, 3);
  return kPresets[i];
}

Renderer::~Renderer() { destroyTargets(); }

void Renderer::destroyTargets() {
  unsigned textures[] = {albedo_, normal_, linDepth_, depth_, colorA_, colorB_, shadowTex_,
                         texGrass_, texFlower_, texLeaf_};
  glDeleteTextures(10, textures);
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
  progLight_ = linkProgram(dir + "/fullscreen.vert", dir + "/light.frag", dir + "/common.glsl");
  progOutline_ = linkProgram(dir + "/fullscreen.vert", dir + "/outline.frag");
  progWater_ = linkProgram(dir + "/water.vert", dir + "/water.frag");
  progRays_ = linkProgram(dir + "/fullscreen.vert", dir + "/rays.frag");
  progPresent_ = linkProgram(dir + "/present.vert", dir + "/present.frag");

  createTargets();
  createPresentQuad();
  const std::filesystem::path sprites = std::filesystem::path(PXE_ASSET_DIR) / "sprites";
  texGrass_ = loadSprite((sprites / "grass.png").string());
  texFlower_ = loadSprite((sprites / "flower.png").string());
  texLeaf_ = loadSprite((sprites / "leaf.png").string());

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
  const DayLight day = evaluateDay(params.hour, scene.sunAzimuth);
  const glm::vec3 lightDir = glm::normalize(day.travel);
  const glm::vec3 shadowUp =
      std::fabs(lightDir.y) > 0.92f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};

  // Cover the view, plus the reach of a low sun, and keep one shadow texel
  // about the size of one low-res pixel so the edge stays crisp.
  const float sunElev = std::asin(std::clamp(-lightDir.y, 0.05f, 1.f));
  const float shadowReach = 6.5f / std::max(std::tan(sunElev), 0.22f);
  float ext = std::max(camera.orthoHalfW, camera.orthoHalfH) * 2.05f + 6.f + shadowReach * 0.35f;
  const float texelCap = camera.texelWorld * static_cast<float>(RenderConfig::kShadow) * 0.5f;
  ext = std::min(ext, std::max(texelCap, std::max(camera.orthoHalfW, camera.orthoHalfH) * 2.4f));
  // Anchor the shadow frustum to the snapped camera, not the unsnapped look-at.
  // A sub-texel pan then cannot slide the shadow map under the pixels.
  const float centerDepth =
      std::round(glm::dot(camera.target - camera.eye, camera.forward) / camera.texelWorld) *
      camera.texelWorld;
  const glm::vec3 shadowCenter = camera.eye + camera.forward * centerDepth;
  const float dist = ext + 24.f;
  const glm::vec3 lightEye = shadowCenter - lightDir * dist;
  const glm::mat4 lightView = glm::lookAt(lightEye, shadowCenter, shadowUp);
  const float nearP = std::max(0.5f, dist - ext - 18.f);
  const float farP = dist + ext + 28.f;
  const glm::mat4 lightProj = glm::ortho(-ext, ext, -ext, ext, nearP, farP);
  const glm::mat4 lightVP =
      snapShadowMatrix(lightProj * lightView, static_cast<float>(RenderConfig::kShadow));
  const float shadowTexel = (2.f * ext) / static_cast<float>(RenderConfig::kShadow);
  const float depthSpan = std::max(farP - nearP, 1.f);

  WindState wind = windPreset(params.weather);
  if (params.windOverride) {
    wind.dirX = params.windDirX;
    wind.dirZ = params.windDirZ;
    wind.speed = params.windSpeed;
    wind.strength = params.windStrength;
    wind.gust = params.windGust;
    wind.fps = params.windFps;
    wind.rain = params.windRain;
  }
  const float windLen = std::sqrt(wind.dirX * wind.dirX + wind.dirZ * wind.dirZ);
  if (windLen > 1e-4f) {
    wind.dirX /= windLen;
    wind.dirZ /= windLen;
  } else {
    wind.dirX = 1.f;
    wind.dirZ = 0.f;
  }
  // Clouds drift with the wind at full frame rate. The flipbook uses a
  // quantised clock inside the foliage shader; this scroll does not.
  // Slow enough that calm clouds barely move, and a storm still drifts them.
  const glm::vec2 cloudScroll{wind.dirX * wind.speed * params.time * 0.16f,
                              wind.dirZ * wind.speed * params.time * 0.16f};

  glm::vec3 sky = day.skyColor;
  float fogNear = 10.f;
  float fogFar = 28.f;
  float fogMax = 0.f;
  if (params.weather == 2) {
    sky = glm::mix(sky, glm::vec3{0.34f, 0.40f, 0.50f}, 0.48f);
    fogNear = 6.f;
    fogFar = 22.f;
    fogMax = 0.22f;
  } else if (params.weather == 3) {
    sky = glm::mix(sky, glm::vec3{0.40f, 0.48f, 0.58f}, 0.35f);
    fogNear = 7.f;
    fogFar = 24.f;
    fogMax = 0.16f;
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
  glPolygonOffset(2.0f, 4.0f);
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
  const float clearAlb[] = {sky.r, sky.g, sky.b, 0.f};
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
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-1.5f, -2.f);
    glUseProgram(progFoliage_);
    setMat4(progFoliage_, "uViewProj", viewProj);
    setMat4(progFoliage_, "uView", camera.view);
    set3f(progFoliage_, "uCamRight", camera.right);
    set3f(progFoliage_, "uCamUp", camera.up);
    set1f(progFoliage_, "uTime", params.time);
    set1f(progFoliage_, "uTexelWorld", camera.texelWorld);
    set2f(progFoliage_, "uWindDir", wind.dirX, wind.dirZ);
    set1f(progFoliage_, "uWindSpeed", wind.speed);
    set1f(progFoliage_, "uWindStrength", wind.strength);
    set1f(progFoliage_, "uWindGust", wind.gust);
    set1f(progFoliage_, "uWindFps", wind.fps);
    set1f(progFoliage_, "uRain", wind.rain);
    setMat4(progFoliage_, "uLightViewProj", lightVP);
    set3f(progFoliage_, "uLightDir", lightDir);
    set1f(progFoliage_, "uNormalBias", shadowTexel * 1.6f);
    set1f(progFoliage_, "uDepthBias", (shadowTexel * 0.85f) / depthSpan);
    set2f(progFoliage_, "uCloudScroll", cloudScroll.x, cloudScroll.y);
    bindTex(0, texGrass_);
    bindTex(1, texFlower_);
    bindTex(2, texLeaf_);
    bindTex(3, shadowTex_);
    set1i(progFoliage_, "uGrass", 0);
    set1i(progFoliage_, "uFlower", 1);
    set1i(progFoliage_, "uLeaf", 2);
    set1i(progFoliage_, "uShadow", 3);
    glBindVertexArray(foliageVao_);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, foliageCount_);
    glDisable(GL_POLYGON_OFFSET_FILL);
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
  set3f(progLight_, "uSunColor", day.sunColor);
  set3f(progLight_, "uShadowTint", day.shadowTint);
  set3f(progLight_, "uSkyColor", sky);
  set1f(progLight_, "uFocusDepth", focusDepth);
  set1f(progLight_, "uFogNear", fogNear);
  set1f(progLight_, "uFogFar", fogFar);
  set1f(progLight_, "uFogMax", fogMax);
  set1f(progLight_, "uNormalBias", shadowTexel * 1.6f);
  set1f(progLight_, "uDepthBias", (shadowTexel * 0.85f) / depthSpan);
  set2f(progLight_, "uCloudScroll", cloudScroll.x, cloudScroll.y);
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
  set1f(progOutline_, "uDepthThresh", groundStep * 2.4f);
  set3f(progOutline_, "uSunColor", day.sunColor);
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
    set1f(progWater_, "uPixelDepth", groundStep);
    scene.water.draw();
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
  }

  // ---- god rays, hard bands from the sun shadow map ----
  float facing = glm::dot(camera.forward, -lightDir);
  facing = glm::clamp(facing, 0.f, 1.f);
  // The weather cycle is wind, not haze. Rays stay available but off.
  float rayStrength = 0.f;
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
