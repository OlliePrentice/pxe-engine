#include "pxe/firegrid.hpp"
#include "pxe/renderer.hpp"

#include "gl.hpp"
#include "shader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pxe {
namespace {

constexpr int kIgniteMax = 8;
constexpr int kStrokeMax = 8;
// Per cell the flame shader draws a glow, two flames, a puff of smoke and an ember,
// and in a crown a glow, a flame and smoke or a spark.
constexpr int kParts = 8;
// The light from a patch of burning cells: it shines from this high, and
// at full strength once this many cells burn fully in it. Patches' lights
// overlap several deep, so each is a share of a campfire's.
constexpr float kLightHeight = 0.45f;
constexpr float kFullPatch = 16.f;
constexpr float kPatchLight = 0.3f;
// A burning crown's patch light sits this far over the fire-weighted
// height of its leaves, and is this share of a campfire's.
constexpr float kCrownLightHeight = 0.3f;
constexpr float kCrownLight = 0.45f;

int uloc(unsigned prog, const char* name) { return glGetUniformLocation(prog, name); }

unsigned makeTexture(int w, int h, int internal, int format, int type, const void* data, int filter) {
  unsigned tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, internal, w, h, 0, static_cast<GLenum>(format), static_cast<GLenum>(type), data);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  return tex;
}

unsigned makeFbo(unsigned tex, unsigned second = 0, unsigned third = 0) {
  unsigned fbo = 0;
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  if (second) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, second, 0);
  if (third) glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, third, 0);
  const unsigned draw[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
  glDrawBuffers(third ? 3 : second ? 2 : 1, draw);
  return fbo;
}

}  // namespace

FireGrid::~FireGrid() {
  destroy();
  for (unsigned prog : {progStep_, progBurn_, progSum_, progFlame_, progGlow_})
    if (prog) glDeleteProgram(prog);
  if (vao_) glDeleteVertexArrays(1, &vao_);
  if (flameVao_) glDeleteVertexArrays(1, &flameVao_);
  if (flameVbo_) glDeleteBuffers(1, &flameVbo_);
}

void FireGrid::init(const std::string& dir) {
  progStep_ = linkProgram(dir + "/edge.vert", dir + "/gridstep.frag");
  progBurn_ = linkProgram(dir + "/edge.vert", dir + "/gridburn.frag");
  progSum_ = linkProgram(dir + "/edge.vert", dir + "/gridsum.frag");
  progGlow_ = linkProgram(dir + "/edge.vert", dir + "/gridglow.frag");
  progFlame_ = linkProgram(dir + "/gridflame.vert", dir + "/fire.frag");
  glGenVertexArrays(1, &vao_);
  glGenVertexArrays(1, &flameVao_);
  glGenBuffers(1, &flameVbo_);
  glBindVertexArray(flameVao_);
  glBindBuffer(GL_ARRAY_BUFFER, flameVbo_);
  glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));
  glVertexAttribDivisor(0, 1);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void FireGrid::destroy() {
  unsigned textures[] = {ground_,  state_[0], state_[1], burn_,         patches_,
                         glow_[0], glow_[1],  canopyGround_, canopy_[0], canopy_[1], crownPatches_,
                         life_[0], life_[1]};
  glDeleteTextures(13, textures);
  unsigned fbos[] = {fbo_[0], fbo_[1], fboBurn_, fboPatches_, fboGlow_[0], fboGlow_[1]};
  glDeleteFramebuffers(6, fbos);
  if (pbo_[0]) glDeleteBuffers(2, pbo_);
  if (crownPbo_[0]) glDeleteBuffers(2, crownPbo_);
  ground_ = state_[0] = state_[1] = burn_ = patches_ = glow_[0] = glow_[1] = 0;
  canopyGround_ = canopy_[0] = canopy_[1] = crownPatches_ = life_[0] = life_[1] = 0;
  crownPbo_[0] = crownPbo_[1] = 0;
  fbo_[0] = fbo_[1] = fboBurn_ = fboPatches_ = fboGlow_[0] = fboGlow_[1] = 0;
  pbo_[0] = pbo_[1] = 0;
}

