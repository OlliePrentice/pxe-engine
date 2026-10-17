#include "scene.hpp"

#include "pxe/gltf.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace game {
namespace {

constexpr float kPi = 3.1415926535f;
constexpr float kTau = 6.28318530718f;

// Flat palette. The lit colour is close to these values at midday; the
// shadow tint in the renderer supplies the second band.
const glm::vec3 kGrass{0.58f, 0.72f, 0.32f};
const glm::vec3 kGrassDeep{0.48f, 0.62f, 0.27f};
const glm::vec3 kGrassLight{0.64f, 0.76f, 0.36f};
const glm::vec3 kDirt{0.64f, 0.50f, 0.32f};
const glm::vec3 kBed{0.30f, 0.46f, 0.44f};

// Ground axes of the default 45° camera, so the ruin sits in frame on launch.
const glm::vec3 kFore{-0.70710678f, 0.f, 0.70710678f};  // toward the camera

const glm::vec2 kPond{5.6f, -3.9f};
constexpr float kPondR = 2.35f;
constexpr float kWaterY = 0.80f;

struct MeshBuild {
  std::vector<pxe::Vertex> verts;
  std::vector<std::uint32_t> idx;

  void tri(glm::vec3 a, glm::vec3 na, glm::vec3 b, glm::vec3 nb, glm::vec3 c, glm::vec3 nc,
           glm::vec3 color, float surface) {
    const auto base = static_cast<std::uint32_t>(verts.size());
    auto push = [&](glm::vec3 p, glm::vec3 n) {
      verts.push_back(pxe::Vertex{p.x, p.y, p.z, n.x, n.y, n.z, color.r, color.g, color.b, surface});
    };
    push(a, na);
    push(b, nb);
    push(c, nc);
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
  }

  void face(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float surface) {
    glm::vec3 n = glm::cross(b - a, c - a);
    const float len = glm::length(n);
    if (len < 1e-6f) return;
    n /= len;
    tri(a, n, b, n, c, n, color, surface);
  }

  void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color, float surface,
            glm::vec3 outward = glm::vec3{0.f}) {
    glm::vec3 n = glm::cross(b - a, d - a);
    if (glm::dot(n, outward) < 0.f && glm::length(outward) > 0.f) {
      face(a, d, c, color, surface);
      face(a, c, b, color, surface);
    } else {
      face(a, b, c, color, surface);
      face(a, c, d, color, surface);
    }
  }

};

