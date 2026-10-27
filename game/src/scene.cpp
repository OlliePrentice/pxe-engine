#include "scene.hpp"

#include "pxe/gltf.hpp"
#include "pxe/scatter.hpp"
#include "pxe/scene_geometry.hpp"
#include "pxe/triangle_grid.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace game {
namespace {

constexpr float kPi = 3.14159265358979323846f;
// Where the shelter test (--shelter-test) builds its roofed campfire.
constexpr float kShelterTestX = -8.5f;
constexpr float kShelterTestZ = -9.f;

glm::vec3 hex(std::uint32_t rgb) {
  return glm::vec3{static_cast<float>((rgb >> 16) & 0xff), static_cast<float>((rgb >> 8) & 0xff),
                   static_cast<float>(rgb & 0xff)} /
         255.f;
}

// shadow, mid, lit
void setRamp(glm::vec3 (&out)[3], std::uint32_t shadow, std::uint32_t mid, std::uint32_t lit) {
  out[0] = hex(shadow);
  out[1] = hex(mid);
  out[2] = hex(lit);
}

// The grass the options ask for, its clump heights in patches.
pxe::GrassScatter grassScatter(const BuildOptions& options) {
  pxe::GrassScatter grass;
  grass.clumpSpacing = options.minDist;
  grass.tuftSpacing = options.tuftDist;
  grass.tufts = options.tufts;
  grass.height = pxe::grassPatchiness;
  return grass;
}

// ---------------------------------------------------------------------------
// Hills: the rolling heightfield the grass shaders were first tuned on.

constexpr float kHillsYaw = 0.7853981633974483f;  // 45°, fixed with the sun and the hill
constexpr float kHalf = 45.f;
constexpr float kGrid = 0.25f;
constexpr float kNormalEps = 0.05f;

float hillHeight(float x, float z) {
  constexpr float kHillH = 6.f, kHillW = 8.f, kRipA = 0.6f, kRipL = 14.f, kWarpA = 1.2f, kCross = 0.25f,
                  kDet = 0.018f;
  const float hill =
      kHillH / (1.f + std::exp(-(std::sin(kHillsYaw) * x + std::cos(kHillsYaw) * z) / kHillW));
  const float warp = kWarpA * std::sin(2.f * kPi * (x - 0.5f * z) / 23.f) +
                     0.5f * kWarpA * std::sin(2.f * kPi * (0.4f * x + z) / 31.f);
  const float kx = std::sin(kHillsYaw + 0.25f);
  const float kz = std::cos(kHillsYaw + 0.25f);
  float rip = kRipA * std::sin(2.f * kPi * (kx * x + kz * z) / kRipL + warp);
  rip += kCross * kRipA *
         std::sin(2.f * kPi * (std::sin(kHillsYaw - 0.5f) * x + std::cos(kHillsYaw - 0.5f) * z) /
                      (kRipL * 0.73f) +
                  1.3f);
  const float det = kDet * std::sin(2.f * kPi * (0.9f * x - 0.4f * z) / 2.3f + std::sin(1.7f * z)) *
                    std::sin(2.f * kPi * (0.3f * x + z) / 1.9f + std::cos(1.3f * x));
  return hill + rip + det;
}

BuiltScene buildHills(const BuildOptions& options) {
  BuiltScene built;
  pxe::Scene& scene = built.scene;
  // The engine's default palettes, with greener, brighter day grass.
  pxe::Palette& day = scene.palette[pxe::kPhaseDay][pxe::kSurfGrass];
  setRamp(day.band, 0x46766e, 0x83ab5c, 0xc7e487);
  setRamp(day.detail, 0x5b8a6e, 0x6b944e, 0x9cc266);
  setRamp(day.tip, 0x46766e, 0xa2c66c, 0xe2f3a8);
  scene.tLit = 0.75f;
  scene.tMid = 0.585f;
  built.yawDegrees = 45.f;

  const int n = static_cast<int>(std::lround(2.f * kHalf / kGrid)) + 1;
  std::vector<pxe::Vertex> verts(static_cast<std::size_t>(n * n));
  auto hash = [](int a, int b) {
    std::uint32_t h = static_cast<std::uint32_t>(a) * 374761393u ^ static_cast<std::uint32_t>(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (static_cast<int>(h & 0xffff) - 32768) / 32768.f;
  };
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const float x = -kHalf + static_cast<float>(i) * kGrid;
      const float z = -kHalf + static_cast<float>(j) * kGrid;
      const float hx = (hillHeight(x + kNormalEps, z) - hillHeight(x - kNormalEps, z)) / (2.f * kNormalEps);
      const float hz = (hillHeight(x, z + kNormalEps) - hillHeight(x, z - kNormalEps)) / (2.f * kNormalEps);
      // A few degrees of stable noise so shallow N·L edges wobble instead of
      // sitting on a straight pixel run.
      glm::vec3 nrm = glm::normalize(glm::vec3{-hx + hash(i, j) * 0.028f, 1.f, -hz + hash(j + 19, i * 3 + 1) * 0.028f});
      verts[static_cast<std::size_t>(j * n + i)] = {x, hillHeight(x, z), z, nrm.x, nrm.y, nrm.z,
                                                    static_cast<float>(pxe::kSurfGrass)};
    }
  }
  std::vector<std::uint32_t> indices;
  indices.reserve(static_cast<std::size_t>(n - 1) * (n - 1) * 6);
  for (int j = 0; j < n - 1; ++j) {
    for (int i = 0; i < n - 1; ++i) {
      const auto i00 = static_cast<std::uint32_t>(j * n + i);
      const auto i10 = i00 + 1;
      const auto i01 = i00 + static_cast<std::uint32_t>(n);
      const auto i11 = i01 + 1;
      indices.insert(indices.end(), {i00, i11, i10});
      indices.insert(indices.end(), {i00, i01, i11});
    }
  }
  pxe::SurfaceGeometry geometry;
  geometry.append(pxe::kSurfGrass, verts, indices);
  pxe::buildSceneMaps(scene, geometry);
  pxe::uploadScene(scene, geometry);