void FireGrid::build(const Scene& scene, float rain) {
  destroy();
  // The grid covers the lawn and the tree crowns: every blade clump, tuft
  // and leaf cluster, a metre round.
  glm::vec2 lo{1e9f}, hi{-1e9f};
  for (const GrassInstance& g : scene.grass) {
    lo = glm::min(lo, glm::vec2{g.px, g.pz});
    hi = glm::max(hi, glm::vec2{g.px, g.pz});
  }
  if (lo.x > hi.x) lo = hi = glm::vec2{0.f};
  min_ = lo - 1.f;
  w_ = std::max(1, static_cast<int>(std::ceil((hi.x + 1.f - min_.x) / kCell)));
  h_ = std::max(1, static_cast<int>(std::ceil((hi.y + 1.f - min_.y) / kCell)));
  pw_ = (w_ + kPatch - 1) / kPatch;
  ph_ = (h_ + kPatch - 1) / kPatch;

  // A cell with grass rooted in it has fuel. Cells the scatter happened to
  // miss inside a lawn are filled in from their neighbours, so a fire does
  // not stop at a gap a blade wide.
  const auto cells = static_cast<std::size_t>(w_ * h_);
  std::vector<int> count(cells, 0);
  std::vector<float> sumY(cells, 0.f);
  for (const GrassInstance& g : scene.grass) {
    if (g.kind > 1.5f) continue;
    const int x = std::clamp(static_cast<int>((g.px - min_.x) / kCell), 0, w_ - 1);
    const int z = std::clamp(static_cast<int>((g.pz - min_.y) / kCell), 0, h_ - 1);
    ++count[static_cast<std::size_t>(z * w_ + x)];
    sumY[static_cast<std::size_t>(z * w_ + x)] += g.py;
  }
  std::vector<float> fuel(cells, 0.f);
  height_.assign(cells, 0.f);
  for (int z = 0; z < h_; ++z)
    for (int x = 0; x < w_; ++x) {
      const auto i = static_cast<std::size_t>(z * w_ + x);
      if (count[i] > 0) {
        fuel[i] = 1.f;
        height_[i] = sumY[i] / static_cast<float>(count[i]);
        continue;
      }
      int around = 0;
      float y = 0.f;
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
          const int qx = x + dx, qz = z + dz;
          if (qx < 0 || qz < 0 || qx >= w_ || qz >= h_) continue;
          const auto q = static_cast<std::size_t>(qz * w_ + qx);
          if (count[q] == 0) continue;
          ++around;
          y += sumY[q] / static_cast<float>(count[q]);
        }
      if (around >= 4) {
        fuel[i] = 0.7f;
        height_[i] = y / static_cast<float>(around);
      }
    }
  // Ground: fuel, whether a roof keeps the rain off, ground height. State:
  // fire, fuel left, wetness, embers. Ground already soaked if it is raining.
  std::vector<float> ground(cells * 4, 0.f), state(cells * 4, 0.f);
  for (int z = 0; z < h_; ++z)
    for (int x = 0; x < w_; ++x) {
      const auto i = static_cast<std::size_t>(z * w_ + x);
      const glm::vec3 p{min_.x + (static_cast<float>(x) + 0.5f) * kCell, height_[i],
                        min_.y + (static_cast<float>(z) + 0.5f) * kCell};
      const bool roofed = fuel[i] > 0.f && scene.shelter.sheltered(p);
      ground[i * 4 + 0] = fuel[i];
      ground[i * 4 + 1] = roofed ? 1.f : 0.f;
      ground[i * 4 + 2] = height_[i];
      state[i * 4 + 1] = fuel[i];
      state[i * 4 + 2] = roofed ? 0.f : std::clamp(2.f * rain, 0.f, 1.f);
    }
  // Crowns: a cell with leaf clusters in it has leaves to burn, at their
  // mean height and up to the highest. Gaps a cell or two wide inside a
  // crown are filled in from round them, so the whole crown can burn and
  // char, edge to edge.
  std::vector<int> leafCount(cells, 0);
  std::vector<float> leafSum(cells, 0.f), leafTop(cells, -1e9f);
  for (const GrassInstance& g : scene.grass) {
    if (g.kind < 1.5f) continue;
    const int x = std::clamp(static_cast<int>((g.px - min_.x) / kCell), 0, w_ - 1);
    const int z = std::clamp(static_cast<int>((g.pz - min_.y) / kCell), 0, h_ - 1);
    const auto i = static_cast<std::size_t>(z * w_ + x);
    ++leafCount[i];
    leafSum[i] += g.py;
    leafTop[i] = std::max(leafTop[i], g.py);
  }
  std::vector<float> canopyGround(cells * 4, 0.f), canopy(cells * 4, 0.f);
  for (int pass = 0; pass < 2; ++pass) {
    const std::vector<int> had = leafCount;
    const std::vector<float> sum = leafSum, top = leafTop;
    for (int z = 0; z < h_; ++z)
      for (int x = 0; x < w_; ++x) {
        const auto i = static_cast<std::size_t>(z * w_ + x);
        if (had[i] > 0) continue;
        int around = 0, n = 0;
        float y = 0.f, t = -1e9f;
        for (int dz = -1; dz <= 1; ++dz)
          for (int dx = -1; dx <= 1; ++dx) {
            const int qx = x + dx, qz = z + dz;
            if (qx < 0 || qz < 0 || qx >= w_ || qz >= h_) continue;
            const auto q = static_cast<std::size_t>(qz * w_ + qx);
            if (had[q] == 0) continue;
            ++around;
            n += had[q];
            y += sum[q];
            t = std::max(t, top[q]);
          }
        if (around >= 3) {
          leafCount[i] = n / around + 1;
          leafSum[i] = y / static_cast<float>(n) * static_cast<float>(leafCount[i]);
          leafTop[i] = t;
        }
      }
  }
  for (std::size_t i = 0; i < cells; ++i) {
    if (leafCount[i] == 0) continue;
    canopyGround[i * 4 + 0] = 1.f;
    canopyGround[i * 4 + 1] = leafSum[i] / static_cast<float>(leafCount[i]);
    canopyGround[i * 4 + 2] = leafTop[i];
    canopy[i * 4 + 1] = 1.f;
  }
  if (const char* debug = std::getenv("PXE_FIRE_DEBUG"); debug && debug[0] == '3') {
    // Each crown, as a patch of joined canopy cells: where it is and how high.
    std::vector<char> seen(cells, 0);
    for (std::size_t i = 0; i < cells; ++i) {
      if (leafCount[i] == 0 || seen[i]) continue;
      std::vector<std::size_t> todo{i};
      seen[i] = 1;
      glm::vec2 sum{0.f};
      float top = -1e9f;
      int n = 0;
      while (!todo.empty()) {
        const std::size_t q = todo.back();
        todo.pop_back();
        const int x = static_cast<int>(q % static_cast<std::size_t>(w_)), z = static_cast<int>(q / static_cast<std::size_t>(w_));
        sum += glm::vec2{min_.x + (static_cast<float>(x) + 0.5f) * kCell, min_.y + (static_cast<float>(z) + 0.5f) * kCell};
        top = std::max(top, leafTop[q]);
        ++n;
        for (const glm::ivec2 d : {glm::ivec2{1, 0}, glm::ivec2{-1, 0}, glm::ivec2{0, 1}, glm::ivec2{0, -1}}) {
          const int qx = x + d.x, qz = z + d.y;
          if (qx < 0 || qz < 0 || qx >= w_ || qz >= h_) continue;
          const auto r = static_cast<std::size_t>(qz * w_ + qx);
          if (leafCount[r] > 0 && !seen[r]) {
            seen[r] = 1;
            todo.push_back(r);
          }
        }
      }
      if (n >= 40) std::fprintf(stderr, "pxe: crown at %.1f, %.1f, top %.1f m, %d cells\n", sum.x / n, sum.y / n, top, n);
    }
  }
  ground_ = makeTexture(w_, h_, GL_RGBA32F, GL_RGBA, GL_FLOAT, ground.data(), GL_NEAREST);
  canopyGround_ = makeTexture(w_, h_, GL_RGBA32F, GL_RGBA, GL_FLOAT, canopyGround.data(), GL_NEAREST);
  for (int i = 0; i < 2; ++i) {
    state_[i] = makeTexture(w_, h_, GL_RGBA16F, GL_RGBA, GL_FLOAT, state.data(), GL_NEAREST);
    canopy_[i] = makeTexture(w_, h_, GL_RGBA16F, GL_RGBA, GL_FLOAT, canopy.data(), GL_NEAREST);
    // Full floats: a long life runs down by less per step than half
    // precision can tell from 1.
    life_[i] = makeTexture(w_, h_, GL_RG32F, GL_RG, GL_FLOAT, nullptr, GL_NEAREST);
    fbo_[i] = makeFbo(state_[i], canopy_[i], life_[i]);
    const float none[4] = {0.f, 0.f, 0.f, 0.f};
    glClearBufferfv(GL_COLOR, 2, none);
  }
  const std::vector<float> dark(cells * 4, 0.f);
  for (int i = 0; i < 2; ++i) {
    glow_[i] = makeTexture(w_, h_, GL_RGBA16F, GL_RGBA, GL_FLOAT, dark.data(), GL_LINEAR);
    fboGlow_[i] = makeFbo(glow_[i]);
    glowBox_[i][0] = pw_;
    glowBox_[i][1] = ph_;
    glowBox_[i][2] = glowBox_[i][3] = -1;
  }
  std::vector<std::uint8_t> clear(cells * kBurnScale * kBurnScale * 2, 0);
  burn_ = makeTexture(w_ * kBurnScale, h_ * kBurnScale, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, clear.data(), GL_LINEAR);
  fboBurn_ = makeFbo(burn_);
  patches_ = makeTexture(pw_, ph_, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr, GL_NEAREST);
  crownPatches_ = makeTexture(pw_, ph_, GL_RGBA32F, GL_RGBA, GL_FLOAT, nullptr, GL_NEAREST);
  fboPatches_ = makeFbo(patches_, crownPatches_);
  glGenBuffers(2, pbo_);
  glGenBuffers(2, crownPbo_);
  for (unsigned pbo : {pbo_[0], pbo_[1], crownPbo_[0], crownPbo_[1]}) {
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo);
    glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(pw_ * ph_ * 4 * sizeof(float)), nullptr, GL_STREAM_READ);
  }
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (std::getenv("PXE_FIRE_DEBUG"))
    std::fprintf(stderr, "pxe: grass fire grid %dx%d cells from (%.1f, %.1f), %dx%d patches\n", w_, h_, min_.x, min_.y, pw_,
                 ph_);
  scene_ = &scene;
  cur_ = 0;
  tick_ = 0;
  reads_ = 0;
  lastTime_ = -1.f;
  patchPrev_.assign(static_cast<std::size_t>(pw_ * ph_ * 4), 0.f);
  patchCur_ = patchPrev_;
  crownPrev_ = patchPrev_;
  crownCur_ = patchPrev_;
  active_.clear();
  ignite_.clear();
  strokes_.clear();
}