float hash01(int x, int z, int salt) {
  std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u +
                    static_cast<std::uint32_t>(z) * 668265263u +
                    static_cast<std::uint32_t>(salt) * 1442695041u;
  n = (n ^ (n >> 13)) * 1274126177u;
  return static_cast<float>(n & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

float smoothFalloff(float d, float radius) {
  if (d >= radius) return 0.f;
  const float t = 1.f - d / radius;
  return t * t * (3.f - 2.f * t);
}

struct Height {
  static constexpr int N = 181;  // -90 .. 90
  static constexpr float kOrigin = -90.f;
  static constexpr float kStep = 1.f;
  std::vector<float> h;

  Height() : h(static_cast<std::size_t>(N * N), 0.f) {}

  float at(int x, int z) const {
    x = std::clamp(x, 0, N - 1);
    z = std::clamp(z, 0, N - 1);
    return h[static_cast<std::size_t>(z * N + x)];
  }
  void set(int x, int z, float v) {
    if (x < 0 || z < 0 || x >= N || z >= N) return;
    h[static_cast<std::size_t>(z * N + x)] = v;
  }
  void cell(float x, float z, int& ix, int& iz) const {
    ix = std::clamp(static_cast<int>(std::round((x - kOrigin) / kStep)), 0, N - 1);
    iz = std::clamp(static_cast<int>(std::round((z - kOrigin) / kStep)), 0, N - 1);
  }
  float sample(float x, float z) const {
    const float gx = (x - kOrigin) / kStep;
    const float gz = (z - kOrigin) / kStep;
    const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, N - 2);
    const int z0 = std::clamp(static_cast<int>(std::floor(gz)), 0, N - 2);
    const float tx = gx - static_cast<float>(x0);
    const float tz = gz - static_cast<float>(z0);
    const float a = at(x0, z0);
    const float b = at(x0 + 1, z0);
    const float c = at(x0, z0 + 1);
    const float d = at(x0 + 1, z0 + 1);
    return (a * (1.f - tx) + b * tx) * (1.f - tz) + (c * (1.f - tx) + d * tx) * tz;
  }
};

float hill(float x, float z, float cx, float cz, float radius, float amp) {
  const float dx = x - cx;
  const float dz = z - cz;
  return amp * smoothFalloff(std::sqrt(dx * dx + dz * dz), radius);
}

Height makeHeight() {
  Height hm;
  for (int z = 0; z < Height::N; ++z) {
    for (int x = 0; x < Height::N; ++x) {
      const float wx = Height::kOrigin + static_cast<float>(x);
      const float wz = Height::kOrigin + static_cast<float>(z);
      // Steep enough that a low sun lays a real terminator across the knoll.
      // The grass only reads where that edge, a cast shadow, or a cloud crosses it.
      float y = 0.55f;
      y += 0.62f * std::sin(wx * 0.045f) + 0.46f * std::cos(wz * 0.038f);
      y += 0.30f * std::sin((wx + wz) * 0.027f);
      y += hill(wx, wz, -1.4f, 2.2f, 9.2f, 2.35f);
      y += hill(wx, wz, 7.2f, -2.4f, 8.0f, 1.85f);
      y += hill(wx, wz, -7.5f, -5.5f, 7.5f, -0.95f);
      y += hill(wx, wz, -22.f, 16.f, 18.f, 2.15f);
      y += hill(wx, wz, 26.f, -20.f, 22.f, 2.35f);
      y += hill(wx, wz, -34.f, -28.f, 24.f, 1.55f);
      y += hill(wx, wz, 18.f, 24.f, 16.f, 1.35f);

      const float pdx = wx - kPond.x;
      const float pdz = wz - kPond.y;
      const float pd = std::sqrt(pdx * pdx + pdz * pdz);
      const float flat = smoothFalloff(pd, kPondR * 1.55f);
      y = glm::mix(y, 0.90f, flat * 0.95f);
      if (pd < kPondR) {
        const float u = pd / kPondR;
        y -= (1.f - u * u) * 0.72f;
      }
      hm.set(x, z, y);
    }
  }
  return hm;
}

float distToSegment(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
  const glm::vec2 ab = b - a;
  const float len2 = glm::dot(ab, ab);
  if (len2 < 1e-6f) return glm::length(p - a);
  float t = glm::dot(p - a, ab) / len2;
  t = std::clamp(t, 0.f, 1.f);
  return glm::length(p - (a + ab * t));
}

glm::vec3 terrainColor(float x, float z, float y) {
  const float pdx = x - kPond.x;
  const float pdz = z - kPond.y;
  if (pdx * pdx + pdz * pdz < kPondR * kPondR && y < kWaterY - 0.02f) return kBed;

  const glm::vec2 pathA = glm::vec2{-1.6f, 2.4f};
  const glm::vec2 pathB = pathA + glm::vec2{kFore.x, kFore.z} * 9.5f;
  if (distToSegment({x, z}, pathA, pathB) < 0.72f && y > kWaterY) return kDirt;

  // Two or three greens over many units, not a per-cell checker.
  const float n = std::sin(x * 0.028f + 0.4f) + 0.75f * std::cos(z * 0.024f + 1.1f) +
                  0.40f * std::sin((x * 0.5f - z) * 0.018f);
  if (n > 0.85f) return kGrassLight;
  if (n < -0.75f) return kGrassDeep;
  return kGrass;
}

struct Disk {
  float x, z, r;
};

struct Surf {
  glm::vec3 pos;
  glm::vec3 n;
  glm::vec3 color;
};

// Same diagonal as the terrain mesh, so a tuft's height, normal, and colour
// match the rasterised triangle instead of a bilinear sample.
Surf sampleSurf(const Height& hm, float x, float z) {
  const float gx = (x - Height::kOrigin) / Height::kStep;
  const float gz = (z - Height::kOrigin) / Height::kStep;
  const int x0 = std::clamp(static_cast<int>(std::floor(gx)), 0, Height::N - 2);
  const int z0 = std::clamp(static_cast<int>(std::floor(gz)), 0, Height::N - 2);
  const float u = gx - static_cast<float>(x0);
  const float v = gz - static_cast<float>(z0);

  auto corner = [&](int ix, int iz) {
    const float wx = Height::kOrigin + static_cast<float>(ix);
    const float wz = Height::kOrigin + static_cast<float>(iz);
    const float y = hm.at(ix, iz);
    const float hx = hm.at(ix + 1, iz) - hm.at(ix - 1, iz);
    const float hz = hm.at(ix, iz + 1) - hm.at(ix, iz - 1);
    Surf s;
    s.pos = {wx, y, wz};
    s.n = glm::normalize(glm::vec3{-hx, 2.f * Height::kStep, -hz});
    s.color = terrainColor(wx, wz, y);
    return s;
  };
  const Surf c00 = corner(x0, z0);
  const Surf c10 = corner(x0 + 1, z0);
  const Surf c01 = corner(x0, z0 + 1);
  const Surf c11 = corner(x0 + 1, z0 + 1);
  float w00, w10, w01, w11;
  if (u + v <= 1.f) {
    w00 = 1.f - u - v;
    w10 = u;
    w01 = v;
    w11 = 0.f;
  } else {
    w00 = 0.f;
    w10 = 1.f - v;
    w01 = 1.f - u;
    w11 = u + v - 1.f;
  }
  Surf s;
  s.pos = c00.pos * w00 + c10.pos * w10 + c01.pos * w01 + c11.pos * w11;
  s.n = glm::normalize(c00.n * w00 + c10.n * w10 + c01.n * w01 + c11.n * w11);
  s.color = c00.color * w00 + c10.color * w10 + c01.color * w01 + c11.color * w11;
  return s;
}

// Bridson Poisson disk. One pass at load, then the list is static.
std::vector<glm::vec2> poissonDisk(float radius, float minX, float maxX, float minZ, float maxZ,
                                   std::uint32_t seed) {
  const float cell = radius / std::sqrt(2.f);
  const int gw = std::max(1, static_cast<int>(std::ceil((maxX - minX) / cell)));
  const int gh = std::max(1, static_cast<int>(std::ceil((maxZ - minZ) / cell)));
  std::vector<int> grid(static_cast<std::size_t>(gw * gh), -1);
  std::vector<glm::vec2> pts;
  std::vector<int> active;
  auto cellOf = [&](glm::vec2 p) {
    const int x = std::clamp(static_cast<int>((p.x - minX) / cell), 0, gw - 1);
    const int z = std::clamp(static_cast<int>((p.y - minZ) / cell), 0, gh - 1);
    return z * gw + x;
  };
  auto tooClose = [&](glm::vec2 p) {
    const int cx = static_cast<int>((p.x - minX) / cell);
    const int cz = static_cast<int>((p.y - minZ) / cell);
    for (int dz = -2; dz <= 2; ++dz) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int x = cx + dx;
        const int z = cz + dz;
        if (x < 0 || z < 0 || x >= gw || z >= gh) continue;
        const int id = grid[static_cast<std::size_t>(z * gw + x)];
        if (id < 0) continue;
        const glm::vec2 d = pts[static_cast<std::size_t>(id)] - p;
        if (glm::dot(d, d) < radius * radius) return true;
      }
    }
    return false;
  };

  std::mt19937 rng(seed);
  std::uniform_real_distribution<float> uni(0.f, 1.f);
  const glm::vec2 origin{(minX + maxX) * 0.5f, (minZ + maxZ) * 0.5f};
  pts.push_back(origin);
  active.push_back(0);
  grid[static_cast<std::size_t>(cellOf(origin))] = 0;

  while (!active.empty()) {
    const int ai = static_cast<int>(rng() % active.size());
    const int idx = active[static_cast<std::size_t>(ai)];
    bool found = false;
    for (int k = 0; k < 24; ++k) {
      const float ang = uni(rng) * kTau;
      const float rad = radius * (1.f + uni(rng));
      const glm::vec2 q = pts[static_cast<std::size_t>(idx)] + glm::vec2{std::cos(ang), std::sin(ang)} * rad;
      if (q.x < minX || q.x > maxX || q.y < minZ || q.y > maxZ) continue;
      if (tooClose(q)) continue;
      const int id = static_cast<int>(pts.size());
      pts.push_back(q);
      active.push_back(id);
      grid[static_cast<std::size_t>(cellOf(q))] = id;
      found = true;
    }
    if (!found) {
      active[static_cast<std::size_t>(ai)] = active.back();
      active.pop_back();
    }
  }
  return pts;
}

