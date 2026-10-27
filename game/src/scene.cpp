#include "scene.hpp"

#include "pxe/gltf.hpp"

#include <glm/geometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <optional>
#include <random>
#include <unordered_map>
#include <stdexcept>
#include <vector>

namespace game {
namespace {

constexpr float kPi = 3.14159265358979323846f;
// Where the shelter test (--shelter-test) builds its roofed campfire.
constexpr float kShelterTestX = -8.5f;
constexpr float kShelterTestZ = -9.f;

// `v` turned `yaw` radians about +y, as a model is placed at that yaw: its
// +z goes to (sin yaw, 0, cos yaw).
glm::vec3 turned(glm::vec3 v, float yaw) {
  const float c = std::cos(yaw), s = std::sin(yaw);
  return {c * v.x + s * v.z, v.y, -s * v.x + c * v.z};
}

// What is higher than this over a doorway's floor, over the doorway itself
// and the floor just through it, the hero (and a rolling fireball) passes
// under: the head of the doorway, which is 3.4 m tall, and the wall, rails
// and roof above it. Left out of the scene's ground there, or the highest
// surface over those cells would be the head and the doorway a wall.
constexpr float kDoorHeadroom = 3.3f;
// A rectangle on the ground (xz), lo to hi.
struct Rect {
  glm::vec2 lo, hi;
  bool holds(glm::vec2 p) const { return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y; }
};
// The Sleeping Fox's exterior (its own coordinates, assets.md): its doorway's
// clear opening, from the wall's inside face out to the eaves' edge, and the
// vestibule behind it, out to its walls.
constexpr Rect kTavernDoorway{{-1.1f, -0.25f}, {1.1f, 0.85f}};
constexpr Rect kTavernVestibule{{-1.9f, -3.f}, {1.9f, -0.25f}};
// Its room (tavern_interior.glb): the inside door's opening through the +x
// wall, its jambs included, and the step outside it.
constexpr Rect kRoomDoorway{{11.3f, -2.4f}, {14.1f, 0.2f}};

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

// Material colours for each time of day. Day is sampled from the finished
// frame of t3ssel8r's scene video; golden hour, twilight and night follow
// his day/night video: warm lit faces against violet shadow, then a
// low-contrast dusk, then everything in moonlit navy. A building's slate
// roof (blue-grey) is the stone's ramps carried over, each phase's colour
// scaled from the slate's day colour as the stone's is from its own. Its
// wood (warm brown) and plaster (cream) follow the stone and bark through
// the day in their mid and lit tones, but indoors their shadow tone is the
// one most seen, so that stays warm in every phase: a dark warm brown for
// the wood and a muted warm beige for the plaster, each dimmed with the
// phase so it stays in key with the rest of it. A room's oak furniture is a
// honey oak, warm at every hour: amber-gold in golden hour, darker warm
// browns at twilight and night, never blue or grey (each tone's red above
// its green, its green above its blue), and lighter than the floor in
// every phase.
void setPhasePalettes(pxe::Scene& scene) {
  using namespace pxe;
  struct Ramps {
    std::uint32_t band[3], detail[3], tip[3];
  };
  auto set = [&](Phase ph, Surface s, Ramps r) {
    setRamp(scene.palette[ph][s].band, r.band[0], r.band[1], r.band[2]);
    setRamp(scene.palette[ph][s].detail, r.detail[0], r.detail[1], r.detail[2]);
    setRamp(scene.palette[ph][s].tip, r.tip[0], r.tip[1], r.tip[2]);
  };
  auto plain = [](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    return Ramps{{a, b, c}, {a, b, c}, {a, b, c}};
  };

  set(kPhaseDay, kSurfGrass, {{0x648474, 0x94b064, 0xacbc6c}, {0x74947c, 0x7c9c54, 0x8ca454}, {0x648474, 0xacbc6c, 0xc8d690}});
  set(kPhaseDay, kSurfDirt, plain(0x5c6468, 0x7c7874, 0x948c80));
  set(kPhaseDay, kSurfStone, plain(0x747480, 0xa49c8c, 0xbcac94));
  set(kPhaseDay, kSurfBark, plain(0x6a6a7a, 0x8a8684, 0xbcac90));
  set(kPhaseDay, kSurfLeaf, {{0x4c6e5c, 0x68905a, 0x88a850}, {0x58806a, 0x7ca25a, 0xaac65c}, {0x4c6e5c, 0x68905a, 0x88a850}});
  set(kPhaseDay, kSurfWater, {{0x74949c, 0x8cb4cc, 0x8cb4cc}, {0x84a4ac, 0xa4c8d8, 0xa4c8d8}, {0x74949c, 0x8cb4cc, 0x8cb4cc}});
  set(kPhaseDay, kSurfFoam, plain(0xc4d4d8, 0xdce8ec, 0xe8f2f2));
  set(kPhaseDay, kSurfWood, plain(0x4c3626, 0x86664e, 0xa88058));
  set(kPhaseDay, kSurfPlaster, plain(0x9c8a70, 0xc8bca2, 0xe2d6b6));
  set(kPhaseDay, kSurfRoof, plain(0x3c4454, 0x5a6676, 0x7e8a98));
  set(kPhaseDay, kSurfOak, plain(0x8c5a30, 0xb47a42, 0xd49c5c));
  scene.rim[kPhaseDay] = hex(0xf5f0dc);
  scene.ink[kPhaseDay] = hex(0x4c4c66);

  set(kPhaseGolden, kSurfGrass, {{0x44665c, 0x84a444, 0xb4c050}, {0x547464, 0x6c943c, 0x94ac3c}, {0x44665c, 0xa8b850, 0xe0d070}});
  set(kPhaseGolden, kSurfDirt, plain(0x4c4c5c, 0x6c6058, 0x9c8460));
  set(kPhaseGolden, kSurfStone, plain(0x5c5874, 0x9c8474, 0xdcb070));
  set(kPhaseGolden, kSurfBark, plain(0x545468, 0x84746c, 0xc09c70));
  set(kPhaseGolden, kSurfLeaf, {{0x3c5854, 0x5c8448, 0x94ac44}, {0x48685c, 0x749c44, 0xc0bc50}, {0x3c5854, 0x5c8448, 0x94ac44}});
  set(kPhaseGolden, kSurfWater, {{0x5c7488, 0x8ca0b4, 0x8ca0b4}, {0x6c8498, 0xc4bca8, 0xc4bca8}, {0x5c7488, 0x8ca0b4, 0x8ca0b4}});
  set(kPhaseGolden, kSurfFoam, plain(0xa8b4c4, 0xd8d4cc, 0xf0e0c0));
  set(kPhaseGolden, kSurfWood, plain(0x463022, 0x7a5a4a, 0xb27a48));
  set(kPhaseGolden, kSurfPlaster, plain(0x947c66, 0xb49c88, 0xf2c88a));
  set(kPhaseGolden, kSurfRoof, plain(0x30344c, 0x565662, 0x938d73));
  set(kPhaseGolden, kSurfOak, plain(0x8a5028, 0xb4703a, 0xe49c52));
  scene.rim[kPhaseGolden] = hex(0xffe0a0);
  scene.ink[kPhaseGolden] = hex(0x403860);

  set(kPhaseTwilight, kSurfGrass, {{0x2c4c50, 0x34565a, 0x3c5e5c}, {0x345a58, 0x3c625c, 0x446a60}, {0x2c4c50, 0x446a64, 0x507468}});
  set(kPhaseTwilight, kSurfDirt, plain(0x34364c, 0x3c3e54, 0x46465c));
  set(kPhaseTwilight, kSurfStone, plain(0x4c4868, 0x585474, 0x6c6484));
  set(kPhaseTwilight, kSurfBark, plain(0x40405c, 0x4c4c66, 0x585874));
  set(kPhaseTwilight, kSurfLeaf, {{0x203c44, 0x284648, 0x30504c}, {0x2c4a4c, 0x345450, 0x3c5e54}, {0x203c44, 0x284648, 0x30504c}});
  set(kPhaseTwilight, kSurfWater, {{0x34486c, 0x445a80, 0x445a80}, {0x40567c, 0x5c6c94, 0x5c6c94}, {0x34486c, 0x445a80, 0x445a80}});
  set(kPhaseTwilight, kSurfFoam, plain(0x6c7898, 0x848cac, 0x9ca4bc));
  set(kPhaseTwilight, kSurfWood, plain(0x34261f, 0x443e58, 0x504862));
  set(kPhaseTwilight, kSurfPlaster, plain(0x5c4e46, 0x645e7a, 0x787090));
  set(kPhaseTwilight, kSurfRoof, plain(0x272a44, 0x303762, 0x485088));
  set(kPhaseTwilight, kSurfOak, plain(0x5c3c24, 0x70482c, 0x825838));
  scene.rim[kPhaseTwilight] = hex(0x9c94bc);
  scene.ink[kPhaseTwilight] = hex(0x24203c);

  set(kPhaseNight, kSurfGrass, {{0x1c2448, 0x222c54, 0x28345e}, {0x222c54, 0x28345e, 0x2e3c6a}, {0x1c2448, 0x2e3c6a, 0x384a7c}});
  set(kPhaseNight, kSurfDirt, plain(0x1a1e3e, 0x20264a, 0x262e54));
  set(kPhaseNight, kSurfStone, plain(0x2a3260, 0x34406e, 0x445484));
  set(kPhaseNight, kSurfBark, plain(0x222a52, 0x2a345e, 0x343e6c));
  set(kPhaseNight, kSurfLeaf, {{0x141c3c, 0x1a2446, 0x202c52}, {0x1a2446, 0x202c52, 0x283662}, {0x141c3c, 0x1a2446, 0x202c52}});
  set(kPhaseNight, kSurfWater, {{0x1c2a58, 0x243668, 0x243668}, {0x2c4078, 0x3c5492, 0x3c5492}, {0x1c2a58, 0x243668, 0x243668}});
  set(kPhaseNight, kSurfFoam, plain(0x44548a, 0x54649a, 0x6474aa));
  set(kPhaseNight, kSurfWood, plain(0x221a1a, 0x262c50, 0x30385e));
  set(kPhaseNight, kSurfPlaster, plain(0x3e3432, 0x3e4876, 0x4e5a8a));
  set(kPhaseNight, kSurfRoof, plain(0x161d3f, 0x1d2a5d, 0x2e4388));
  set(kPhaseNight, kSurfOak, plain(0x3c2818, 0x4a3220, 0x583c28));
  scene.rim[kPhaseNight] = hex(0x7084c0);
  scene.ink[kPhaseNight] = hex(0x12163a);
}

struct Triangle {
  glm::vec3 p[3];
  glm::vec3 n[3];
};

// XZ bucket grid over upward-facing triangles, so a point on the lawn can
// find the triangle under it and read back its height and smooth normal.
class GroundIndex {
 public:
  GroundIndex(std::vector<Triangle> tris, float cell) : tris_(std::move(tris)), cell_(cell) {
    for (const Triangle& t : tris_) {
      for (const glm::vec3& p : t.p) {
        min_ = glm::min(min_, glm::vec2(p.x, p.z));
        max_ = glm::max(max_, glm::vec2(p.x, p.z));
      }
    }
    w_ = std::max(1, static_cast<int>(std::ceil((max_.x - min_.x) / cell_)));
    h_ = std::max(1, static_cast<int>(std::ceil((max_.y - min_.y) / cell_)));
    buckets_.resize(static_cast<std::size_t>(w_ * h_));
    for (std::size_t i = 0; i < tris_.size(); ++i) {
      const Triangle& t = tris_[i];
      glm::vec2 lo{1e9f}, hi{-1e9f};
      for (const glm::vec3& p : t.p) {
        lo = glm::min(lo, glm::vec2(p.x, p.z));
        hi = glm::max(hi, glm::vec2(p.x, p.z));
      }
      const auto [x0, z0] = cellOf(lo);
      const auto [x1, z1] = cellOf(hi);
      for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) buckets_[static_cast<std::size_t>(z * w_ + x)].push_back(i);
    }
  }