void FireGrid::quench() {
  if (!running()) return;
  // Fire (r) and embers (a) in the grass, and in the crowns their fire (r),
  // warmth (b) and embers (a), in both of the ping-pong pair; and the fire's
  // life left. Fuel and leaves left (g) and wetness (grass b) stay.
  const float none[4] = {0.f, 0.f, 0.f, 0.f};
  for (int i = 0; i < 2; ++i) {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_[i]);
    glColorMaski(0, GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearBufferfv(GL_COLOR, 0, none);
    glColorMaski(1, GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glClearBufferfv(GL_COLOR, 1, none);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearBufferfv(GL_COLOR, 2, none);
    glBindFramebuffer(GL_FRAMEBUFFER, fboGlow_[i]);
    glClearBufferfv(GL_COLOR, 0, none);
    glowBox_[i][0] = pw_;
    glowBox_[i][1] = ph_;
    glowBox_[i][2] = glowBox_[i][3] = -1;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fboPatches_);
  glClearBufferfv(GL_COLOR, 0, none);
  glClearBufferfv(GL_COLOR, 1, none);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  // Nothing burns now: no lights or flames, and the next readback waits on
  // the GPU again rather than reading one queued while the fire burned.
  std::fill(patchPrev_.begin(), patchPrev_.end(), 0.f);
  std::fill(patchCur_.begin(), patchCur_.end(), 0.f);
  std::fill(crownPrev_.begin(), crownPrev_.end(), 0.f);
  std::fill(crownCur_.begin(), crownCur_.end(), 0.f);
  reads_ = 0;
  active_.clear();
  ignite_.clear();
  recentIgnite_.clear();
  strokes_.clear();
  recentStrokes_.clear();
  lastTime_ = -1.f;
}

void FireGrid::advance(const Scene& scene, float time, const std::vector<glm::vec2>& ignite,
                       const std::vector<Stroke>& strokes, float rain, glm::vec2 wind) {
  if (scene_ != &scene) build(scene, rain);
  ignite_.insert(ignite_.end(), ignite.begin(), ignite.end());
  strokes_.insert(strokes_.end(), strokes.begin(), strokes.end());
  // Steps run ahead of the frame: the latest is at or just after `time`,
  // and what is drawn is blended from the one before it. A first frame, or
  // a jump in time, takes one step now.
  if (lastTime_ < 0.f || time < lastTime_ || time - lastTime_ > 0.5f) stepTime_ = time - kStep;
  lastTime_ = time;
  for (int n = 0; stepTime_ < time && n < 4; ++n) {
    step(rain, wind);
    stepTime_ += kStep;
  }
  if (stepTime_ < time) stepTime_ = time;  // fell behind: carry on from here

  // Patches to draw flames over: those burning or glowing at the last
  // readback, their neighbours (fire may have crossed since), and any just
  // set alight.
  std::vector<char> want(static_cast<std::size_t>(pw_ * ph_), 0);
  for (int pz = 0; pz < ph_; ++pz)
    for (int px = 0; px < pw_; ++px) {
      const auto i = static_cast<std::size_t>((pz * pw_ + px) * 4);
      if (patchCur_[i] <= 0.f && patchCur_[i + 3] <= 0.f && crownCur_[i] <= 0.f) continue;
      for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx) {
          const int qx = px + dx, qz = pz + dz;
          if (qx >= 0 && qz >= 0 && qx < pw_ && qz < ph_) want[static_cast<std::size_t>(qz * pw_ + qx)] = 1;
        }
    }
  for (const glm::vec2& p : recentIgnite_) {
    const int px = static_cast<int>((p.x - min_.x) / (kCell * kPatch));
    const int pz = static_cast<int>((p.y - min_.y) / (kCell * kPatch));
    if (px >= 0 && pz >= 0 && px < pw_ && pz < ph_) want[static_cast<std::size_t>(pz * pw_ + px)] = 1;
  }
  for (const glm::ivec4& b : recentStrokes_)
    for (int pz = std::max(b.y, 0); pz <= std::min(b.w, ph_ - 1); ++pz)
      for (int px = std::max(b.x, 0); px <= std::min(b.z, pw_ - 1); ++px) want[static_cast<std::size_t>(pz * pw_ + px)] = 1;
  active_.clear();
  for (int i = 0; i < pw_ * ph_; ++i)
    if (want[static_cast<std::size_t>(i)]) active_.push_back(i);
  blend_ = std::clamp(1.f - (stepTime_ - time) / kStep, 0.f, 1.f);
}

