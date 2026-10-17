#include "scene.hpp"

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace game {
namespace {

constexpr int kGrid = 36;
constexpr float kStep = 0.5f;

enum Cell : std::uint8_t { kCellGrass, kCellDirt, kCellStone, kCellWater };

struct Builder {
  std::vector<pxe::Vertex> verts;
  std::vector<std::uint32_t> idx;

  void tri(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color, float surface) {
    glm::vec3 n = glm::cross(b - a, c - a);
    const float len = glm::length(n);
    if (len < 1e-6f) return;
    n /= len;
    const auto base = static_cast<std::uint32_t>(verts.size());
    auto push = [&](glm::vec3 p) {
      verts.push_back(pxe::Vertex{p.x, p.y, p.z, n.x, n.y, n.z, color.r, color.g, color.b, surface});
    };
    push(a);
    push(b);
    push(c);
    idx.push_back(base);
    idx.push_back(base + 1);
    idx.push_back(base + 2);
  }

  // CCW when looking along cross(b-a, d-a) is not assumed; the normal is
  // taken from the winding a-b-c / a-c-d and flipped to face `outward` if given.
  void quad(glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color, float surface,
            glm::vec3 outward = glm::vec3{0.f}) {
    glm::vec3 n = glm::cross(b - a, d - a);
    if (glm::dot(n, outward) < 0.f && glm::length(outward) > 0.f) {
      tri(a, d, c, color, surface);
      tri(a, c, b, color, surface);
    } else {
      tri(a, b, c, color, surface);
      tri(a, c, d, color, surface);
    }
  }

  void box(glm::vec3 mn, glm::vec3 mx, glm::vec3 color, float surface) {
    const glm::vec3 c = (mn + mx) * 0.5f;
    const glm::vec3 h = (mx - mn) * 0.5f;
    auto face = [&](glm::vec3 center, glm::vec3 u, glm::vec3 v) {
      quad(center - u - v, center + u - v, center + u + v, center - u + v, color, surface,
           glm::cross(u, v));
    };
    face(c + glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0), glm::vec3(0, 0, -h.z));  // +Y
    face(c - glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0), glm::vec3(0, 0, h.z));   // -Y
    face(c + glm::vec3(h.x, 0, 0), glm::vec3(0, h.y, 0), glm::vec3(0, 0, h.z));   // +X
    face(c - glm::vec3(h.x, 0, 0), glm::vec3(0, 0, h.z), glm::vec3(0, h.y, 0));   // -X
    face(c + glm::vec3(0, 0, h.z), glm::vec3(h.x, 0, 0), glm::vec3(0, h.y, 0));   // +Z
    face(c - glm::vec3(0, 0, h.z), glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0));   // -Z
  }
};