  const glm::vec3 flatForward{std::sin(kHillsYaw), 0.f, std::cos(kHillsYaw)};
  const glm::vec3 right{-std::cos(kHillsYaw), 0.f, std::sin(kHillsYaw)};
  scene.sunEast = right;
  scene.sunSouth = flatForward;
  built.focus = glm::vec3{0.f, 2.f, 0.f};

  std::mt19937 rng(7);
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  const pxe::TriangleGrid ground(pxe::trianglesOf(verts, indices), 1.f);
  if (options.stamps) {
    const float step = 2.6f;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 4; ++col) {
        const glm::vec3 p = right * (static_cast<float>(col) - 1.5f) * step +
                            flatForward * (static_cast<float>(row) - 0.5f) * step;
        if (const auto hit = ground.at(p.x, p.z)) {
          scene.grass.push_back({hit->pos.x, hit->pos.y, hit->pos.z, hit->normal.x, hit->normal.y,
                                 hit->normal.z, static_cast<float>(row), static_cast<float>(col) * 0.25f, 1.f,
                                 static_cast<float>(pxe::kSurfGrass)});
        }
      }
    }
  } else {
    pxe::scatterGrass(scene, ground, grassScatter(options), rng);
  }
  return built;
}

// ---------------------------------------------------------------------------
// Campfire geometry, appended to the per-surface vertex lists.

using VertexLists = std::vector<pxe::Vertex> (&)[pxe::kSurfCount];
using IndexLists = std::vector<std::uint32_t> (&)[pxe::kSurfCount];

