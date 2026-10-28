#include "world.hpp"

#include "pxe/gltf.hpp"
#include "pxe/mesh.hpp"
#include "pxe/scene_geometry.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace desert {
namespace {

constexpr float kTau = 6.28318530717958647692f;
constexpr float kDeg = kTau / 360.f;

// One layer of ridges. The wind blows toward `heading` (compass-like, in
// degrees: 0 toward +z, 90 toward +x), and the ridges run across it,
// `wavelength` metres apart. Each rises over the `windward` share of a
// wavelength to its crest, then drops down a steep downwind face to the next
// trough. The crest line wanders `meander` wavelengths either way over
// `meanderLength` metres along it, and the ridge's height swells between 70%
// and 100% of `height` over `swellLength` metres. `phase` slides the layer
// along the wind (in wavelengths), and `seed` offsets its wander and swell.
struct Ridges {
  float heading;
  float wavelength;
  float height;
  float windward;
  float meander;
  float meanderLength;
  float swellLength;
  float phase;
  float seed;
};

// The dunes: broad ridges with the wind toward the spawn's back (about
// -z), and two smaller layers turned a little either way. The phases put
// the palm on a windward slope of about 10 degrees with no crest between it
// and the spawn's eye.
constexpr Ridges kRidges[] = {
    {200.f, 72.f, 3.6f, 0.72f, 0.10f, 150.f, 190.f, 0.9f, 0.f},
    {214.f, 38.f, 1.5f, 0.70f, 0.12f, 90.f, 110.f, 0.8f, 1.f},
    {186.f, 23.f, 0.6f, 0.68f, 0.15f, 60.f, 70.f, 0.7f, 2.f},
};

// A ridge's profile over one wavelength, u in 0..1 from trough to trough:
// a windward slope leaving the trough level and steepening to the crest,
// then a downwind face starting steep from the crest and flattening into
// the next trough. 0 in the troughs, 1 on the crest.
float ridgeProfile(float u, float windward) {
  if (u < windward) {
    const float t = u / windward;
    return t * t * (1.5f - 0.5f * t);
  }
  const float v = 1.f - (u - windward) / (1.f - windward);
  return v * v;
}

void appendGround(pxe::SurfaceGeometry& geometry) {
  const int n = static_cast<int>(2.f * kGroundHalf / kGroundCell);
  // duneHeight at every corner of the grid, row by row along +x.
  std::vector<float> corner(static_cast<std::size_t>((n + 1) * (n + 1)));
  for (int j = 0; j <= n; ++j)
    for (int i = 0; i <= n; ++i)
      corner[static_cast<std::size_t>(j * (n + 1) + i)] =
          duneHeight(-kGroundHalf + static_cast<float>(i) * kGroundCell, -kGroundHalf + static_cast<float>(j) * kGroundCell);
  auto at = [&](int i, int j) {
    return glm::vec3{-kGroundHalf + static_cast<float>(i) * kGroundCell, corner[static_cast<std::size_t>(j * (n + 1) + i)],
                     -kGroundHalf + static_cast<float>(j) * kGroundCell};
  };

  // Each cell is two triangles split corner (i, j) to (i+1, j+1), as
  // groundHeight reads them, each with its own face normal, so the toon
  // ramp bands the slopes facet by facet.
  std::vector<pxe::Vertex> v;
  std::vector<std::uint32_t> idx;
  v.reserve(static_cast<std::size_t>(n * n * 6));
  idx.reserve(static_cast<std::size_t>(n * n * 6));
  auto triangle = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c) {
    // Wound counter-clockwise seen from above: meshes are drawn with back
    // faces culled.
    const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
    const auto base = static_cast<std::uint32_t>(v.size());
    for (const glm::vec3& p : {a, b, c})
      v.push_back({p.x, p.y, p.z, normal.x, normal.y, normal.z, static_cast<float>(pxe::kSurfSand)});
    idx.insert(idx.end(), {base, base + 1, base + 2});
  };
  for (int j = 0; j < n; ++j)
    for (int i = 0; i < n; ++i) {
      const glm::vec3 p00 = at(i, j), p10 = at(i + 1, j), p01 = at(i, j + 1), p11 = at(i + 1, j + 1);
      triangle(p00, p11, p10);
      triangle(p00, p01, p11);
    }
  geometry.append(pxe::kSurfSand, v, idx);
}

}  // namespace

float duneHeight(float x, float z) {
  float h = 0.f;
  for (const Ridges& r : kRidges) {
    const glm::vec2 wind{std::sin(r.heading * kDeg), std::cos(r.heading * kDeg)};
    const glm::vec2 crest{wind.y, -wind.x};  // along the ridge
    const float across = x * wind.x + z * wind.y;
    const float along = x * crest.x + z * crest.y;
    float u = across / r.wavelength + r.meander * std::sin(kTau * along / r.meanderLength + 1.3f * r.seed + 0.4f) +
              r.phase;
    u -= std::floor(u);
    const float swell = 0.7f + 0.3f * std::sin(kTau * along / r.swellLength + 2.1f * r.seed + 1.f);
    h += r.height * swell * ridgeProfile(u, r.windward);
  }
  return h;
}

float groundHeight(float x, float z) {
  const float gx = (x + kGroundHalf) / kGroundCell;
  const float gz = (z + kGroundHalf) / kGroundCell;
  const float i = std::floor(gx), j = std::floor(gz);
  const float fx = gx - i, fz = gz - j;
  auto corner = [&](float di, float dj) {
    return duneHeight(-kGroundHalf + (i + di) * kGroundCell, -kGroundHalf + (j + dj) * kGroundCell);
  };
  const float h00 = corner(0.f, 0.f), h11 = corner(1.f, 1.f);
  // The cell's two triangles, either side of its (i, j)-(i+1, j+1) diagonal.
  if (fx >= fz) {
    const float h10 = corner(1.f, 0.f);
    return h00 + fx * (h10 - h00) + fz * (h11 - h10);
  }
  const float h01 = corner(0.f, 1.f);
  return h00 + fz * (h01 - h00) + fx * (h11 - h01);
}

World buildWorld() {
  World world;
  // A new scene starts with the engine's default palettes, Sand's among
  // them, and its sky colours; the desert keeps them all.
  pxe::Scene& scene = world.scene;
  // The sun rises on the right of the spawn's view (it looks along +z),
  // peaks behind it toward -z, and sets on the left, so through the
  // afternoon the palm's shadow falls across the sand to the view's right.
  scene.sunEast = {-1.f, 0.f, 0.f};
  scene.sunSouth = {0.f, 0.f, -1.f};

  pxe::SurfaceGeometry geometry;
  appendGround(geometry);

  // The palm: Bark and Leaf, at real scale, its origin set on the ground.
  const float palmBase = groundHeight(kPalmSpot.x, kPalmSpot.y);
  for (const pxe::GltfPrimitive& prim : pxe::loadGltf(std::string(PXE_ASSET_DIR) + "/scenes/date_palm.glb")) {
    std::vector<pxe::Vertex> placed = prim.vertices;
    for (pxe::Vertex& vert : placed) {
      vert.px += kPalmSpot.x;
      vert.py += palmBase;
      vert.pz += kPalmSpot.y;
    }
    geometry.append(prim.surface, placed, prim.indices);
  }

  pxe::buildSceneMaps(scene, geometry);
  pxe::uploadScene(scene, geometry);
  // Nothing is scattered: no grass on the sand, no leaf sprites on the palm.
  return world;
}

}  // namespace desert