float hash01(int x, int z, int salt) {
  std::uint32_t n = static_cast<std::uint32_t>(x) * 374761393u +
                    static_cast<std::uint32_t>(z) * 668265263u +
                    static_cast<std::uint32_t>(salt) * 1442695041u;
  n = (n ^ (n >> 13)) * 1274126177u;
  return static_cast<float>(n & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

glm::vec3 grassTint(int x, int z) {
  const float h = hash01(x, z, 3);
  if (h < 0.33f) return {0.45f, 0.56f, 0.32f};
  if (h < 0.66f) return {0.53f, 0.63f, 0.38f};
  return {0.40f, 0.50f, 0.30f};
}

const glm::vec3 kDirt{0.54f, 0.45f, 0.34f};
const glm::vec3 kBed{0.36f, 0.40f, 0.34f};
const glm::vec3 kStones[] = {
    {0.54f, 0.50f, 0.53f},
    {0.43f, 0.45f, 0.49f},
    {0.48f, 0.46f, 0.43f},
    {0.36f, 0.37f, 0.42f},
};
const glm::vec3 kBark{0.50f, 0.28f, 0.22f};
const glm::vec3 kLeaves[] = {
    {0.47f, 0.64f, 0.28f},
    {0.36f, 0.52f, 0.22f},
    {0.58f, 0.72f, 0.36f},
    {0.42f, 0.58f, 0.26f},
};

struct World {
  int steps[kGrid * kGrid]{};
  Cell cells[kGrid * kGrid]{};
  bool blocked[kGrid * kGrid]{};

  int at(int x, int z) const { return steps[z * kGrid + x]; }
  void set(int x, int z, int s) { steps[z * kGrid + x] = s; }
  float y(int x, int z) const { return static_cast<float>(at(x, z)) * kStep; }
};

bool inside(int x, int z) { return x >= 0 && z >= 0 && x < kGrid && z < kGrid; }

void stampDisc(World& w, int cx, int cz, float r, int s) {
  const int ri = static_cast<int>(std::ceil(r));
  for (int z = cz - ri; z <= cz + ri; ++z) {
    for (int x = cx - ri; x <= cx + ri; ++x) {
      if (!inside(x, z)) continue;
      const float dx = static_cast<float>(x - cx) + 0.5f;
      const float dz = static_cast<float>(z - cz) + 0.5f;
      if (dx * dx + dz * dz <= r * r) w.set(x, z, s);
    }
  }
}

void addCylinder(Builder& b, glm::vec3 base, float r0, float r1, float height, int sides,
                 glm::vec3 color, float surface) {
  constexpr float kTau = 6.28318530718f;
  for (int i = 0; i < sides; ++i) {
    const float a0 = kTau * static_cast<float>(i) / static_cast<float>(sides);
    const float a1 = kTau * static_cast<float>(i + 1) / static_cast<float>(sides);
    const glm::vec3 d0{std::cos(a0), 0.f, std::sin(a0)};
    const glm::vec3 d1{std::cos(a1), 0.f, std::sin(a1)};
    const glm::vec3 bl = base + d0 * r0;
    const glm::vec3 br = base + d1 * r0;
    const glm::vec3 tl = base + d0 * r1 + glm::vec3{0.f, height, 0.f};
    const glm::vec3 tr = base + d1 * r1 + glm::vec3{0.f, height, 0.f};
    b.quad(bl, br, tr, tl, color, surface, d0 + d1);
  }
}

void addFacetSphere(Builder& b, glm::vec3 center, float radius, glm::vec3 color, float surface,
                    int seed, float jag) {
  constexpr int kStacks = 5;
  constexpr int kSlices = 7;
  constexpr float kPi = 3.1415926535f;
  constexpr float kTau = 6.28318530718f;
  auto point = [&](int st, int sl) {
    const float v = static_cast<float>(st) / static_cast<float>(kStacks);
    const float u = static_cast<float>(sl % kSlices) / static_cast<float>(kSlices);
    const float phi = v * kPi;
    const float th = u * kTau;
    glm::vec3 d{std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th)};
    const float warp = 1.f - jag * 0.5f + jag * hash01(st + seed, sl, seed);
    return center + d * radius * warp;
  };
  for (int st = 0; st < kStacks; ++st) {
    for (int sl = 0; sl < kSlices; ++sl) {
      glm::vec3 a = point(st, sl);
      glm::vec3 btm = point(st, sl + 1);
      glm::vec3 c = point(st + 1, sl + 1);
      glm::vec3 d = point(st + 1, sl);
      const glm::vec3 mid = (a + btm + c + d) * 0.25f;
      b.quad(a, btm, c, d, color, surface, mid - center);
    }
  }
}

void addTree(Builder& solid, Builder& shadow, std::vector<pxe::FoliageInstance>& foliage,
             glm::vec3 base, float scale, int seed) {
  const float trunkH = 2.5f * scale;
  addCylinder(solid, base, 0.22f * scale, 0.11f * scale, trunkH, 6, kBark, pxe::kSurfBark);
  addCylinder(shadow, base, 0.22f * scale, 0.11f * scale, trunkH, 6, kBark, pxe::kSurfBark);

  const glm::vec3 crown = base + glm::vec3{0.f, trunkH + 0.85f * scale, 0.f};
  const float crownR = 1.75f * scale;
  // Jagged sphere: shadow only. Leaves are drawn over it and lit from its normal.
  addFacetSphere(shadow, crown, crownR * 0.92f, kLeaves[0], pxe::kSurfLeaf, seed, 0.35f);

  constexpr int kLeafCount = 84;
  constexpr float kGolden = 2.39996323f;
  for (int i = 0; i < kLeafCount; ++i) {
    const float y = 1.f - (static_cast<float>(i) / static_cast<float>(kLeafCount - 1)) * 2.f;
    const float ring = std::sqrt(std::max(0.f, 1.f - y * y));
    const float th = static_cast<float>(i) * kGolden;
    glm::vec3 dir{std::cos(th) * ring, y * 0.85f + 0.08f, std::sin(th) * ring};
    dir = glm::normalize(dir);
    const float radial = crownR * (0.55f + 0.5f * hash01(i, seed, 9));
    pxe::FoliageInstance leaf{};
    const glm::vec3 p = crown + dir * radial;
    leaf.px = p.x;
    leaf.py = p.y;
    leaf.pz = p.z;
    leaf.nx = dir.x;
    leaf.ny = dir.y;
    leaf.nz = dir.z;
    const glm::vec3 tint = kLeaves[i % 4];
    leaf.cr = tint.r;
    leaf.cg = tint.g;
    leaf.cb = tint.b;
    leaf.phase = hash01(i, seed, 2) * 6.28f;
    leaf.scale = scale * (0.85f + 0.4f * hash01(i, seed, 4));
    leaf.kind = 3.f;
    foliage.push_back(leaf);
  }
}

}  // namespace