// A flat quad a-b-c-d with normal n, wound to face along n so back-face
// culling keeps it.
void addQuad(VertexLists verts, IndexLists indices, int surface, glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
             glm::vec3 n) {
  auto& v = verts[surface];
  auto& idx = indices[surface];
  if (glm::dot(glm::cross(b - a, c - a), n) < 0.f) std::swap(b, d);
  const auto base = static_cast<std::uint32_t>(v.size());
  const float s = static_cast<float>(surface);
  for (const glm::vec3& p : {a, b, c, d}) v.push_back({p.x, p.y, p.z, n.x, n.y, n.z, s});
  idx.insert(idx.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

// A box with flat faces: centre, half-extents along its own axes.
void addBox(VertexLists verts, IndexLists indices, int surface, glm::vec3 centre, glm::vec3 half, const glm::vec3 (&axis)[3]) {
  for (int a = 0; a < 3; ++a)
    for (float sign : {-1.f, 1.f}) {
      const glm::vec3 n = axis[a] * sign;
      const glm::vec3 u = axis[(a + 1) % 3] * half[(a + 1) % 3];
      const glm::vec3 w = axis[(a + 2) % 3] * half[(a + 2) % 3];
      const glm::vec3 c = centre + n * half[a];
      addQuad(verts, indices, surface, c - u - w, c + u - w, c + u + w, c - u + w, n);
    }
}

// A log: a six-sided prism from p0 to p1, flat-faced like the trunks.
void addLog(VertexLists verts, IndexLists indices, glm::vec3 p0, glm::vec3 p1, float radius, float twist) {
  const glm::vec3 a = glm::normalize(p1 - p0);
  const glm::vec3 helper = std::fabs(a.y) < 0.9f ? glm::vec3{0.f, 1.f, 0.f} : glm::vec3{1.f, 0.f, 0.f};
  const glm::vec3 b1 = glm::normalize(glm::cross(a, helper));
  const glm::vec3 b2 = glm::cross(a, b1);
  auto dir = [&](float t) { return b1 * std::cos(t) + b2 * std::sin(t); };
  const int sides = 6;
  for (int i = 0; i < sides; ++i) {
    const float t0 = twist + 2.f * kPi * static_cast<float>(i) / sides;
    const float t1 = twist + 2.f * kPi * static_cast<float>(i + 1) / sides;
    const glm::vec3 n = dir(0.5f * (t0 + t1));
    addQuad(verts, indices, pxe::kSurfBark, p0 + dir(t0) * radius, p0 + dir(t1) * radius, p1 + dir(t1) * radius,
            p1 + dir(t0) * radius, n);
  }
  // End caps, as fans of quads round the axis.
  for (float end : {0.f, 1.f}) {
    const glm::vec3 c = end > 0.5f ? p1 : p0;
    const glm::vec3 n = end > 0.5f ? a : -a;
    for (int i = 0; i < sides; i += 2) {
      const float t0 = twist + 2.f * kPi * static_cast<float>(i) / sides;
      const float t1 = twist + 2.f * kPi * static_cast<float>(i + 1) / sides;
      const float t2 = twist + 2.f * kPi * static_cast<float>(i + 2) / sides;
      addQuad(verts, indices, pxe::kSurfBark, c, c + dir(t0) * radius, c + dir(t1) * radius, c + dir(t2) * radius, n);
    }
  }
}

// A small campfire on the lawn at (x, z): a scorched dirt patch, a ring of
// rough stones, and logs leant together in a teepee. Returns where the
// flames rise from.
glm::vec3 addCampfire(VertexLists verts, IndexLists indices, const pxe::TriangleGrid& ground, float x, float z,
                      std::mt19937& rng) {
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  auto heightAt = [&](float px, float pz) {
    const auto hit = ground.at(px, pz);
    return hit ? hit->pos.y : 0.f;
  };
  const glm::vec3 centre{x, heightAt(x, z), z};

  // Dirt patch: a ragged disc just above the lawn.
  const int segments = 20;
  const glm::vec3 up{0.f, 1.f, 0.f};
  std::vector<glm::vec3> rim;
  for (int i = 0; i < segments; ++i) {
    const float t = 2.f * kPi * static_cast<float>(i) / segments;
    const float r = 0.82f + 0.14f * unit(rng);
    const float px = x + r * std::cos(t), pz = z + r * std::sin(t);
    rim.push_back({px, heightAt(px, pz) + 0.02f, pz});
  }
  const glm::vec3 mid = centre + glm::vec3{0.f, 0.02f, 0.f};
  for (int i = 0; i < segments; i += 2)
    addQuad(verts, indices, pxe::kSurfDirt, mid, rim[static_cast<std::size_t>(i)],
            rim[static_cast<std::size_t>((i + 1) % segments)], rim[static_cast<std::size_t>((i + 2) % segments)], up);

  // Stones round the edge, each turned to face the middle and tipped a little.
  const int stones = 9;
  for (int i = 0; i < stones; ++i) {
    const float t = 2.f * kPi * (static_cast<float>(i) + 0.3f * (unit(rng) - 0.5f)) / stones;
    const float r = 0.62f + 0.05f * (unit(rng) - 0.5f);
    const glm::vec3 half{0.12f + 0.04f * unit(rng), 0.08f + 0.04f * unit(rng), 0.1f + 0.04f * unit(rng)};
    const float px = x + r * std::cos(t), pz = z + r * std::sin(t);
    const glm::vec3 out{std::cos(t), 0.f, std::sin(t)};
    glm::vec3 ax[3];
    ax[2] = out;
    ax[1] = glm::normalize(up + 0.25f * glm::vec3{unit(rng) - 0.5f, 0.f, unit(rng) - 0.5f});
    ax[0] = glm::normalize(glm::cross(ax[1], ax[2]));
    ax[2] = glm::cross(ax[0], ax[1]);
    addBox(verts, indices, pxe::kSurfStone, {px, heightAt(px, pz) + half.y - 0.03f, pz}, half, ax);
  }

  // Logs leant into a teepee over the middle.
  const int logs = 4;
  for (int i = 0; i < logs; ++i) {
    const float t = 2.f * kPi * static_cast<float>(i) / logs + 0.4f + 0.3f * (unit(rng) - 0.5f);
    const glm::vec3 out{std::cos(t), 0.f, std::sin(t)};
    const glm::vec3 foot = centre + out * (0.42f + 0.05f * unit(rng)) + glm::vec3{0.f, 0.05f, 0.f};
    const glm::vec3 top = centre - out * 0.04f + glm::vec3{0.f, 0.48f + 0.06f * unit(rng), 0.f};
    addLog(verts, indices, foot, top, 0.055f + 0.01f * unit(rng), unit(rng));
  }
  return centre + glm::vec3{0.f, 0.08f, 0.f};
}

// The tops of the stonework, for standing candles on: per 0.5 m cell of the
// ground plane, the stone triangles over it, so the highest stone face over
// a point is quick to find.
class StoneTops {
 public:
  explicit StoneTops(std::vector<glm::vec3> corners) : corners_(std::move(corners)) {
    for (const glm::vec3& p : corners_) {
      min_ = glm::min(min_, glm::vec2(p.x, p.z));
      max_ = glm::max(max_, glm::vec2(p.x, p.z));
    }
    w_ = std::max(1, static_cast<int>(std::ceil((max_.x - min_.x) / kCell)) + 1);
    h_ = std::max(1, static_cast<int>(std::ceil((max_.y - min_.y) / kCell)) + 1);
    cells_.resize(static_cast<std::size_t>(w_ * h_));
    for (std::size_t i = 0; i + 2 < corners_.size(); i += 3) {
      glm::vec2 lo{1e9f}, hi{-1e9f};
      for (std::size_t k = 0; k < 3; ++k) {
        lo = glm::min(lo, glm::vec2(corners_[i + k].x, corners_[i + k].z));
        hi = glm::max(hi, glm::vec2(corners_[i + k].x, corners_[i + k].z));
      }
      for (int z = cellZ(lo.y); z <= cellZ(hi.y); ++z)
        for (int x = cellX(lo.x); x <= cellX(hi.x); ++x) cells_[static_cast<std::size_t>(z * w_ + x)].push_back(i);
    }
  }

  // The highest stone face over (x, z): its height and normal.
  std::optional<std::pair<float, glm::vec3>> at(float x, float z) const {
    if (x < min_.x || z < min_.y || x > max_.x || z > max_.y) return std::nullopt;
    std::optional<std::pair<float, glm::vec3>> best;
    for (std::size_t i : cells_[static_cast<std::size_t>(cellZ(z) * w_ + cellX(x))]) {
      const glm::vec3 &a = corners_[i], &b = corners_[i + 1], &c = corners_[i + 2];
      const float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
      if (std::fabs(d) < 1e-9f) continue;  // a wall seen edge-on from above
      const float u = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / d;
      const float v = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / d;
      if (u < 0.f || v < 0.f || u + v > 1.f) continue;
      const float y = a.y * u + b.y * v + c.y * (1.f - u - v);
      if (best && best->first >= y) continue;
      glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
      if (n.y < 0.f) n = -n;
      best = {y, n};
    }
    return best;
  }

 private:
  static constexpr float kCell = 0.5f;
  int cellX(float x) const { return std::clamp(static_cast<int>((x - min_.x) / kCell), 0, w_ - 1); }
  int cellZ(float z) const { return std::clamp(static_cast<int>((z - min_.y) / kCell), 0, h_ - 1); }
  std::vector<glm::vec3> corners_;
  std::vector<std::vector<std::size_t>> cells_;
  glm::vec2 min_{1e9f}, max_{-1e9f};
  int w_ = 1, h_ = 1;
};

// A candle standing at `foot`: a stub of pale wax. Returns where its flame
// rises from.
constexpr float kCandleSize = 0.2f;  // its flame against a campfire's
glm::vec3 addCandle(VertexLists verts, IndexLists indices, glm::vec3 foot) {
  const glm::vec3 ax[3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
  const glm::vec3 half{0.04f, 0.07f, 0.04f};
  addBox(verts, indices, pxe::kSurfFoam, foot + glm::vec3{0.f, half.y - 0.01f, 0.f}, half, ax);
  return foot + glm::vec3{0.f, 2.f * half.y, 0.f};
}

// For the shelter test: a little stone shelter over (x, z) - four posts and
// a two-tier slab roof - to build a campfire under.
void addShelter(VertexLists verts, IndexLists indices, const pxe::TriangleGrid& ground, float x, float z) {
  auto heightAt = [&](float px, float pz) {
    const auto hit = ground.at(px, pz);
    return hit ? hit->pos.y : 0.f;
  };
  const glm::vec3 ax[3] = {{1.f, 0.f, 0.f}, {0.f, 1.f, 0.f}, {0.f, 0.f, 1.f}};
  const float floor = heightAt(x, z);
  const float eaves = floor + 1.75f;
  for (float sx : {-1.f, 1.f})
    for (float sz : {-1.f, 1.f}) {
      const float px = x + 0.95f * sx, pz = z + 0.95f * sz;
      const float foot = heightAt(px, pz) - 0.05f;
      addBox(verts, indices, pxe::kSurfStone, {px, 0.5f * (foot + eaves), pz}, {0.13f, 0.5f * (eaves - foot), 0.13f}, ax);
    }
  addBox(verts, indices, pxe::kSurfStone, {x, eaves + 0.1f, z}, {1.3f, 0.1f, 1.3f}, ax);
  addBox(verts, indices, pxe::kSurfStone, {x, eaves + 0.28f, z}, {0.95f, 0.08f, 0.95f}, ax);
}

// Ruins: the pond-and-pillars scene from tools/blender/build_ruins.py.

// Its camera, sun, ridge and colours.
void setUpRuins(BuiltScene& built) {
  pxe::Scene& scene = built.scene;
  // The engine's default palettes throughout.
  scene.tLit = 0.6f;
  scene.tMid = 0.18f;

  // The Blender layout is built facing Blender +Y, which is glTF -Z.
  built.yawDegrees = 180.f;
  const glm::vec3 forward{0.f, 0.f, -1.f};
  const glm::vec3 right{1.f, 0.f, 0.f};
  // Rises on the right of the screen and sets on the left, arcing over the
  // pond, so by mid-afternoon the pillars' left faces are lit as in the video.
  scene.sunEast = right;
  scene.sunSouth = forward;
  built.focus = glm::vec3{2.f, 1.f, -6.f};
  // Wooded hills just past the edge of the frame. Pillar tops (about 10 m)
  // catch the first sun; the lawn waits until the sun is ~15 degrees up.
  scene.horizonCentre = glm::vec2{2.f, -6.f};
  scene.horizonRadius = 46.f;
  scene.horizonHeight = 12.f;
  scene.horizonRelief = 9.f;
}

// The ruins before anything grows on them: ruins.glb, with the campfires
// (and any test shelter and candles) added, one vertex list per surface.
// Laying them out also gives the scene its fires, shelter, ground and
// water top.
struct RuinsGeometry {
  pxe::SurfaceGeometry surfaces;
  std::optional<pxe::TriangleGrid> lawn;  // under the grass
  std::vector<glm::vec3> lawnCorners;
  std::vector<pxe::Triangle> leaves;
  std::size_t prims = 0;
  std::size_t candles = 0;
  int candlesOnStone = 0, candlesOnLawn = 0;
  std::vector<glm::vec2> campfires;
  std::vector<glm::vec3> solids;  // see SceneLayout
};

void layOutRuins(const BuildOptions& options, BuiltScene& built, RuinsGeometry& geo) {
  pxe::Scene& scene = built.scene;
  const auto prims = pxe::loadGltf(std::string(PXE_ASSET_DIR) + "/scenes/ruins.glb");
  geo.prims = prims.size();

  // One mesh per surface keeps draw calls low.
  auto& verts = geo.surfaces.vertices;
  auto& indices = geo.surfaces.indices;
  std::vector<pxe::Triangle> lawn;
  std::vector<pxe::Triangle>& leaves = geo.leaves;
  for (const auto& prim : prims) {
    geo.surfaces.append(prim);
    if (prim.node != "Ground")
      for (std::uint32_t i : prim.indices) {
        const pxe::Vertex& pv = prim.vertices[i];
        geo.solids.push_back({pv.px, pv.py, pv.pz});
      }

    if (prim.surface != pxe::kSurfGrass && prim.surface != pxe::kSurfLeaf) continue;
    for (const pxe::Triangle& t : pxe::trianglesOf(prim.vertices, prim.indices)) {
      if (prim.surface == pxe::kSurfGrass) {
        lawn.push_back(t);
        if (prim.node == "Ground") geo.lawnCorners.insert(geo.lawnCorners.end(), {t.p[0], t.p[1], t.p[2]});
      } else {
        leaves.push_back(t);
      }
    }
  }
  // Everything added from here on stands in the scene.
  std::size_t added[pxe::kSurfCount];
  for (int s = 0; s < pxe::kSurfCount; ++s) added[s] = indices[s].size();
  // The campfire sits on open lawn left of the pond.
  geo.lawn.emplace(std::move(lawn), 1.f);
  const pxe::TriangleGrid& ground = *geo.lawn;
  std::mt19937 fireRng(11);
  const glm::vec2 campfire = kRuinsCampfire;
  built.fires.push_back({addCampfire(verts, indices, ground, campfire.x, campfire.y, fireRng)});
  geo.campfires.push_back(campfire);
  if (options.shelterTest) {
    // A second fire under a stone shelter, and a third under a tree crown
    // (the nearest bit of open lawn to the middle with a full crown over it
    // and no trunk close by).
    const glm::vec2 roofed{kShelterTestX, kShelterTestZ};
    addShelter(verts, indices, ground, roofed.x, roofed.y);
    built.fires.push_back({addCampfire(verts, indices, ground, roofed.x, roofed.y, fireRng)});
    geo.campfires.push_back(roofed);
    std::vector<glm::vec3> crownCorners;
    for (const pxe::Triangle& t : leaves) crownCorners.insert(crownCorners.end(), {t.p[0], t.p[1], t.p[2]});
    pxe::ShelterMap crowns;
    crowns.build(crownCorners);
    const std::vector<glm::vec3> bark = geo.surfaces.corners(pxe::kSurfBark);
    std::optional<glm::vec3> under;
    float best = 1e9f;
    for (float z = built.focus.z - 12.f; z <= built.focus.z + 10.f; z += 0.5f)
      for (float x = built.focus.x - 14.f; x <= built.focus.x + 14.f; x += 0.5f) {
        const auto hit = ground.at(x, z);
        if (!hit) continue;
        const float d = glm::length(glm::vec2{x, z} - glm::vec2{built.focus.x, built.focus.z});
        if (d >= best) continue;
        bool clear = true;
        for (const FireSite& f : built.fires) clear = clear && glm::length(glm::vec2{x - f.base.x, z - f.base.z}) > 3.f;
        for (const glm::vec3& p : bark)
          clear = clear && (p.y > hit->pos.y + 2.5f || glm::length(glm::vec2{p.x - x, p.z - z}) > 1.2f);
        if (clear && crowns.coveredLines(hit->pos) == 9) {
          best = d;
          under = hit->pos;
        }
      }
    if (under) {
      built.fires.push_back({addCampfire(verts, indices, ground, under->x, under->z, fireRng)});
      geo.campfires.push_back({under->x, under->z});
    }
  }
  if (options.candles) {
    // Candles on flat stone tops - a ledge, a plinth, a fallen block, from
    // knee height up - and a few on the open lawn, spread apart and clear of
    // the campfires; and two under the shelter test's roof.
    const StoneTops tops(geo.surfaces.corners(pxe::kSurfStone));
    std::vector<glm::vec3> feet;
    auto clear = [&](float x, float z, float apart) {
      for (const FireSite& f : built.fires)
        if (glm::length(glm::vec2{x - f.base.x, z - f.base.z}) < (f.size < 1.f ? apart : 1.6f)) return false;
      for (const glm::vec3& f : feet)
        if (glm::length(glm::vec2{x - f.x, z - f.z}) < apart) return false;
      return true;
    };
    std::vector<glm::vec2> spots;
    for (float z = built.focus.z - 8.f; z <= built.focus.z + 7.f; z += 0.25f)
      for (float x = built.focus.x - 12.f; x <= built.focus.x + 12.f; x += 0.25f) spots.push_back({x, z});
    std::mt19937 candleRng(41);
    std::shuffle(spots.begin(), spots.end(), candleRng);
    int& onStone = geo.candlesOnStone;
    int& onLawn = geo.candlesOnLawn;
    for (const glm::vec2& at : spots) {
      if (onStone >= 7 && onLawn >= 3) break;
      const auto lawnHit = ground.at(at.x, at.y);
      const auto top = tops.at(at.x, at.y);
      const float lawnY = lawnHit ? lawnHit->pos.y : -1e9f;
      if (top && top->first > lawnY + 0.3f && top->first < lawnY + 3.f && top->second.y > 0.97f) {
        if (onStone >= 7) continue;
        // Room for the candle: the same face all round it.
        bool flat = true;
        for (glm::vec2 o : {glm::vec2{0.1f, 0.f}, glm::vec2{-0.1f, 0.f}, glm::vec2{0.f, 0.1f}, glm::vec2{0.f, -0.1f}}) {
          const auto t = tops.at(at.x + o.x, at.y + o.y);
          flat = flat && t && std::fabs(t->first - top->first) < 0.02f;
        }
        if (!flat || !clear(at.x, at.y, 1.6f)) continue;
        feet.push_back({at.x, top->first, at.y});
        ++onStone;
      } else if (lawnHit && (!top || top->first < lawnY + 0.03f) && lawnHit->normal.y > 0.9f) {
        if (onLawn >= 3 || !clear(at.x, at.y, 2.5f)) continue;
        feet.push_back(lawnHit->pos);
        ++onLawn;
      }
    }
    if (options.shelterTest)
      for (float sx : {-0.7f, 0.7f}) {
        const auto hit = ground.at(kShelterTestX + sx, kShelterTestZ + 0.7f);
        if (hit) feet.push_back(hit->pos);
      }
    for (const glm::vec3& foot : feet) built.fires.push_back({addCandle(verts, indices, foot), false, kCandleSize});
    geo.candles = feet.size();
  }
  for (int s = 0; s < pxe::kSurfCount; ++s)
    for (std::size_t i = added[s]; i < indices[s].size(); ++i) {
      const pxe::Vertex& v = verts[s][indices[s][i]];
      geo.solids.push_back({v.px, v.py, v.pz});
    }
  pxe::buildSceneMaps(scene, geo.surfaces);
  for (FireSite& f : built.fires) f.sheltered = scene.shelter.sheltered(f.base);
}

BuiltScene buildRuins(const BuildOptions& options) {
  BuiltScene built;
  pxe::Scene& scene = built.scene;
  setUpRuins(built);
  RuinsGeometry geo;
  layOutRuins(options, built, geo);
  const pxe::TriangleGrid& ground = *geo.lawn;
  if (options.candles)
    std::fprintf(stderr, "pxe: %zu candles, %d on stone, %d on the lawn\n", geo.candles, geo.candlesOnStone,
                 geo.candlesOnLawn);
  for (const FireSite& f : built.fires)
    std::fprintf(stderr, "pxe: %s at %.1f, %.1f %s\n", f.size < 1.f ? "candle" : "fire", f.base.x, f.base.z,
                 f.sheltered ? "sheltered" : "open to the rain");

  pxe::uploadScene(scene, geo.surfaces);

  // One generator feeds the grass and then the leaves, so the lawn and the
  // crowns come out the same every time.
  std::mt19937 rng(7);
  pxe::scatterGrass(scene, ground, grassScatter(options), rng);
  // The lawn runs on under the paving, plinths and trunks, so drop grass
  // rooted beneath them: from inside the stone it showed through its faces.
  const std::size_t underSolids = pxe::dropGrassUnder(scene, geo.surfaces, {pxe::kSurfStone, pxe::kSurfBark});
  std::fprintf(stderr, "pxe: %zu grass sprites under stone or trunks dropped\n", underSolids);
  // Firefly homes: scattered over the open lawn round the ruins, from knee
  // to head height.
  {
    std::mt19937 flyRng(23);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    for (int tries = 0; tries < 400 && scene.fireflyHomes.size() < 40; ++tries) {
      const float x = built.focus.x - 14.f + 28.f * u(flyRng);
      const float z = built.focus.z - 12.f + 22.f * u(flyRng);
      const auto hit = ground.at(x, z);
      if (!hit) continue;
      scene.fireflyHomes.push_back(hit->pos + glm::vec3{0.f, 0.5f + 1.2f * u(flyRng), 0.f});
    }
  }

  // No grass in the fire pits.
  std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) {
    for (const FireSite& f : built.fires)
      if (glm::length(glm::vec2{g.px, g.pz} - glm::vec2{f.base.x, f.base.z}) < (f.size < 1.f ? 0.3f : 1.05f)) return true;
    return false;
  });
  // Nor under the sleeping fox, when it is shown: blades rooted under it
  // drew over its curl and broke up its outline. Dropped after scattering,
  // so the rest of the lawn is exactly as without it.
  if (!options.beds.empty()) {
    const std::size_t before = scene.grass.size();
    std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) {
      for (const BedFootprint& b : options.beds)
        if (glm::length(glm::vec2{g.px, g.pz} - b.xz) < b.radius) return true;
      return false;
    });
    std::fprintf(stderr, "pxe: %zu grass sprites under %zu fox%s dropped\n", before - scene.grass.size(),
                 options.beds.size(), options.beds.size() == 1 ? "" : "es");
  }

  // Leaves grow in clumps over every crown, as in the video.
  const int leafCount = pxe::scatterLeaves(scene, geo.leaves, options.leafDensity, rng);
  // Bare flames for testing many fires at once, as spells or spreading fire
  // would make: scattered over the open lawn, with no pit or logs.
  if (options.extraFires > 0) {
    std::mt19937 extraRng(31);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    const std::size_t want = built.fires.size() + static_cast<std::size_t>(options.extraFires);
    for (int tries = 0; tries < 2000 && built.fires.size() < want; ++tries) {
      const float x = built.focus.x - 12.f + 24.f * u(extraRng);
      const float z = built.focus.z - 9.f + 16.f * u(extraRng);
      const auto hit = ground.at(x, z);
      if (!hit) continue;
      bool apart = true;
      for (const FireSite& f : built.fires) apart = apart && glm::length(glm::vec2{x - f.base.x, z - f.base.z}) > 1.5f;
      if (!apart) continue;
      const glm::vec3 base = hit->pos + glm::vec3{0.f, 0.08f, 0.f};
      built.fires.push_back({base, scene.shelter.sheltered(base)});
    }
    std::fprintf(stderr, "pxe: %d extra fires\n", options.extraFires);
  }
  std::fprintf(stderr, "pxe: ruins %zu primitives, %d leaf clusters\n", geo.prims, leafCount);
  return built;
}

}  // namespace

BuiltScene buildScene(const BuildOptions& options) {
  BuiltScene built = (options.scene == "hills") ? buildHills(options) : buildRuins(options);
  std::fprintf(stderr, "pxe: scene %s, %zu sprites\n", options.scene.c_str(), built.scene.grass.size());
  return built;
}

SceneLayout laidOut(const BuildOptions& options) {
  SceneLayout layout;
  if (options.scene == "hills") return layout;
  BuiltScene built;
  setUpRuins(built);
  RuinsGeometry geo;
  layOutRuins(options, built, geo);
  layout.campfires = std::move(geo.campfires);
  layout.solids = std::move(geo.solids);
  layout.ground = std::move(built.scene.ground);
  layout.lawn.build(geo.lawnCorners);
  layout.bedsLo = glm::vec2{built.focus.x, built.focus.z} - 20.f;
  layout.bedsHi = glm::vec2{built.focus.x, built.focus.z} + 20.f;
  return layout;
}

}  // namespace game
