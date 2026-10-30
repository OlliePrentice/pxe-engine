#include "pxe/renderer.hpp"

#include "pxe/config.hpp"
#include "renderer_internal.hpp"
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

// Windows a room's sky light comes through, at most (kSkyWindows in common.glsl).
constexpr int kSkyWindows = 8;
// Dark regions drawn, at most (kDarkRegions in common.glsl and edge.frag).
constexpr int kDarkRegions = 8;

void requireFbo(const char* what) {
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    std::fprintf(stderr, "pxe: framebuffer incomplete (%s)\n", what);
    throw std::runtime_error("incomplete framebuffer");
  }
}

}  // namespace

namespace detail {

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

// The characters, each skinned in the vertex shader by its own joints, in
// the program in use (mesh.vert or shadow.vert); one draw each.
void drawActors(unsigned prog, const std::vector<ActorDraw>& actors) {
  if (actors.empty()) return;
  u1i(prog, "uSkinned", 1);
  const int joints = uloc(prog, "uJoints");
  for (const ActorDraw& a : actors) {
    if (!a.mesh || a.joints.empty()) continue;
    const auto count = static_cast<int>(std::min<std::size_t>(a.joints.size(), kMaxJoints));
    glUniformMatrix4fv(joints, count, GL_FALSE, &a.joints[0][0][0]);
    a.mesh->draw();
  }
  u1i(prog, "uSkinned", 0);
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

// The scene's dark regions (Scene::darkRegions), in the toon shaders or the
// edge pass: per region its centre, its own +x and +z in the world, its half
// extents, the open face's outward normal in its own frame, and its start
// and end strengths and fade distance.
void bindDarkRegions(unsigned prog, const Scene& scene) {
  std::vector<float> centre, axisX, axisZ, half, open, fade;
  for (const DarkRegion& r : scene.darkRegions) {
    if (centre.size() / 3 >= static_cast<std::size_t>(kDarkRegions)) break;
    const float c = std::cos(r.yaw), s = std::sin(r.yaw);
    const int face = static_cast<int>(r.openFace);
    glm::vec3 out{0.f};
    out[face / 2] = face % 2 == 0 ? 1.f : -1.f;
    centre.insert(centre.end(), {r.centre.x, r.centre.y, r.centre.z});
    axisX.insert(axisX.end(), {c, 0.f, -s});
    axisZ.insert(axisZ.end(), {s, 0.f, c});
    half.insert(half.end(), {r.halfExtents.x, r.halfExtents.y, r.halfExtents.z});
    open.insert(open.end(), {out.x, out.y, out.z});
    fade.insert(fade.end(), {r.startStrength, r.endStrength, r.fadeDistance});
  }
  const int count = static_cast<int>(centre.size() / 3);
  u1i(prog, "uDarkCount", count);
  if (count > 0) {
    u3fv(prog, "uDarkCentre", count, centre.data());
    u3fv(prog, "uDarkAxisX", count, axisX.data());
    u3fv(prog, "uDarkAxisZ", count, axisZ.data());
    u3fv(prog, "uDarkHalf", count, half.data());
    u3fv(prog, "uDarkOpen", count, open.data());
    u3fv(prog, "uDarkFade", count, fade.data());
  }
}

// The firelights' shadow cubes are on unit 7.
void bindToon(unsigned prog, const Lighting& lighting, const Scene& scene, const glm::mat4& lightVP,
              const glm::mat4& flashVP, const FireUniforms& fire, float normalBias, bool testColours) {
  const bool indoors = scene.indoors;
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
  u1i(prog, "uIndoors", indoors ? 1 : 0);
  // The room's fill light, indoors only.
  u1f(prog, "uFill", indoors ? lighting.fill : 0.f);
  const glm::vec3 fill = glm::length(scene.fillDir) > 1e-4f ? glm::normalize(scene.fillDir) : glm::vec3{0.f, 1.f, 0.f};
  u3f(prog, "uFillDir", fill.x, fill.y, fill.z);
  // The sky's light through the room's windows, indoors only; its tint
  // goes on the fill light too.
  {
    std::vector<float> centre, inward, half;
    for (const SkyWindow& w : scene.windows) {
      if (!indoors || centre.size() / 3 >= static_cast<std::size_t>(kSkyWindows)) break;
      const glm::vec3 in = glm::normalize(glm::vec3{w.inward.x, 0.f, w.inward.z});
      centre.insert(centre.end(), {w.centre.x, w.centre.y, w.centre.z});
      inward.insert(inward.end(), {in.x, in.y, in.z});
      half.insert(half.end(), {0.5f * w.width, 0.5f * w.height});
    }
    const int count = static_cast<int>(centre.size() / 3);
    u1i(prog, "uWindowCount", count);
    if (count > 0) {
      u3fv(prog, "uWindowCentre", count, centre.data());
      u3fv(prog, "uWindowInward", count, inward.data());
      glUniform2fv(uloc(prog, "uWindowHalf"), count, half.data());
    }
    u1f(prog, "uSkyLight", indoors && count > 0 ? lighting.skyLight : 0.f);
    const glm::vec3 tint = indoors ? lighting.skyTint : glm::vec3{1.f};
    u3f(prog, "uSkyTint", tint.x, tint.y, tint.z);
  }
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
  bindDarkRegions(prog, scene);
}

}  // namespace detail

using namespace detail;

Renderer::~Renderer() {
  if (queries_[0]) glDeleteQueries(PassTimes::kCount, queries_);
  if (grassVao_) glDeleteVertexArrays(1, &grassVao_);
  if (grassCorner_) glDeleteBuffers(1, &grassCorner_);
  if (grassInst_) glDeleteBuffers(1, &grassInst_);
  if (presentVao_) glDeleteVertexArrays(1, &presentVao_);
  if (presentVbo_) glDeleteBuffers(1, &presentVbo_);
  if (skyVao_) glDeleteVertexArrays(1, &skyVao_);
  if (skyVbo_) glDeleteBuffers(1, &skyVbo_);
  if (progSky_) glDeleteProgram(progSky_);
  if (progBackdrop_) glDeleteProgram(progBackdrop_);
  if (dustVao_) glDeleteVertexArrays(1, &dustVao_);
  if (progDust_) glDeleteProgram(progDust_);
  if (progMeshLit_) glDeleteProgram(progMeshLit_);
  for (const auto& row : toon_)
    for (const ToonPrograms& t : row)
      for (unsigned prog : {t.mesh, t.grass, t.water})
        if (prog) glDeleteProgram(prog);
  if (progShadow_) glDeleteProgram(progShadow_);
  if (progShadowGrid_) glDeleteProgram(progShadowGrid_);
  if (progPresent_) glDeleteProgram(progPresent_);
  for (unsigned prog : edge_)
    if (prog) glDeleteProgram(prog);
  if (edgePerspective_) glDeleteProgram(edgePerspective_);
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
  for (ShadowCube& cube : cubes_) cube.drawn = false;
  if (fbo_) glDeleteFramebuffers(1, &fbo_);
  if (fboFinal_) glDeleteFramebuffers(1, &fboFinal_);
  if (fboRefl_) glDeleteFramebuffers(1, &fboRefl_);
  fboFinal_ = fboRefl_ = 0;
  if (fboShadow_) glDeleteFramebuffers(1, &fboShadow_);
  if (fboFlash_) glDeleteFramebuffers(1, &fboFlash_);
  if (fboShadowStatic_) glDeleteFramebuffers(1, &fboShadowStatic_);
  fbo_ = fboShadow_ = fboFlash_ = fboShadowStatic_ = 0;
  shadowValid_ = false;
  shadowStaticValid_ = false;
  flashValid_ = false;
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

  glGenVertexArrays(1, &skyVao_);
  glGenBuffers(1, &skyVbo_);
  glBindVertexArray(skyVao_);
  glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, reinterpret_cast<void*>(0));
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::init(RenderMode mode) {
  mode_ = mode;
  for (int grid = 0; grid < 2; ++grid)
    for (int many = 0; many < 2; ++many) {
      const std::string defines = std::string(many ? "" : "#define ONE_FIRE_LIGHT\n") + (grid ? "#define FIRE_GRID\n" : "");
      ToonPrograms& t = toon_[grid][many];
      t.mesh = linkProgram("mesh.vert", "mesh.frag", "common.glsl", defines);
      t.grass = linkProgram("grass.vert", "grass.frag", "common.glsl", defines);
      t.water = linkProgram("water.vert", "water.frag", "common.glsl", defines);
    }
  grid_.init();
  progShadow_ = linkProgram("shadow.vert", "shadow.frag");
  progShadowGrid_ = linkProgram("shadow.vert", "shadow.frag", {}, "#define FIRE_GRID\n");
  progPresent_ = linkProgram("present.vert", "present.frag");
  if (mode == RenderMode::kOrthographic) {
    edge_[0] = linkProgram("edge.vert", "edge.frag");
    edge_[1] = linkProgram("edge.vert", "edge.frag", {}, "#define FIRE_GRID\n");
  } else {
    edgePerspective_ = linkProgram("edge.vert", "edge_perspective.frag");
  }
  progFire_ = linkProgram("fire.vert", "fire.frag");
  progFireShadow_ = linkProgram("fireshadow.vert", "fireshadow.frag");
  progSky_ = linkProgram("sky.vert", "sky.frag");
  if (mode == RenderMode::kPerspective) {
    progBackdrop_ = linkProgram("edge.vert", "backdrop.frag");
    progMeshLit_ = linkProgram("mesh.vert", "mesh_perspective.frag", "lit_perspective.glsl");
    progDust_ = linkProgram("dust.vert", "dust.frag");
    glGenVertexArrays(1, &dustVao_);  // empty: the shader reads only gl_VertexID
  }
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

void Renderer::upload(const Scene& scene, bool keepGrassFire) {
  shadowValid_ = false;
  shadowStaticValid_ = false;
  flashValid_ = false;
  if (!keepGrassFire) grid_.reset();
  fires_.clear();
  for (ShadowCube& cube : cubes_) cube = ShadowCube{};
  for (LightSlot& slot : slots_) slot = LightSlot{};
  fireLights_.clear();
  fireTime_ = -1.f;
  dustOffset_ = glm::vec3{0.f};
  dustTime_ = -1.f;
  uploadGrass(scene);

  // A pane of sky behind each window, in a plane a little outside the
  // wall's outer face and a little bigger than the opening, so the wall
  // round the opening hides its edges and only the opening shows it.
  std::vector<float> sky;
  if (scene.indoors)
    for (const SkyWindow& w : scene.windows) {
      const glm::vec3 in = glm::normalize(glm::vec3{w.inward.x, 0.f, w.inward.z});
      const glm::vec3 across{-in.z, 0.f, in.x};
      const glm::vec3 mid = w.centre - in * (w.depth + 0.02f);
      const glm::vec3 a = across * (0.5f * w.width + 0.05f);
      const glm::vec3 b{0.f, 0.5f * w.height + 0.05f, 0.f};
      const glm::vec3 c[4] = {mid - a - b, mid + a - b, mid + a + b, mid - a + b};
      for (int i : {0, 1, 2, 0, 2, 3}) sky.insert(sky.end(), {c[i].x, c[i].y, c[i].z});
    }
  skyVertices_ = static_cast<int>(sky.size() / 3);
  glBindBuffer(GL_ARRAY_BUFFER, skyVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sky.size() * sizeof(float)), sky.empty() ? nullptr : sky.data(),
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::uploadGrass(const Scene& scene) {
  grassCount_ = static_cast<int>(scene.grass.size());
  glBindBuffer(GL_ARRAY_BUFFER, grassInst_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(scene.grass.size() * sizeof(GrassInstance)),
               scene.grass.empty() ? nullptr : scene.grass.data(), GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Renderer::drawPresent(glm::vec2 texelError, const FrameParams& params) {
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
  const float u0 = (pad + texelError.x) / rtW;
  const float v0 = (pad + texelError.y) / rtH;
  const float u1 = (pad + viewW + texelError.x) / rtW;
  const float v1 = (pad + viewH + texelError.y) / rtH;

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

void Renderer::drawMeshes(const Scene& scene, const FrameParams& params, const glm::mat4& viewProj,
                          const glm::mat4& mirror, float clipBelow) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glUseProgram(progMesh_);
  umat(progMesh_, "uViewProj", viewProj);
  umat(progMesh_, "uMirror", mirror);
  u1f(progMesh_, "uClipBelow", clipBelow);
  for (const Mesh& m : scene.meshes) m.draw();
  drawActors(progMesh_, params.actors);
  glDisable(GL_CULL_FACE);
  for (const Mesh* m : params.actorEffects) m->draw();
  glEnable(GL_CULL_FACE);
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
