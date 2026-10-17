#include "scene.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
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
const glm::vec3 kPillar{0.67f, 0.62f, 0.54f};
const glm::vec3 kStep{0.72f, 0.63f, 0.48f};
const glm::vec3 kSlab{0.60f, 0.56f, 0.50f};
const glm::vec3 kRock{0.56f, 0.53f, 0.49f};
const glm::vec3 kBark{0.50f, 0.37f, 0.28f};
const glm::vec3 kLeaf{0.50f, 0.66f, 0.27f};
const glm::vec3 kLeafDeep{0.42f, 0.56f, 0.22f};
const glm::vec3 kBush{0.44f, 0.60f, 0.24f};

// Ground axes of the default 45° camera, so the ruin sits in frame on launch.
const glm::vec3 kFore{-0.70710678f, 0.f, 0.70710678f};   // toward the camera
const glm::vec3 kAcross{0.70710678f, 0.f, 0.70710678f};  // screen right

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

  void box(glm::vec3 mn, glm::vec3 mx, glm::vec3 color, float surface) {
    const glm::vec3 c = (mn + mx) * 0.5f;
    const glm::vec3 h = (mx - mn) * 0.5f;
    auto side = [&](glm::vec3 center, glm::vec3 u, glm::vec3 v) {
      quad(center - u - v, center + u - v, center + u + v, center - u + v, color, surface,
           glm::cross(u, v));
    };
    side(c + glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0), glm::vec3(0, 0, -h.z));
    side(c - glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0), glm::vec3(0, 0, h.z));
    side(c + glm::vec3(h.x, 0, 0), glm::vec3(0, h.y, 0), glm::vec3(0, 0, h.z));
    side(c - glm::vec3(h.x, 0, 0), glm::vec3(0, 0, h.z), glm::vec3(0, h.y, 0));
    side(c + glm::vec3(0, 0, h.z), glm::vec3(h.x, 0, 0), glm::vec3(0, h.y, 0));
    side(c - glm::vec3(0, 0, h.z), glm::vec3(0, h.y, 0), glm::vec3(h.x, 0, 0));
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
      float y = 0.62f;
      y += 0.42f * std::sin(wx * 0.045f) + 0.30f * std::cos(wz * 0.040f);
      y += 0.18f * std::sin((wx + wz) * 0.027f);
      y += hill(wx, wz, 0.4f, -0.2f, 7.4f, 1.15f);
      y += hill(wx, wz, -22.f, 16.f, 18.f, 0.85f);
      y += hill(wx, wz, 26.f, -20.f, 22.f, 1.05f);
      y += hill(wx, wz, -34.f, -28.f, 24.f, 0.7f);
      y += hill(wx, wz, 18.f, 24.f, 16.f, 0.55f);

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

void addCylinder(MeshBuild& m, glm::vec3 base, glm::vec3 axis, float r0, float r1, int sides,
                 glm::vec3 color, float surface) {
  const float height = glm::length(axis);
  if (height < 1e-4f || sides < 3) return;
  const glm::vec3 up = axis / height;
  const glm::vec3 helper = std::fabs(up.y) < 0.92f ? glm::vec3{0.f, 1.f, 0.f} : glm::vec3{1.f, 0.f, 0.f};
  const glm::vec3 side = glm::normalize(glm::cross(up, helper));
  const glm::vec3 front = glm::cross(side, up);
  for (int i = 0; i < sides; ++i) {
    const float a0 = kTau * static_cast<float>(i) / static_cast<float>(sides);
    const float a1 = kTau * static_cast<float>(i + 1) / static_cast<float>(sides);
    const glm::vec3 d0 = side * std::cos(a0) + front * std::sin(a0);
    const glm::vec3 d1 = side * std::cos(a1) + front * std::sin(a1);
    const glm::vec3 bl = base + d0 * r0;
    const glm::vec3 br = base + d1 * r0;
    const glm::vec3 tl = base + axis + d0 * r1;
    const glm::vec3 tr = base + axis + d1 * r1;
    m.quad(bl, br, tr, tl, color, surface, d0 + d1);
  }
}

