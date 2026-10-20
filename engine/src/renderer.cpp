#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "pxe/fireflies.hpp"
#include "shader.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <iterator>
#include <string>

namespace pxe {
namespace {

constexpr float kGrassBiasMetres = 0.6f;   // leaves, through their own crown
constexpr float kBladeBiasMetres = 0.12f;  // grass, just clear of the ground
constexpr float kWindPx = 1.6f;  // peak sway at the default wind strength (1)

// Wind strength (0 still, 1 default, up to 6) as how far sprites sway and how
// fast the gusts roll, both 1 at the default so it looks as it always did:
// near still in no wind, bending hard and quick in a gale.
float windSway(float s) { return s <= 1.f ? 0.3f + 0.7f * s : 1.f + 0.3f * (s - 1.f); }
float windRate(float s) { return s <= 1.f ? 0.35f + 0.65f * std::max(s, 0.f) : 1.f + 0.35f * (s - 1.f); }
// Fire is drawn this far toward the camera, so the logs in front of it hide
// only the bottom of the flames.
constexpr float kFireBiasMetres = 0.25f;
// The firelight shines from above the logs, so they shadow only the ground
// inside the ring of stones.
constexpr float kFireLightHeight = 0.62f;
constexpr int kFireShadow = 512;  // per cube face
// How far a firefly's light carries, metres.
constexpr float kFireflyReach = 2.3f;

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

// A fixed-size square around the scene's bounding sphere. Fitting the box
// corners instead changes the texel size every time the sun moves, which
// makes shadow edges crawl. No texel snapping: that steadies a map that
// slides with the camera, but under a turning sun the snap offset jumps by
// half a texel back and forth, and shadow edges wobble instead of moving
// steadily one way.
LightFit fitLight(const Scene& scene, glm::vec3 lightDir) {
  LightFit fit;
  const glm::vec3 L = glm::normalize(lightDir);
  const glm::vec3 center = (scene.boundsMin + scene.boundsMax) * 0.5f;
  const float radius = 0.5f * glm::length(scene.boundsMax - scene.boundsMin) + 1.f;
  const glm::vec3 up = std::fabs(L.y) > 0.95f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{0.f, 1.f, 0.f};
  const glm::mat4 view = glm::lookAt(center + L * (radius + 10.f), center, up);

  fit.texel = 2.f * radius / static_cast<float>(RenderConfig::kShadow);
  fit.viewProj = glm::ortho(-radius, radius, -radius, radius, 1.f, 2.f * radius + 20.f) * view;
  return fit;
}

// Flatten one Palette field across all surfaces into shadow/mid/lit
// triples, or pure red/green/blue per band for --test-colours.
// Flatten one Palette field across all surfaces into shadow/mid/lit
// triples, or pure red/green/blue per band for --test-colours.
std::vector<float> ramp(const Lighting& lighting, glm::vec3 (Palette::*field)[3], bool testColours) {
  std::vector<float> out;
  out.reserve(kSurfCount * 9);
  for (int s = 0; s < kSurfCount; ++s) {
    for (int b = 0; b < 3; ++b) {
      const glm::vec3 c = testColours ? glm::vec3(b == 2, b == 1, b == 0) : (lighting.palette[s].*field)[b];
      out.insert(out.end(), {c.x, c.y, c.z});
    }
  }
  return out;
}

// This frame's firelights, flattened for the toon shaders.
struct FireUniforms {
  int count = 0;
  std::vector<float> pos;       // xyz each
  std::vector<float> strength;  // with the flicker
  std::vector<int> cube;        // layer / 6 in the cube array, -1 none
  std::vector<float> shadowed;  // 0 no shadows .. 1 full
};

// The firelights' shadow cubes are on unit 7.
void bindToon(unsigned prog, const Lighting& lighting, const glm::mat4& lightVP, const glm::mat4& flashVP,
              const FireUniforms& fire, float normalBias, bool testColours) {
  u1f(prog, "uTLit", lighting.tLit);
  u1f(prog, "uTMid", lighting.tMid);
  const glm::vec3 light = glm::normalize(lighting.lightDir);
  u3f(prog, "uSunDir", light.x, light.y, light.z);
  u3fv(prog, "uBand", kSurfCount * 3, ramp(lighting, &Palette::band, testColours).data());
  u3fv(prog, "uDetail", kSurfCount * 3, ramp(lighting, &Palette::detail, testColours).data());
  umat(prog, "uLightViewProj", lightVP);
  u1i(prog, "uShadowMap", 1);
  u1f(prog, "uNormalBias", normalBias);
  u1f(prog, "uShadowTexel", 1.f / static_cast<float>(RenderConfig::kShadow));
  u1f(prog, "uCloudCover", lighting.cloudCover);
  u2f(prog, "uCloudOffset", lighting.cloudOffset.x, lighting.cloudOffset.y);
  u1f(prog, "uCloudStrength", lighting.cloudStrength);
  u1f(prog, "uOvercast", lighting.overcast);
  u1f(prog, "uRain", lighting.rain);
  u1f(prog, "uFlash", lighting.flash);
  const glm::vec3 flash = glm::normalize(lighting.flashDir);
  u3f(prog, "uFlashDir", flash.x, flash.y, flash.z);
  umat(prog, "uFlashViewProj", flashVP);
  u1i(prog, "uFlashShadowMap", 6);
  u1i(prog, "uFireShadow", 7);
  u1i(prog, "uFireCount", fire.count);
  if (fire.count > 0) {
    glUniform3fv(uloc(prog, "uFirePos"), fire.count, fire.pos.data());
    glUniform1fv(uloc(prog, "uFireLight"), fire.count, fire.strength.data());
    glUniform1iv(uloc(prog, "uFireCube"), fire.count, fire.cube.data());
    glUniform1fv(uloc(prog, "uFireShadowed"), fire.count, fire.shadowed.data());
  }
  u1f(prog, "uFireReach", lighting.fireReach);
  u1f(prog, "uFireDim", lighting.fireDim);
  std::vector<float> fireBand;
  for (int s = 0; s < kSurfCount; ++s) fireBand.insert(fireBand.end(), {lighting.fireBand[s].x, lighting.fireBand[s].y, lighting.fireBand[s].z});
  u3fv(prog, "uFireBand", kSurfCount, fireBand.data());
  glUniform4f(uloc(prog, "uHorizon"), lighting.horizon.x, lighting.horizon.y, lighting.horizon.z, lighting.horizon.w);
  u1f(prog, "uHorizonRelief", lighting.horizonRelief);
}

}  // namespace

Renderer::~Renderer() {
  if (queries_[0]) glDeleteQueries(PassTimes::kCount, queries_);
  if (grassVao_) glDeleteVertexArrays(1, &grassVao_);
  if (grassCorner_) glDeleteBuffers(1, &grassCorner_);
  if (grassInst_) glDeleteBuffers(1, &grassInst_);
  if (presentVao_) glDeleteVertexArrays(1, &presentVao_);
  if (presentVbo_) glDeleteBuffers(1, &presentVbo_);
  for (const auto& row : toon_)
    for (const ToonPrograms& t : row)
      for (unsigned prog : {t.mesh, t.grass, t.water})
        if (prog) glDeleteProgram(prog);
  if (progShadow_) glDeleteProgram(progShadow_);
  if (progShadowGrid_) glDeleteProgram(progShadowGrid_);
  if (progPresent_) glDeleteProgram(progPresent_);
  for (unsigned prog : edge_)
    if (prog) glDeleteProgram(prog);
  if (progFire_) glDeleteProgram(progFire_);
  if (progFireShadow_) glDeleteProgram(progFireShadow_);
  if (fireVbo_) glDeleteBuffers(1, &fireVbo_);
  if (fireVao_) glDeleteVertexArrays(1, &fireVao_);
  for (unsigned buf : {pointBuf_, tileBuf_})
    if (buf) glDeleteBuffers(1, &buf);
  for (unsigned tex : {pointTex_, tileTex_})
    if (tex) glDeleteTextures(1, &tex);
  destroyTargets();
}

void Renderer::destroyTargets() {
  unsigned textures[] = {color_, normals_, final_, ground_, depth_, shadow_, shadowStatic_, flashShadow_, heat_, canopy_, refl_, reflDepth_, bed_, bedDepth_};
  glDeleteTextures(static_cast<GLsizei>(std::size(textures)), textures);
  color_ = normals_ = final_ = ground_ = depth_ = shadow_ = shadowStatic_ = flashShadow_ = heat_ = canopy_ = refl_ = reflDepth_ = bed_ = bedDepth_ = 0;
  if (fboHeat_) glDeleteFramebuffers(1, &fboHeat_);
  fboHeat_ = 0;
  if (fireCubes_) glDeleteTextures(1, &fireCubes_);
  if (fireCubeDepth_) glDeleteTextures(1, &fireCubeDepth_);
  if (fboFireCube_) glDeleteFramebuffers(1, &fboFireCube_);
  fireCubes_ = fireCubeDepth_ = fboFireCube_ = 0;
  for (ShadowCube& cube : cubes_) cube.drawnScene = nullptr;
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
  if (fboFinal_) glDeleteFramebuffers(1, &fboFinal_);
  if (fboRefl_) glDeleteFramebuffers(1, &fboRefl_);
  fboFinal_ = fboRefl_ = 0;
  if (fboShadow_) glDeleteFramebuffers(1, &fboShadow_);
  if (fboFlash_) glDeleteFramebuffers(1, &fboFlash_);
  if (fboShadowStatic_) glDeleteFramebuffers(1, &fboShadowStatic_);
  fbo_ = fboShadow_ = fboFlash_ = fboShadowStatic_ = 0;
  shadowScene_ = nullptr;
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
    // Linear filtering on a compare sampler returns the fraction of the four
    // nearest texels that pass, a free 2x2 PCF.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  } else {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
  }
  return tex;
}

