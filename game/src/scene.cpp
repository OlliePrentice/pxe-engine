#include "scene.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

namespace game {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kYaw = 0.7853981633974483f;  // 45°, fixed with the sun and the hill
constexpr float kHalf = 45.f;
constexpr float kWorld = 90.f;
constexpr float kGrid = 0.25f;
constexpr int kNormalRes = 720;  // 0.125 m texels
constexpr float kNormalEps = 0.05f;

constexpr float kHillH = 6.f;
constexpr float kHillW = 8.f;
constexpr float kRipA = 0.6f;
constexpr float kRipL = 14.f;
constexpr float kWarpA = 1.2f;
constexpr float kCross = 0.25f;
constexpr float kDet = 0.018f;

float height(float x, float z) {
  const float hill =
      kHillH / (1.f + std::exp(-(std::sin(kYaw) * x + std::cos(kYaw) * z) / kHillW));
  const float warp = kWarpA * std::sin(2.f * kPi * (x - 0.5f * z) / 23.f) +
                     0.5f * kWarpA * std::sin(2.f * kPi * (0.4f * x + z) / 31.f);
  const float kx = std::sin(kYaw + 0.25f);
  const float kz = std::cos(kYaw + 0.25f);
  float rip = kRipA * std::sin(2.f * kPi * (kx * x + kz * z) / kRipL + warp);
  rip += kCross * kRipA *
         std::sin(2.f * kPi * (std::sin(kYaw - 0.5f) * x + std::cos(kYaw - 0.5f) * z) /
                      (kRipL * 0.73f) +
                  1.3f);
  const float det = kDet * std::sin(2.f * kPi * (0.9f * x - 0.4f * z) / 2.3f + std::sin(1.7f * z)) *
                    std::sin(2.f * kPi * (0.3f * x + z) / 1.9f + std::cos(1.3f * x));
  return hill + rip + det;
}

glm::vec3 normalAt(float x, float z) {
  const float hx = (height(x + kNormalEps, z) - height(x - kNormalEps, z)) / (2.f * kNormalEps);
  const float hz = (height(x, z + kNormalEps) - height(x, z - kNormalEps)) / (2.f * kNormalEps);
  return glm::normalize(glm::vec3{-hx, 1.f, -hz});
}

// Bilinear sample of the baked normal, then renormalise. Matches the terrain
// fragment shader's texture() + normalize() at this xz.
glm::vec3 sampleNormal(const std::vector<float>& tex, float x, float z) {
  const float fx = (x - (-kHalf)) / kWorld * static_cast<float>(kNormalRes) - 0.5f;
  const float fz = (z - (-kHalf)) / kWorld * static_cast<float>(kNormalRes) - 0.5f;
  const int x0 = static_cast<int>(std::floor(fx));
  const int z0 = static_cast<int>(std::floor(fz));
  const float tx = fx - static_cast<float>(x0);
  const float tz = fz - static_cast<float>(z0);
  auto at = [&](int i, int j) {
    i = std::clamp(i, 0, kNormalRes - 1);
    j = std::clamp(j, 0, kNormalRes - 1);
    const int k = (j * kNormalRes + i) * 4;
    return glm::vec3{tex[static_cast<std::size_t>(k)], tex[static_cast<std::size_t>(k + 1)],
                     tex[static_cast<std::size_t>(k + 2)]};
  };
  const glm::vec3 n = glm::mix(glm::mix(at(x0, z0), at(x0 + 1, z0), tx),
                               glm::mix(at(x0, z0 + 1), at(x0 + 1, z0 + 1), tx), tz);
  return glm::normalize(n);
}

// Slow 0..1 field so grass grows in taller and shorter patches; a uniform
// height makes every band edge the same fine comb.
float patchiness(float x, float z) {
  const float a = std::sin(0.31f * x + 1.7f * std::sin(0.23f * z));
  const float b = std::sin(0.27f * z - 1.3f * std::sin(0.19f * x + 0.5f));
  return std::clamp(0.5f + 0.5f * a * b + 0.15f * std::sin(0.9f * x + 0.7f * z), 0.f, 1.f);
}

pxe::GrassInstance makeInstance(float x, float z, const std::vector<float>& normals, float kind, float seed,
                                float scale) {
  pxe::GrassInstance g{};
  const glm::vec3 n = sampleNormal(normals, x, z);
  g.px = x;
  g.py = height(x, z);
  g.pz = z;
  g.nx = n.x;
  g.ny = n.y;
  g.nz = n.z;
  g.kind = kind;
  g.seed = seed;
  g.scale = scale;
  g.pad = 0.f;
  return g;
}

// Bridson Poisson disk in horizontal metres (default radius 0.62 m, about
// 1.6 tufts/m²). The seed is an area-weighted
// point on the mesh; every later point keeps at least `minDist` metres of
// XZ separation, which is the density the brief counts in tufts/m². That is
// blue noise, not a jittered grid. Separation is horizontal rather than
// along the slope so a steep face does not pack extra tufts into one screen
// pixel.
std::vector<std::pair<float, float>> poisson(const std::vector<pxe::TerrainVertex>& verts, int n,
                                             float minDist, std::mt19937& rng) {
  const int cells = n - 1;
  std::vector<float> cdf;
  cdf.reserve(static_cast<std::size_t>(cells * cells * 2));
  float total = 0.f;
  auto at = [&](int i, int j) {
    const pxe::TerrainVertex& v = verts[static_cast<std::size_t>(j * n + i)];
    return glm::vec3{v.x, v.y, v.z};
  };
  for (int j = 0; j < cells; ++j) {
    for (int i = 0; i < cells; ++i) {
      const glm::vec3 v00 = at(i, j);
      const glm::vec3 v10 = at(i + 1, j);
      const glm::vec3 v01 = at(i, j + 1);
      const glm::vec3 v11 = at(i + 1, j + 1);
      total += 0.5f * glm::length(glm::cross(v10 - v00, v11 - v00));
      cdf.push_back(total);
      total += 0.5f * glm::length(glm::cross(v11 - v00, v01 - v00));
      cdf.push_back(total);
    }
  }

  std::uniform_real_distribution<float> unit(0.f, 1.f);
  auto sampleTriangle = [&]() {
    const float pick = unit(rng) * total;
    auto it = std::lower_bound(cdf.begin(), cdf.end(), pick);
    int idx = static_cast<int>(it - cdf.begin());
    if (idx >= static_cast<int>(cdf.size())) idx = static_cast<int>(cdf.size()) - 1;
    const int which = idx & 1;
    const int cell = idx >> 1;
    const int i = cell % cells;
    const int j = cell / cells;
    const glm::vec3 v00 = at(i, j);
    const glm::vec3 v10 = at(i + 1, j);
    const glm::vec3 v01 = at(i, j + 1);
    const glm::vec3 v11 = at(i + 1, j + 1);
    const glm::vec3 a = v00;
    const glm::vec3 b = which ? v11 : v10;
    const glm::vec3 c = which ? v01 : v11;
    float u = unit(rng);
    float v = unit(rng);
    if (u + v > 1.f) {
      u = 1.f - u;
      v = 1.f - v;
    }
    const glm::vec3 p = a * (1.f - u - v) + b * u + c * v;
    return std::pair<float, float>{p.x, p.z};
  };

  const float cellSize = minDist / std::sqrt(2.f);
  const int gw = static_cast<int>(std::ceil(kWorld / cellSize));
  std::vector<int> grid(static_cast<std::size_t>(gw * gw), -1);
  std::vector<std::pair<float, float>> points;
  std::vector<int> active;
  points.reserve(20000);
  active.reserve(1024);

  auto cellIndex = [&](float x, float z) {
    const int ix = std::clamp(static_cast<int>((x + kHalf) / cellSize), 0, gw - 1);
    const int iz = std::clamp(static_cast<int>((z + kHalf) / cellSize), 0, gw - 1);
    return iz * gw + ix;
  };
  auto farEnough = [&](float x, float z) {
    if (x <= -kHalf || x >= kHalf || z <= -kHalf || z >= kHalf) return false;
    const int ix = static_cast<int>((x + kHalf) / cellSize);
    const int iz = static_cast<int>((z + kHalf) / cellSize);
    for (int dz = -2; dz <= 2; ++dz) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int jx = ix + dx;
        const int jz = iz + dz;
        if (jx < 0 || jz < 0 || jx >= gw || jz >= gw) continue;
        const int id = grid[static_cast<std::size_t>(jz * gw + jx)];
        if (id < 0) continue;
        const float ddx = points[static_cast<std::size_t>(id)].first - x;
        const float ddz = points[static_cast<std::size_t>(id)].second - z;
        if (ddx * ddx + ddz * ddz < minDist * minDist) return false;
      }
    }
    return true;
  };
  auto add = [&](float x, float z) {
    const int id = static_cast<int>(points.size());
    points.emplace_back(x, z);
    active.push_back(id);
    grid[static_cast<std::size_t>(cellIndex(x, z))] = id;
  };

  auto seed = sampleTriangle();
  seed.first = std::clamp(seed.first, -kHalf + 0.05f, kHalf - 0.05f);
  seed.second = std::clamp(seed.second, -kHalf + 0.05f, kHalf - 0.05f);
  add(seed.first, seed.second);
  while (!active.empty()) {
    std::uniform_int_distribution<int> pick(0, static_cast<int>(active.size()) - 1);
    const int ai = pick(rng);
    const int id = active[static_cast<std::size_t>(ai)];
    bool found = false;
    for (int k = 0; k < 30; ++k) {
      const float ang = unit(rng) * 2.f * kPi;
      const float rad = minDist * (1.f + unit(rng));
      const float x = points[static_cast<std::size_t>(id)].first + std::cos(ang) * rad;
      const float z = points[static_cast<std::size_t>(id)].second + std::sin(ang) * rad;
      if (!farEnough(x, z)) continue;
      add(x, z);
      found = true;
      break;
    }
    if (!found) {
      active[static_cast<std::size_t>(ai)] = active.back();
      active.pop_back();
    }
  }
  return points;
}

}  // namespace

