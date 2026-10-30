#include "world.hpp"

#include "pxe/gltf.hpp"
#include "pxe/mesh.hpp"
#include "pxe/scene_geometry.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
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

// How far either side of a point the dunes' slope is read for its normal,
// metres. The same step everywhere, so a vertex the fine and coarse grids
// share has one normal and the two shade alike where they meet.
constexpr float kNormalStep = 0.5f;

// The dunes' smooth normal at world xz, from duneHeight's slope across the
// point, so the toon ramp bands each slope in contours that follow its
// curve rather than facet by facet.
glm::vec3 duneNormal(float x, float z) {
  const float e = kNormalStep;
  return glm::normalize(glm::vec3{duneHeight(x - e, z) - duneHeight(x + e, z), 2.f * e,
                                  duneHeight(x, z - e) - duneHeight(x, z + e)});
}

void appendGround(pxe::SurfaceGeometry& geometry) {
  std::vector<pxe::Vertex> v;
  std::vector<std::uint32_t> idx;
  auto vertex = [&](float x, float z) {
    const glm::vec3 n = duneNormal(x, z);
    v.push_back({x, duneHeight(x, z), z, n.x, n.y, n.z, static_cast<float>(pxe::kSurfSand)});
    return static_cast<std::uint32_t>(v.size() - 1);
  };
  // Wound counter-clockwise seen from above, whatever order the corners
  // come in: meshes are drawn with back faces culled.
  auto triangle = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    const pxe::Vertex &pa = v[a], &pb = v[b], &pc = v[c];
    if ((pb.pz - pa.pz) * (pc.px - pa.px) - (pb.px - pa.px) * (pc.pz - pa.pz) < 0.f) std::swap(b, c);
    idx.insert(idx.end(), {a, b, c});
  };

  // The fine grid, 1 m cells over the middle, each cell two triangles split
  // corner (i, j) to (i+1, j+1), as groundHeight reads them.
  const int nf = static_cast<int>(2.f * kFineHalf / kFineCell);
  std::vector<std::uint32_t> fine(static_cast<std::size_t>((nf + 1) * (nf + 1)));
  auto f = [&](int i, int j) { return fine[static_cast<std::size_t>(j * (nf + 1) + i)]; };
  for (int j = 0; j <= nf; ++j)
    for (int i = 0; i <= nf; ++i)
      fine[static_cast<std::size_t>(j * (nf + 1) + i)] =
          vertex(-kFineHalf + static_cast<float>(i) * kFineCell, -kFineHalf + static_cast<float>(j) * kFineCell);
  for (int j = 0; j < nf; ++j)
    for (int i = 0; i < nf; ++i) {
      triangle(f(i, j), f(i + 1, j + 1), f(i + 1, j));
      triangle(f(i, j), f(i, j + 1), f(i + 1, j + 1));
    }

  // The coarse ring, 4 m cells out to the ground's edge. Its corners on the
  // fine grid's border are the fine grid's own vertices; [c0, c1] is the
  // fine grid's span in coarse corners, and each coarse side along it holds
  // `ratio` fine cells.
  const int nc = static_cast<int>(2.f * kGroundHalf / kCoarseCell);
  const int c0 = static_cast<int>((kGroundHalf - kFineHalf) / kCoarseCell);
  const int c1 = nc - c0;
  const int ratio = static_cast<int>(kCoarseCell / kFineCell);
  std::vector<std::uint32_t> coarse(static_cast<std::size_t>((nc + 1) * (nc + 1)));
  auto c = [&](int i, int j) { return coarse[static_cast<std::size_t>(j * (nc + 1) + i)]; };
  for (int j = 0; j <= nc; ++j)
    for (int i = 0; i <= nc; ++i) {
      std::uint32_t& slot = coarse[static_cast<std::size_t>(j * (nc + 1) + i)];
      if (i >= c0 && i <= c1 && j >= c0 && j <= c1) {
        // On the fine grid's border, its vertex; inside it, unused.
        if (i == c0 || i == c1 || j == c0 || j == c1) slot = f((i - c0) * ratio, (j - c0) * ratio);
        continue;
      }
      slot = vertex(-kGroundHalf + static_cast<float>(i) * kCoarseCell, -kGroundHalf + static_cast<float>(j) * kCoarseCell);
    }

  // A coarse cell beside the fine grid: its near side is the fine border's
  // vertices side[0..ratio], and its far corners a (beside side[0]) and b
  // (beside side[ratio]). Fanning the near side's first half to a, its
  // second half to b, and its middle to both, uses every fine vertex along
  // the side, so no edge of the fine grid ends part-way along a coarse one.
  std::vector<std::uint32_t> side(static_cast<std::size_t>(ratio + 1));
  auto stitch = [&](std::uint32_t a, std::uint32_t b) {
    const int mid = ratio / 2;
    for (int k = 0; k < mid; ++k) triangle(side[static_cast<std::size_t>(k)], side[static_cast<std::size_t>(k + 1)], a);
    triangle(side[static_cast<std::size_t>(mid)], b, a);
    for (int k = mid; k < ratio; ++k) triangle(side[static_cast<std::size_t>(k)], side[static_cast<std::size_t>(k + 1)], b);
  };
  for (int j = 0; j < nc; ++j)
    for (int i = 0; i < nc; ++i) {
      const bool alongX = i >= c0 && i < c1, alongZ = j >= c0 && j < c1;
      if (alongX && alongZ) continue;  // the fine grid's
      if (alongX && (j == c0 - 1 || j == c1)) {
        // Below or above the fine grid in z: its near side runs along x.
        const int row = j == c1 ? nf : 0, far = j == c1 ? j + 1 : j;
        for (int k = 0; k <= ratio; ++k) side[static_cast<std::size_t>(k)] = f((i - c0) * ratio + k, row);
        stitch(c(i, far), c(i + 1, far));
      } else if (alongZ && (i == c0 - 1 || i == c1)) {
        // Either side of the fine grid in x: its near side runs along z.
        const int column = i == c1 ? nf : 0, far = i == c1 ? i + 1 : i;
        for (int k = 0; k <= ratio; ++k) side[static_cast<std::size_t>(k)] = f(column, (j - c0) * ratio + k);
        stitch(c(far, j), c(far, j + 1));
      } else {
        triangle(c(i, j), c(i + 1, j + 1), c(i + 1, j));
        triangle(c(i, j), c(i, j + 1), c(i + 1, j + 1));
      }
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
  const float gx = (x + kFineHalf) / kFineCell;
  const float gz = (z + kFineHalf) / kFineCell;
  const float i = std::floor(gx), j = std::floor(gz);
  const float fx = gx - i, fz = gz - j;
  auto corner = [&](float di, float dj) {
    return duneHeight(-kFineHalf + (i + di) * kFineCell, -kFineHalf + (j + dj) * kFineCell);
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
  // A new scene starts with the engine's default palettes, sky colours and
  // perspective light; the desert keeps them all, apart from a warm sand
  // bounce (a display colour and a linear strength per phase).
  pxe::Scene& scene = world.scene;
  const auto rgb = [](float r, float g, float b) { return glm::vec3{r, g, b} / 255.f; };
  scene.groundBounce[pxe::kPhaseDay] = rgb(214, 176, 118);
  scene.groundBounceStrength[pxe::kPhaseDay] = 0.12f;
  scene.groundBounce[pxe::kPhaseGolden] = rgb(220, 160, 110);
  scene.groundBounceStrength[pxe::kPhaseGolden] = 0.10f;
  scene.groundBounce[pxe::kPhaseTwilight] = rgb(150, 120, 130);
  scene.groundBounceStrength[pxe::kPhaseTwilight] = 0.05f;
  scene.groundBounce[pxe::kPhaseNight] = rgb(60, 65, 85);
  scene.groundBounceStrength[pxe::kPhaseNight] = 0.03f;
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