void Renderer::createTargets() {
  color_ = makeColor(rtW(), rtH());
  normals_ = makeColor(rtW(), rtH());
  final_ = makeColor(rtW(), rtH());
  ground_ = makeColor(rtW(), rtH());
  depth_ = makeDepth(rtW(), rtH(), false);
  refl_ = makeColor(rtW(), rtH());
  reflDepth_ = makeDepth(rtW(), rtH(), false);
  bed_ = makeColor(rtW(), rtH());
  bedDepth_ = makeDepth(rtW(), rtH(), false);
  shadow_ = makeDepth(RenderConfig::kShadow, RenderConfig::kShadow, true);
  shadowStatic_ = makeDepth(RenderConfig::kShadow, RenderConfig::kShadow, true);
  flashShadow_ = makeDepth(RenderConfig::kShadow, RenderConfig::kShadow, true);
  glGenTextures(1, &heat_);
  glBindTexture(GL_TEXTURE_2D, heat_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, rtW(), rtH(), 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, normals_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const unsigned draws[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  glDrawBuffers(2, draws);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  requireFbo("colour");

  glGenFramebuffers(1, &fboRefl_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboRefl_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, refl_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, reflDepth_, 0);
  const unsigned reflDraw = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &reflDraw);
  requireFbo("reflection");

  // The god rays' canopy: two octaves of smooth value noise, tiling, baked
  // once so each slice of air costs one lookup.
  {
    constexpr int kSize = 128;
    auto lattice = [](int x, int y, int period) {
      x = ((x % period) + period) % period;
      y = ((y % period) + period) % period;
      std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u;
      h = (h ^ (h >> 13)) * 1274126177u;
      return static_cast<float>((h ^ (h >> 16)) & 0xffff) / 65535.f;
    };
    auto noise = [&](float u, float v, int period) {
      const int x = static_cast<int>(std::floor(u)), y = static_cast<int>(std::floor(v));
      float fx = u - static_cast<float>(x), fy = v - static_cast<float>(y);
      fx = fx * fx * (3.f - 2.f * fx);
      fy = fy * fy * (3.f - 2.f * fy);
      const float a = lattice(x, y, period), b = lattice(x + 1, y, period);
      const float c = lattice(x, y + 1, period), d = lattice(x + 1, y + 1, period);
      return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    };
    std::vector<std::uint8_t> px(static_cast<std::size_t>(kSize * kSize));
    for (int y = 0; y < kSize; ++y)
      for (int x = 0; x < kSize; ++x) {
        const float u = static_cast<float>(x) / kSize, v = static_cast<float>(y) / kSize;
        const float n = 0.65f * noise(u * 16.f, v * 16.f, 16) + 0.35f * noise(u * 32.f, v * 32.f, 32);
        px[static_cast<std::size_t>(y * kSize + x)] = static_cast<std::uint8_t>(std::clamp(n, 0.f, 1.f) * 255.f);
      }
    glGenTextures(1, &canopy_);
    glBindTexture(GL_TEXTURE_2D, canopy_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kSize, kSize, 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  }

  glGenTextures(1, &fireCubes_);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, fireCubes_);
  glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_R32F, kFireShadow, kFireShadow, 6 * kShadowCubes, 0, GL_RED, GL_FLOAT,
               nullptr);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  fireCubeDepth_ = makeDepth(kFireShadow, kFireShadow, false);
  glGenFramebuffers(1, &fboFireCube_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboFireCube_);
  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fireCubes_, 0, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fireCubeDepth_, 0);
  const unsigned cubeDraw = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &cubeDraw);
  requireFbo("fire shadow");

  // Fire heat, depth-tested against the finished scene without writing it.
  glGenFramebuffers(1, &fboHeat_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboHeat_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, heat_, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_, 0);
  const unsigned heatDraw = GL_COLOR_ATTACHMENT0;
  glDrawBuffers(1, &heatDraw);
  requireFbo("heat");

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
  glGenFramebuffers(1, &fboShadowStatic_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboShadowStatic_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowStatic_, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glGenFramebuffers(1, &fboFlash_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboFlash_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, flashShadow_, 0);
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

  glGenVertexArrays(1, &fireVao_);
  glGenBuffers(1, &fireVbo_);
  glBindVertexArray(fireVao_);
  glBindBuffer(GL_ARRAY_BUFFER, fireVbo_);
  glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(FireParticle), reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FireParticle), reinterpret_cast<void*>(sizeof(float) * 4));
  glBindVertexArray(0);

  // Point lights and their tile lists, as buffer textures for the edge pass.
  glGenBuffers(1, &pointBuf_);
  glGenBuffers(1, &tileBuf_);
  glGenTextures(1, &pointTex_);
  glGenTextures(1, &tileTex_);
  const float noLight[8] = {};
  glBindBuffer(GL_TEXTURE_BUFFER, pointBuf_);
  glBufferData(GL_TEXTURE_BUFFER, sizeof(noLight), noLight, GL_STREAM_DRAW);
  glBindTexture(GL_TEXTURE_BUFFER, pointTex_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, pointBuf_);
  const std::uint32_t noTile[2] = {};
  glBindBuffer(GL_TEXTURE_BUFFER, tileBuf_);
  glBufferData(GL_TEXTURE_BUFFER, sizeof(noTile), noTile, GL_STREAM_DRAW);
  glBindTexture(GL_TEXTURE_BUFFER, tileTex_);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_R32UI, tileBuf_);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

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
  for (int grid = 0; grid < 2; ++grid)
    for (int many = 0; many < 2; ++many) {
      const std::string defines = std::string(many ? "" : "#define ONE_FIRE_LIGHT\n") + (grid ? "#define FIRE_GRID\n" : "");
      ToonPrograms& t = toon_[grid][many];
      t.mesh = linkProgram(dir + "/mesh.vert", dir + "/mesh.frag", dir + "/common.glsl", defines);
      t.grass = linkProgram(dir + "/grass.vert", dir + "/grass.frag", dir + "/common.glsl", defines);
      t.water = linkProgram(dir + "/water.vert", dir + "/water.frag", dir + "/common.glsl", defines);
    }
  grid_.init(dir);
  progShadow_ = linkProgram(dir + "/shadow.vert", dir + "/shadow.frag");
  progShadowGrid_ = linkProgram(dir + "/shadow.vert", dir + "/shadow.frag", {}, "#define FIRE_GRID\n");
  progPresent_ = linkProgram(dir + "/present.vert", dir + "/present.frag");
  edge_[0] = linkProgram(dir + "/edge.vert", dir + "/edge.frag");
  edge_[1] = linkProgram(dir + "/edge.vert", dir + "/edge.frag", {}, "#define FIRE_GRID\n");
  progFire_ = linkProgram(dir + "/fire.vert", dir + "/fire.frag");
  progFireShadow_ = linkProgram(dir + "/fireshadow.vert", dir + "/fireshadow.frag");
  createTargets();
  createQuads();

  glDisable(GL_DITHER);
  glDisable(GL_MULTISAMPLE);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glDisable(GL_BLEND);
  glEnable(GL_PROGRAM_POINT_SIZE);
  ready_ = true;

  const unsigned char* vendor = glGetString(GL_RENDERER);
  const unsigned char* version = glGetString(GL_VERSION);
  std::fprintf(stderr, "pxe: %s — %s\n", vendor ? reinterpret_cast<const char*>(vendor) : "?",
               version ? reinterpret_cast<const char*>(version) : "?");
  std::fprintf(stderr, "pxe: target %dx%d (view %dx%d + %dpx pad), shadow %d\n", rtW(), rtH(), viewW(), viewH(),
               RenderConfig::kPad, RenderConfig::kShadow);
}