pxe::Scene buildMeadow() {
  World world;
  for (int z = 0; z < kGrid; ++z) {
    for (int x = 0; x < kGrid; ++x) {
      const float fx = static_cast<float>(x);
      const float fz = static_cast<float>(z);
      const float n = std::sin(fx * 0.18f) * 1.15f + std::cos(fz * 0.15f) * 0.9f +
                      std::sin((fx + fz) * 0.07f) * 0.55f;
      int s = static_cast<int>(std::floor(n * 0.85f + 2.1f));
      s = std::clamp(s, 0, 4);
      world.set(x, z, s);
      world.cells[z * kGrid + x] = kCellGrass;
    }
  }

  // Courtyard terrace and a pond cut into the meadow.
  for (int z = 9; z <= 21; ++z) {
    for (int x = 13; x <= 26; ++x) world.set(x, z, 2);
  }
  stampDisc(world, 9, 27, 4.6f, 0);
  for (int z = 0; z < kGrid; ++z) {
    for (int x = 0; x < kGrid; ++x) {
      const float dx = static_cast<float>(x) + 0.5f - 9.f;
      const float dz = static_cast<float>(z) + 0.5f - 27.f;
      const float d = std::sqrt(dx * dx + dz * dz);
      if (d < 4.6f) {
        world.cells[z * kGrid + x] = kCellWater;
        world.set(x, z, d > 3.1f ? 0 : -1);
      }
    }
  }
  for (int z = 11; z <= 19; ++z) {
    for (int x = 15; x <= 24; ++x) {
      if (world.cells[z * kGrid + x] != kCellWater) world.cells[z * kGrid + x] = kCellStone;
    }
  }

  // A dirt track climbing toward the courtyard.
  for (int z = 22; z < 35; ++z) {
    const int x = 18 + static_cast<int>(std::sin(static_cast<float>(z) * 0.35f) * 1.2f);
    for (int dx = -1; dx <= 1; ++dx) {
      if (!inside(x + dx, z)) continue;
      if (world.cells[z * kGrid + (x + dx)] == kCellWater) continue;
      world.cells[z * kGrid + (x + dx)] = kCellDirt;
    }
  }
  for (int x = 16; x <= 22; ++x) {
    if (world.cells[12 * kGrid + x] != kCellWater) world.cells[12 * kGrid + x] = kCellDirt;
    if (world.cells[20 * kGrid + x] != kCellWater) world.cells[20 * kGrid + x] = kCellDirt;
  }

  Builder solid;
  Builder shadow;
  Builder water;
  std::vector<pxe::FoliageInstance> foliage;

  auto cellColor = [&](int x, int z) -> glm::vec3 {
    switch (world.cells[z * kGrid + x]) {
      case kCellDirt:
        return kDirt;
      case kCellStone:
        return kStones[(x * 3 + z) & 3];
      case kCellWater:
        return kBed;
      default:
        return grassTint(x, z);
    }
  };
  auto cellSurface = [&](int x, int z) -> float {
    switch (world.cells[z * kGrid + x]) {
      case kCellDirt:
        return pxe::kSurfDirt;
      case kCellStone:
        return pxe::kSurfStone;
      default:
        return pxe::kSurfGround;
    }
  };

  for (int z = 0; z < kGrid; ++z) {
    for (int x = 0; x < kGrid; ++x) {
      const float y = world.y(x, z);
      const float x0 = static_cast<float>(x);
      const float z0 = static_cast<float>(z);
      const glm::vec3 color = cellColor(x, z);
      const float surface = cellSurface(x, z);
      solid.quad({x0, y, z0 + 1}, {x0 + 1, y, z0 + 1}, {x0 + 1, y, z0}, {x0, y, z0}, color, surface,
                 {0, 1, 0});
      shadow.quad({x0, y, z0 + 1}, {x0 + 1, y, z0 + 1}, {x0 + 1, y, z0}, {x0, y, z0}, color, surface,
                  {0, 1, 0});

      auto wall = [&](int nx, int nz, glm::vec3 a, glm::vec3 b, glm::vec3 outward) {
        const float ny = inside(nx, nz) ? world.y(nx, nz) : -0.75f;
        if (ny >= y - 0.01f) return;
        solid.quad(a, b, b + glm::vec3{0, ny - y, 0}, a + glm::vec3{0, ny - y, 0}, color, surface,
                   outward);
        shadow.quad(a, b, b + glm::vec3{0, ny - y, 0}, a + glm::vec3{0, ny - y, 0}, color, surface,
                    outward);
      };
      wall(x + 1, z, {x0 + 1, y, z0}, {x0 + 1, y, z0 + 1}, {1, 0, 0});
      wall(x - 1, z, {x0, y, z0 + 1}, {x0, y, z0}, {-1, 0, 0});
      wall(x, z + 1, {x0 + 1, y, z0 + 1}, {x0, y, z0 + 1}, {0, 0, 1});
      wall(x, z - 1, {x0, y, z0}, {x0 + 1.f, y, z0}, {0, 0, -1});

      if (world.cells[z * kGrid + x] == kCellWater) {
        const float wy = 0.40f;
        water.quad({x0, wy, z0 + 1}, {x0 + 1, wy, z0 + 1}, {x0 + 1, wy, z0}, {x0, wy, z0},
                   {0.4f, 0.6f, 0.55f}, pxe::kSurfGround, {0, 1, 0});
      }
    }
  }

  auto stoneColor = [](int salt) { return kStones[salt & 3]; };

  auto ruin = [&](int x, int z, float h, float depth, int salt) {
    if (!inside(x, z)) return;
    const float y = world.y(x, z);
    const float x0 = static_cast<float>(x);
    const float z0 = static_cast<float>(z);
    const glm::vec3 color = stoneColor(salt);
    solid.box({x0 + 0.04f, y, z0 + 0.06f}, {x0 + 0.96f, y + h, z0 + depth}, color, pxe::kSurfStone);
    shadow.box({x0 + 0.04f, y, z0 + 0.06f}, {x0 + 0.96f, y + h, z0 + depth}, color, pxe::kSurfStone);
    world.blocked[z * kGrid + x] = true;
  };

  // Broken cloister. Gaps and uneven courses keep it from reading as a kit wall.
  for (int x = 15; x <= 25; ++x) {
    if (((x * 5) % 7) == 0) continue;
    const float h = 0.7f + hash01(x, 9, 1) * 1.7f;
    ruin(x, 10, h, 0.78f, x);
  }
  for (int z = 11; z <= 20; ++z) {
    if (((z * 3) % 5) == 0) continue;
    const float h = 0.6f + hash01(14, z, 2) * 1.9f;
    ruin(14, z, h, 0.82f, z + 4);
  }
  for (int x = 18; x <= 24; x += 2) {
    if (hash01(x, 20, 6) > 0.55f) continue;
    ruin(x, 20, 0.45f + hash01(x, 21, 3) * 1.1f, 0.7f, x + 8);
  }
  // Corner pier and scattered rubble.
  ruin(14, 10, 2.4f, 0.9f, 1);
  ruin(25, 10, 1.6f, 0.7f, 2);
  solid.box({19.2f, world.y(19, 15), 15.1f}, {21.4f, world.y(19, 15) + 0.55f, 16.6f},
            stoneColor(5), pxe::kSurfStone);
  shadow.box({19.2f, world.y(19, 15), 15.1f}, {21.4f, world.y(19, 15) + 0.55f, 16.6f},
             stoneColor(5), pxe::kSurfStone);
  solid.box({21.6f, world.y(22, 16), 16.4f}, {23.1f, world.y(22, 16) + 0.85f, 17.5f},
            stoneColor(6), pxe::kSurfStone);
  shadow.box({21.6f, world.y(22, 16), 16.4f}, {23.1f, world.y(22, 16) + 0.85f, 17.5f},
             stoneColor(6), pxe::kSurfStone);
  solid.box({16.3f, world.y(16, 14), 13.4f}, {17.6f, world.y(16, 14) + 0.42f, 15.3f},
            stoneColor(7), pxe::kSurfStone);
  shadow.box({16.3f, world.y(16, 14), 13.4f}, {17.6f, world.y(16, 14) + 0.42f, 15.3f},
             stoneColor(7), pxe::kSurfStone);

  auto rock = [&](float x, float z, float r, int seed) {
    const int ix = std::clamp(static_cast<int>(x), 0, kGrid - 1);
    const int iz = std::clamp(static_cast<int>(z), 0, kGrid - 1);
    const glm::vec3 c{x, world.y(ix, iz) + r * 0.45f, z};
    const glm::vec3 color = stoneColor(seed);
    addFacetSphere(solid, c, r, color, pxe::kSurfStone, seed, 0.55f);
    addFacetSphere(shadow, c, r, color, pxe::kSurfStone, seed, 0.55f);
  };
  rock(11.5f, 18.f, 0.7f, 11);
  rock(12.4f, 19.2f, 0.45f, 12);
  rock(28.2f, 14.f, 0.85f, 13);
  rock(27.2f, 15.1f, 0.4f, 14);
  rock(8.f, 12.f, 0.6f, 15);
  rock(30.5f, 24.f, 0.75f, 16);
  rock(22.f, 8.f, 0.55f, 17);
  rock(6.5f, 22.f, 0.5f, 18);

  struct TreeSpec {
    float x, z, scale;
    int seed;
  };
  const TreeSpec trees[] = {
      {6.5f, 8.f, 1.15f, 1},  {10.f, 15.f, 0.95f, 2}, {30.f, 8.5f, 1.25f, 3},
      {32.5f, 16.f, 1.05f, 4}, {31.f, 26.f, 1.2f, 5},  {16.f, 31.f, 1.1f, 6},
      {24.f, 30.f, 0.9f, 7},  {5.f, 30.f, 1.0f, 8},
  };
  for (const TreeSpec& t : trees) {
    const int ix = std::clamp(static_cast<int>(t.x), 0, kGrid - 1);
    const int iz = std::clamp(static_cast<int>(t.z), 0, kGrid - 1);
    if (world.cells[iz * kGrid + ix] == kCellWater) continue;
    addTree(solid, shadow, foliage, {t.x, world.y(ix, iz), t.z}, t.scale, t.seed);
    world.blocked[iz * kGrid + ix] = true;
  }

  for (int z = 0; z < kGrid; ++z) {
    for (int x = 0; x < kGrid; ++x) {
      const Cell cell = world.cells[z * kGrid + x];
      if (cell == kCellWater || cell == kCellDirt) continue;
      if (world.blocked[z * kGrid + x]) continue;
      // Steep cliffs stay bare; tufts belong on the terraces.
      bool steep = false;
      const int h = world.at(x, z);
      const int nbs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto nb : nbs) {
        const int nx = x + nb[0];
        const int nz = z + nb[1];
        if (inside(nx, nz) && std::abs(world.at(nx, nz) - h) > 1) steep = true;
      }
      if (steep) continue;

      const int tufts = cell == kCellStone ? 1 : 3;
      const float oxs[] = {0.22f, 0.58f, 0.80f};
      const float ozs[] = {0.30f, 0.74f, 0.42f};
      for (int i = 0; i < tufts; ++i) {
        if (cell == kCellStone && hash01(x, z, 20 + i) > 0.35f) continue;
        const float ox = oxs[i];
        const float oz = ozs[i];
        const float jx = (hash01(x, z, 30 + i) - 0.5f) * 0.18f;
        const float jz = (hash01(x, z, 40 + i) - 0.5f) * 0.18f;
        const glm::vec3 tint = grassTint(x, z);
        const float roll = hash01(x, z, 50 + i);
        pxe::FoliageInstance g{};
        g.px = static_cast<float>(x) + ox + jx;
        g.py = world.y(x, z);
        g.pz = static_cast<float>(z) + oz + jz;
        g.nx = 0.f;
        g.ny = 1.f;
        g.nz = 0.f;
        g.phase = hash01(x, z, 60 + i) * 6.28f;
        g.scale = 0.85f + hash01(x, z, 70 + i) * 0.55f;
        if (roll > 0.93f) {
          g.kind = 1.f;
          g.cr = 0.58f;
          g.cg = 0.40f;
          g.cb = 0.70f;
          g.scale *= 0.85f;
        } else if (roll > 0.86f) {
          g.kind = 2.f;
          g.cr = 0.80f;
          g.cg = 0.72f;
          g.cb = 0.30f;
          g.scale *= 0.7f;
        } else {
          g.kind = 0.f;
          g.cr = tint.r;
          g.cg = tint.g;
          g.cb = tint.b;
        }
        foliage.push_back(g);
      }
    }
  }

  pxe::Scene scene;
  scene.opaque.upload(solid.verts, solid.idx);
  scene.shadowCasters.upload(shadow.verts, shadow.idx);
  scene.water.upload(water.verts, water.idx);
  scene.foliage = std::move(foliage);
  scene.sunDirection = glm::normalize(glm::vec3{-0.42f, 0.78f, -0.46f});
  scene.shadowCenter = {18.f, 2.f, 18.f};
  scene.shadowExtent = 26.f;
  scene.focus = {17.5f, 1.0f, 15.0f};
  return scene;
}

}  // namespace game