  bool empty() const { return tris_.empty(); }
  glm::vec2 min() const { return min_; }
  glm::vec2 max() const { return max_; }

  struct Hit {
    glm::vec3 pos;
    glm::vec3 normal;
  };

  std::optional<Hit> at(float x, float z) const {
    if (x < min_.x || z < min_.y || x > max_.x || z > max_.y) return std::nullopt;
    const auto [cx, cz] = cellOf({x, z});
    for (std::size_t i : buckets_[static_cast<std::size_t>(cz * w_ + cx)]) {
      const Triangle& t = tris_[i];
      const glm::vec2 a{t.p[0].x, t.p[0].z}, b{t.p[1].x, t.p[1].z}, c{t.p[2].x, t.p[2].z};
      const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
      if (std::fabs(d) < 1e-9f) continue;
      const float u = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (z - c.y)) / d;
      const float v = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (z - c.y)) / d;
      const float w = 1.f - u - v;
      if (u < -1e-5f || v < -1e-5f || w < -1e-5f) continue;
      return Hit{t.p[0] * u + t.p[1] * v + t.p[2] * w, glm::normalize(t.n[0] * u + t.n[1] * v + t.n[2] * w)};
    }
    return std::nullopt;
  }

 private:
  std::pair<int, int> cellOf(glm::vec2 p) const {
    return {std::clamp(static_cast<int>((p.x - min_.x) / cell_), 0, w_ - 1),
            std::clamp(static_cast<int>((p.y - min_.y) / cell_), 0, h_ - 1)};
  }

  std::vector<Triangle> tris_;
  std::vector<std::vector<std::size_t>> buckets_;
  float cell_;
  glm::vec2 min_{1e9f}, max_{-1e9f};
  int w_ = 1, h_ = 1;
};