#ifndef PXE_ASSET_DIR
#define PXE_ASSET_DIR "assets"
#endif

struct StairSpec {
  int count = 7;
  float width = 1.4f;
  float depth = 0.64f;
  float rise = 0.18f;
  float thick = 0.24f;
};

StairSpec loadStairs(const std::filesystem::path& path) {
  StairSpec spec;
  std::ifstream in(path);
  if (!in) {
    std::fprintf(stderr, "pxe: missing stair description %s\n", path.string().c_str());
    std::exit(1);
  }
  std::string key;
  while (in >> key) {
    if (key == "count") in >> spec.count;
    else if (key == "width") in >> spec.width;
    else if (key == "depth") in >> spec.depth;
    else if (key == "rise") in >> spec.rise;
    else if (key == "thick") in >> spec.thick;
  }
  if (spec.count < 2 || spec.depth <= 0.f || spec.rise <= 0.f) {
    std::fprintf(stderr, "pxe: bad stair description %s\n", path.string().c_str());
    std::exit(1);
  }
  return spec;
}

pxe::GltfAsset loadProp(const char* rel) {
  const auto path = (std::filesystem::path(PXE_ASSET_DIR) / rel).string();
  try {
    pxe::GltfAsset asset = pxe::loadGltf(path);
    int tris = 0;
    for (const pxe::GltfNode& node : asset.nodes) {
      for (const pxe::GltfPrimitive& prim : node.primitives) {
        tris += static_cast<int>(prim.indices.size() / 3);
      }
    }
    std::fprintf(stderr, "pxe: loaded %s (%d tris)\n", rel, tris);
    return asset;
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "pxe: %s\n", ex.what());
    std::exit(1);
  }
}