void FireGrid::step(float rain, glm::vec2 wind) {
  glDisable(GL_BLEND);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(vao_);

  // This step's brush strokes, in cell units: from, to, then radius, kind.
  const std::size_t strokeCount = std::min<std::size_t>(strokes_.size(), kStrokeMax);
  std::vector<Stroke> strokes(strokes_.begin(), strokes_.begin() + static_cast<std::ptrdiff_t>(strokeCount));
  strokes_.erase(strokes_.begin(), strokes_.begin() + static_cast<std::ptrdiff_t>(strokeCount));
  std::vector<float> ends, shapes;
  for (const Stroke& k : strokes) {
    ends.insert(ends.end(), {(k.from.x - min_.x) / kCell, (k.from.y - min_.y) / kCell, (k.to.x - min_.x) / kCell,
                             (k.to.y - min_.y) / kCell});
    shapes.insert(shapes.end(), {k.radius / kCell, static_cast<float>(k.kind)});
  }
  auto bindStrokes = [&](unsigned prog) {
    glUniform1i(uloc(prog, "uStrokeCount"), static_cast<int>(strokes.size()));
    if (strokes.empty()) return;
    glUniform4fv(uloc(prog, "uStroke"), static_cast<GLsizei>(strokes.size()), ends.data());
    glUniform2fv(uloc(prog, "uStrokeShape"), static_cast<GLsizei>(strokes.size()), shapes.data());
  };

  // The automaton: the latest state in, the next one out.
  const int next = 1 - cur_;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_[next]);
  glViewport(0, 0, w_, h_);
  glUseProgram(progStep_);
  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, state_[cur_]);
  glActiveTexture(GL_TEXTURE12);
  glBindTexture(GL_TEXTURE_2D, ground_);
  glActiveTexture(GL_TEXTURE13);
  glBindTexture(GL_TEXTURE_2D, canopy_[cur_]);
  glActiveTexture(GL_TEXTURE14);
  glBindTexture(GL_TEXTURE_2D, canopyGround_);
  glUniform1i(uloc(progStep_, "uState"), 11);
  glUniform1i(uloc(progStep_, "uGround"), 12);
  glUniform1i(uloc(progStep_, "uCanopy"), 13);
  glUniform1i(uloc(progStep_, "uCanopyGround"), 14);
  glActiveTexture(GL_TEXTURE15);
  glBindTexture(GL_TEXTURE_2D, life_[cur_]);
  glUniform1i(uloc(progStep_, "uLife"), 15);
  glUniform1f(uloc(progStep_, "uLifeStep"), lifetime_ > 0.f ? kStep / lifetime_ : 0.f);
  glUniform1f(uloc(progStep_, "uDt"), kStep);
  glUniform1f(uloc(progStep_, "uRain"), rain);
  glUniform2f(uloc(progStep_, "uWind"), wind.x, wind.y);
  glUniform1f(uloc(progStep_, "uTick"), static_cast<float>(tick_ % 4096));
  std::vector<float> cells;
  for (const glm::vec2& p : ignite_) {
    if (static_cast<int>(cells.size()) >= 2 * kIgniteMax) break;
    cells.push_back((p.x - min_.x) / kCell);
    cells.push_back((p.y - min_.y) / kCell);
  }
  glUniform1i(uloc(progStep_, "uIgniteCount"), static_cast<int>(cells.size() / 2));
  if (!cells.empty()) glUniform2fv(uloc(progStep_, "uIgnite"), static_cast<GLsizei>(cells.size() / 2), cells.data());
  bindStrokes(progStep_);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // Grass burns only in the patches near the fire, so only those can need
  // painting: the ones burning at the last readback and round them, and any
  // being set alight or brushed.
  int lo[2] = {pw_, ph_}, hi[2] = {-1, -1};
  auto take = [&](int px, int pz) {
    lo[0] = std::min(lo[0], px - 1);
    lo[1] = std::min(lo[1], pz - 1);
    hi[0] = std::max(hi[0], px + 1);
    hi[1] = std::max(hi[1], pz + 1);
  };
  for (int i : active_) take(i % pw_, i / pw_);
  for (const glm::vec2& p : ignite_)
    take(static_cast<int>((p.x - min_.x) / (kCell * kPatch)), static_cast<int>((p.y - min_.y) / (kCell * kPatch)));
  recentStrokes_.clear();
  int regrowLo[2] = {pw_, ph_}, regrowHi[2] = {-1, -1};
  for (const Stroke& k : strokes) {
    const glm::vec2 a = (glm::min(k.from, k.to) - k.radius - min_) / (kCell * kPatch);
    const glm::vec2 b = (glm::max(k.from, k.to) + k.radius - min_) / (kCell * kPatch);
    const glm::ivec4 box{static_cast<int>(std::floor(a.x)), static_cast<int>(std::floor(a.y)),
                         static_cast<int>(std::floor(b.x)), static_cast<int>(std::floor(b.y))};
    take(box.x, box.y);
    take(box.z, box.w);
    if (k.kind == Stroke::kFire) recentStrokes_.push_back(box);
    if (k.kind == Stroke::kRegrow) {
      regrowLo[0] = std::min(regrowLo[0], box.x);
      regrowLo[1] = std::min(regrowLo[1], box.y);
      regrowHi[0] = std::max(regrowHi[0], box.z);
      regrowHi[1] = std::max(regrowHi[1], box.w);
    }
  }
  recentIgnite_ = ignite_;
  ignite_.clear();
  cur_ = next;
  ++tick_;
  auto scissorPatches = [&](const int* l, const int* h, int texels) {
    const int x0 = std::max(l[0], 0), z0 = std::max(l[1], 0);
    const int x1 = std::min(h[0], pw_ - 1), z1 = std::min(h[1], ph_ - 1);
    if (x1 < x0 || z1 < z0) return false;
    glEnable(GL_SCISSOR_TEST);
    glScissor(x0 * texels, z0 * texels, (x1 - x0 + 1) * texels, (z1 - z0 + 1) * texels);
    return true;
  };

  // Burning cells paint the burn texture; it keeps the darkest it has seen.
  glBindFramebuffer(GL_FRAMEBUFFER, fboBurn_);
  glViewport(0, 0, w_ * kBurnScale, h_ * kBurnScale);
  glUseProgram(progBurn_);
  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, state_[cur_]);
  glActiveTexture(GL_TEXTURE13);
  glBindTexture(GL_TEXTURE_2D, canopy_[cur_]);
  glUniform1i(uloc(progBurn_, "uState"), 11);
  glUniform1i(uloc(progBurn_, "uGround"), 12);
  glUniform1i(uloc(progBurn_, "uCanopy"), 13);
  glUniform1i(uloc(progBurn_, "uCanopyGround"), 14);
  glUniform2f(uloc(progBurn_, "uMin"), min_.x, min_.y);
  glUniform1f(uloc(progBurn_, "uTexel"), kCell / kBurnScale);
  glUniform1f(uloc(progBurn_, "uScale"), static_cast<float>(kBurnScale));
  glUniform1i(uloc(progBurn_, "uRegrow"), 0);
  glUniform1i(uloc(progBurn_, "uStrokeCount"), 0);
  if (scissorPatches(lo, hi, kPatch * kBurnScale)) {
    glEnable(GL_BLEND);
    glBlendEquation(GL_MAX);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  // Under a regrowth brush the grass comes back, so there the burn texture
  // is redrawn from the grass left and keeps the lightest it has seen.
  if (scissorPatches(regrowLo, regrowHi, kPatch * kBurnScale)) {
    glUniform1i(uloc(progBurn_, "uRegrow"), 1);
    bindStrokes(progBurn_);
    glEnable(GL_BLEND);
    glBlendEquation(GL_MIN);
    glBlendFunc(GL_ONE, GL_ONE);
    glDrawArrays(GL_TRIANGLES, 0, 3);
  }
  glBlendEquation(GL_FUNC_ADD);
  glDisable(GL_BLEND);
  glDisable(GL_SCISSOR_TEST);

  // The blurred grid for the glow, over the patches that can glow now and
  // those that could when this texture was last drawn, which clears them.
  {
    int* last = glowBox_[cur_];
    const int l[2] = {std::min(lo[0], last[0]), std::min(lo[1], last[1])};
    const int h[2] = {std::max(hi[0], last[2]), std::max(hi[1], last[3])};
    last[0] = lo[0];
    last[1] = lo[1];
    last[2] = hi[0];
    last[3] = hi[1];
    glBindFramebuffer(GL_FRAMEBUFFER, fboGlow_[cur_]);
    glViewport(0, 0, w_, h_);
    if (scissorPatches(l, h, kPatch)) {
      glUseProgram(progGlow_);
      glUniform1i(uloc(progGlow_, "uState"), 11);
      glUniform1i(uloc(progGlow_, "uGround"), 12);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glDisable(GL_SCISSOR_TEST);
    }
  }

  // Each patch's fire summed, for its light.
  glBindFramebuffer(GL_FRAMEBUFFER, fboPatches_);
  glViewport(0, 0, pw_, ph_);
  glUseProgram(progSum_);
  glUniform1i(uloc(progSum_, "uState"), 11);
  glUniform1i(uloc(progSum_, "uCanopy"), 13);
  glUniform1i(uloc(progSum_, "uCanopyGround"), 14);
  glUniform2f(uloc(progSum_, "uMin"), min_.x, min_.y);
  glUniform1f(uloc(progSum_, "uCell"), kCell);
  glUniform1i(uloc(progSum_, "uPatch"), kPatch);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  readPatches();

  glBindVertexArray(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glEnable(GL_DEPTH_TEST);
}

void FireGrid::readPatches() {
  // Read back a step late through pixel buffers, so the CPU never waits on
  // the GPU. The first readback waits, so a fire lights up at once.
  const auto n = static_cast<std::size_t>(pw_ * ph_ * 4);
  patchPrev_ = patchCur_;
  crownPrev_ = crownCur_;
  auto read = [&](unsigned attachment, unsigned* pbos, std::vector<float>& cur, std::vector<float>& prev) {
    glReadBuffer(attachment);
    if (reads_ == 0) {
      glPixelStorei(GL_PACK_ALIGNMENT, 4);
      glReadPixels(0, 0, pw_, ph_, GL_RGBA, GL_FLOAT, cur.data());
      std::fill(prev.begin(), prev.end(), 0.f);
    } else {
      glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[(reads_ - 1) % 2]);
      if (const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(n * sizeof(float)),
                                                GL_MAP_READ_BIT)) {
        std::memcpy(cur.data(), mapped, n * sizeof(float));
        glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
      }
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbos[reads_ % 2]);
    glReadPixels(0, 0, pw_, ph_, GL_RGBA, GL_FLOAT, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
  };
  read(GL_COLOR_ATTACHMENT0, pbo_, patchCur_, patchPrev_);
  read(GL_COLOR_ATTACHMENT1, crownPbo_, crownCur_, crownPrev_);
  glReadBuffer(GL_COLOR_ATTACHMENT0);
  if (std::getenv("PXE_FIRE_DEBUG") && tick_ % 20 == 0) {
    float fire = 0.f, embers = 0.f, crown = 0.f;
    int burning = 0;
    for (std::size_t i = 0; i < n; i += 4) {
      fire += patchCur_[i];
      embers += patchCur_[i + 3];
      crown += crownCur_[i];
      burning += patchCur_[i] > 0.f || crownCur_[i] > 0.f;
    }
    std::fprintf(stderr, "pxe: grass fire step %lld, %d patches burning, fire %.1f cells, embers %.1f, crowns %.1f\n",
                 tick_, burning, fire, embers, crown);
  }
  ++reads_;
}

