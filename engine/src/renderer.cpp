#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "pxe/fireflies.hpp"
#include "shader.hpp"
#include "gl.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <iterator>
#include <string>

namespace pxe {
namespace {

constexpr float kGrassBiasMetres = 0.6f;   // leaves, through their own crown
constexpr float kBladeBiasMetres = 0.12f;  // grass, just clear of the ground
constexpr float kWindPx = 1.6f;
// Fire is drawn this far toward the camera, so the logs in front of it hide
// only the bottom of the flames.
constexpr float kFireBiasMetres = 0.25f;
// The firelight shines from above the logs, so they shadow only the ground
// inside the ring of stones.
constexpr float kFireLightHeight = 0.62f;
constexpr int kFireShadow = 512;  // per cube face

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

// `fire`: the firelight's position (xyz) and strength with its flicker (w).
void bindToon(unsigned prog, const Lighting& lighting, const glm::mat4& lightVP, const glm::mat4& flashVP,
              const glm::vec4& fire, float normalBias, bool testColours) {
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
  u3f(prog, "uFirePos", fire.x, fire.y, fire.z);
  u1f(prog, "uFireLight", fire.w);
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
  if (progMesh_) glDeleteProgram(progMesh_);
  if (progGrass_) glDeleteProgram(progGrass_);
  if (progShadow_) glDeleteProgram(progShadow_);
  if (progPresent_) glDeleteProgram(progPresent_);
  if (progEdge_) glDeleteProgram(progEdge_);
  if (progWater_) glDeleteProgram(progWater_);
  if (progFire_) glDeleteProgram(progFire_);
  if (progFireShadow_) glDeleteProgram(progFireShadow_);
  if (fireVbo_) glDeleteBuffers(1, &fireVbo_);
  if (fireVao_) glDeleteVertexArrays(1, &fireVao_);
  destroyTargets();
}

void Renderer::destroyTargets() {
  unsigned textures[] = {color_, normals_, final_, ground_, depth_, shadow_, flashShadow_, heat_, canopy_, refl_, reflDepth_, bed_, bedDepth_};
  glDeleteTextures(static_cast<GLsizei>(std::size(textures)), textures);
  color_ = normals_ = final_ = ground_ = depth_ = shadow_ = flashShadow_ = heat_ = canopy_ = refl_ = reflDepth_ = bed_ = bedDepth_ = 0;
  if (fboHeat_) glDeleteFramebuffers(1, &fboHeat_);
  fboHeat_ = 0;
  if (fireCube_) glDeleteTextures(1, &fireCube_);
  if (fireCubeDepth_) glDeleteTextures(1, &fireCubeDepth_);
  if (fboFireCube_) glDeleteFramebuffers(1, &fboFireCube_);
  fireCube_ = fireCubeDepth_ = fboFireCube_ = 0;
  fireShadowScene_ = nullptr;
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
  if (fboFinal_) glDeleteFramebuffers(1, &fboFinal_);
  if (fboRefl_) glDeleteFramebuffers(1, &fboRefl_);
  fboFinal_ = fboRefl_ = 0;
  if (fboShadow_) glDeleteFramebuffers(1, &fboShadow_);
  if (fboFlash_) glDeleteFramebuffers(1, &fboFlash_);
  fbo_ = fboShadow_ = fboFlash_ = 0;
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

  glGenTextures(1, &fireCube_);
  glBindTexture(GL_TEXTURE_CUBE_MAP, fireCube_);
  for (int i = 0; i < 6; ++i)
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_R32F, kFireShadow, kFireShadow, 0, GL_RED, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
  fireCubeDepth_ = makeDepth(kFireShadow, kFireShadow, false);
  glGenFramebuffers(1, &fboFireCube_);
  glBindFramebuffer(GL_FRAMEBUFFER, fboFireCube_);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X, fireCube_, 0);
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
  progWater_ = linkProgram(dir + "/water.vert", dir + "/water.frag", dir + "/common.glsl");
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
  fires_.clear();
  for (std::size_t i = 0; i < scene.fires.size(); ++i)
    fires_.emplace_back(scene.fires[i], 0x9e3779b9u + 7919u * static_cast<std::uint32_t>(i));
  grassCount_ = static_cast<int>(scene.grass.size());
  glBindBuffer(GL_ARRAY_BUFFER, grassInst_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scene.grass.size() * sizeof(GrassInstance)),
               scene.grass.empty() ? nullptr : scene.grass.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
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

void Renderer::drawMeshes(const Scene& scene, const glm::mat4& viewProj, const glm::mat4& mirror, float clipBelow) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glUseProgram(progMesh_);
  umat(progMesh_, "uViewProj", viewProj);
  umat(progMesh_, "uMirror", mirror);
  u1f(progMesh_, "uClipBelow", clipBelow);
  for (const Mesh& m : scene.meshes) m.draw();
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

void Renderer::render(const Scene& scene, const CameraFrame& camera, const FrameParams& params) {
  const Lighting& lighting = params.lighting;
  const LightFit light = fitLight(scene, lighting.lightDir);
  const LightFit flash = fitLight(scene, lighting.flashDir);
  // Fires burn on real time and lean with the wind; the first one lights
  // the scene around it, flickering with its flames.
  glm::vec4 fireLight{0.f};
  if (params.fire) {
    const glm::vec2 wind = (glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{0.f}) *
                           0.6f * lighting.windStrength;
    for (Fire& f : fires_) f.advanceTo(params.time, wind);
    if (!fires_.empty() && lighting.fireLight > 0.f)
      fireLight = glm::vec4{fires_[0].base() + glm::vec3{0.f, kFireLightHeight, 0.f}, lighting.fireLight * fires_[0].flicker()};
  }
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
  // paused (or has not ticked) last frame's shadow map is still right.
  if (shadowScene_ != &scene || shadowLight_ != lighting.lightDir) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboShadow_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow_);
    umat(progShadow_, "uLightViewProj", light.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    shadowScene_ = &scene;
    shadowLight_ = lighting.lightDir;
  }
  // Lightning's map: drawn once when a flash strikes from a new direction.
  if (lighting.flash > 0.f && (flashScene_ != &scene || flashLight_ != lighting.flashDir)) {
    glBindFramebuffer(GL_FRAMEBUFFER, fboFlash_);
    glViewport(0, 0, RenderConfig::kShadow, RenderConfig::kShadow);
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(progShadow_);
    umat(progShadow_, "uLightViewProj", flash.viewProj);
    for (const Mesh& m : scene.meshes) m.draw();
    flashScene_ = &scene;
    flashLight_ = lighting.flashDir;
  }
  glDisable(GL_POLYGON_OFFSET_FILL);
  endPass();
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  // The fire's shadow cube: six 90-degree views from the firelight, each
  // storing the distance to the nearest surface.
  const glm::vec3 firePos{fireLight};
  if (fireLight.w > 0.f && (fireShadowScene_ != &scene || fireShadowPos_ != firePos)) {
    static const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    const glm::mat4 proj = glm::perspective(glm::radians(90.f), 1.f, 0.05f, 40.f);
    glBindFramebuffer(GL_FRAMEBUFFER, fboFireCube_);
    glViewport(0, 0, kFireShadow, kFireShadow);
    glUseProgram(progFireShadow_);
    u3f(progFireShadow_, "uLightPos", firePos.x, firePos.y, firePos.z);
    glClearColor(1e4f, 1e4f, 1e4f, 1e4f);
    for (int i = 0; i < 6; ++i) {
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, fireCube_, 0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      umat(progFireShadow_, "uLightViewProj", proj * glm::lookAt(firePos, firePos + dirs[i], ups[i]));
      for (const Mesh& m : scene.meshes) m.draw();
    }
    fireShadowScene_ = &scene;
    fireShadowPos_ = firePos;
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
  glBindTexture(GL_TEXTURE_CUBE_MAP, fireCube_);
  const glm::mat4 identity{1.f};

  // Program state shared by every mesh and sprite draw this frame.
  glUseProgram(progMesh_);
  u1i(progMesh_, "uShowNdl", params.showNdl ? 1 : 0);
  u1f(progMesh_, "uTime", params.time);
  bindToon(progMesh_, lighting, light.viewProj, flash.viewProj, fireLight, normalBias, params.testColours);
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
    u3fv(progGrass_, "uTip", kSurfCount * 3, ramp(lighting, &Palette::tip, params.testColours).data());
    u1i(progGrass_, "uGround", 2);
    // Bound before any sprite draw: the reflection pass draws sprites before
    // this frame's ground copy exists, and an empty unit makes the driver
    // substitute a zero texture (and log a warning). Last frame's copy is
    // fine there, since tips only matter on screen.
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, ground_);
    u1f(progGrass_, "uTime", params.time);
    u1f(progGrass_, "uWindPx", kWindPx);
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
      drawMeshes(scene, viewProj, mirror, h);
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
  drawMeshes(scene, viewProj, identity, -1e9f);
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
    for (const Fire& f : fires_) parts.insert(parts.end(), f.particles().begin(), f.particles().end());
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
  u1i(progEdge_, "uFire", fireCount > 0 ? 1 : 0);
  // Firefly light, lit in this pass from the depth and normals already
  // drawn rather than in every shader: t3ssel8r's light volumes, a lot of
  // small lights for little cost.
  {
    constexpr int kMaxFlies = 48;
    std::vector<float> fly;
    for (const Firefly& f : flies)
      if (f.glow > 0.02f && static_cast<int>(fly.size()) < kMaxFlies * 4) fly.insert(fly.end(), {f.pos.x, f.pos.y, f.pos.z, f.glow});
    const int count = lighting.fireflyLight > 0.f ? static_cast<int>(fly.size() / 4) : 0;
    u1i(progEdge_, "uFlyCount", count);
    if (count > 0) glUniform4fv(uloc(progEdge_, "uFly"), count, fly.data());
    u1f(progEdge_, "uFlyLight", lighting.fireflyLight);
    u1f(progEdge_, "uFlyDim", lighting.fireflyDim);
    std::vector<float> band;
    for (int s = 0; s < kSurfCount; ++s) band.insert(band.end(), {lighting.fireflyBand[s].x, lighting.fireflyBand[s].y, lighting.fireflyBand[s].z});
    u3fv(progEdge_, "uFlyBand", kSurfCount, band.data());
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