glm::mat4 propXform(glm::vec3 pos, float yaw, glm::vec3 scale) {
  return glm::translate(glm::mat4(1.f), pos) * glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0.f, 1.f, 0.f)) *
         glm::scale(glm::mat4(1.f), scale);
}

// Append a glTF asset into the solid and shadow meshes. blockRadius < 0 derives a
// disk from the non-leaf bounds. Canopy vertices each get one leaf sprite.
void placeProp(MeshBuild& solid, MeshBuild& shadow, std::vector<pxe::FoliageInstance>& foliage,
               std::vector<Disk>& blocked, const pxe::GltfAsset& asset, const glm::mat4& world,
               float blockRadius, bool scatterLeaves) {
  const glm::vec3 origin = glm::vec3(world[3]);
  std::vector<pxe::Vertex> leafVerts;
  float autoR = 0.f;

  auto consume = [&](MeshBuild& dst, bool record) {
    for (const pxe::GltfNode& node : asset.nodes) {
      const glm::mat4 m = world * node.transform;
      const glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(m)));
      const bool flip = glm::determinant(glm::mat3(m)) < 0.f;
      for (const pxe::GltfPrimitive& prim : node.primitives) {
        const auto base = static_cast<std::uint32_t>(dst.verts.size());
        for (const pxe::Vertex& src : prim.vertices) {
          const glm::vec3 p = glm::vec3(m * glm::vec4(src.px, src.py, src.pz, 1.f));
          glm::vec3 n = nm * glm::vec3(src.nx, src.ny, src.nz);
          const float nlen = glm::length(n);
          if (nlen > 1e-8f) n /= nlen;
          pxe::Vertex v = src;
          v.px = p.x;
          v.py = p.y;
          v.pz = p.z;
          v.nx = n.x;
          v.ny = n.y;
          v.nz = n.z;
          dst.verts.push_back(v);
          if (!record) continue;
          if (src.surface > 3.5f) {
            leafVerts.push_back(v);
          } else {
            const float dx = p.x - origin.x;
            const float dz = p.z - origin.z;
            autoR = std::max(autoR, std::sqrt(dx * dx + dz * dz));
          }
        }
        for (std::size_t i = 0; i + 2 < prim.indices.size(); i += 3) {
          const auto i0 = base + prim.indices[i];
          const auto i1 = base + prim.indices[i + 1];
          const auto i2 = base + prim.indices[i + 2];
          if (!flip) {
            dst.idx.push_back(i0);
            dst.idx.push_back(i1);
            dst.idx.push_back(i2);
          } else {
            dst.idx.push_back(i0);
            dst.idx.push_back(i2);
            dst.idx.push_back(i1);
          }
        }
      }
    }
  };

  consume(solid, true);
  consume(shadow, false);

  if (blockRadius > 0.f) {
    blocked.push_back({origin.x, origin.z, blockRadius});
  } else if (blockRadius < 0.f && autoR > 0.05f) {
    blocked.push_back({origin.x, origin.z, autoR * 0.8f});
  }

  if (!scatterLeaves || leafVerts.empty()) return;

  // One sprite per canopy vertex. Positions are quantised so a split normal
  // (same point, two wedges) does not stack two cards.
  std::unordered_map<std::uint64_t, int> seen;
  seen.reserve(leafVerts.size());
  auto keyOf = [](float x, float y, float z) {
    auto q = [](float v) { return static_cast<std::uint64_t>(static_cast<int>(std::lround(v / 0.02f)) & 0x1FFFFF); };
    return q(x) | (q(y) << 21) | (q(z) << 42);
  };
  for (const pxe::Vertex& v : leafVerts) {
    const glm::vec3 n{v.nx, v.ny, v.nz};
    if (glm::dot(n, n) < 1e-6f) continue;
    const std::uint64_t key = keyOf(v.px, v.py, v.pz);
    if (!seen.insert({key, 1}).second) continue;
    const int qx = static_cast<int>(std::lround(v.px * 50.f));
    const int qy = static_cast<int>(std::lround(v.py * 50.f));
    const int qz = static_cast<int>(std::lround(v.pz * 50.f));
    pxe::FoliageInstance leaf{};
    leaf.px = v.px;
    leaf.py = v.py;
    leaf.pz = v.pz;
    leaf.nx = n.x;
    leaf.ny = n.y;
    leaf.nz = n.z;
    leaf.cr = v.cr;
    leaf.cg = v.cg;
    leaf.cb = v.cb;
    leaf.phase = hash01(qx, qz, qy + 3);
    leaf.scale = 1.f;
    leaf.kind = 2.f;
    leaf.pad = hash01(qx, qy, 9) > 0.5f ? 1.f : 0.f;
    foliage.push_back(leaf);
  }
}

}  // namespace