void FireGrid::addLights(std::vector<PointLight>& out, float fireLight, float fireReach, float time) const {
  if (!running()) return;
  const std::size_t first = out.size();
  for (int i = 0; i < pw_ * ph_; ++i) {
    const float* a = &patchPrev_[static_cast<std::size_t>(i * 4)];
    const float* b = &patchCur_[static_cast<std::size_t>(i * 4)];
    const float sum = a[0] + (b[0] - a[0]) * blend_;
    if (sum < 0.02f) continue;
    // The patch's fire-weighted middle, which glides as the fire moves.
    const glm::vec2 ca = a[0] > 1e-4f ? glm::vec2{a[1], a[2]} / a[0] : glm::vec2{b[1], b[2]} / b[0];
    const glm::vec2 cb = b[0] > 1e-4f ? glm::vec2{b[1], b[2]} / b[0] : ca;
    const glm::vec2 c = ca + (cb - ca) * blend_;
    const int x = std::clamp(static_cast<int>((c.x - min_.x) / kCell), 0, w_ - 1);
    const int z = std::clamp(static_cast<int>((c.y - min_.y) / kCell), 0, h_ - 1);
    const float full = std::min(sum / kFullPatch, 1.f);
    // A slow, smooth breathing of its own, like a campfire's.
    const float k = static_cast<float>(i);
    const float flicker = 0.9f + 0.06f * std::sin(time * 6.3f + k * 1.7f) + 0.04f * std::sin(time * 11.1f + k * 0.6f);
    out.push_back({{c.x, height_[static_cast<std::size_t>(z * w_ + x)] + kLightHeight, c.y},
                   fireReach * (0.45f + 0.25f * full), kPatchLight * fireLight * full * flicker, PointLight::kFire});
  }
  // Burning crowns: a light of their own per patch, up among the flames,
  // brighter and reaching farther than the grass's.
  for (int i = 0; i < pw_ * ph_; ++i) {
    const float* a = &crownPrev_[static_cast<std::size_t>(i * 4)];
    const float* b = &crownCur_[static_cast<std::size_t>(i * 4)];
    const float sum = a[0] + (b[0] - a[0]) * blend_;
    if (sum < 0.02f) continue;
    const glm::vec3 ca = a[0] > 1e-4f ? glm::vec3{a[1], a[3], a[2]} / a[0] : glm::vec3{b[1], b[3], b[2]} / b[0];
    const glm::vec3 cb = b[0] > 1e-4f ? glm::vec3{b[1], b[3], b[2]} / b[0] : ca;
    const glm::vec3 c = ca + (cb - ca) * blend_;
    const float full = std::min(sum / kFullPatch, 1.f);
    const float k = static_cast<float>(i) + 0.5f;
    const float flicker = 0.88f + 0.07f * std::sin(time * 5.7f + k * 1.3f) + 0.05f * std::sin(time * 10.3f + k * 0.8f);
    out.push_back({c + glm::vec3{0.f, kCrownLightHeight, 0.f}, fireReach * (0.6f + 0.35f * full),
                   kCrownLight * fireLight * full * flicker, PointLight::kFire});
  }
  if (const char* debug = std::getenv("PXE_FIRE_DEBUG"); debug && debug[0] == '2') {
    float sum = 0.f, unflickered = 0.f;
    glm::vec3 mid{0.f};
    for (std::size_t i = first; i < out.size(); ++i) {
      sum += out[i].strength;
      mid += out[i].pos * out[i].strength;
    }
    for (int i = 0; i < pw_ * ph_; ++i) {
      const float* a = &patchPrev_[static_cast<std::size_t>(i * 4)];
      const float* b = &patchCur_[static_cast<std::size_t>(i * 4)];
      unflickered += a[0] + (b[0] - a[0]) * blend_;
    }
    if (sum > 0.f) mid /= sum;
    std::fprintf(stderr, "pxe: grass fire light t %.4f blend %.3f lights %zu strength %.4f fire %.3f at %.3f %.3f\n", time,
                 blend_, out.size() - first, sum, unflickered, mid.x, mid.z);
  }
}

