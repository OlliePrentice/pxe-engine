#include "pxe/scatter.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <unordered_map>

namespace pxe {
namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

std::vector<glm::vec2> poissonDisk(glm::vec2 lo, glm::vec2 hi, float minDist, std::mt19937& rng,
                                   const std::function<bool(float, float)>& accept) {
  const float cellSize = minDist / std::sqrt(2.f);
  const int gw = static_cast<int>(std::ceil((hi.x - lo.x) / cellSize));
  const int gh = static_cast<int>(std::ceil((hi.y - lo.y) / cellSize));
  std::vector<int> grid(static_cast<std::size_t>(gw * gh), -1);
  std::vector<glm::vec2> points;
  std::vector<int> active;
  std::uniform_real_distribution<float> unit(0.f, 1.f);

  auto cellIndex = [&](float x, float z) {
    const int ix = std::clamp(static_cast<int>((x - lo.x) / cellSize), 0, gw - 1);
    const int iz = std::clamp(static_cast<int>((z - lo.y) / cellSize), 0, gh - 1);
    return iz * gw + ix;
  };
  auto farEnough = [&](float x, float z) {
    if (x <= lo.x || x >= hi.x || z <= lo.y || z >= hi.y) return false;
    const int ix = static_cast<int>((x - lo.x) / cellSize);
    const int iz = static_cast<int>((z - lo.y) / cellSize);
    for (int dz = -2; dz <= 2; ++dz) {
      for (int dx = -2; dx <= 2; ++dx) {
        const int jx = ix + dx, jz = iz + dz;
        if (jx < 0 || jz < 0 || jx >= gw || jz >= gh) continue;
        const int id = grid[static_cast<std::size_t>(jz * gw + jx)];
        if (id < 0) continue;
        const glm::vec2 d = points[static_cast<std::size_t>(id)] - glm::vec2(x, z);
        if (glm::dot(d, d) < minDist * minDist) return false;
      }
    }
    return true;
  };
  auto add = [&](float x, float z) {
    grid[static_cast<std::size_t>(cellIndex(x, z))] = static_cast<int>(points.size());
    active.push_back(static_cast<int>(points.size()));
    points.emplace_back(x, z);
  };
  auto grow = [&]() {
    while (!active.empty()) {
      std::uniform_int_distribution<int> pick(0, static_cast<int>(active.size()) - 1);
      const int ai = pick(rng);
      const glm::vec2 from = points[static_cast<std::size_t>(active[static_cast<std::size_t>(ai)])];
      bool found = false;
      for (int k = 0; k < 30; ++k) {
        const float ang = unit(rng) * 2.f * kPi;
        const float rad = minDist * (1.f + unit(rng));
        const float x = from.x + std::cos(ang) * rad;
        const float z = from.y + std::sin(ang) * rad;
        if (!farEnough(x, z) || !accept(x, z)) continue;
        add(x, z);
        found = true;
        break;
      }
      if (!found) {
        active[static_cast<std::size_t>(ai)] = active.back();
        active.pop_back();
      }
    }
  };

  std::vector<int> order(static_cast<std::size_t>(gw * gh));
  for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
  std::shuffle(order.begin(), order.end(), rng);
  for (int c : order) {
    if (grid[static_cast<std::size_t>(c)] >= 0) continue;
    const float x = lo.x + (static_cast<float>(c % gw) + unit(rng)) * cellSize;
    const float z = lo.y + (static_cast<float>(c / gw) + unit(rng)) * cellSize;
    if (!farEnough(x, z) || !accept(x, z)) continue;
    add(x, z);
    grow();
  }
  return points;
}

float grassPatchiness(float x, float z) {
  const float a = std::sin(0.31f * x + 1.7f * std::sin(0.23f * z));
  const float b = std::sin(0.27f * z - 1.3f * std::sin(0.19f * x + 0.5f));
  return std::clamp(0.5f + 0.5f * a * b + 0.15f * std::sin(0.9f * x + 0.7f * z), 0.f, 1.f);
}

void scatterGrass(Scene& scene, const TriangleGrid& ground, const GrassScatter& params, std::mt19937& rng) {
  if (ground.empty()) return;
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  auto onGround = [&](float x, float z) { return ground.at(x, z).has_value(); };
  auto emit = [&](glm::vec2 p, float kind, float scale) {
    const auto hit = ground.at(p.x, p.y);
    if (!hit) return;
    scene.grass.push_back({hit->pos.x, hit->pos.y, hit->pos.z, hit->normal.x, hit->normal.y, hit->normal.z, kind,
                           unit(rng), scale, static_cast<float>(kSurfGrass)});
  };
  const auto clumps = poissonDisk(ground.min(), ground.max(), params.clumpSpacing, rng, onGround);
  for (const glm::vec2& p : clumps) emit(p, 0.f, 0.55f + 0.75f * params.height(p.x, p.y) + 0.25f * unit(rng));
  std::size_t tufts = 0;
  if (params.tufts) {
    const auto points = poissonDisk(ground.min(), ground.max(), params.tuftSpacing, rng, onGround);
    for (const glm::vec2& p : points) emit(p, 1.f, 0.85f + 0.3f * unit(rng));
    tufts = points.size();
  }
  std::fprintf(stderr, "pxe: clumps %zu, tufts %zu\n", clumps.size(), tufts);
}

std::size_t dropGrassUnder(Scene& scene, const SurfaceGeometry& geometry, std::initializer_list<Surface> surfaces) {
  // The surfaces' highest point over each 10 cm cell.
  constexpr float kCell = 0.1f;
  const glm::vec2 lo{scene.boundsMin.x, scene.boundsMin.z};
  const int gw = static_cast<int>(std::ceil((scene.boundsMax.x - lo.x) / kCell)) + 1;
  const int gh = static_cast<int>(std::ceil((scene.boundsMax.z - lo.y) / kCell)) + 1;
  std::vector<float> top(static_cast<std::size_t>(gw * gh), -1e9f);
  for (Surface s : surfaces) {
    const auto& v = geometry.vertices[s];
    const auto& idx = geometry.indices[s];
    for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
      const Vertex& a = v[idx[i]];
      const Vertex& b = v[idx[i + 1]];
      const Vertex& c = v[idx[i + 2]];
      const float d = (b.pz - c.pz) * (a.px - c.px) + (c.px - b.px) * (a.pz - c.pz);
      if (std::fabs(d) < 1e-9f) continue;  // a wall seen edge-on from above
      const int x0 = std::max(0, static_cast<int>((std::min({a.px, b.px, c.px}) - lo.x) / kCell));
      const int x1 = std::min(gw - 1, static_cast<int>((std::max({a.px, b.px, c.px}) - lo.x) / kCell) + 1);
      const int z0 = std::max(0, static_cast<int>((std::min({a.pz, b.pz, c.pz}) - lo.y) / kCell));
      const int z1 = std::min(gh - 1, static_cast<int>((std::max({a.pz, b.pz, c.pz}) - lo.y) / kCell) + 1);
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) {
          const float px = lo.x + (static_cast<float>(x) + 0.5f) * kCell;
          const float pz = lo.y + (static_cast<float>(z) + 0.5f) * kCell;
          const float u = ((b.pz - c.pz) * (px - c.px) + (c.px - b.px) * (pz - c.pz)) / d;
          const float w = ((c.pz - a.pz) * (px - c.px) + (a.px - c.px) * (pz - c.pz)) / d;
          if (u < 0.f || w < 0.f || u + w > 1.f) continue;
          float& t = top[static_cast<std::size_t>(z * gw + x)];
          t = std::max(t, a.py * u + b.py * w + c.py * (1.f - u - w));
        }
    }
  }
  const std::size_t before = scene.grass.size();
  std::erase_if(scene.grass, [&](const GrassInstance& g) {
    if (g.surface != static_cast<float>(kSurfGrass)) return false;
    const int x = static_cast<int>((g.px - lo.x) / kCell), z = static_cast<int>((g.pz - lo.y) / kCell);
    if (x < 0 || z < 0 || x >= gw || z >= gh) return false;
    return top[static_cast<std::size_t>(z * gw + x)] > g.py + 0.03f;
  });
  return before - scene.grass.size();
}