void addFacetRock(MeshBuild& m, glm::vec3 center, float radius, glm::vec3 color, int seed, float jag) {
  constexpr int kStacks = 4;
  constexpr int kSlices = 6;
  auto point = [&](int st, int sl) {
    const float v = static_cast<float>(st) / static_cast<float>(kStacks);
    const float u = static_cast<float>(sl % kSlices) / static_cast<float>(kSlices);
    const float phi = v * kPi;
    const float th = u * kTau;
    glm::vec3 d{std::sin(phi) * std::cos(th), std::cos(phi), std::sin(phi) * std::sin(th)};
    const float warp = 1.f - jag * 0.45f + jag * hash01(st + seed * 3, sl, seed);
    return center + d * radius * warp;
  };
  for (int st = 0; st < kStacks; ++st) {
    for (int sl = 0; sl < kSlices; ++sl) {
      glm::vec3 a = point(st, sl);
      glm::vec3 b = point(st, sl + 1);
      glm::vec3 c = point(st + 1, sl + 1);
      glm::vec3 d = point(st + 1, sl);
      const glm::vec3 mid = (a + b + c + d) * 0.25f;
      m.quad(a, b, c, d, color, pxe::kSurfStone, mid - center);
    }
  }
}

struct EdgeKey {
  int a;
  int b;
  bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
};
struct EdgeHash {
  std::size_t operator()(const EdgeKey& k) const {
    return static_cast<std::size_t>(k.a) * 73856093u ^ static_cast<std::size_t>(k.b) * 19349663u;
  }
};

// Displaced icosphere. Normals come from the smooth pre-displacement shell
// around shadeCenter, so the whole canopy lights as one blob.
void addCanopy(MeshBuild& m, glm::vec3 center, float radius, glm::vec3 shadeCenter, glm::vec3 color,
               int seed, float jag, int subdiv) {
  const float g = 1.6180339887f;
  std::vector<glm::vec3> v = {
      {-1, g, 0}, {1, g, 0}, {-1, -g, 0}, {1, -g, 0}, {0, -1, g}, {0, 1, g},
      {0, -1, -g}, {0, 1, -g}, {g, 0, -1}, {g, 0, 1}, {-g, 0, -1}, {-g, 0, 1},
  };
  for (glm::vec3& p : v) p = glm::normalize(p);
  std::vector<glm::ivec3> f = {
      {0, 11, 5}, {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
      {11, 10, 2}, {10, 7, 6}, {7, 1, 8}, {3, 9, 4}, {3, 4, 2},   {3, 2, 6},  {3, 6, 8},
      {3, 8, 9},  {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},  {9, 8, 1},
  };
  for (int s = 0; s < subdiv; ++s) {
    std::unordered_map<EdgeKey, int, EdgeHash> mid;
    mid.reserve(f.size() * 2);
    auto midpoint = [&](int a, int b) {
      if (a > b) std::swap(a, b);
      const EdgeKey key{a, b};
      const auto it = mid.find(key);
      if (it != mid.end()) return it->second;
      const int id = static_cast<int>(v.size());
      v.push_back(glm::normalize(v[static_cast<std::size_t>(a)] + v[static_cast<std::size_t>(b)]));
      mid.emplace(key, id);
      return id;
    };
    std::vector<glm::ivec3> next;
    next.reserve(f.size() * 4);
    for (glm::ivec3 t : f) {
      const int ab = midpoint(t.x, t.y);
      const int bc = midpoint(t.y, t.z);
      const int ca = midpoint(t.z, t.x);
      next.push_back({t.x, ab, ca});
      next.push_back({t.y, bc, ab});
      next.push_back({t.z, ca, bc});
      next.push_back({ab, bc, ca});
    }
    f.swap(next);
  }

  auto place = [&](int index, glm::vec3& pos, glm::vec3& nrm) {
    const glm::vec3 dir = v[static_cast<std::size_t>(index)];
    const float warp = 1.f + jag * (hash01(index, seed, seed * 17 + 3) - 0.5f);
    const glm::vec3 smoothP = center + dir * radius;
    pos = center + dir * radius * warp;
    nrm = glm::normalize(smoothP - shadeCenter);
  };

  for (glm::ivec3 t : f) {
    glm::vec3 p0, p1, p2, n0, n1, n2;
    place(t.x, p0, n0);
    place(t.y, p1, n1);
    place(t.z, p2, n2);
    const glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
    const glm::vec3 outward = (p0 + p1 + p2) / 3.f - center;
    if (glm::dot(fn, outward) < 0.f) {
      m.tri(p0, n0, p2, n2, p1, n1, color, pxe::kSurfLeaf);
    } else {
      m.tri(p0, n0, p1, n1, p2, n2, color, pxe::kSurfLeaf);
    }
  }
}

void addPillar(MeshBuild& m, glm::vec3 base, float shaft, float width, float chip) {
  float y = base.y;
  const float w = width;
  m.box({base.x - w * 0.78f, y, base.z - w * 0.78f},
        {base.x + w * 0.78f, y + 0.22f, base.z + w * 0.78f}, kPillar, pxe::kSurfStone);
  y += 0.22f;
  m.box({base.x - w * 0.50f, y, base.z - w * 0.50f},
        {base.x + w * 0.50f, y + shaft, base.z + w * 0.50f}, kPillar, pxe::kSurfStone);
  y += shaft;
  m.box({base.x - w * 0.70f, y, base.z - w * 0.70f},
        {base.x + w * 0.70f - chip, y + 0.26f, base.z + w * 0.64f}, kPillar, pxe::kSurfStone);
}

struct Disk {
  float x, z, r;
};

}  // namespace