void FireGrid::drawFlames(const glm::mat4& viewProj, glm::vec2 target, float pxPerMetre, float depthBiasNdc,
                          float time, glm::vec2 wind) const {
  if (!running() || active_.empty()) return;
  std::vector<float> starts;
  starts.reserve(active_.size() * 2);
  for (int i : active_) {
    starts.push_back(static_cast<float>((i % pw_) * kPatch));
    starts.push_back(static_cast<float>((i / pw_) * kPatch));
  }
  glBindBuffer(GL_ARRAY_BUFFER, flameVbo_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(starts.size() * sizeof(float)), starts.data(), GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glUseProgram(progFlame_);
  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, state_[cur_]);
  glActiveTexture(GL_TEXTURE12);
  glBindTexture(GL_TEXTURE_2D, ground_);
  glActiveTexture(GL_TEXTURE13);
  glBindTexture(GL_TEXTURE_2D, state_[1 - cur_]);
  glActiveTexture(GL_TEXTURE14);
  glBindTexture(GL_TEXTURE_2D, glow_[cur_]);
  glActiveTexture(GL_TEXTURE15);
  glBindTexture(GL_TEXTURE_2D, glow_[1 - cur_]);
  // The crowns on units the renderer binds afresh before it next needs them.
  glActiveTexture(GL_TEXTURE4);
  glBindTexture(GL_TEXTURE_2D, canopy_[cur_]);
  glActiveTexture(GL_TEXTURE5);
  glBindTexture(GL_TEXTURE_2D, canopy_[1 - cur_]);
  glActiveTexture(GL_TEXTURE6);
  glBindTexture(GL_TEXTURE_2D, canopyGround_);
  glUniform1i(uloc(progFlame_, "uCanopy"), 4);
  glUniform1i(uloc(progFlame_, "uCanopyPrev"), 5);
  glUniform1i(uloc(progFlame_, "uCanopyGround"), 6);
  glUniform1i(uloc(progFlame_, "uGlow"), 14);
  glUniform1i(uloc(progFlame_, "uGlowPrev"), 15);
  glUniform1i(uloc(progFlame_, "uState"), 11);
  glUniform1i(uloc(progFlame_, "uGround"), 12);
  glUniform1i(uloc(progFlame_, "uPrev"), 13);
  glUniform1f(uloc(progFlame_, "uBlend"), blend_);
  glUniform2f(uloc(progFlame_, "uMin"), min_.x, min_.y);
  glUniform1f(uloc(progFlame_, "uCell"), kCell);
  glUniform1i(uloc(progFlame_, "uPatch"), kPatch);
  glUniformMatrix4fv(uloc(progFlame_, "uViewProj"), 1, GL_FALSE, &viewProj[0][0]);
  glUniform2f(uloc(progFlame_, "uTarget"), target.x, target.y);
  glUniform1f(uloc(progFlame_, "uPxPerMetre"), pxPerMetre);
  glUniform1f(uloc(progFlame_, "uDepthBiasNdc"), depthBiasNdc);
  glUniform1f(uloc(progFlame_, "uTime"), time);
  glUniform2f(uloc(progFlame_, "uWind"), wind.x, wind.y);
  glBindVertexArray(flameVao_);
  glDrawArraysInstanced(GL_POINTS, 0, kPatch * kPatch * kParts, static_cast<GLsizei>(active_.size()));
  glBindVertexArray(0);
}

}  // namespace pxe