// Bridson Poisson disk in horizontal metres over a rectangle, keeping only
// points `accept` likes. Every grid cell is tried as a fresh seed, so lawns
// split by paving or water still fill. Separation is horizontal rather than
// along the slope so a steep bank does not pack extra tufts into one pixel.
std::vector<glm::vec2> poisson(glm::vec2 lo, glm::vec2 hi, float minDist, std::mt19937& rng,
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

// Blade clumps carpet the lawn at blade spacing so every band edge gets a
// continuous saw-tooth; tufts are a sparse second layer on top. Clump height
// follows `patch` so the grass grows in taller and shorter patches.
void scatterGrass(pxe::Scene& scene, const GroundIndex& ground, const BuildOptions& options,
                  const std::function<float(float, float)>& patch, std::mt19937& rng) {
  if (ground.empty()) return;
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  auto onGround = [&](float x, float z) { return ground.at(x, z).has_value(); };
  auto emit = [&](glm::vec2 p, float kind, float scale) {
    const auto hit = ground.at(p.x, p.y);
    if (!hit) return;
    scene.grass.push_back({hit->pos.x, hit->pos.y, hit->pos.z, hit->normal.x, hit->normal.y, hit->normal.z, kind,
                           unit(rng), scale, static_cast<float>(pxe::kSurfGrass)});
  };
  const auto clumps = poisson(ground.min(), ground.max(), options.minDist, rng, onGround);
  for (const glm::vec2& p : clumps) emit(p, 0.f, 0.55f + 0.75f * patch(p.x, p.y) + 0.25f * unit(rng));
  std::size_t tufts = 0;
  if (options.tufts) {
    const auto points = poisson(ground.min(), ground.max(), options.tuftDist, rng, onGround);
    for (const glm::vec2& p : points) emit(p, 1.f, 0.85f + 0.3f * unit(rng));
    tufts = points.size();
  }
  std::fprintf(stderr, "pxe: clumps %zu, tufts %zu\n", clumps.size(), tufts);
}

// Slow 0..1 field so grass grows in taller and shorter patches; a uniform
// height makes every band edge the same fine comb.
float patchiness(float x, float z) {
  const float a = std::sin(0.31f * x + 1.7f * std::sin(0.23f * z));
  const float b = std::sin(0.27f * z - 1.3f * std::sin(0.19f * x + 0.5f));
  return std::clamp(0.5f + 0.5f * a * b + 0.15f * std::sin(0.9f * x + 0.7f * z), 0.f, 1.f);
}

void computeBounds(pxe::Scene& scene, const std::vector<pxe::Vertex>& verts) {
  for (const pxe::Vertex& v : verts) {
    scene.boundsMin = glm::min(scene.boundsMin, glm::vec3(v.px, v.py, v.pz));
    scene.boundsMax = glm::max(scene.boundsMax, glm::vec3(v.px, v.py, v.pz));
  }
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

pxe::Scene buildHills(const BuildOptions& options) {
  pxe::Scene scene;
  setPhasePalettes(scene);
  pxe::Palette& day = scene.palette[pxe::kPhaseDay][pxe::kSurfGrass];
  setRamp(day.band, 0x46766e, 0x83ab5c, 0xc7e487);
  setRamp(day.detail, 0x5b8a6e, 0x6b944e, 0x9cc266);
  setRamp(day.tip, 0x46766e, 0xa2c66c, 0xe2f3a8);
  scene.tLit = 0.75f;
  scene.tMid = 0.585f;
  scene.yawDegrees = 45.f;

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
  std::vector<Triangle> tris;
  indices.reserve(static_cast<std::size_t>(n - 1) * (n - 1) * 6);
  auto tri = [&](std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    indices.insert(indices.end(), {a, b, c});
    Triangle t;
    for (int k = 0; k < 3; ++k) {
      const pxe::Vertex& v = verts[(k == 0) ? a : (k == 1) ? b : c];
      t.p[k] = {v.px, v.py, v.pz};
      t.n[k] = {v.nx, v.ny, v.nz};
    }
    tris.push_back(t);
  };
  for (int j = 0; j < n - 1; ++j) {
    for (int i = 0; i < n - 1; ++i) {
      const auto i00 = static_cast<std::uint32_t>(j * n + i);
      const auto i10 = i00 + 1;
      const auto i01 = i00 + static_cast<std::uint32_t>(n);
      const auto i11 = i01 + 1;
      tri(i00, i11, i10);
      tri(i00, i01, i11);
    }
  }
  computeBounds(scene, verts);
  {
    std::vector<glm::vec3> corners;
    for (const Triangle& t : tris) corners.insert(corners.end(), {t.p[0], t.p[1], t.p[2]});
    scene.ground.build(corners);
  }
  scene.meshes.emplace_back();
  scene.meshes.back().upload(verts, indices);

  const glm::vec3 flatForward{std::sin(kHillsYaw), 0.f, std::cos(kHillsYaw)};
  const glm::vec3 right{-std::cos(kHillsYaw), 0.f, std::sin(kHillsYaw)};
  scene.sunEast = right;
  scene.sunSouth = flatForward;
  scene.focus = glm::vec3{0.f, 2.f, 0.f};

  std::mt19937 rng(7);
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  GroundIndex ground(std::move(tris), 1.f);
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
    scatterGrass(scene, ground, options, patchiness, rng);
  }
  return scene;
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
glm::vec3 addCampfire(VertexLists verts, IndexLists indices, const GroundIndex& ground, float x, float z,
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

std::vector<glm::vec3> cornersOf(const std::vector<pxe::Vertex>& v, const std::vector<std::uint32_t>& idx) {
  std::vector<glm::vec3> out;
  out.reserve(idx.size());
  for (std::uint32_t i : idx) out.push_back({v[i].px, v[i].py, v[i].pz});
  return out;
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
void addShelter(VertexLists verts, IndexLists indices, const GroundIndex& ground, float x, float z) {
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
void setUpRuins(pxe::Scene& scene) {
  setPhasePalettes(scene);
  scene.tLit = 0.6f;
  scene.tMid = 0.18f;

  // The Blender layout is built facing Blender +Y, which is glTF -Z.
  scene.yawDegrees = 180.f;
  const glm::vec3 forward{0.f, 0.f, -1.f};
  const glm::vec3 right{1.f, 0.f, 0.f};
  // Rises on the right of the screen and sets on the left, arcing over the
  // pond, so by mid-afternoon the pillars' left faces are lit as in the video.
  scene.sunEast = right;
  scene.sunSouth = forward;
  scene.focus = glm::vec3{2.f, 1.f, -6.f};
  // Wooded hills just past the edge of the frame. Pillar tops (about 10 m)
  // catch the first sun; the lawn waits until the sun is ~15 degrees up.
  scene.horizonCentre = glm::vec2{2.f, -6.f};
  scene.horizonRadius = 46.f;
  scene.horizonHeight = 12.f;
  scene.horizonRelief = 9.f;
}

// The ruins before anything grows on them: ruins.glb, with the campfires
// (and any test shelter and candles) added, one vertex list per surface.
// Laying them out also gives the scene its fires, bounds, shelter, ground
// and water.
struct RuinsGeometry {
  std::vector<pxe::Vertex> verts[pxe::kSurfCount];
  std::vector<std::uint32_t> indices[pxe::kSurfCount];
  std::optional<GroundIndex> lawn;  // under the grass
  std::vector<glm::vec3> lawnCorners;
  std::vector<Triangle> leaves;
  std::vector<float> leafArea;
  std::size_t prims = 0;
  std::size_t tavernTriangles = 0;
  std::size_t candles = 0;
  int candlesOnStone = 0, candlesOnLawn = 0;
  std::vector<glm::vec2> campfires;
  std::vector<glm::vec3> solids;  // see SceneLayout
};

void layOutRuins(const BuildOptions& options, pxe::Scene& scene, RuinsGeometry& geo) {
  const auto prims = pxe::loadGltf(std::string(PXE_ASSET_DIR) + "/scenes/ruins.glb");
  geo.prims = prims.size();

  // One mesh per surface keeps draw calls low.
  auto& verts = geo.verts;
  auto& indices = geo.indices;
  std::vector<Triangle> lawn;
  std::vector<Triangle>& leaves = geo.leaves;
  for (const auto& prim : prims) {
    auto& v = verts[prim.surface];
    auto& idx = indices[prim.surface];
    const auto base = static_cast<std::uint32_t>(v.size());
    v.insert(v.end(), prim.vertices.begin(), prim.vertices.end());
    for (std::uint32_t i : prim.indices) idx.push_back(base + i);
    computeBounds(scene, prim.vertices);
    if (prim.node != "Ground")
      for (std::uint32_t i : prim.indices) {
        const pxe::Vertex& pv = prim.vertices[i];
        geo.solids.push_back({pv.px, pv.py, pv.pz});
      }

    if (prim.surface != pxe::kSurfGrass && prim.surface != pxe::kSurfLeaf) continue;
    for (std::size_t i = 0; i + 2 < prim.indices.size(); i += 3) {
      Triangle t;
      for (int k = 0; k < 3; ++k) {
        const pxe::Vertex& pv = prim.vertices[prim.indices[i + static_cast<std::size_t>(k)]];
        t.p[k] = {pv.px, pv.py, pv.pz};
        t.n[k] = {pv.nx, pv.ny, pv.nz};
      }
      if (prim.surface == pxe::kSurfGrass) {
        lawn.push_back(t);
        if (prim.node == "Ground") geo.lawnCorners.insert(geo.lawnCorners.end(), {t.p[0], t.p[1], t.p[2]});
      } else {
        leaves.push_back(t);
        geo.leafArea.push_back(0.5f * glm::length(glm::cross(t.p[1] - t.p[0], t.p[2] - t.p[0])));
      }
    }
  }
  // Everything added from here on stands in the scene.
  std::size_t added[pxe::kSurfCount];
  for (int s = 0; s < pxe::kSurfCount; ++s) added[s] = indices[s].size();
  // The campfire sits on open lawn left of the pond.
  geo.lawn.emplace(std::move(lawn), 1.f);
  const GroundIndex& ground = *geo.lawn;
  std::mt19937 fireRng(11);
  const glm::vec2 campfire = kRuinsCampfire;
  scene.fires.push_back({addCampfire(verts, indices, ground, campfire.x, campfire.y, fireRng)});
  geo.campfires.push_back(campfire);
  if (options.shelterTest) {
    // A second fire under a stone shelter, and a third under a tree crown
    // (the nearest bit of open lawn to the middle with a full crown over it
    // and no trunk close by).
    const glm::vec2 roofed{kShelterTestX, kShelterTestZ};
    addShelter(verts, indices, ground, roofed.x, roofed.y);
    scene.fires.push_back({addCampfire(verts, indices, ground, roofed.x, roofed.y, fireRng)});
    geo.campfires.push_back(roofed);
    std::vector<glm::vec3> crownCorners;
    for (const Triangle& t : leaves) crownCorners.insert(crownCorners.end(), {t.p[0], t.p[1], t.p[2]});
    pxe::ShelterMap crowns;
    crowns.build(crownCorners);
    const std::vector<glm::vec3> bark = cornersOf(verts[pxe::kSurfBark], indices[pxe::kSurfBark]);
    std::optional<glm::vec3> under;
    float best = 1e9f;
    for (float z = scene.focus.z - 12.f; z <= scene.focus.z + 10.f; z += 0.5f)
      for (float x = scene.focus.x - 14.f; x <= scene.focus.x + 14.f; x += 0.5f) {
        const auto hit = ground.at(x, z);
        if (!hit || inTavernFootprint({x, z})) continue;
        const float d = glm::length(glm::vec2{x, z} - glm::vec2{scene.focus.x, scene.focus.z});
        if (d >= best) continue;
        bool clear = true;
        for (const pxe::FireSite& f : scene.fires) clear = clear && glm::length(glm::vec2{x - f.base.x, z - f.base.z}) > 3.f;
        for (const glm::vec3& p : bark)
          clear = clear && (p.y > hit->pos.y + 2.5f || glm::length(glm::vec2{p.x - x, p.z - z}) > 1.2f);
        if (clear && crowns.coveredLines(hit->pos) == 9) {
          best = d;
          under = hit->pos;
        }
      }
    if (under) {
      scene.fires.push_back({addCampfire(verts, indices, ground, under->x, under->z, fireRng)});
      geo.campfires.push_back({under->x, under->z});
    }
  }
  if (options.candles) {
    // Candles on flat stone tops - a ledge, a plinth, a fallen block, from
    // knee height up - and a few on the open lawn, spread apart and clear of
    // the campfires; and two under the shelter test's roof.
    const StoneTops tops(cornersOf(verts[pxe::kSurfStone], indices[pxe::kSurfStone]));
    std::vector<glm::vec3> feet;
    auto clear = [&](float x, float z, float apart) {
      for (const pxe::FireSite& f : scene.fires)
        if (glm::length(glm::vec2{x - f.base.x, z - f.base.z}) < (f.size < 1.f ? apart : 1.6f)) return false;
      for (const glm::vec3& f : feet)
        if (glm::length(glm::vec2{x - f.x, z - f.z}) < apart) return false;
      return true;
    };
    std::vector<glm::vec2> spots;
    for (float z = scene.focus.z - 8.f; z <= scene.focus.z + 7.f; z += 0.25f)
      for (float x = scene.focus.x - 12.f; x <= scene.focus.x + 12.f; x += 0.25f) spots.push_back({x, z});
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
      } else if (lawnHit && (!top || top->first < lawnY + 0.03f) && lawnHit->normal.y > 0.9f &&
                 !inTavernFootprint(at)) {
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
    for (const glm::vec3& foot : feet) scene.fires.push_back({addCandle(verts, indices, foot), false, kCandleSize});
    geo.candles = feet.size();
  }
  // The Sleeping Fox on its lawn (kTavernSpot), added after the shelter
  // test and the candles, which keep off its footprint, so they are laid
  // out as they were before it stood there.
  for (const pxe::GltfPrimitive& prim : pxe::loadGltf(std::string(PXE_ASSET_DIR) + "/scenes/tavern_exterior.glb")) {
    auto& v = verts[prim.surface];
    auto& idx = indices[prim.surface];
    const auto base = static_cast<std::uint32_t>(v.size());
    for (const pxe::Vertex& pv : prim.vertices) {
      const glm::vec3 p = tavernPoint({pv.px, pv.py, pv.pz});
      const glm::vec3 n = turned({pv.nx, pv.ny, pv.nz}, kTavernYaw);
      v.push_back({p.x, p.y, p.z, n.x, n.y, n.z, pv.surface});
    }
    for (std::uint32_t i : prim.indices) idx.push_back(base + i);
    computeBounds(scene, std::vector<pxe::Vertex>(v.begin() + base, v.end()));
    geo.tavernTriangles += prim.indices.size() / 3;
  }
  for (int s = 0; s < pxe::kSurfCount; ++s)
    for (std::size_t i = added[s]; i < indices[s].size(); ++i) {
      const pxe::Vertex& v = verts[s][indices[s][i]];
      geo.solids.push_back({v.px, v.py, v.pz});
    }
  // Only built structure keeps the rain off: the stone, and the Sleeping
  // Fox's slates. A tree crown lets it through.
  {
    std::vector<glm::vec3> roofs = cornersOf(verts[pxe::kSurfStone], indices[pxe::kSurfStone]);
    const std::vector<glm::vec3> slates = cornersOf(verts[pxe::kSurfRoof], indices[pxe::kSurfRoof]);
    roofs.insert(roofs.end(), slates.begin(), slates.end());
    scene.shelter.build(roofs);
  }
  {
    // The ground is everything solid but the water and the leaves - and the
    // Sleeping Fox's roof, which is all overhead: its walls stand under it
    // and stop whatever comes at them, and its eaves would only make a wall
    // in the air half a metre out from them, and over its door. Over the
    // doorway and the vestibule behind it, what is above head height is
    // left out too, so the hero can walk in.
    std::vector<glm::vec3> solid;
    for (int s : {pxe::kSurfGrass, pxe::kSurfDirt, pxe::kSurfStone, pxe::kSurfBark, pxe::kSurfFoam, pxe::kSurfWood,
                  pxe::kSurfPlaster, pxe::kSurfOak}) {
      const std::vector<glm::vec3> c = cornersOf(verts[s], indices[s]);
      solid.insert(solid.end(), c.begin(), c.end());
    }
    scene.ground.build(solid, 0.2f, [](glm::vec2 xz, float y) {
      const glm::vec2 local = tavernLocal(xz);
      return y > kTavernSpot.y + kDoorHeadroom && (kTavernDoorway.holds(local) || kTavernVestibule.holds(local));
    });
    scene.waterTop.build(cornersOf(verts[pxe::kSurfWater], indices[pxe::kSurfWater]));
  }
  for (pxe::FireSite& f : scene.fires) f.sheltered = scene.shelter.sheltered(f.base);
}

pxe::Scene buildRuins(const BuildOptions& options) {
  pxe::Scene scene;
  setUpRuins(scene);
  RuinsGeometry geo;
  layOutRuins(options, scene, geo);
  auto& verts = geo.verts;
  auto& indices = geo.indices;
  const GroundIndex& ground = *geo.lawn;
  const std::vector<Triangle>& leaves = geo.leaves;
  const std::vector<float>& leafArea = geo.leafArea;
  if (options.candles)
    std::fprintf(stderr, "pxe: %zu candles, %d on stone, %d on the lawn\n", geo.candles, geo.candlesOnStone,
                 geo.candlesOnLawn);
  for (const pxe::FireSite& f : scene.fires)
    std::fprintf(stderr, "pxe: %s at %.1f, %.1f %s\n", f.size < 1.f ? "candle" : "fire", f.base.x, f.base.z,
                 f.sheltered ? "sheltered" : "open to the rain");

  for (int s = 0; s < pxe::kSurfCount; ++s) {
    if (indices[s].empty()) continue;
    auto& target = (s == pxe::kSurfWater) ? scene.water : scene.meshes;
    target.emplace_back();
    target.back().upload(verts[s], indices[s]);
  }
  if (!verts[pxe::kSurfWater].empty()) {
    scene.waterMin = glm::vec3{1e9f};
    scene.waterMax = glm::vec3{-1e9f};
    for (const pxe::Vertex& v : verts[pxe::kSurfWater]) {
      scene.waterMin = glm::min(scene.waterMin, glm::vec3(v.px, v.py, v.pz));
      scene.waterMax = glm::max(scene.waterMax, glm::vec3(v.px, v.py, v.pz));
    }
    scene.waterLevel = 0.5f * (scene.waterMin.y + scene.waterMax.y);
  }

  std::mt19937 rng(7);
  std::uniform_real_distribution<float> unit(0.f, 1.f);
  scatterGrass(scene, ground, options, patchiness, rng);
  // The lawn runs on under the paving, plinths and trunks, so drop grass
  // rooted beneath them: from inside the stone it showed through its faces.
  {
    constexpr float kCell = 0.1f;
    const glm::vec2 lo{scene.boundsMin.x, scene.boundsMin.z};
    const int gw = static_cast<int>(std::ceil((scene.boundsMax.x - lo.x) / kCell)) + 1;
    const int gh = static_cast<int>(std::ceil((scene.boundsMax.z - lo.y) / kCell)) + 1;
    std::vector<float> top(static_cast<std::size_t>(gw * gh), -1e9f);
    for (int s : {static_cast<int>(pxe::kSurfStone), static_cast<int>(pxe::kSurfBark)}) {
      const auto& v = verts[s];
      const auto& idx = indices[s];
      for (std::size_t i = 0; i + 2 < idx.size(); i += 3) {
        const pxe::Vertex& a = v[idx[i]];
        const pxe::Vertex& b = v[idx[i + 1]];
        const pxe::Vertex& c = v[idx[i + 2]];
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
    std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) {
      if (g.surface != static_cast<float>(pxe::kSurfGrass)) return false;
      const int x = static_cast<int>((g.px - lo.x) / kCell), z = static_cast<int>((g.pz - lo.y) / kCell);
      if (x < 0 || z < 0 || x >= gw || z >= gh) return false;
      return top[static_cast<std::size_t>(z * gw + x)] > g.py + 0.03f;
    });
    std::fprintf(stderr, "pxe: %zu grass sprites under stone or trunks dropped\n", before - scene.grass.size());
  }
  // Firefly homes: scattered over the open lawn round the ruins, from knee
  // to head height.
  {
    std::mt19937 flyRng(23);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    for (int tries = 0; tries < 400 && scene.fireflyHomes.size() < 40; ++tries) {
      const float x = scene.focus.x - 14.f + 28.f * u(flyRng);
      const float z = scene.focus.z - 12.f + 22.f * u(flyRng);
      const auto hit = ground.at(x, z);
      if (!hit) continue;
      scene.fireflyHomes.push_back(hit->pos + glm::vec3{0.f, 0.5f + 1.2f * u(flyRng), 0.f});
    }
    // None inside the Sleeping Fox (dropped after, so the rest are where they were).
    std::erase_if(scene.fireflyHomes, [](const glm::vec3& p) { return inTavernFootprint({p.x, p.z}); });
  }

  // No grass in the fire pits.
  std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) {
    for (const pxe::FireSite& f : scene.fires)
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
  // Nor inside the Sleeping Fox's footprint (its plinth's stone already
  // hid most of it), so none shows through its floor or walls and a grass
  // fire stops at it. Dropped after scattering too.
  {
    const std::size_t before = scene.grass.size();
    std::erase_if(scene.grass, [&](const pxe::GrassInstance& g) { return g.kind < 1.5f && inTavernFootprint({g.px, g.pz}); });
    std::fprintf(stderr, "pxe: %zu grass sprites under the tavern dropped\n", before - scene.grass.size());
  }

  // Leaves grow in clumps, as in the video: clump centres are scattered over
  // every crown, and leaves gather around them with gaps between where the
  // darker crown body shows. A clump shares a tilt, so its leaves tend to
  // light together and the crown breaks into patches; each leaf adds its
  // own tilt on top so patch edges stay leafy. Some leaves stand proud of
  // the surface, which rags the crown's outline.
  float total = 0.f;
  std::vector<float> cdf;
  for (float a : leafArea) cdf.push_back(total += a);
  auto samplePoint = [&](glm::vec3& p, glm::vec3& n) {
    const auto it = std::lower_bound(cdf.begin(), cdf.end(), unit(rng) * total);
    const Triangle& t = leaves[static_cast<std::size_t>(
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

  const int candidates = cdf.empty() ? 0 : static_cast<int>(total * options.leafDensity * 1.6f);
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
                           0.85f + 0.3f * unit(rng), static_cast<float>(pxe::kSurfLeaf)});
    ++leafCount;
  }
  // Bare flames for testing many fires at once, as spells or spreading fire
  // would make: scattered over the open lawn, with no pit or logs.
  if (options.extraFires > 0) {
    std::mt19937 extraRng(31);
    std::uniform_real_distribution<float> u(0.f, 1.f);
    const std::size_t want = scene.fires.size() + static_cast<std::size_t>(options.extraFires);
    for (int tries = 0; tries < 2000 && scene.fires.size() < want; ++tries) {
      const float x = scene.focus.x - 12.f + 24.f * u(extraRng);
      const float z = scene.focus.z - 9.f + 16.f * u(extraRng);
      const auto hit = ground.at(x, z);
      if (!hit || inTavernFootprint({x, z})) continue;
      bool apart = true;
      for (const pxe::FireSite& f : scene.fires) apart = apart && glm::length(glm::vec2{x - f.base.x, z - f.base.z}) > 1.5f;
      if (!apart) continue;
      const glm::vec3 base = hit->pos + glm::vec3{0.f, 0.08f, 0.f};
      scene.fires.push_back({base, scene.shelter.sheltered(base)});
    }
    std::fprintf(stderr, "pxe: %d extra fires\n", options.extraFires);
  }
  std::fprintf(stderr, "pxe: ruins %zu primitives, %d leaf clusters; tavern %zu triangles at %.2f, %.2f, %.2f\n", geo.prims,
               leafCount, geo.tavernTriangles, kTavernSpot.x, kTavernSpot.y, kTavernSpot.z);
  std::fprintf(stderr, "pxe: bounds %.2f %.2f %.2f .. %.2f %.2f %.2f\n", scene.boundsMin.x, scene.boundsMin.y,
               scene.boundsMin.z, scene.boundsMax.x, scene.boundsMax.y, scene.boundsMax.z);
  return scene;
}

// ---------------------------------------------------------------------------
// The tavern: the Sleeping Fox's one room, inside (tools/blender/
// build_tavern_interior.py), 22.6 x 17 m, a cutaway seen from yaw 45 with
// its roof and its two walls nearest that camera drawn only into the shadow
// map. Its hearth and a brazier burn, a candle on every table and on the
// bar, and a sconce on four wall posts. Everything here is as assets.md gives it, in the room's own
// coordinates: the origin is the middle of its floor.

// The hearth's fire, where the flames rise from: 0.08 m over its ash bed, as
// addCampfire's.
constexpr glm::vec3 kHearthFire{-4.2f, 0.41f, 7.75f};
// The brazier's fire, 0.08 m over its coal bed, and its size.
constexpr glm::vec3 kBrazierFire{8.6f, 1.16f, -4.4f};
constexpr float kBrazierSize = 0.5f;
// The candles' flames, each from the top of its wax stub: on tables A-F,
// then on the bar. The candles themselves are in the glb.
constexpr glm::vec3 kRoomCandles[] = {{-7.3f, 1.45f, 1.9f},  {-2.2f, 1.45f, 1.9f},  {2.55f, 1.45f, 1.9f},
                                      {-7.3f, 1.45f, -4.1f}, {-1.6f, 1.45f, -4.1f}, {4.2f, 1.45f, -4.1f},
                                      {8.f, 1.95f, 5.55f}};
// Their size: a little bigger than the ruins' test candles (kCandleSize),
// so each lights its table.
constexpr float kRoomCandleSize = 0.3f;
// The wall sconces' flames, each from its dish: on the +z (hearth) wall over
// the stage and over the bar's far end, then on the +x (door) wall between
// the door and its window and in the corner by the -z cutaway. The sconces
// themselves are in the glb.
// The four windows, open holes in the drawn walls (assets.md): three in the
// +z (hearth) wall, one in the +x (door) wall, each 1.4 m wide from a sill
// at 1.5 m to a head at 3.2 m, through walls 0.4 m thick whose inner faces
// are at z 8.5 and x 11.3.
constexpr pxe::SkyWindow kRoomWindows[] = {
    {{-9.9f, 2.35f, 8.5f}, {0.f, 0.f, -1.f}, 1.4f, 1.7f, 0.4f},
    {{1.65f, 2.35f, 8.5f}, {0.f, 0.f, -1.f}, 1.4f, 1.7f, 0.4f},
    {{4.7f, 2.35f, 8.5f}, {0.f, 0.f, -1.f}, 1.4f, 1.7f, 0.4f},
    {{11.3f, 2.35f, -6.6f}, {-1.f, 0.f, 0.f}, 1.4f, 1.7f, 0.4f}};
constexpr glm::vec3 kRoomSconces[] = {
    {-11.175f, 2.6f, 8.f}, {7.408f, 2.6f, 8.f}, {10.8f, 2.6f, -4.15f}, {10.8f, 2.6f, -8.375f}};
constexpr float kSconceSize = 0.35f;
// What the hero is kept off by a Blocker (tavernBlockers) rather than by the
// ground: the seats and the brazier; and the stubs drawn at the foot of the
// two cutaway walls, which are lower than a step, so in the ground they
// would only lift the hero onto them as it comes up against those walls.
bool blockedNotGround(const std::string& node) {
  return node.starts_with("Chair_") || node.starts_with("Stool_") || node.starts_with("Bench_") || node == "Brazier" ||
         node.starts_with("WallStub_");
}

pxe::Scene buildTavern(const BuildOptions&) {
  pxe::Scene scene;
  setPhasePalettes(scene);
  // The room's stone - the hearth, the sconces' irons - is seen by its own
  // fires once the sun is down, so at twilight and night it is a warm grey
  // rather than the moonlit blue outside (no tone bluer than it is red).
  // Only here: the ruins' stone keeps its palette.
  {
    using namespace pxe;
    auto warm = [&](Phase ph, std::uint32_t shadow, std::uint32_t mid, std::uint32_t lit) {
      setRamp(scene.palette[ph][kSurfStone].band, shadow, mid, lit);
      setRamp(scene.palette[ph][kSurfStone].detail, shadow, mid, lit);
      setRamp(scene.palette[ph][kSurfStone].tip, shadow, mid, lit);
    };
    warm(kPhaseTwilight, 0x524a46, 0x625850, 0x74685c);
    warm(kPhaseNight, 0x342e2c, 0x3e3734, 0x4a423c);
  }
  // The sky through the windows: it lights the room in front of each one,
  // and the fill light takes its tint - a warm white by day, amber in
  // golden hour, a violet-blue through twilight and a cool moonlight at
  // night (pxe::Lighting::skyTint) - and the openings show it: a pale
  // blue, a gold, a violet dusk and a night navy.
  scene.windows.assign(std::begin(kRoomWindows), std::end(kRoomWindows));
  scene.skyTint[pxe::kPhaseDay] = {1.02f, 0.98f, 0.9f};
  scene.skyTint[pxe::kPhaseGolden] = {1.12f, 0.82f, 0.52f};
  scene.skyTint[pxe::kPhaseTwilight] = {0.8f, 0.7f, 1.12f};
  scene.skyTint[pxe::kPhaseNight] = {0.6f, 0.74f, 1.1f};
  scene.skyColour[pxe::kPhaseDay] = hex(0xc4dcec);
  scene.skyColour[pxe::kPhaseGolden] = hex(0xecb874);
  scene.skyColour[pxe::kPhaseTwilight] = hex(0x5c4c84);
  scene.skyColour[pxe::kPhaseNight] = hex(0x1a2050);
  // Lit as the ruins outside are, by the same sun and moon.
  scene.tLit = 0.6f;
  scene.tMid = 0.18f;
  scene.indoors = true;
  // The room is designed for this yaw, its cutaway walls toward the camera.
  scene.yawDegrees = 45.f;
  scene.focus = glm::vec3{0.f, 1.f, 0.f};
  // The fill light falls from a little above and from the camera's right at
  // the design yaw (the -x side), so the tops, the -x sides and the -z sides
  // the camera sees each take their own tone: the -x sides most, then the
  // -z sides, then the tops. Kept low so the floor, all top, stays dark and
  // the daylight falling on it through the windows still stands out.
  scene.fillDir = glm::normalize(glm::vec3{-0.8f, 0.35f, -0.45f});
  // The sun crosses the sky as it does over the ruins, so daylight falls in
  // through the windows from the side it would outside: the room's +x is the
  // way the inside door faces, which is the way the exterior's door faces,
  // so the room is the exterior turned a quarter-turn (yaw kTavernYaw - 90
  // degrees in the world), and the ruins' east and south are turned back by
  // as much. No ridge rings it: nothing outside is seen.
  {
    const float roomYaw = kTavernYaw - 0.5f * kPi;
    scene.sunEast = turned({1.f, 0.f, 0.f}, -roomYaw);
    scene.sunSouth = turned({0.f, 0.f, -1.f}, -roomYaw);
  }

  const auto prims = pxe::loadGltf(std::string(PXE_ASSET_DIR) + "/scenes/tavern_interior.glb");
  // One mesh per surface for what is seen, and one per surface for what only
  // casts shadows (nodes named "..._ShadowOnly").
  std::vector<pxe::Vertex> verts[pxe::kSurfCount], shadowVerts[pxe::kSurfCount];
  std::vector<std::uint32_t> indices[pxe::kSurfCount], shadowIndices[pxe::kSurfCount];
  std::size_t shadowOnly = 0;
  std::vector<glm::vec3> groundCorners;
  for (const pxe::GltfPrimitive& prim : prims) {
    const bool unseen = prim.node.ends_with("_ShadowOnly");
    if (!unseen && !blockedNotGround(prim.node))
      for (std::uint32_t i : prim.indices) {
        const pxe::Vertex& pv = prim.vertices[i];
        groundCorners.push_back({pv.px, pv.py, pv.pz});
      }
    auto& v = (unseen ? shadowVerts : verts)[prim.surface];
    auto& idx = (unseen ? shadowIndices : indices)[prim.surface];
    const auto base = static_cast<std::uint32_t>(v.size());
    v.insert(v.end(), prim.vertices.begin(), prim.vertices.end());
    for (std::uint32_t i : prim.indices) idx.push_back(base + i);
    computeBounds(scene, prim.vertices);
    if (unseen) shadowOnly += prim.indices.size() / 3;
  }
  // What keeps the rain off: the stone (the hearth's chimney breast over its
  // fire), and the roof and walls the camera does not see.
  {
    std::vector<glm::vec3> roofs = cornersOf(verts[pxe::kSurfStone], indices[pxe::kSurfStone]);
    for (int s = 0; s < pxe::kSurfCount; ++s) {
      const std::vector<glm::vec3> c = cornersOf(shadowVerts[s], shadowIndices[s]);
      roofs.insert(roofs.end(), c.begin(), c.end());
    }
    scene.shelter.build(roofs);
  }
  // The ground is everything seen - the floor, and the walls, bar, tables,
  // barrels, shelves, hearth and stage standing on it, all taller than a
  // step - but not what only casts shadows, nor the seats, the brazier and
  // the near walls' stubs (blockedNotGround), nor the head of the inside
  // door and what is over it. The game keeps the hero off those and inside
  // the near walls with Blockers (tavernBlockers).
  scene.ground.build(groundCorners, 0.2f,
                     [](glm::vec2 xz, float y) { return y > kDoorHeadroom && kRoomDoorway.holds(xz); });
  for (int s = 0; s < pxe::kSurfCount; ++s) {
    if (!indices[s].empty()) {
      scene.meshes.emplace_back();
      scene.meshes.back().upload(verts[s], indices[s]);
    }
    if (!shadowIndices[s].empty()) {
      scene.shadowOnly.emplace_back();
      scene.shadowOnly.back().upload(shadowVerts[s], shadowIndices[s]);
    }
  }
  // The hearth, a campfire's size, the brazier, the candles and the
  // sconces, each roofed over. The hearth and candles burn at every hour.
  scene.fires.push_back({kHearthFire, false, 1.f});
  // The brazier burns only at night (the game lights it at dusk).
  scene.fires.push_back({kBrazierFire, false, kBrazierSize, true});
  for (const glm::vec3& c : kRoomCandles) scene.fires.push_back({c, false, kRoomCandleSize});
  // The sconces too burn only at night, as the brazier does.
  for (const glm::vec3& c : kRoomSconces) scene.fires.push_back({c, false, kSconceSize, true});
  for (pxe::FireSite& f : scene.fires) {
    f.sheltered = scene.shelter.sheltered(f.base);
    const char* what = f.size >= 1.f            ? "hearth"
                       : f.size >= kBrazierSize ? "brazier"
                       : f.size >= kSconceSize  ? "sconce"
                                                : "candle";
    std::fprintf(stderr, "pxe: %s at %.1f, %.1f %s\n", what, f.base.x, f.base.z,
                 f.sheltered ? "sheltered" : "open to the rain");
  }
  std::fprintf(stderr, "pxe: tavern %zu primitives, %zu shadow-only triangles\n", prims.size(), shadowOnly);
  return scene;
}

}  // namespace

glm::vec3 tavernPoint(glm::vec3 local) { return kTavernSpot + turned(local, kTavernYaw); }

glm::vec2 tavernLocal(glm::vec2 world) {
  const glm::vec3 local = turned({world.x - kTavernSpot.x, 0.f, world.y - kTavernSpot.z}, -kTavernYaw);
  return {local.x, local.z};
}

bool inTavernFootprint(glm::vec2 world) {
  const glm::vec2 p = tavernLocal(world);
  bool inside = false;
  const std::size_t n = std::size(kTavernFootprint);
  for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
    const glm::vec2 a = kTavernFootprint[i], b = kTavernFootprint[j];
    if ((a.y > p.y) != (b.y > p.y) && p.x < (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
  }
  return inside;
}

pxe::Scene buildScene(const BuildOptions& options) {
  pxe::Scene scene = options.scene == "hills"    ? buildHills(options)
                     : options.scene == "tavern" ? buildTavern(options)
                                                 : buildRuins(options);
  std::fprintf(stderr, "pxe: scene %s, %zu sprites\n", options.scene.c_str(), scene.grass.size());
  return scene;
}

SceneLayout laidOut(const BuildOptions& options) {
  SceneLayout layout;
  if (options.scene == "hills" || options.scene == "tavern") return layout;
  pxe::Scene scene;
  setUpRuins(scene);
  RuinsGeometry geo;
  layOutRuins(options, scene, geo);
  layout.campfires = std::move(geo.campfires);
  layout.solids = std::move(geo.solids);
  layout.ground = std::move(scene.ground);
  layout.lawn.build(geo.lawnCorners);
  layout.bedsLo = glm::vec2{scene.focus.x, scene.focus.z} - 20.f;
  layout.bedsHi = glm::vec2{scene.focus.x, scene.focus.z} + 20.f;
  return layout;
}

}  // namespace game
