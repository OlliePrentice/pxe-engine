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
// `meanderLength` metres along it, and the ridge's height swells between 50%
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

// The dunes, after desert/docs/concept/wanderers.png: broad ridges with the
// wind toward the spawn's back (about -z), two smaller layers turned a
// little either way, and long low hills under them that layer the far
// silhouettes into the haze. Crest to trough comes to about 4-12 m. The
// phases (found with docs/tasks/desert-ground/scripts/tune_phases.py) put
// the palm on a gentle slope that rises away from the spawn, with a crest
// about 22 m behind it, and clear its trunk from the spawn's eye:
// --terrain-report (main.cpp) checks that the slope at the palm is at most
// 15 degrees and rises away from the spawn, that the ground rises at least
// 2 m above the palm's base somewhere 5-60 m behind it, and that the sight
// line from the eye to 0.5 m up the trunk clears the ground by at least
// 0.1 m.
constexpr Ridges kRidges[] = {
    {200.f, 110.f, 6.5f, 0.75f, 0.10f, 150.f, 190.f, 0.875f, 0.f},
    {214.f, 48.f, 2.4f, 0.75f, 0.12f, 90.f, 110.f, 0.30f, 1.f},
    {186.f, 26.f, 0.8f, 0.75f, 0.15f, 60.f, 70.f, 0.55f, 2.f},
    {196.f, 260.f, 5.0f, 0.75f, 0.08f, 400.f, 500.f, 0.25f, 3.f},
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

// The dunes' smooth normal at world xz, from duneHeight's exact slope, so the
// light falls off continuously across each slope's curve, and a vertex the
// fine and coarse grids share has one normal and the two shade alike where
// they meet.
glm::vec3 duneNormal(float x, float z) {
  const glm::vec2 g = duneGradient(x, z);
  return glm::normalize(glm::vec3{-g.x, 1.f, -g.y});
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

  // The fine grid, 0.5 m cells over the middle, each cell two triangles split
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
  const int ratio = static_cast<int>(kCoarseCell / kFineCell);  // 8
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
    const float swell = 0.75f + 0.25f * std::sin(kTau * along / r.swellLength + 2.1f * r.seed + 1.f);
    h += r.height * swell * ridgeProfile(u, r.windward);
  }
  return h;
}

glm::vec2 duneGradient(float x, float z) {
  glm::vec2 g{0.f};
  for (const Ridges& r : kRidges) {
    const glm::vec2 wind{std::sin(r.heading * kDeg), std::cos(r.heading * kDeg)};
    const glm::vec2 crest{wind.y, -wind.x};
    const float across = x * wind.x + z * wind.y;
    const float along = x * crest.x + z * crest.y;
    const float meanderArg = kTau * along / r.meanderLength + 1.3f * r.seed + 0.4f;
    float u = across / r.wavelength + r.meander * std::sin(meanderArg) + r.phase;
    u -= std::floor(u);
    const float duda = 1.f / r.wavelength;
    const float duds = r.meander * (kTau / r.meanderLength) * std::cos(meanderArg);
    const float swellArg = kTau * along / r.swellLength + 2.1f * r.seed + 1.f;
    const float swell = 0.75f + 0.25f * std::sin(swellArg);
    const float dswells = 0.25f * (kTau / r.swellLength) * std::cos(swellArg);
    // The profile and its slope in u: ridgeProfile's two pieces.
    float p, dp;
    if (u < r.windward) {
      const float t = u / r.windward;
      p = t * t * (1.5f - 0.5f * t);
      dp = (3.f * t - 1.5f * t * t) / r.windward;
    } else {
      const float v = 1.f - (u - r.windward) / (1.f - r.windward);
      p = v * v;
      dp = -2.f * v / (1.f - r.windward);
    }
    const float dhda = r.height * swell * dp * duda;
    const float dhds = r.height * (dswells * p + swell * dp * duds);
    g.x += dhda * wind.x + dhds * crest.x;
    g.y += dhda * wind.y + dhds * crest.y;
  }
  return g;
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
  // perspective light; the desert keeps them, apart from a warm sand
  // bounce (a display colour and a linear strength per phase) and the
  // ground and sky values below.
  pxe::Scene& scene = world.scene;
  const auto rgb = [](float r, float g, float b) { return glm::vec3{r, g, b} / 255.f; };
  scene.groundBounce[pxe::kPhaseDay] = rgb(214, 176, 118);
  scene.groundBounce[pxe::kPhaseGolden] = rgb(220, 160, 110);
  scene.groundBounce[pxe::kPhaseTwilight] = rgb(150, 120, 130);
  scene.groundBounceStrength[pxe::kPhaseTwilight] = 0.05f;
  scene.groundBounce[pxe::kPhaseNight] = rgb(60, 65, 85);
  scene.groundBounceStrength[pxe::kPhaseNight] = 0.03f;
  // The ground and sky after desert/docs/concept/wanderers.png: beige-ochre
  // sand, lit faces warm against a pale dusty sky, and shadowed faces that
  // stay warm and sandy (a warm sky fill, and the sand's own bounce), not
  // mauve. Night keeps the engine's cool fill. The sand's terminator is
  // sharpened so light turns to shadow in a clear line along the crests, and
  // wind ripples cross it near the eye, aligned with the main dunes' wind.
  scene.albedo[pxe::kSurfSand] = rgb(212, 180, 136);
  scene.skyFill[pxe::kPhaseDay] = rgb(170, 180, 200);
  scene.skyFillStrength[pxe::kPhaseDay] = 0.30f;
  scene.skyFill[pxe::kPhaseGolden] = rgb(200, 175, 160);
  scene.skyFillStrength[pxe::kPhaseGolden] = 0.30f;
  scene.skyFill[pxe::kPhaseTwilight] = rgb(150, 125, 140);
  scene.skyFillStrength[pxe::kPhaseTwilight] = 0.22f;
  scene.groundBounceStrength[pxe::kPhaseDay] = 0.20f;
  scene.groundBounceStrength[pxe::kPhaseGolden] = 0.18f;
  scene.skyHorizon[pxe::kPhaseGolden] = rgb(236, 212, 176);
  scene.skyZenith[pxe::kPhaseGolden] = rgb(150, 160, 175);
  scene.skyHorizon[pxe::kPhaseDay] = rgb(226, 214, 190);
  scene.skyZenith[pxe::kPhaseDay] = rgb(150, 170, 190);
  scene.terminator[pxe::kSurfSand] = 0.35f;
  scene.ripples = {pxe::kSurfSand, 0.25f, 0.35f, 200.f, 6.f, 20.f, 22.f};
  // the desert's sky: two streaky cloud layers, high and low, and a little airborne dust
  scene.skyClouds[0].cover = 0.40f;
  scene.skyClouds[0].opacity = 0.55f;
  scene.skyClouds[1].cover = 0.25f;
  scene.dustDensity = 0.4f;
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