void Renderer::upload(const Scene& scene) {
  shadowScene_ = nullptr;
  grid_.reset();
  fires_.clear();
  for (ShadowCube& cube : cubes_) cube = ShadowCube{};
  for (LightSlot& slot : slots_) slot = LightSlot{};
  fireLights_.clear();
  fireTime_ = -1.f;
  grassCount_ = static_cast<int>(scene.grass.size());
  glBindBuffer(GL_ARRAY_BUFFER, grassInst_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scene.grass.size() * sizeof(GrassInstance)),
               scene.grass.empty() ? nullptr : scene.grass.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// The window maps onto the low-res target as drawPresent lays it out: the
// view scaled by a whole number and centred, shifted by the snap error.
glm::vec3 Renderer::rayThrough(const CameraFrame& camera, int fbW, int fbH, glm::vec2 px) const {
  const int scale = std::max(1, std::min(fbW / viewW(), fbH / viewH()));
  const glm::vec2 corner{static_cast<float>((fbW - viewW() * scale) / 2), static_cast<float>((fbH - viewH() * scale) / 2)};
  const glm::vec2 up{px.x, static_cast<float>(fbH) - px.y};
  const glm::vec2 texel = static_cast<float>(RenderConfig::kPad) + camera.texelError + (up - corner) / static_cast<float>(scale);
  const glm::vec2 ndc = texel / glm::vec2{static_cast<float>(rtW()), static_cast<float>(rtH())} * 2.f - 1.f;
  return camera.eye + camera.right * (ndc.x * camera.orthoHalfW) + camera.up * (ndc.y * camera.orthoHalfH);
}

glm::vec2 Renderer::windowPoint(const CameraFrame& camera, int fbW, int fbH, glm::vec3 world) const {
  const int scale = std::max(1, std::min(fbW / viewW(), fbH / viewH()));
  const glm::vec2 corner{static_cast<float>((fbW - viewW() * scale) / 2), static_cast<float>((fbH - viewH() * scale) / 2)};
  const glm::vec2 ndc{glm::dot(world - camera.eye, camera.right) / camera.orthoHalfW,
                      glm::dot(world - camera.eye, camera.up) / camera.orthoHalfH};
  const glm::vec2 texel = (ndc + 1.f) * 0.5f * glm::vec2{static_cast<float>(rtW()), static_cast<float>(rtH())};
  const glm::vec2 up = corner + (texel - static_cast<float>(RenderConfig::kPad) - camera.texelError) * static_cast<float>(scale);
  return {up.x, static_cast<float>(fbH) - up.y};
}

void Renderer::drawPresent(const CameraFrame& camera, const FrameParams& params) {
  const int fbW = std::max(params.framebufferW, 1);
  const int fbH = std::max(params.framebufferH, 1);
  int scale = std::min(fbW / viewW(), fbH / viewH());
  if (scale < 1) scale = 1;
  const int dw = viewW() * scale;
  const int dh = viewH() * scale;
  const int x0 = (fbW - dw) / 2;
  const int y0 = (fbH - dh) / 2;
  const int x1 = x0 + dw;
  const int y1 = y0 + dh;

  auto nx = [&](int x) { return (static_cast<float>(x) / static_cast<float>(fbW)) * 2.f - 1.f; };
  auto ny = [&](int y) { return (static_cast<float>(y) / static_cast<float>(fbH)) * 2.f - 1.f; };

  const float rtW = static_cast<float>(this->rtW());
  const float rtH = static_cast<float>(this->rtH());
  const float pad = static_cast<float>(RenderConfig::kPad);
  const float viewW = static_cast<float>(this->viewW());
  const float viewH = static_cast<float>(this->viewH());
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

const char* PassTimes::name(int pass) {
  static const char* names[kCount] = {"shadow", "reflection", "meshes", "water", "sprites", "edge", "present"};
  return names[pass];
}

void Renderer::beginPass(int pass) {
  if (!profiling_) return;
  if (!queries_[0]) glGenQueries(PassTimes::kCount, queries_);
  glBeginQuery(GL_TIME_ELAPSED, queries_[pass]);
}

void Renderer::endPass() {
  if (profiling_) glEndQuery(GL_TIME_ELAPSED);
}

void Renderer::drawMeshes(const Scene& scene, const std::vector<const Mesh*>& actors, const glm::mat4& viewProj,
                          const glm::mat4& mirror, float clipBelow) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glUseProgram(progMesh_);
  umat(progMesh_, "uViewProj", viewProj);
  umat(progMesh_, "uMirror", mirror);
  u1f(progMesh_, "uClipBelow", clipBelow);
  for (const Mesh& m : scene.meshes) m.draw();
  for (const Mesh* m : actors) m->draw();
}

void Renderer::drawSprites(const Scene&, const CameraFrame&, const FrameParams& params, const glm::mat4& viewProj,
                           const glm::mat4& mirror, float spriteFlip, float clipBelow) {
  glUseProgram(progGrass_);
  umat(progGrass_, "uViewProj", viewProj);
  umat(progGrass_, "uMirror", mirror);
  u1f(progGrass_, "uSpriteFlip", spriteFlip);
  u1f(progGrass_, "uClipBelow", clipBelow);
  u1i(progGrass_, "uLeavesOnly", spriteFlip < 0.f ? 1 : 0);
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

void Renderer::updateFires(const Scene& scene, const FrameParams& params) {
  fireLights_.clear();
  pointLights_.clear();
  if (!params.fire) return;
  const Lighting& lighting = params.lighting;
  // Seconds since the last frame, for the fades. A first frame, or a jump
  // in time, settles them at once.
  const float dt = (fireTime_ < 0.f || params.time < fireTime_ || params.time - fireTime_ > 0.5f) ? -1.f
                                                                                                 : params.time - fireTime_;
  fireTime_ = params.time;

  // Fires burn on real time and lean with the wind, each as well as it is
  // burning. A fire seen for the first time starts burning at once; one no
  // longer listed is dropped.
  // Above the default strength the flames, sparks and smoke are pushed
  // harder than in proportion, so a gale lays the fire over and streams its
  // smoke away; at the default it leans as it always has.
  const float gale = 1.f + 0.8f * std::max(lighting.windStrength - 1.f, 0.f);
  const glm::vec2 wind = (glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{0.f}) *
                         std::min(0.6f * lighting.windStrength * gale, 6.f);
  struct Live {
    std::uint32_t id;
    const Fire* fire;
    float strength;
    bool fresh;
    float size;
  };
  std::vector<Live> live;
  live.reserve(params.fires.size());
  for (const FireDraw& d : params.fires) {
    auto it = fires_.find(d.id);
    const bool fresh = it == fires_.end();
    if (fresh) it = fires_.emplace(d.id, Fire(d.base, 0x9e3779b9u + 7919u * d.id)).first;
    else it->second.moveTo(d.base);
    it->second.setSize(d.size);
    it->second.advanceTo(params.time, wind, d.level, d.smoke);
    live.push_back({d.id, &it->second, lighting.fireLight * d.level, fresh, d.size});
  }
  std::erase_if(fires_, [&](const auto& entry) {
    return std::none_of(live.begin(), live.end(), [&](const Live& l) { return l.id == entry.first; });
  });

  // A small fire - a candle, a torch - lights only through the point
  // lights: a light without shadows, carrying and shining by the square
  // root of its size.
  for (const Live& l : live) {
    if (l.size >= 1.f || l.strength <= 0.f) continue;
    const float k = std::sqrt(std::max(l.size, 0.f));
    pointLights_.push_back({l.fire->base() + glm::vec3{0.f, kFireLightHeight * l.size, 0.f}, lighting.fireReach * k,
                            l.strength * l.fire->flicker() * k, PointLight::kFire});
  }

  auto find = [&](std::uint32_t id) -> const Live* {
    for (const Live& l : live)
      if (l.id == id) return &l;
    return nullptr;
  };
  auto slotOf = [&](std::uint32_t id) {
    for (int s = 0; s < kFireLights; ++s)
      if (slots_[s].held && slots_[s].fire == id) return s;
    return -1;
  };
  auto cubeOf = [&](std::uint32_t id) {
    for (int c = 0; c < kShadowCubes; ++c)
      if (cubes_[c].held && cubes_[c].fire == id) return c;
    return -1;
  };
  // Hand a pool of slots (firelight slots or shadow cubes) to the fires that
  // want them: a held slot fades its fire in, or out once its fire is no
  // longer wanted, and is given up once faded, or at once if its fire is out
  // or gone. A wanted fire takes a free slot, fully at once if it is new.
  const float step = dt < 0.f ? 1.f : dt / kShadowFade;
  auto share = [&](auto* pool, int count, auto amount, const std::vector<const Live*>& wanted,
                   auto holderOf, const char* what) {
    auto isWanted = [&](std::uint32_t id) {
      return std::any_of(wanted.begin(), wanted.end(), [&](const Live* l) { return l->id == id; });
    };
    for (int i = 0; i < count; ++i) {
      auto& slot = pool[i];
      if (!slot.held) continue;
      const Live* l = find(slot.fire);
      if (!l || l->strength <= 0.f) {
        slot.held = false;
        continue;
      }
      slot.leaving = !isWanted(slot.fire);
      float& a = amount(slot);
      a = std::clamp(a + (slot.leaving ? -step : step), 0.f, 1.f);
      if (slot.leaving && a <= 0.f) {
        slot.held = false;
        if (std::getenv("PXE_FIRE_DEBUG"))
          std::fprintf(stderr, "pxe: t %.2f fire %u faded out of %s %d\n", params.time, slot.fire, what, i);
      }
    }
    for (const Live* l : wanted) {
      if (holderOf(l->id) >= 0) continue;
      for (int i = 0; i < count; ++i) {
        auto& slot = pool[i];
        if (slot.held) continue;
        slot.held = true;
        slot.fire = l->id;
        slot.leaving = false;
        amount(slot) = (l->fresh || dt < 0.f) ? 1.f : 0.f;
        if (std::getenv("PXE_FIRE_DEBUG"))
          std::fprintf(stderr, "pxe: t %.2f fire %u takes %s %d%s\n", params.time, l->id, what, i,
                       amount(slot) > 0.f ? "" : ", fading in");
        break;
      }
    }
  };

  // The brightest campfires want a firelight slot; ties go to the earlier
  // listed. A fire holding one counts half as bright again, so two fires of
  // about the same brightness do not trade a slot back and forth.
  std::vector<const Live*> campfires;
  for (const Live& l : live)
    if (l.size >= 1.f && l.strength > 0.f) campfires.push_back(&l);
  std::vector<const Live*> wantLight = campfires;
  std::stable_sort(wantLight.begin(), wantLight.end(), [&](const Live* a, const Live* b) {
    return a->strength * (slotOf(a->id) >= 0 ? 1.5f : 1.f) > b->strength * (slotOf(b->id) >= 0 ? 1.5f : 1.f);
  });
  if (static_cast<int>(wantLight.size()) > kFireLights) wantLight.resize(kFireLights);
  share(slots_, kFireLights, [](LightSlot& s) -> float& { return s.weight; }, wantLight, slotOf, "light");

  // The fires holding a slot light the scene, brightest first. Whatever
  // share of a fire's light is not in a slot goes to the point lights.
  std::vector<const Live*> lit;
  for (const Live* l : campfires) {
    const int s = slotOf(l->id);
    const float weight = s >= 0 ? slots_[s].weight : 0.f;
    if (s >= 0) lit.push_back(l);
    if (weight < 1.f)
      pointLights_.push_back({l->fire->base() + glm::vec3{0.f, kFireLightHeight, 0.f}, lighting.fireReach,
                              l->strength * l->fire->flicker() * (1.f - weight), PointLight::kFire});
  }
  std::stable_sort(lit.begin(), lit.end(), [](const Live* a, const Live* b) { return a->strength > b->strength; });

  // The brightest of those not fading out want shadows, with the same
  // allowance for a fire already holding a cube.
  std::vector<const Live*> wanted;
  for (const Live* l : lit)
    if (!slots_[slotOf(l->id)].leaving) wanted.push_back(l);
  std::stable_sort(wanted.begin(), wanted.end(), [&](const Live* a, const Live* b) {
    return a->strength * (cubeOf(a->id) >= 0 ? 1.5f : 1.f) > b->strength * (cubeOf(b->id) >= 0 ? 1.5f : 1.f);
  });
  if (static_cast<int>(wanted.size()) > kShadowCubes) wanted.resize(kShadowCubes);
  share(cubes_, kShadowCubes, [](ShadowCube& c) -> float& { return c.shadowed; }, wanted, cubeOf, "cube");

  for (const Live* l : lit) {
    FireLight light;
    light.pos = l->fire->base() + glm::vec3{0.f, kFireLightHeight, 0.f};
    light.strength = l->strength * l->fire->flicker() * slots_[slotOf(l->id)].weight;
    light.cube = cubeOf(l->id);
    light.shadowed = light.cube >= 0 ? cubes_[light.cube].shadowed : 0.f;
    fireLights_.push_back(light);
  }

  // Each cube in use: six 90-degree views from its light, each storing the
  // distance to the nearest surface. Redrawn only when its fire moves.
  for (const FireLight& light : fireLights_) {
    if (light.cube < 0) continue;
    ShadowCube& cube = cubes_[light.cube];
    if (cube.drawnScene == &scene && cube.drawnPos == light.pos) continue;
    static const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.05f, 40.f);
    const glm::vec3 firePos = light.pos;
    glBindFramebuffer(GL_FRAMEBUFFER, fboFireCube_);
    glViewport(0, 0, kFireShadow, kFireShadow);
    glUseProgram(progFireShadow_);
    u3f(progFireShadow_, "uLightPos", firePos.x, firePos.y, firePos.z);
    glClearColor(1e4f, 1e4f, 1e4f, 1e4f);
    for (int i = 0; i < 6; ++i) {
      glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, fireCubes_, 0, 6 * light.cube + i);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      umat(progFireShadow_, "uLightViewProj", proj * glm::lookAt(firePos, firePos + dirs[i], ups[i]));
      for (const Mesh& m : scene.meshes) m.draw();
    }
    cube.drawnScene = &scene;
    cube.drawnPos = firePos;
  }
}