int scatterLeaves(Scene& scene, const std::vector<Triangle>& crowns, float density, std::mt19937& rng) {
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  // Pick points by area, so leaves spread evenly over the crowns.
  std::vector<float> area;
  for (const Triangle& t : crowns) area.push_back(0.5f * glm::length(glm::cross(t.p[1] - t.p[0], t.p[2] - t.p[0])));
  float total = 0.f;
  std::vector<float> cdf;
  for (float a : area) cdf.push_back(total += a);
  auto samplePoint = [&](glm::vec3& p, glm::vec3& n) {
    const auto it = std::lower_bound(cdf.begin(), cdf.end(), unit(rng) * total);
    const Triangle& t = crowns[static_cast<std::size_t>(
        std::min<std::ptrdiff_t>(it - cdf.begin(), static_cast<std::ptrdiff_t>(cdf.size()) - 1))];
    float u = unit(rng), v = unit(rng);
    if (u + v > 1.f) {
      u = 1.f - u;
      v = 1.f - v;
    }
    const float w = 1.f - u - v;
    p = t.p[0] * w + t.p[1] * u + t.p[2] * v;
    n = glm::normalize(t.n[0] * w + t.n[1] * u + t.n[2] * v);
  };
  auto randomTilt = [&]() { return glm::vec3{unit(rng) - 0.5f, unit(rng) - 0.5f, unit(rng) - 0.5f}; };

  struct Clump {
    glm::vec3 centre;
    glm::vec3 tilt;
  };
  std::vector<Clump> clumps;
  const int clumpCount = cdf.empty() ? 0 : static_cast<int>(total * 0.7f);
  for (int i = 0; i < clumpCount; ++i) {
    glm::vec3 p, n;
    samplePoint(p, n);
    clumps.push_back({p, randomTilt()});
  }
  // Bucket clumps on a 1 m grid so each leaf only checks its neighbours.
  const float kClumpRadius = 0.55f;
  std::unordered_map<std::int64_t, std::vector<int>> clumpGrid;
  auto cellKey = [](glm::vec3 p) {
    const auto ix = static_cast<std::int64_t>(std::floor(p.x)) & 0xfffff;
    const auto iy = static_cast<std::int64_t>(std::floor(p.y)) & 0xfffff;
    const auto iz = static_cast<std::int64_t>(std::floor(p.z)) & 0xfffff;
    return (ix << 40) | (iy << 20) | iz;
  };
  for (int i = 0; i < static_cast<int>(clumps.size()); ++i) clumpGrid[cellKey(clumps[static_cast<std::size_t>(i)].centre)].push_back(i);

  const int candidates = cdf.empty() ? 0 : static_cast<int>(total * density * 1.6f);
  int leafCount = 0;
  for (int i = 0; i < candidates; ++i) {
    glm::vec3 p, n;
    samplePoint(p, n);
    // Nearest clump within reach decides whether the leaf grows here.
    float best = 1e9f;
    const Clump* near = nullptr;
    for (int dz = -1; dz <= 1; ++dz)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          const auto found = clumpGrid.find(cellKey(p + glm::vec3(dx, dy, dz)));
          if (found == clumpGrid.end()) continue;
          for (int ci : found->second) {
            const float d2 = glm::dot(clumps[static_cast<std::size_t>(ci)].centre - p, clumps[static_cast<std::size_t>(ci)].centre - p);
            if (d2 < best) {
              best = d2;
              near = &clumps[static_cast<std::size_t>(ci)];
            }
          }
        }
    const float keep = near ? std::exp(-best / (2.f * kClumpRadius * kClumpRadius)) : 0.f;
    if (unit(rng) > 0.12f + 0.88f * keep) continue;
    const glm::vec3 clumpTilt = near ? near->tilt : glm::vec3{0.f};
    const glm::vec3 tilt = glm::normalize(n + 0.7f * clumpTilt + 0.8f * randomTilt());
    // Leaves near a clump's edge stand out from the surface a little.
    const glm::vec3 root = p + n * (unit(rng) < 0.3f ? 0.35f * unit(rng) : 0.f);
    // About one leaf in six takes the lighter detail shade, so even the
    // shadowed side of a crown shows separate leaves.
    const float kind = unit(rng) < 0.17f ? 3.f : 2.f;
    scene.grass.push_back({root.x, root.y, root.z, tilt.x, tilt.y, tilt.z, kind, unit(rng),
                           0.85f + 0.3f * unit(rng), static_cast<float>(kSurfLeaf)});
    ++leafCount;
  }
  return leafCount;
}

}  // namespace pxe