pxe::Scene buildTerrain(const BuildOptions& options) {
  pxe::Scene scene;
  scene.worldMin = -kHalf;
  scene.worldSize = kWorld;

  const int n = static_cast<int>(std::lround(kWorld / kGrid)) + 1;
  std::vector<pxe::TerrainVertex> verts(static_cast<std::size_t>(n * n));
  float minY = 1e9f;
  float maxY = -1e9f;
  for (int j = 0; j < n; ++j) {
    for (int i = 0; i < n; ++i) {
      const float x = -kHalf + static_cast<float>(i) * kGrid;
      const float z = -kHalf + static_cast<float>(j) * kGrid;
      const float y = height(x, z);
      verts[static_cast<std::size_t>(j * n + i)] = {x, y, z};
      minY = std::min(minY, y);
      maxY = std::max(maxY, y);
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
      indices.push_back(i00);
      indices.push_back(i10);
      indices.push_back(i11);
      indices.push_back(i00);
      indices.push_back(i11);
      indices.push_back(i01);
    }
  }
  scene.terrain.upload(verts, indices);
  scene.boundsMin = {-kHalf, minY, -kHalf};
  scene.boundsMax = {kHalf, maxY, kHalf};

  scene.normalSize = kNormalRes;
  scene.normals.resize(static_cast<std::size_t>(kNormalRes * kNormalRes * 4));
  for (int j = 0; j < kNormalRes; ++j) {
    for (int i = 0; i < kNormalRes; ++i) {
      const float x = -kHalf + (static_cast<float>(i) + 0.5f) / static_cast<float>(kNormalRes) * kWorld;
      const float z = -kHalf + (static_cast<float>(j) + 0.5f) / static_cast<float>(kNormalRes) * kWorld;
      glm::vec3 normal = normalAt(x, z);
      // A few degrees of stable noise so shallow N·L edges wobble instead of
      // sitting on a straight pixel run. Grass reads this same texture.
      auto hash = [](int a, int b) {
        std::uint32_t h = static_cast<std::uint32_t>(a) * 374761393u ^
                          static_cast<std::uint32_t>(b) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return (static_cast<int>(h & 0xffff) - 32768) / 32768.f;
      };
      normal.x += hash(i, j) * 0.028f;
      normal.z += hash(j + 19, i * 3 + 1) * 0.028f;
      normal = glm::normalize(normal);
      const std::size_t k = static_cast<std::size_t>(j * kNormalRes + i) * 4;
      scene.normals[k] = normal.x;
      scene.normals[k + 1] = normal.y;
      scene.normals[k + 2] = normal.z;
      scene.normals[k + 3] = 1.f;
    }
  }

  const glm::vec3 flatForward{std::sin(kYaw), 0.f, std::cos(kYaw)};
  const glm::vec3 right{std::cos(kYaw), 0.f, -std::sin(kYaw)};
  scene.sunDir = glm::normalize(0.62f * flatForward - 0.30f * right + glm::vec3{0.f, 0.72f, 0.f});

  std::mt19937 rng(7);
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  if (options.stamps) {
    const float step = 2.6f;
    int s = 0;
    for (int row = 0; row < 2; ++row) {
      for (int col = 0; col < 4; ++col, ++s) {
        const float x = right.x * (static_cast<float>(col) - 1.5f) * step +
                        flatForward.x * (static_cast<float>(row) - 0.5f) * step;
        const float z = right.z * (static_cast<float>(col) - 1.5f) * step +
                        flatForward.z * (static_cast<float>(row) - 0.5f) * step;
        const float kind = static_cast<float>(row);
        scene.grass.push_back(makeInstance(x, z, scene.normals, kind, static_cast<float>(col) * 0.25f, 1.f));
      }
    }
  } else {
    // Clumps carpet the ground at blade spacing so every band edge gets a
    // continuous saw-tooth; tufts are a sparse second layer on top.
    const auto clumps = poisson(verts, n, options.minDist, rng);
    const auto tufts = options.tufts ? poisson(verts, n, options.tuftDist, rng)
                                     : std::vector<std::pair<float, float>>{};
    scene.grass.reserve(clumps.size() + tufts.size());
    for (const auto& p : clumps)
      scene.grass.push_back(makeInstance(p.first, p.second, scene.normals, 0.f, unit(rng),
                                         0.55f + 0.75f * patchiness(p.first, p.second) + 0.25f * unit(rng)));
    for (const auto& p : tufts)
      scene.grass.push_back(makeInstance(p.first, p.second, scene.normals, 1.f, unit(rng), 0.85f + 0.3f * unit(rng)));
    std::fprintf(stderr, "pxe: clumps %zu, tufts %zu\n", clumps.size(), tufts.size());
  }

  const float density = static_cast<float>(scene.grass.size()) / (kWorld * kWorld);
  std::fprintf(stderr, "pxe: grid %d, grass %zu (%.3f /m^2), sun %.3f %.3f %.3f\n", n, scene.grass.size(),
               density, scene.sunDir.x, scene.sunDir.y, scene.sunDir.z);
  return scene;
}

}  // namespace game