pxe::Scene buildMeadow() {
  Height hm = makeHeight();

  // The flight is the Blender array in props/stairs.glb. Local +Z runs downhill,
  // which is kFore at yaw -45°, so the steps face the default camera.
  const StairSpec stairs = loadStairs(std::filesystem::path(PXE_ASSET_DIR) / "props/stairs.meta");
  const float stairYaw = -kPi * 0.25f;
  const glm::vec3 stairAnchor{0.15f, 0.f, 0.55f};
  const glm::mat4 stairRot = glm::rotate(glm::mat4(1.f), stairYaw, glm::vec3(0.f, 1.f, 0.f));
  const glm::vec3 stairAlong = glm::vec3(stairRot * glm::vec4(0.f, 0.f, 1.f, 0.f));
  const float meshDrop = stairs.rise * static_cast<float>(stairs.count - 1);
  const glm::vec3 stairEnd =
      stairAnchor + stairAlong * (stairs.depth * static_cast<float>(stairs.count - 1));
  const float yHillTop = hm.sample(stairAnchor.x, stairAnchor.z);
  const float yHillBot = hm.sample(stairEnd.x, stairEnd.z);
  float yPlace = yHillTop;
  if (yPlace - meshDrop > yHillBot + 0.02f) yPlace -= (yPlace - meshDrop) - yHillBot;
  const glm::mat4 stairWorld =
      glm::translate(glm::mat4(1.f), glm::vec3(stairAnchor.x, yPlace, stairAnchor.z)) * stairRot;
  const glm::mat4 stairInv = glm::inverse(stairWorld);
  const float halfW = stairs.width * 0.5f;
  const float halfD = stairs.depth * 0.5f;
  const float z0 = -halfD;
  const float z1 = stairs.depth * static_cast<float>(stairs.count - 1) + halfD;

  for (int z = 0; z < Height::N; ++z) {
    for (int x = 0; x < Height::N; ++x) {
      const float wx = Height::kOrigin + static_cast<float>(x);
      const float wz = Height::kOrigin + static_cast<float>(z);
      const glm::vec3 local = glm::vec3(stairInv * glm::vec4(wx, 0.f, wz, 1.f));
      const float outsideX = std::max(0.f, std::fabs(local.x) - halfW);
      const float outsideZ = std::max(0.f, std::max(z0 - local.z, local.z - z1));
      const float outside = std::max(outsideX, outsideZ);
      if (outside > 0.7f) continue;
      const int step = std::clamp(static_cast<int>(std::round(local.z / stairs.depth)), 0, stairs.count - 1);
      // Full cut on the treads, feathered over the last 0.7 m so the bank is not a trench wall.
      const float blend = outside <= 0.f ? 1.f : smoothFalloff(outside, 0.7f);
      const float tread = yPlace - static_cast<float>(step) * stairs.rise - 0.06f;
      const float carved = std::min(hm.at(x, z), tread);
      hm.set(x, z, glm::mix(hm.at(x, z), carved, blend));
    }
  }

  MeshBuild solid;
  MeshBuild shadow;
  MeshBuild water;
  std::vector<pxe::FoliageInstance> foliage;
  std::vector<Disk> blocked;
  blocked.push_back({kPond.x, kPond.y, kPondR + 0.3f});

  for (int z = 0; z < Height::N - 1; ++z) {
    for (int x = 0; x < Height::N - 1; ++x) {
      auto corner = [&](int ix, int iz, pxe::Vertex& out) {
        const float wx = Height::kOrigin + static_cast<float>(ix);
        const float wz = Height::kOrigin + static_cast<float>(iz);
        const float y = hm.at(ix, iz);
        const float hx = hm.at(ix + 1, iz) - hm.at(ix - 1, iz);
        const float hz = hm.at(ix, iz + 1) - hm.at(ix, iz - 1);
        glm::vec3 n = glm::normalize(glm::vec3{-hx, 2.f * Height::kStep, -hz});
        const glm::vec3 c = terrainColor(wx, wz, y);
        out = pxe::Vertex{wx, y, wz, n.x, n.y, n.z, c.r, c.g, c.b, pxe::kSurfGround};
      };
      pxe::Vertex v00, v10, v11, v01;
      corner(x, z, v00);
      corner(x + 1, z, v10);
      corner(x + 1, z + 1, v11);
      corner(x, z + 1, v01);
      auto pushTri = [&](MeshBuild& m, const pxe::Vertex& a, const pxe::Vertex& b, const pxe::Vertex& c) {
        const auto base = static_cast<std::uint32_t>(m.verts.size());
        m.verts.push_back(a);
        m.verts.push_back(b);
        m.verts.push_back(c);
        m.idx.push_back(base);
        m.idx.push_back(base + 1);
        m.idx.push_back(base + 2);
      };
      // a (x,z+1), b (x+1,z+1), c (x+1,z), d (x,z) — same winding as before.
      pushTri(solid, v01, v11, v10);
      pushTri(solid, v01, v10, v00);
      pushTri(shadow, v01, v11, v10);
      pushTri(shadow, v01, v10, v00);

      const float cx = Height::kOrigin + static_cast<float>(x) + 0.5f;
      const float cz = Height::kOrigin + static_cast<float>(z) + 0.5f;
      const float cy = hm.sample(cx, cz);
      const float pdx = cx - kPond.x;
      const float pdz = cz - kPond.y;
      if (pdx * pdx + pdz * pdz < (kPondR + 0.4f) * (kPondR + 0.4f) && cy < kWaterY + 0.02f) {
        const float x0 = Height::kOrigin + static_cast<float>(x);
        const float z0 = Height::kOrigin + static_cast<float>(z);
        water.quad({x0, kWaterY, z0 + 1}, {x0 + 1, kWaterY, z0 + 1}, {x0 + 1, kWaterY, z0},
                   {x0, kWaterY, z0}, {0.5f, 0.75f, 0.85f}, pxe::kSurfGround, {0, 1, 0});
      }
    }
  }

  auto ground = [&](float x, float z) { return hm.sample(x, z); };
  auto put = [&](const pxe::GltfAsset& asset, glm::vec3 pos, float yaw, glm::vec3 scale, float block,
                 bool leaves) {
    placeProp(solid, shadow, foliage, blocked, asset, propXform(pos, yaw, scale), block, leaves);
  };
  auto putOn = [&](const pxe::GltfAsset& asset, float x, float z, float yaw, float scale, float block,
                   bool leaves) {
    put(asset, {x, ground(x, z), z}, yaw, glm::vec3(scale), block, leaves);
  };

  const pxe::GltfAsset pillar = loadProp("props/pillar.glb");
  const pxe::GltfAsset pillarBroken = loadProp("props/pillar_broken.glb");
  const pxe::GltfAsset stairMesh = loadProp("props/stairs.glb");
  const pxe::GltfAsset wall = loadProp("props/wall.glb");
  const pxe::GltfAsset wallLow = loadProp("props/wall_low.glb");
  const pxe::GltfAsset slab = loadProp("props/slab.glb");
  const pxe::GltfAsset rockA = loadProp("props/rock_a.glb");
  const pxe::GltfAsset rockB = loadProp("props/rock_b.glb");
  const pxe::GltfAsset rockC = loadProp("props/rock_c.glb");
  const pxe::GltfAsset treeA = loadProp("props/tree_a.glb");
  const pxe::GltfAsset treeB = loadProp("props/tree_b.glb");
  const pxe::GltfAsset treeC = loadProp("props/tree_c.glb");
  const pxe::GltfAsset bush = loadProp("props/bush.glb");

  // Square pillars aligned near the world axes so the 45° camera sees two faces.
  putOn(pillar, -0.35f, 0.15f, 0.18f, 1.f, 0.75f, false);
  putOn(pillar, 1.25f, -1.05f, -0.22f, 1.05f, 0.8f, false);
  {
    const glm::vec3 broken = kFore * 3.15f + glm::vec3{-0.35f, 0.f, 0.15f};
    put(pillarBroken, {broken.x, ground(broken.x, broken.z), broken.z}, 0.4f, glm::vec3(0.95f), 0.7f,
        false);
  }

  placeProp(solid, shadow, foliage, blocked, stairMesh, stairWorld, 0.f, false);
  for (int i = 0; i < stairs.count; ++i) {
    const glm::vec3 at = glm::vec3(stairWorld * glm::vec4(0.f, 0.f, stairs.depth * static_cast<float>(i), 1.f));
    blocked.push_back({at.x, at.z, stairs.width * 0.42f});
  }

  // Long axis of the wall mesh is local +X, which this yaw lays across the view.
  putOn(wall, -1.15f, -1.55f, stairYaw, 1.f, -1.f, false);
  putOn(wallLow, 2.55f, 1.15f, stairYaw + 1.2f, 1.f, -1.f, false);

  put(slab, {-1.7f, ground(-1.7f, -1.35f), -1.35f}, 0.4f, {1.35f, 1.f, 1.15f}, -1.f, false);
  put(slab, {2.35f, ground(2.35f, 0.85f), 0.85f}, -0.3f, {1.05f, 1.f, 1.45f}, -1.f, false);
  put(slab, {-0.15f, ground(-0.15f, -2.35f), -2.35f}, 0.9f, {0.85f, 1.f, 0.9f}, -1.f, false);
  put(slab, {3.5f, ground(3.5f, -2.7f), -2.7f}, 0.2f, {1.4f, 1.f, 1.05f}, -1.f, false);

  putOn(rockA, kPond.x - 1.85f, kPond.y + 1.2f, 0.4f, 1.05f, -1.f, false);
  putOn(rockB, kPond.x - 1.05f, kPond.y + 1.75f, 1.1f, 0.9f, -1.f, false);
  putOn(rockC, kPond.x + 1.65f, kPond.y - 0.85f, 0.2f, 1.15f, -1.f, false);
  putOn(rockB, kPond.x - 0.35f, kPond.y - 1.45f, 2.0f, 1.f, -1.f, false);
  putOn(rockA, -3.6f, 3.4f, 0.8f, 0.85f, -1.f, false);
  putOn(rockC, 4.4f, 2.6f, 1.4f, 0.7f, -1.f, false);
  putOn(rockA, -7.2f, -3.1f, 0.3f, 0.95f, -1.f, false);
  // A stone breaking the water line. The pivot is the bottom of the rock.
  put(rockB, {kPond.x + 0.35f, kWaterY - 0.22f, kPond.y - 0.15f}, 0.6f, glm::vec3(0.85f), 0.f, false);

  // tree_a is authored leaning; yaw turns that lean toward the pond.
  putOn(treeA, 3.55f, -2.15f, 0.9f, 1.f, 0.7f, true);
  putOn(treeB, -8.2f, -2.4f, 0.4f, 1.05f, 0.65f, true);
  putOn(treeC, -3.4f, -8.2f, 1.2f, 1.f, 0.6f, true);
  putOn(treeB, 9.4f, 2.2f, -0.5f, 0.95f, 0.6f, true);
  putOn(treeC, 7.2f, 6.4f, 0.2f, 0.85f, 0.55f, true);
  putOn(treeA, -9.5f, 5.8f, 2.1f, 1.1f, 0.75f, true);

  putOn(bush, -2.4f, 4.6f, 0.3f, 1.15f, 0.55f, true);
  putOn(bush, -1.05f, 5.35f, 1.4f, 0.85f, 0.45f, true);
  putOn(bush, 1.9f, 4.15f, 0.8f, 1.f, 0.5f, true);
  putOn(bush, 7.4f, -2.2f, 2.2f, 1.05f, 0.5f, true);
  putOn(bush, 3.6f, -5.4f, 0.5f, 0.9f, 0.45f, true);
  putOn(bush, -6.8f, 1.8f, 1.7f, 1.2f, 0.55f, true);

  auto occupied = [&](float x, float z) {
    for (const Disk& d : blocked) {
      const float dx = x - d.x;
      const float dz = z - d.z;
      if (dx * dx + dz * dz < d.r * d.r) return true;
    }
    return false;
  };

  // Blue noise once. Spacing is about half a metre, so a band edge breaks into
  // tufts and the interior of a band stays empty-looking.
  const std::vector<glm::vec2> grassPts = poissonDisk(0.48f, -40.f, 40.f, -40.f, 40.f, 0x6A09E667u);
  int grassN = 0;
  int flowerN = 0;
  for (std::size_t i = 0; i < grassPts.size(); ++i) {
    const float wx = grassPts[i].x;
    const float wz = grassPts[i].y;
    if (occupied(wx, wz)) continue;
    const Surf s = sampleSurf(hm, wx, wz);
    if (s.pos.y < kWaterY + 0.04f) continue;
    if (s.n.y < 0.55f) continue;
    const glm::vec3 tag = terrainColor(wx, wz, s.pos.y);
    if (tag == kDirt || tag == kBed) continue;

    const int ix = static_cast<int>(std::lround(wx * 20.f));
    const int iz = static_cast<int>(std::lround(wz * 20.f));
    const float roll = hash01(ix, iz, 14);
    pxe::FoliageInstance g{};
    g.px = s.pos.x;
    g.py = s.pos.y;
    g.pz = s.pos.z;
    g.nx = s.n.x;
    g.ny = s.n.y;
    g.nz = s.n.z;
    g.phase = hash01(ix, iz, 15);
    g.scale = 1.f;
    g.pad = hash01(ix, iz, 17) > 0.5f ? 1.f : 0.f;
    if (roll < 0.012f) {
      g.kind = 1.f;
      if (roll < 0.004f) {
        g.cr = 0.40f;
        g.cg = 0.56f;
        g.cb = 0.50f;
      } else {
        g.cr = 0.93f;
        g.cg = 0.84f;
        g.cb = 0.40f;
      }
      ++flowerN;
    } else {
      g.kind = 0.f;
      g.cr = s.color.r;
      g.cg = s.color.g;
      g.cb = s.color.b;
      ++grassN;
    }
    foliage.push_back(g);
  }
  int leafN = 0;
  for (const pxe::FoliageInstance& f : foliage) {
    if (f.kind > 1.5f) ++leafN;
  }
  std::fprintf(stderr, "pxe: grass %d  flowers %d  leaves %d\n", grassN, flowerN, leafN);

  pxe::Scene scene;
  scene.opaque.upload(solid.verts, solid.idx);
  scene.shadowCasters.upload(shadow.verts, shadow.idx);
  scene.water.upload(water.verts, water.idx);
  scene.foliage = std::move(foliage);
  // Tuned so morning light (hour 9) falls from the upper left of the default view.
  scene.sunAzimuth = 1.62f;
  scene.focus = {1.15f, 1.2f, -0.35f};
  return scene;
}

}  // namespace game