pxe::Scene buildMeadow() {
  Height hm = makeHeight();

  // Cut the ground just under the stair treads so the steps read as a flight
  // set into the knoll, not a row of floating boxes.
  const glm::vec3 stairTop{0.15f, 0.f, 0.55f};
  const glm::vec3 stairBot = stairTop + kFore * 6.6f;
  const float yTop = hm.sample(stairTop.x, stairTop.z) + 0.02f;
  const float yBot = hm.sample(stairBot.x, stairBot.z) + 0.04f;
  for (int z = 0; z < Height::N; ++z) {
    for (int x = 0; x < Height::N; ++x) {
      const float wx = Height::kOrigin + static_cast<float>(x);
      const float wz = Height::kOrigin + static_cast<float>(z);
      const glm::vec2 p{wx, wz};
      const glm::vec2 a{stairTop.x, stairTop.z};
      const glm::vec2 b{stairBot.x, stairBot.z};
      const glm::vec2 ab = b - a;
      const float len2 = glm::dot(ab, ab);
      float t = glm::dot(p - a, ab) / len2;
      if (t < -0.05f || t > 1.05f) continue;
      const float across = glm::length(p - (a + ab * std::clamp(t, 0.f, 1.f)));
      if (across > 1.35f) continue;
      t = std::clamp(t, 0.f, 1.f);
      const float tread = glm::mix(yTop, yBot, t) - 0.08f;
      const float edge = smoothFalloff(across, 1.35f);
      hm.set(x, z, glm::mix(hm.at(x, z), std::min(hm.at(x, z), tread), edge));
    }
  }

  MeshBuild solid;
  MeshBuild shadow;
  MeshBuild water;
  std::vector<pxe::FoliageInstance> foliage;
  std::vector<Disk> blocked;
  blocked.push_back({kPond.x, kPond.y, kPondR + 0.3f});

  auto prop = [&](auto&& fn) {
    fn(solid);
    fn(shadow);
  };

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

  // Two pillars on the knoll, a short broken one in the foreground.
  const glm::vec3 pillarA{-0.35f, ground(-0.35f, 0.15f), 0.15f};
  const glm::vec3 pillarB{1.25f, ground(1.25f, -1.05f), -1.05f};
  const glm::vec3 pillarC = kFore * 3.4f + glm::vec3{-0.4f, 0.f, 0.2f};
  prop([&](MeshBuild& m) { addPillar(m, {pillarA.x, pillarA.y, pillarA.z}, 2.55f, 0.62f, 0.10f); });
  prop([&](MeshBuild& m) { addPillar(m, pillarB, 2.85f, 0.58f, 0.18f); });
  prop([&](MeshBuild& m) {
    addPillar(m, {pillarC.x, ground(pillarC.x, pillarC.z), pillarC.z}, 1.15f, 0.55f, 0.22f);
  });
  blocked.push_back({pillarA.x, pillarA.z, 0.9f});
  blocked.push_back({pillarB.x, pillarB.z, 0.9f});
  blocked.push_back({pillarC.x, pillarC.z, 0.8f});

  constexpr int kSteps = 8;
  for (int i = 0; i < kSteps; ++i) {
    const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(kSteps);
    const glm::vec3 p = glm::mix(stairTop, stairBot, t);
    const float y = glm::mix(yTop, yBot, t);
    const glm::vec3 side = kAcross;
    const float halfW = 0.72f;
    const float halfD = 0.40f;
    prop([&](MeshBuild& m) {
      m.box(p - side * halfW - kFore * halfD + glm::vec3{0, y - p.y - 0.18f, 0},
            p + side * halfW + kFore * halfD + glm::vec3{0, y - p.y, 0}, kStep, pxe::kSurfStone);
    });
    blocked.push_back({p.x, p.z, 0.95f});
  }

  auto slab = [&](glm::vec3 c, float sx, float sz, float thick) {
    const float y = ground(c.x, c.z);
    prop([&](MeshBuild& m) {
      m.box({c.x - sx, y, c.z - sz}, {c.x + sx, y + thick, c.z + sz}, kSlab, pxe::kSurfStone);
    });
    blocked.push_back({c.x, c.z, std::max(sx, sz) * 0.8f});
  };
  slab({-1.7f, 0.f, -1.35f}, 1.15f, 0.85f, 0.14f);
  slab({2.3f, 0.f, 0.9f}, 0.9f, 1.25f, 0.12f);
  slab({-0.2f, 0.f, -2.4f}, 0.7f, 0.55f, 0.16f);
  slab({3.4f, 0.f, -2.6f}, 1.3f, 0.7f, 0.10f);

  auto rock = [&](float x, float z, float r, int seed) {
    const glm::vec3 c{x, ground(x, z) + r * 0.42f, z};
    prop([&](MeshBuild& m) { addFacetRock(m, c, r, kRock, seed, 0.42f); });
    blocked.push_back({x, z, r * 0.8f});
  };
  rock(kPond.x - 1.7f, kPond.y + 1.15f, 0.62f, 21);
  rock(kPond.x - 1.05f, kPond.y + 1.7f, 0.36f, 22);
  rock(kPond.x + 1.55f, kPond.y - 0.9f, 0.7f, 23);
  rock(kPond.x - 0.3f, kPond.y - 1.35f, 0.42f, 25);
  rock(-3.6f, 3.4f, 0.55f, 26);
  rock(4.2f, 2.8f, 0.4f, 27);
  rock(-7.4f, -3.2f, 0.65f, 28);

  {
    const float x = kPond.x + 0.45f;
    const float z = kPond.y - 0.25f;
    prop([&](MeshBuild& m) { addFacetRock(m, {x, kWaterY + 0.02f, z}, 0.34f, kRock, 31, 0.35f); });
  }

  auto leaves = [&](glm::vec3 shadeCenter, glm::vec3 lobe, float radius, glm::vec3 color, int seed,
                    int count) {
    constexpr float kGolden = 2.39996323f;
    for (int i = 0; i < count; ++i) {
      const float y = 1.f - (static_cast<float>(i) + 0.5f) / static_cast<float>(count) * 2.f;
      const float ring = std::sqrt(std::max(0.f, 1.f - y * y));
      const float th = static_cast<float>(i) * kGolden + hash01(i, seed, 1);
      glm::vec3 dir{std::cos(th) * ring, y * 0.92f, std::sin(th) * ring};
      dir = glm::normalize(dir);
      const glm::vec3 p = lobe + dir * radius * 0.98f;
      const glm::vec3 n = glm::normalize(p - shadeCenter);
      pxe::FoliageInstance leaf{};
      leaf.px = p.x;
      leaf.py = p.y;
      leaf.pz = p.z;
      leaf.nx = n.x;
      leaf.ny = n.y;
      leaf.nz = n.z;
      leaf.cr = color.r;
      leaf.cg = color.g;
      leaf.cb = color.b;
      leaf.phase = hash01(i, seed, 2) * kTau;
      leaf.scale = 0.85f + hash01(i, seed, 4) * 0.5f;
      leaf.kind = 3.f;
      foliage.push_back(leaf);
    }
  };

  auto tree = [&](glm::vec3 base, glm::vec3 lean, float trunkH, float crownR, int seed, int lobes,
                  bool bush) {
    const glm::vec3 axis = glm::vec3{lean.x, trunkH, lean.z};
    const glm::vec3 top = base + axis;
    if (!bush) {
      prop([&](MeshBuild& m) {
        addCylinder(m, base, axis, 0.20f * std::max(crownR / 1.8f, 0.7f), 0.11f, 7, kBark,
                    pxe::kSurfBark);
      });
    }
    const glm::vec3 crown = top + glm::vec3{lean.x * 0.15f, crownR * 0.55f, lean.z * 0.15f};
    const glm::vec3 shade = crown;
    const int subdiv = 2;
    const float jag = 0.07f;
    prop([&](MeshBuild& m) {
      addCanopy(m, crown, crownR, shade, kLeaf, seed, jag, subdiv);
      if (lobes > 1) {
        const glm::vec3 o{crownR * 0.62f, crownR * 0.05f, crownR * 0.18f};
        addCanopy(m, crown + o, crownR * 0.62f, shade, kLeaf, seed + 5, jag, 1);
      }
      if (lobes > 2) {
        const glm::vec3 o{-crownR * 0.48f, -crownR * 0.02f, -crownR * 0.5f};
        addCanopy(m, crown + o, crownR * 0.55f, shade, kLeafDeep, seed + 9, jag, 1);
      }
    });
    leaves(shade, crown, crownR, kLeaf, seed, bush ? 6 : 10);
    blocked.push_back({base.x, base.z, std::max(0.6f, crownR * 0.45f)});
  };

  // Hero tree leaning over the pond. The canopy is the shadow caster.
  {
    const float x = 3.55f;
    const float z = -2.15f;
    const glm::vec3 base{x, ground(x, z), z};
    tree(base, {0.55f, 0.f, -0.7f}, 2.45f, 1.85f, 3, 3, false);
  }
  tree({-8.2f, ground(-8.2f, -2.4f), -2.4f}, {0.1f, 0.f, 0.05f}, 2.2f, 1.65f, 4, 3, false);
  tree({-3.4f, ground(-3.4f, -8.2f), -8.2f}, {-0.1f, 0.f, 0.15f}, 2.4f, 1.75f, 5, 3, false);
  tree({9.4f, ground(9.4f, 2.2f), 2.2f}, {0.f, 0.f, 0.1f}, 2.05f, 1.5f, 6, 2, false);
  tree({7.2f, ground(7.2f, 6.4f), 6.4f}, {0.15f, 0.f, -0.1f}, 1.7f, 1.25f, 7, 2, false);
  tree({-9.5f, ground(-9.5f, 5.8f), 5.8f}, {0.f, 0.f, 0.f}, 2.3f, 1.8f, 8, 3, false);

  auto bush = [&](float x, float z, float r, int seed) {
    const glm::vec3 base{x, ground(x, z), z};
    tree(base, {0.f, 0.f, 0.f}, 0.15f, r, seed, 2, true);
  };
  bush(-2.4f, 4.6f, 0.85f, 40);
  bush(-1.1f, 5.3f, 0.62f, 41);
  bush(1.8f, 4.2f, 0.7f, 42);
  bush(7.4f, -2.2f, 0.78f, 43);
  bush(3.6f, -5.4f, 0.66f, 44);
  bush(-6.8f, 1.8f, 0.9f, 45);

  auto occupied = [&](float x, float z) {
    for (const Disk& d : blocked) {
      const float dx = x - d.x;
      const float dz = z - d.z;
      if (dx * dx + dz * dz < d.r * d.r) return true;
    }
    return false;
  };

  for (int z = 2; z < Height::N - 2; ++z) {
    for (int x = 2; x < Height::N - 2; ++x) {
      if (hash01(x, z, 11) > 0.11f) continue;
      const float jx = (hash01(x, z, 12) - 0.5f) * 0.8f;
      const float jz = (hash01(x, z, 13) - 0.5f) * 0.8f;
      const float wx = Height::kOrigin + static_cast<float>(x) + 0.5f + jx;
      const float wz = Height::kOrigin + static_cast<float>(z) + 0.5f + jz;
      if (occupied(wx, wz)) continue;
      const float y = ground(wx, wz);
      if (y < kWaterY + 0.05f) continue;
      const float slope = hm.at(x + 1, z) - hm.at(x - 1, z);
      const float slopeZ = hm.at(x, z + 1) - hm.at(x, z - 1);
      if (slope * slope + slopeZ * slopeZ > 0.75f) continue;

      const glm::vec3 tint = terrainColor(wx, wz, y);
      if (tint == kDirt || tint == kBed) continue;
      const float hx = hm.at(x + 1, z) - hm.at(x - 1, z);
      const float hz = hm.at(x, z + 1) - hm.at(x, z - 1);
      glm::vec3 n = glm::normalize(glm::vec3{-hx, 2.f, -hz});
      const float roll = hash01(x, z, 14);
      pxe::FoliageInstance g{};
      g.px = wx;
      g.py = y;
      g.pz = wz;
      g.nx = n.x;
      g.ny = n.y;
      g.nz = n.z;
      g.phase = hash01(x, z, 15) * kTau;
      g.scale = 0.75f + hash01(x, z, 16) * 0.7f;
      if (roll > 0.975f) {
        g.kind = 1.f;
        g.cr = 0.58f;
        g.cg = 0.38f;
        g.cb = 0.68f;
      } else if (roll > 0.955f) {
        g.kind = 2.f;
        g.cr = 0.84f;
        g.cg = 0.74f;
        g.cb = 0.28f;
      } else {
        g.kind = 0.f;
        g.cr = tint.r;
        g.cg = tint.g;
        g.cb = tint.b;
      }
      foliage.push_back(g);
    }
  }

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
