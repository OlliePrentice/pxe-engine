#pragma once

#include "pxe/scene.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace pxe {

struct PointLight;

// Grass fire, after t3ssel8r's fire, rain and grass: a grid of cells over
// the ground, kept in render textures and run by fragment-shader passes
// that ping-pong between two of them, so the whole field burns on the GPU.
// Each cell holds how hard it burns, how much dry grass it has left and how
// wet it is. A burning cell sets its neighbours alight at random, faster
// downwind; rain soaks the cells open to the sky, and a wet cell neither
// catches nor keeps burning. Burning cells paint a burn texture (it only
// ever darkens) that the grass and the ground read to char and shorten the
// blades. Flames are drawn into the fire's heat buffer straight from the
// grid, and each burning patch of a few metres lights the scene through a
// point light of its own; a blurred copy of the grid adds a low glow over
// the burning ground and the embers left behind. Brush strokes paint into
// the grid: fire sets the grass alight, water soaks it and puts fire out,
// and regrowth brings burnt grass back.
//
// Tree crowns are a second layer over the same cells, their leaves the
// fuel. A crown warms over burning grass until it catches, then spreads
// crown to crown downwind and rains embers on the grass below; its leaves
// char and fall, and the crown burns away to the bare trunk.
class FireGrid {
 public:
  // Metres per cell; the burn texture is kBurnScale times finer.
  static constexpr float kCell = 0.25f;
  static constexpr int kBurnScale = 2;
  // The grid steps this often; everything drawn from it is blended between
  // the last two steps, so it changes smoothly every frame.
  static constexpr float kStep = 1.f / 20.f;
  // Cells per side of a patch that gets one point light.
  static constexpr int kPatch = 8;

  // A stroke of a brush from `from` to `to` (world xz), `radius` metres
  // wide, soft at its rim. Each is painted once, at the next step.
  struct Stroke {
    enum Kind { kFire, kWater, kRegrow };
    glm::vec2 from{0.f};
    glm::vec2 to{0.f};
    float radius = 1.f;
    int kind = kFire;
  };

  FireGrid() = default;
  ~FireGrid();
  FireGrid(const FireGrid&) = delete;
  FireGrid& operator=(const FireGrid&) = delete;

  void init();
  // Drop the grid; the next advance builds it afresh for the scene it is
  // given. Call whenever the scene changes.
  void reset() { built_ = false; }
  // Whether the grid is built: from the first advance after construction or
  // reset() until the next reset().
  bool running() const { return built_; }
  // Put the fire out at once: every burning cell and crown, its embers and
  // glow, and any ignitions and brush strokes still waiting, as if it had
  // never been lit, but keeping the fuel it burnt, how wet the ground is,
  // and the burn texture - so burnt grass stays burnt, and does not burn
  // again. The grid stays built.
  void quench();

  // Bring the grid to `time` seconds, first setting alight the cells near
  // each of `ignite` (world xz) and painting `strokes`. `rain` 0 dry .. 1
  // downpour; `wind` world xz the wind blows toward, scaled by its strength.
  // Leaves GL state as the renderer expects (no blending, its framebuffer
  // unbound). The first advance after construction or reset() builds the
  // grid from `scene`; until the next reset(), every advance must be given
  // that same scene.
  void advance(const Scene& scene, float time, const std::vector<glm::vec2>& ignite, const std::vector<Stroke>& strokes,
               float rain, glm::vec2 wind);
  // How long a fire set alight by hand keeps spreading, in seconds of
  // burning down the line of cells it spreads through; 0 for ever.
  void setLifetime(float seconds) { lifetime_ = seconds; }

  // Point lights for the burning patches, blended between the last two
  // readbacks of the grid.
  void addLights(std::vector<PointLight>& out, float fireLight, float fireReach, float time) const;
  // Flames, embers and smoke into the bound heat buffer (blending on).
  void drawFlames(const glm::mat4& viewProj, glm::vec2 target, float pxPerMetre, float depthBiasNdc, float time,
                  glm::vec2 wind) const;

  // The burn texture and where it lies: xz of its corner, and 1 / its size
  // in metres. r the grass, g the crowns over it: 0 untouched .. 1 burnt out.
  unsigned burnTexture() const { return burn_; }
  glm::vec4 burnDomain() const { return {min_.x, min_.y, 1.f / (kCell * w_), 1.f / (kCell * h_)}; }
  // The grid blurred, per cell over the same domain: r fire, g embers, b
  // ground height times a, a how much of the neighbourhood has grass. One
  // per step, the latest and the one before, blended by glowBlend().
  unsigned glowTexture(bool latest) const { return glow_[latest ? cur_ : 1 - cur_]; }
  float glowBlend() const { return blend_; }

 private:
  void build(const Scene& scene, float rain);
  void destroy();
  void step(float rain, glm::vec2 wind);
  void regrowBurn(const std::vector<Stroke>& strokes);
  void readPatches();

  unsigned progStep_ = 0, progBurn_ = 0, progSum_ = 0, progFlame_ = 0, progGlow_ = 0;
  unsigned vao_ = 0;
  unsigned flameVao_ = 0, flameVbo_ = 0;  // per instance: a patch's first cell
  unsigned ground_ = 0;    // fuel, roofed, ground height
  unsigned state_[2] = {};  // fire, fuel left, wetness, embers
  unsigned canopyGround_ = 0;  // leaves, mean crown height, crown top
  unsigned canopy_[2] = {};    // crown fire, leaves left, warmth, embers; drawn with state_
  unsigned life_[2] = {};      // grass and crown fire's life left; drawn with state_
  float lifetime_ = 0.f;
  unsigned fbo_[2] = {};
  unsigned burn_ = 0, fboBurn_ = 0;
  unsigned glow_[2] = {}, fboGlow_[2] = {};  // blurred alongside state_
  unsigned patches_ = 0, fboPatches_ = 0;  // per patch: summed fire, fire-weighted x and z, summed embers
  unsigned crownPatches_ = 0;  // per patch: summed crown fire, fire-weighted x, z and height; drawn with patches_
  unsigned pbo_[2] = {}, crownPbo_[2] = {};
  bool built_ = false;  // the grid is built for the scene given since the last reset()
  glm::vec2 min_{0.f};
  int w_ = 0, h_ = 0;    // cells
  int pw_ = 0, ph_ = 0;  // patches
  std::vector<float> height_;  // ground height per cell, for the lights
  int cur_ = 0;               // state_[cur_] is the latest step
  long long tick_ = 0;        // steps taken
  float stepTime_ = 0.f;      // time of the latest step
  float lastTime_ = -1.f;
  float blend_ = 1.f;         // how far this frame is from the step before the latest to the latest
  std::vector<glm::vec2> ignite_;        // waiting for the next step
  std::vector<glm::vec2> recentIgnite_;  // set alight at the latest step
  std::vector<Stroke> strokes_;          // waiting for the next step
  std::vector<glm::ivec4> recentStrokes_;  // patches (lo x, lo z, hi x, hi z) painted with fire at the latest step
  // Per glow texture, the patches that could glow when it was last redrawn
  // (lo x, lo z, hi x, hi z; empty if lo > hi). Each redraw covers those as
  // well as the patches that can glow now, so it clears where fire has left.
  int glowBox_[2][4] = {};
  int reads_ = 0;  // readbacks issued
  // The last two patch readbacks, and which patches burn (with their
  // neighbours, which may catch before the next readback).
  std::vector<float> patchPrev_, patchCur_;
  std::vector<float> crownPrev_, crownCur_;
  std::vector<int> active_;
};

}  // namespace pxe