int Renderer::uploadPointLights(const CameraFrame& camera) {
  // Each light reaches the pixels inside a circle on screen: the view is
  // orthographic, so a sphere of light draws as a circle of the same radius.
  // A tile lists every light whose circle touches it, in the order the
  // lights are listed, so each pixel sums them in the same order every frame.
  const int w = rtW(), h = rtH();
  tilesX_ = (w + kTile - 1) / kTile;
  const int tilesY = (h + kTile - 1) / kTile;
  const int tiles = tilesX_ * tilesY;
  const float pxPerMetre = static_cast<float>(w) / (2.f * camera.orthoHalfW);
  struct Reach {
    float x, y, r;
    int x0, y0, x1, y1;
  };
  std::vector<Reach> reach;
  reach.reserve(pointLights_.size());
  std::vector<std::uint32_t> counts(static_cast<std::size_t>(tiles), 0);
  auto touches = [](const Reach& r, int tx, int ty) {
    const float cx = std::clamp(r.x, static_cast<float>(tx * kTile), static_cast<float>((tx + 1) * kTile));
    const float cy = std::clamp(r.y, static_cast<float>(ty * kTile), static_cast<float>((ty + 1) * kTile));
    return (cx - r.x) * (cx - r.x) + (cy - r.y) * (cy - r.y) <= r.r * r.r;
  };
  for (const PointLight& l : pointLights_) {
    const glm::vec3 d = l.pos - camera.eye;
    Reach r;
    r.x = (glm::dot(d, camera.right) / camera.orthoHalfW * 0.5f + 0.5f) * static_cast<float>(w);
    r.y = (glm::dot(d, camera.up) / camera.orthoHalfH * 0.5f + 0.5f) * static_cast<float>(h);
    r.r = l.radius * pxPerMetre + 1.f;
    r.x0 = std::max(0, static_cast<int>(std::floor((r.x - r.r) / kTile)));
    r.y0 = std::max(0, static_cast<int>(std::floor((r.y - r.r) / kTile)));
    r.x1 = std::min(tilesX_ - 1, static_cast<int>(std::floor((r.x + r.r) / kTile)));
    r.y1 = std::min(tilesY - 1, static_cast<int>(std::floor((r.y + r.r) / kTile)));
    for (int ty = r.y0; ty <= r.y1; ++ty)
      for (int tx = r.x0; tx <= r.x1; ++tx)
        if (touches(r, tx, ty)) ++counts[static_cast<std::size_t>(ty * tilesX_ + tx)];
    reach.push_back(r);
  }
  // Tile headers (first index, count), then the lists.
  std::vector<std::uint32_t> list(static_cast<std::size_t>(2 * tiles));
  std::uint32_t next = static_cast<std::uint32_t>(2 * tiles);
  for (int t = 0; t < tiles; ++t) {
    list[static_cast<std::size_t>(2 * t)] = next;
    list[static_cast<std::size_t>(2 * t + 1)] = 0;
    next += counts[static_cast<std::size_t>(t)];
  }
  list.resize(next);
  for (std::size_t i = 0; i < reach.size(); ++i) {
    const Reach& r = reach[i];
    for (int ty = r.y0; ty <= r.y1; ++ty)
      for (int tx = r.x0; tx <= r.x1; ++tx) {
        if (!touches(r, tx, ty)) continue;
        const auto t = static_cast<std::size_t>(ty * tilesX_ + tx);
        list[list[2 * t] + list[2 * t + 1]++] = static_cast<std::uint32_t>(i);
      }
  }
  std::vector<float> lights;
  lights.reserve(pointLights_.size() * 8 + 8);
  for (const PointLight& l : pointLights_)
    lights.insert(lights.end(), {l.pos.x, l.pos.y, l.pos.z, l.radius, l.strength, static_cast<float>(l.kind), 0.f, 0.f});
  if (lights.empty()) lights.assign(8, 0.f);  // never an empty buffer
  glBindBuffer(GL_TEXTURE_BUFFER, pointBuf_);
  glBufferData(GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(lights.size() * sizeof(float)), lights.data(), GL_STREAM_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, tileBuf_);
  glBufferData(GL_TEXTURE_BUFFER, static_cast<GLsizeiptr>(list.size() * sizeof(std::uint32_t)), list.data(), GL_STREAM_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
  return static_cast<int>(pointLights_.size());
}

void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {
  const Lighting& lighting = params.lighting;
  const LightFit light = fitLight(scene, lighting.lightDir);
  const LightFit flash = fitLight(scene, lighting.flashDir);
  const float normalBias = 3.f * light.texel;
  const glm::mat4 viewProj = camera.proj * camera.view;
  const float biasNdc = kGrassBiasMetres * 2.f / (camera.farPlane - camera.nearPlane);

  glDisable(GL_BLEND);
  glDisable(GL_DITHER);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  // Both faces: water, foam and the ground are open sheets. A slope-scaled
  // offset keeps faces that turn away from the light from shadowing
  // themselves in speckles.
  glDisable(GL_CULL_FACE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(2.f, 4.f);
  beginPass(PassTimes::kShadow);
  // Nothing in the scene moves but the light, so while the time of day is
  // paused (or has not ticked) last frame's shadow map is still right -
  // unless a grass fire runs, when crowns may be burning away.
  const bool crownShadows = params.grassFire && grid_.running();
  const unsigned progShadow = crownShadows ? progShadowGrid_ : progShadow_;
  if (crownShadows) {
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, grid_.burnTexture());
    glUseProgram(progShadow);
    u1i(progShadow, "uBurn", 10);
    const glm::vec4 d = grid_.burnDomain();
    glUniform4f(uloc(progShadow, "uBurnDomain"), d.x, d.y, d.z, d.w);
  }
  // Characters move every frame: with any about, the scene's shadows are
  // drawn into their own map when they change, copied in, and the
  // characters drawn over them.
  const bool actors = !params.actors.empty();
  const bool sceneShadows = shadowScene_ != &scene || shadowLight_ != lighting.lightDir || crownShadows;
  if (sceneShadows) {
    glBindFramebuffer(GL_FRAMEBUFFER, actors ? fboShadowStatic_ : fboShadow_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow);
    umat(progShadow, "uLightViewProj", light.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    shadowScene_ = &scene;
    shadowLight_ = lighting.lightDir;
    shadowStaticValid_ = actors;
  }
  if (actors || shadowHadActors_) {
    if (!shadowStaticValid_) {
      // The scene's map is in shadow_ alone (drawn before any character came): keep a copy.
      glBindFramebuffer(GL_READ_FRAMEBUFFER, fboShadow_);
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboShadowStatic_);
      glBlitFramebuffer(0, 0, RenderConfig::kShadow, RenderConfig::kShadow, 0, 0, RenderConfig::kShadow,
                        RenderConfig::kShadow, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
      shadowStaticValid_ = true;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboShadowStatic_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboShadow_);
    glBlitFramebuffer(0, 0, RenderConfig::kShadow, RenderConfig::kShadow, 0, 0, RenderConfig::kShadow,
                      RenderConfig::kShadow, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glUseProgram(progShadow);
    umat(progShadow, "uLightViewProj", light.viewProj);
    for (const Mesh* m : params.actors) m->draw();
    shadowHadActors_ = actors;
  }
  // Lightning's map: drawn once when a flash strikes from a new direction.
  if (lighting.flash > 0.f && (flashScene_ != &scene || flashLight_ != lighting.flashDir || crownShadows)) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboFlash_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow);
    umat(progShadow, "uLightViewProj", flash.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    flashScene_ = &scene;
    flashLight_ = lighting.flashDir;
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  endPass();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // The wind's phase, for the grass, leaves and water.
  {
    const float rate = windRate(lighting.windStrength);
    if (windTime_ < 0.f || params.time < windTime_ || params.time - windTime_ > 0.5f) windPhase_ = params.time * rate;
    else windPhase_ += (params.time - windTime_) * rate;
    windTime_ = params.time;
  }
  updateFires(scene, params);
  // The grass fire steps on the GPU before anything reads it this frame.
  const glm::vec2 windDir = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{0.f};
  grid_.setLifetime(params.grassFireLife);
  if (params.grassFire)
    grid_.advance(scene, params.time, params.igniteGrass, params.gridStrokes, lighting.rain,
                  windDir * lighting.windStrength);
  const bool grassFire = params.grassFire && grid_.running();
  progEdge_ = edge_[grassFire ? 1 : 0];
  const ToonPrograms& toon = toon_[grassFire ? 1 : 0][fireLights_.size() > 1 ? 1 : 0];
  progMesh_ = toon.mesh;
  progGrass_ = toon.grass;
  progWater_ = toon.water;
  FireUniforms fireLight;
  for (const FireLight& l : fireLights_) {
    ++fireLight.count;
    fireLight.pos.insert(fireLight.pos.end(), {l.pos.x, l.pos.y, l.pos.z});
    fireLight.strength.push_back(l.strength);
    fireLight.cube.push_back(l.cube);
    fireLight.shadowed.push_back(l.shadowed);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, rtW(), rtH());
  glClearColor(0.f, 0.f, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadow_);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, flashShadow_);
  glActiveTexture(GL_TEXTURE7);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, fireCubes_);
  const glm::mat4 identity{1.f};
  // Burnt grass and lawn, on unit 10, for the programs that read it.
  auto bindBurn = [&](unsigned prog) {
    if (!grassFire) return;
    u1i(prog, "uBurn", 10);
    const glm::vec4 d = grid_.burnDomain();
    glUniform4f(uloc(prog, "uBurnDomain"), d.x, d.y, d.z, d.w);
  };
  if (grassFire) {
    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, grid_.burnTexture());
  }

  // Program state shared by every mesh and sprite draw this frame.
  glUseProgram(progMesh_);
  u1i(progMesh_, "uShowNdl", params.showNdl ? 1 : 0);
  u1f(progMesh_, "uTime", params.time);
  bindToon(progMesh_, lighting, light.viewProj, flash.viewProj, fireLight, normalBias, params.testColours);
  bindBurn(progMesh_);
  if (!params.actors.empty()) {
    // The stone's ramp now, over its mid colour by day: the light a character stands in.
    const glm::vec3 day = glm::max(scene.palette[kPhaseDay][kSurfStone].band[1], glm::vec3{0.02f});
    std::vector<float> tint, colour;
    std::vector<int> glow;
    for (int b = 0; b < 3; ++b) {
      const glm::vec3 t = params.testColours ? glm::vec3(b == 2, b == 1, b == 0) : lighting.palette[kSurfStone].band[b] / day;
      tint.insert(tint.end(), {t.x, t.y, t.z});
    }
    for (const ActorMaterial& m : params.actorMaterials) {
      colour.insert(colour.end(), {m.colour.x, m.colour.y, m.colour.z});
      glow.push_back(m.glow ? 1 : 0);
    }
    u3fv(progMesh_, "uActorTint", 3, tint.data());
    u3fv(progMesh_, "uActorColour", kActorMaterials, colour.data());
    glUniform1iv(uloc(progMesh_, "uActorGlow"), kActorMaterials, glow.data());
    const glm::vec3 fire = lighting.fireBand[kSurfStone] / day;
    u3f(progMesh_, "uActorFire", fire.x, fire.y, fire.z);
  }
  if (params.showGrass && grassCount_ > 0) {
    glUseProgram(progGrass_);
    u2f(progGrass_, "uTarget", static_cast<float>(rtW()), static_cast<float>(rtH()));
    u1f(progGrass_, "uDepthBiasNdc", biasNdc);
    {
      const float metresToNdc = 2.f / (camera.farPlane - camera.nearPlane);
      u1f(progGrass_, "uGrassBiasNdc", kBladeBiasMetres * metresToNdc);
      // An upright point h metres up shows h cos(pitch) metres higher on
      // screen and sits h sin(pitch) nearer: tan(pitch) metres per metre.
      u1f(progGrass_, "uUprightNdcPerPx", std::tan(camera.pitchRadians) * camera.texelWorld * metresToNdc);
    }
    bindToon(progGrass_, lighting, light.viewProj, flash.viewProj, fireLight, normalBias, params.testColours);
    bindBurn(progGrass_);
    u3fv(progGrass_, "uTip", kSurfCount * 3, ramp(lighting, &Palette::tip, params.testColours).data());
    u1i(progGrass_, "uGround", 2);
    // Bound before any sprite draw: the reflection pass draws sprites before
    // this frame's ground copy exists, and an empty unit makes the driver
    // substitute a zero texture (and log a warning). Last frame's copy is
    // fine there, since tips only matter on screen.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ground_);
    u1f(progGrass_, "uTime", params.time);
    const float sway = windSway(lighting.windStrength);
    u1f(progGrass_, "uWindPx", kWindPx * sway);
    u1f(progGrass_, "uWindTime", windPhase_);
    u1f(progGrass_, "uWindRate", windRate(lighting.windStrength));
    // Sprite boxes widen by however much further than usual the blades sway.
    u1f(progGrass_, "uSwayPad", std::ceil(std::max(sway - 1.f, 0.f) * kWindPx * 1.9f));
    const glm::vec2 wd = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{1.f, 0.f};
    u2f(progGrass_, "uWindDir", wd.x, wd.y);
    // Wind blowing straight at or away from the camera barely leans the
    // sprites; keep a floor so the grass still breathes.
    const float lean = glm::dot(glm::vec3{wd.x, 0.f, wd.y}, camera.right);
    u1f(progGrass_, "uWindLean", std::copysign(std::max(std::fabs(lean), 0.35f), lean));
    u1i(progGrass_, "uMagenta", params.magentaGrass ? 1 : 0);
  }

  // Reflection: the scene mirrored about the water plane, clipped to what
  // stands above it, drawn only where the water can be on screen.
  const bool hasWater = !scene.water.empty();
  beginPass(PassTimes::kReflection);
  if (hasWater) {
    const float h = scene.waterLevel;
    const glm::mat4 mirror = glm::translate(glm::mat4{1.f}, {0.f, h, 0.f}) * glm::scale(glm::mat4{1.f}, {1.f, -1.f, 1.f}) *
                             glm::translate(glm::mat4{1.f}, {0.f, -h, 0.f});
    glm::vec2 lo{1e9f}, hi{-1e9f};
    for (int i = 0; i < 8; ++i) {
      const glm::vec3 c{(i & 1) ? scene.waterMax.x : scene.waterMin.x, h, (i & 4) ? scene.waterMax.z : scene.waterMin.z};
      const glm::vec4 q = viewProj * glm::vec4(c, 1.f);
      const glm::vec2 px = (glm::vec2(q) / q.w * 0.5f + 0.5f) * glm::vec2(rtW(), rtH());
      lo = glm::min(lo, px);
      hi = glm::max(hi, px);
    }
    const int x0 = std::clamp(static_cast<int>(lo.x) - 4, 0, rtW());
    const int y0 = std::clamp(static_cast<int>(lo.y) - 4, 0, rtH());
    const int x1 = std::clamp(static_cast<int>(hi.x) + 4, 0, rtW());
    const int y1 = std::clamp(static_cast<int>(hi.y) + 4, 0, rtH());
    glBindFramebuffer(GL_FRAMEBUFFER, fboRefl_);
    glViewport(0, 0, rtW(), rtH());
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (x1 > x0 && y1 > y0) {
      glEnable(GL_SCISSOR_TEST);
      glScissor(x0, y0, x1 - x0, y1 - y0);
      glEnable(GL_CLIP_DISTANCE0);
      glFrontFace(GL_CW);  // the mirror flips winding
      drawMeshes(scene, params.actors, viewProj, mirror, h);
      glFrontFace(GL_CCW);
      if (params.showGrass && grassCount_ > 0) drawSprites(scene, camera, params, viewProj, mirror, -1.f, h);
      glDisable(GL_CLIP_DISTANCE0);
      glDisable(GL_SCISSOR_TEST);
    }
  }

  endPass();

  beginPass(PassTimes::kMeshes);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, rtW(), rtH());
  drawMeshes(scene, params.actors, viewProj, identity, -1e9f);
  endPass();
  beginPass(PassTimes::kWater);

  if (hasWater) {
    // What the water covers, and how far away it is, before the water hides it.
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, bed_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, rtW(), rtH());
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, bedDepth_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, rtW(), rtH());
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, refl_);
    glUseProgram(progWater_);
    umat(progWater_, "uViewProj", viewProj);
    bindToon(progWater_, lighting, light.viewProj, flash.viewProj, fireLight, normalBias, params.testColours);
    u1i(progWater_, "uBed", 3);
    u1i(progWater_, "uBedDepth", 4);
    u1i(progWater_, "uReflection", 5);
    u1f(progWater_, "uDepthMetres", camera.farPlane - camera.nearPlane);
    u1f(progWater_, "uTime", params.time);
    u1f(progWater_, "uWindTime", windPhase_);
    {
      const glm::vec2 wd = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{1.f, 0.f};
      u2f(progWater_, "uWindDir", wd.x, wd.y);
      // Wind ripples on the water from a moderate breeze up.
      u1f(progWater_, "uWindRipple", std::clamp((lighting.windStrength - 1.3f) / 2.2f, 0.f, 1.f));
    }
    glDisable(GL_CULL_FACE);
    for (const Mesh& m : scene.water) m.draw();
  }
  endPass();
  beginPass(PassTimes::kSprites);

  if (params.showGrass && grassCount_ > 0) {
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ground_);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, rtW(), rtH());
    drawSprites(scene, camera, params, viewProj, identity, 1.f, -1e9f);
  }
  endPass();

  // Fire: particles summed into the heat buffer against the scene's depth,
  // for the edge pass to cut into flame colours.
  glBindFramebuffer(GL_FRAMEBUFFER, fboHeat_);
  const float clearHeat[4] = {0.f, 0.f, 0.f, 0.f};
  glClearBufferfv(GL_COLOR, 0, clearHeat);
  int fireCount = 0;
  // Fireflies: each a lit pixel or two inside a faint halo, drawn into the
  // same heat buffer (a channel of their own), and a light for the edge pass.
  std::vector<Firefly> flies;
  if (params.fireflies) flies = firefliesAt(scene.fireflyHomes, params.time);
  std::vector<FireParticle> parts;
  if (params.fire)
    for (const FireDraw& d : params.fires) {
      const auto it = fires_.find(d.id);
      if (it != fires_.end()) parts.insert(parts.end(), it->second.particles().begin(), it->second.particles().end());
    }
  for (const Firefly& f : flies) {
    if (f.glow <= 0.02f) continue;
    parts.push_back({f.pos, 0.09f, 0.3f * f.glow, 4.f});
    parts.push_back({f.pos, 0.04f, f.glow, 4.f});
  }
  if (!parts.empty()) {
    fireCount = static_cast<int>(parts.size());
    glBindBuffer(GL_ARRAY_BUFFER, fireVbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(parts.size() * sizeof(FireParticle)), parts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glViewport(0, 0, rtW(), rtH());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(progFire_);
    umat(progFire_, "uViewProj", viewProj);
    u2f(progFire_, "uTarget", static_cast<float>(rtW()), static_cast<float>(rtH()));
    u1f(progFire_, "uPxPerMetre", 1.f / camera.texelWorld);
    u1f(progFire_, "uDepthBiasNdc", kFireBiasMetres * 2.f / (camera.farPlane - camera.nearPlane));
    glBindVertexArray(fireVao_);
    glDrawArrays(GL_POINTS, 0, fireCount);
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
  }
  // The grass fire's flames, embers and smoke, drawn from the grid.
  const bool gridFlames = grassFire && params.fire;
  if (gridFlames) {
    glViewport(0, 0, rtW(), rtH());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    grid_.drawFlames(viewProj, {static_cast<float>(rtW()), static_cast<float>(rtH())}, 1.f / camera.texelWorld,
                     kFireBiasMetres * 2.f / (camera.farPlane - camera.nearPlane), params.time,
                     windDir * 0.6f * lighting.windStrength);
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
  }

  // Edge pass into final_, which is what gets presented and captured.
  beginPass(PassTimes::kEdge);
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
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, heat_);
  u1i(progEdge_, "uHeat", 3);
  u1i(progEdge_, "uFire", fireCount > 0 || gridFlames ? 1 : 0);
  if (grassFire) {
    // The grid's glow (unit 10 still holds the burn texture).
    glActiveTexture(GL_TEXTURE14);
    glBindTexture(GL_TEXTURE_2D, grid_.glowTexture(true));
    glActiveTexture(GL_TEXTURE15);
    glBindTexture(GL_TEXTURE_2D, grid_.glowTexture(false));
    u1i(progEdge_, "uBurn", 10);
    u1i(progEdge_, "uGlow", 14);
    u1i(progEdge_, "uGlowPrev", 15);
    u1f(progEdge_, "uGlowBlend", grid_.glowBlend());
    u1f(progEdge_, "uGlowLight", gridFlames ? 1.f : 0.f);
    const glm::vec4 d = grid_.burnDomain();
    glUniform4f(uloc(progEdge_, "uGridDomain"), d.x, d.y, d.z, d.w);
  }
  // Point lights - fireflies, small fires, campfires without a firelight
  // slot, and any the game adds - lit in this pass from the depth and
  // normals already drawn rather than in every shader: t3ssel8r's light
  // volumes, a lot of small lights for little cost.
  {
    pointLights_.insert(pointLights_.end(), params.lights.begin(), params.lights.end());
    // Each burning patch of the grass fire, a light of its own.
    if (gridFlames) grid_.addLights(pointLights_, lighting.fireLight, lighting.fireReach, params.time);
    if (lighting.fireflyLight > 0.f)
      for (const Firefly& f : flies)
        if (f.glow > 0.02f) pointLights_.push_back({f.pos, kFireflyReach, f.glow, PointLight::kFirefly});
    u1i(progEdge_, "uPointCount", uploadPointLights(camera));
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_BUFFER, pointTex_);
    u1i(progEdge_, "uPoints", 8);
    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_BUFFER, tileTex_);
    u1i(progEdge_, "uTiles", 9);
    u1i(progEdge_, "uTileSize", kTile);
    u1i(progEdge_, "uTilesX", tilesX_);
    u1f(progEdge_, "uFlyLight", lighting.fireflyLight);
    u1f(progEdge_, "uFlyDim", lighting.fireflyDim);
    std::vector<float> band;
    for (int s = 0; s < kSurfCount; ++s) band.insert(band.end(), {lighting.fireflyBand[s].x, lighting.fireflyBand[s].y, lighting.fireflyBand[s].z});
    u3fv(progEdge_, "uFlyBand", kSurfCount, band.data());
    band.clear();
    for (int s = 0; s < kSurfCount; ++s) band.insert(band.end(), {lighting.fireBand[s].x, lighting.fireBand[s].y, lighting.fireBand[s].z});
    u3fv(progEdge_, "uFireBand", kSurfCount, band.data());
    u1f(progEdge_, "uFireDim", lighting.fireDim);
  }
  // God rays sample the sun's shadow map through the air above each pixel.
  // They are strongest looking toward the sun, where the light scatters
  // forward into the view.
  {
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, shadow_);
    u1i(progEdge_, "uShadowMap", 4);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, canopy_);
    u1i(progEdge_, "uCanopy", 5);
    umat(progEdge_, "uLightViewProj", light.viewProj);
    const glm::vec2 towardSun{lighting.lightDir.x, lighting.lightDir.z};
    const glm::vec2 looking{camera.forward.x, camera.forward.z};
    const float facing = (glm::length(towardSun) > 1e-4f && glm::length(looking) > 1e-4f)
                             ? glm::dot(glm::normalize(towardSun), glm::normalize(looking))
                             : 0.f;
    const float rays = params.rays ? lighting.rays * (0.75f + 0.45f * facing) : 0.f;
    u1f(progEdge_, "uRays", std::max(rays, 0.f));
    u3f(progEdge_, "uRayColour", lighting.rayColour.x, lighting.rayColour.y, lighting.rayColour.z);
    u2f(progEdge_, "uRaySpan", scene.boundsMin.y, std::min(scene.boundsMax.y, scene.boundsMin.y + 14.f));
    u1f(progEdge_, "uCloudCover", lighting.cloudCover);
    u2f(progEdge_, "uCloudOffset", lighting.cloudOffset.x, lighting.cloudOffset.y);
    u1f(progEdge_, "uCloudStrength", lighting.cloudStrength);
  }
  u1f(progEdge_, "uDepthMetres", camera.farPlane - camera.nearPlane);
  u1i(progEdge_, "uEnabled", params.edges ? 1 : 0);
  u3f(progEdge_, "uRim", lighting.rim.x, lighting.rim.y, lighting.rim.z);
  const glm::vec3 edgeLight = glm::normalize(lighting.lightDir);
  u3f(progEdge_, "uLight", edgeLight.x, edgeLight.y, edgeLight.z);
  u3f(progEdge_, "uInk", lighting.ink.x, lighting.ink.y, lighting.ink.z);
  u1f(progEdge_, "uRain", lighting.rain);
  u1f(progEdge_, "uFlash", lighting.flash);
  u1f(progEdge_, "uTime", params.time);
  // The camera's snapped position in whole texels anchors the rain's pixel
  // grid to the world, so it does not slide with the view.
  u2f(progEdge_, "uCamTexel", std::round(glm::dot(camera.eye, camera.right) / camera.texelWorld),
      std::round(glm::dot(camera.eye, camera.up) / camera.texelWorld));
  {
    const glm::vec2 wd = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{1.f, 0.f};
    // Rain leans with the part of the wind that blows across the screen,
    // more as the wind strengthens; wind toward or away from the camera
    // leaves it falling straight.
    const float across = glm::dot(glm::vec3{wd.x, 0.f, wd.y}, camera.right);
    u1f(progEdge_, "uRainSlant", 0.3f * across * std::clamp(0.4f + 0.6f * lighting.windStrength, 0.f, 2.5f));
  }
  u3f(progEdge_, "uEye", camera.eye.x, camera.eye.y, camera.eye.z);
  u3f(progEdge_, "uRight", camera.right.x, camera.right.y, camera.right.z);
  u3f(progEdge_, "uUp", camera.up.x, camera.up.y, camera.up.z);
  u3f(progEdge_, "uFwd", camera.forward.x, camera.forward.y, camera.forward.z);
  u2f(progEdge_, "uOrthoHalf", camera.orthoHalfW, camera.orthoHalfH);
  u1f(progEdge_, "uNear", camera.nearPlane);
  u1f(progEdge_, "uYawTexels", camera.yawRadians / camera.texelWorld);
  const glm::vec3 rainCol = lighting.palette[kSurfFoam].band[2];
  u3f(progEdge_, "uRainColour", rainCol.x, rainCol.y, rainCol.z);
  glBindVertexArray(presentVao_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
  glEnable(GL_DEPTH_TEST);

  endPass();

  beginPass(PassTimes::kPresent);
  if (params.present) drawPresent(camera, params);
  endPass();
  if (profiling_) {
    for (int i = 0; i < PassTimes::kCount; ++i) {
      GLuint64 ns = 0;
      glGetQueryObjectui64v(queries_[i], GL_QUERY_RESULT, &ns);
      passTimes_.ms[i] = static_cast<double>(ns) * 1e-6;
    }
  }
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
  const int w = viewW();
  const int h = viewH();
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
