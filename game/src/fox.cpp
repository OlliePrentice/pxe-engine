#include "fox.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat3x3.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <string>

namespace game {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kTwoPi = 2.f * kPi;
// The lean: the ground's normal along the walk is averaged over this far
// either way (m), so bumps don't jitter it, and eases in from (and back to)
// the bed's tilt over this far from the bed.
constexpr float kLeanWindow = 0.6f;
constexpr float kLeanSettle = 1.f;
// Deeper water than this over the ground is not walked through (as the
// hero's).
constexpr float kWade = 0.15f;
// Fox_Walk's phases compared with standing, to start and stop each leg near
// the pose it stands in.
constexpr int kPhases = 72;

// A uniform draw in [0, 1) from the raw 32-bit output (std::mt19937's
// sequence is fixed by the standard; the distributions' are not), so every
// platform draws alike.
float draw01(std::mt19937& rng) { return static_cast<float>(rng()) / 4294967296.f; }

float smooth01(float x) {
  x = std::clamp(x, 0.f, 1.f);
  return x * x * (3.f - 2.f * x);
}

// The ground's slope round xz, from its smooth height `reach` out on either
// side along x and z; false off the map.
bool groundNormal(const pxe::HeightMap& ground, glm::vec2 xz, float reach, glm::vec3& normal) {
  const float x0 = ground.heightAt(xz - glm::vec2{reach, 0.f}), x1 = ground.heightAt(xz + glm::vec2{reach, 0.f});
  const float z0 = ground.heightAt(xz - glm::vec2{0.f, reach}), z1 = ground.heightAt(xz + glm::vec2{0.f, reach});
  if (x0 <= pxe::HeightMap::kNone || x1 <= pxe::HeightMap::kNone || z0 <= pxe::HeightMap::kNone ||
      z1 <= pxe::HeightMap::kNone)
    return false;
  normal = glm::normalize(glm::vec3{(x0 - x1) / (2.f * reach), 1.f, (z0 - z1) / (2.f * reach)});
  return true;
}

// Up turned to `normal`.
glm::quat tiltTo(glm::vec3 normal) {
  const glm::vec3 axis = glm::cross(glm::vec3{0.f, 1.f, 0.f}, normal);
  if (glm::length(axis) > 1e-5f) return glm::angleAxis(std::acos(std::clamp(normal.y, -1.f, 1.f)), glm::normalize(axis));
  return glm::quat{1.f, 0.f, 0.f, 0.f};
}

// --- Beds (Fox::findBeds) ---------------------------------------------------

// The fox's five clips, by name.
constexpr const char* kClips[5] = {"Fox_Rest", "Fox_StandUp", "Fox_LookAround", "Fox_LieDown", "Fox_Walk"};
// The fox is checked as voxels this wide (m, at kSize): each holds the
// vertices of every pose that fall in it, at most half its diagonal from its
// middle, and every check on its middle allows for that.
constexpr float kVoxel = 0.01f;
const float kVoxelSlack = 0.5f * std::sqrt(3.f) * kVoxel;  // a vertex from its voxel's middle, at most
// Seen from above, the fox in an accepted bed is kept as distances on a grid
// of this many metres, every cell's middle as far from it as its nearest
// occupied cell's middle.
constexpr float kPlan = 0.02f;
const float kPlanSlack = 0.5f * std::sqrt(2.f) * kPlan;  // a point from its plan cell's middle, at most
// No vertex of the fox may be more than this far under the ground's smooth
// height: it lies and stands on the lawn, not in it.
constexpr float kSink = 0.03f;
// Beds are tried every this far over the lawn, in an order shuffled by this
// seed.
constexpr float kBedStep = 0.25f;
constexpr std::uint32_t kBedSeed = 20261023u;

// The closest point to p on the triangle abc (Ericson, Real-Time Collision
// Detection, 5.1.5); a triangle with no area is taken as its edges.
glm::vec3 closestOnTriangle(glm::vec3 p, glm::vec3 a, glm::vec3 b, glm::vec3 c) {
  const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
  const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
  if (d1 <= 0.f && d2 <= 0.f) return a;
  const glm::vec3 bp = p - b;
  const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
  if (d3 >= 0.f && d4 <= d3) return b;
  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) return a + ab * (d1 / (d1 - d3));
  const glm::vec3 cp = p - c;
  const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
  if (d6 >= 0.f && d5 <= d6) return c;
  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) return a + ac * (d2 / (d2 - d6));
  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.f && d4 - d3 >= 0.f && d5 - d6 >= 0.f) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
  const float sum = va + vb + vc;
  if (sum > 1e-12f) return a + ab * (vb / sum) + ac * (vc / sum);
  auto onSegment = [&](glm::vec3 s0, glm::vec3 s1) {
    const glm::vec3 d = s1 - s0;
    const float l2 = glm::dot(d, d);
    return l2 > 1e-12f ? s0 + d * std::clamp(glm::dot(p - s0, d) / l2, 0.f, 1.f) : s0;
  };
  glm::vec3 best = onSegment(a, b);
  for (const glm::vec3 q : {onSegment(b, c), onSegment(c, a)})
    if (glm::dot(q - p, q - p) < glm::dot(best - p, best - p)) best = q;
  return best;
}

// The gap between two boxes (0 where they meet).
float boxGap(glm::vec3 lo0, glm::vec3 hi0, glm::vec3 lo1, glm::vec3 hi1) {
  const glm::vec3 d = glm::max(glm::max(lo0 - hi1, lo1 - hi0), glm::vec3{0.f});
  return glm::length(d);
}

// Felzenszwalb and Huttenlocher's distance transform of sampled functions,
// one row or column of n values a step apart: f[i] becomes the least
// f[j] + ((i - j) step)^2.
void squaredDistance(std::vector<float>& f, int n, float step) {
  std::vector<float> d(static_cast<std::size_t>(n)), z(static_cast<std::size_t>(n) + 1);
  std::vector<int> v(static_cast<std::size_t>(n));
  int k = -1;
  for (int q = 0; q < n; ++q) {
    if (f[static_cast<std::size_t>(q)] >= std::numeric_limits<float>::max()) continue;
    const float fq = f[static_cast<std::size_t>(q)] / (step * step) + static_cast<float>(q * q);
    float s = -std::numeric_limits<float>::infinity();
    while (k >= 0) {
      const int p = v[static_cast<std::size_t>(k)];
      const float fp = f[static_cast<std::size_t>(p)] / (step * step) + static_cast<float>(p * p);
      s = (fq - fp) / (2.f * static_cast<float>(q - p));
      if (s > z[static_cast<std::size_t>(k)]) break;
      --k;
    }
    ++k;
    v[static_cast<std::size_t>(k)] = q;
    z[static_cast<std::size_t>(k)] = k == 0 ? -std::numeric_limits<float>::infinity() : s;
    z[static_cast<std::size_t>(k) + 1] = std::numeric_limits<float>::infinity();
  }
  if (k < 0) return;
  int j = 0;
  for (int q = 0; q < n; ++q) {
    while (z[static_cast<std::size_t>(j) + 1] < static_cast<float>(q)) ++j;
    const int p = v[static_cast<std::size_t>(j)];
    d[static_cast<std::size_t>(q)] =
        f[static_cast<std::size_t>(p)] + static_cast<float>((q - p) * (q - p)) * step * step;
  }
  for (int q = 0; q < n; ++q) f[static_cast<std::size_t>(q)] = d[static_cast<std::size_t>(q)];
}

}  // namespace

pxe::SkinnedModel Fox::loadModel() {
  return pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/characters/fox.glb", kFirstMaterial);
}

void Fox::build(const pxe::SkinnedModel& model, int index) {
  model_ = nullptr;
  // The curl and the waking are all in the clips: the rest pose alone is
  // the fox standing in its bind pose. Looking round at the walk's target is
  // Fox_LookAround again; both legs of the walk are Fox_Walk.
  const char* names[kStateCount] = {"Fox_Rest", "Fox_StandUp", "Fox_LookAround", "Fox_LieDown",
                                    "Fox_Walk", "Fox_LookAround", "Fox_Walk"};
  for (int s = 0; s < kStateCount; ++s) {
    clip_[s] = model.clip(names[s]);
    if (clip_[s] < 0) throw std::runtime_error(std::string("no ") + names[s] + " clip in the fox's model");
    if (model.clipDuration(clip_[s]) <= 0.f)
      throw std::runtime_error(std::string(names[s]) + " is empty in the fox's model");
  }
  // The way it faces standing, against the way the curl faces: the turn its
  // root has made by the end of Fox_StandUp (a quarter-turn left; Fox_Walk's
  // root is the same, so it walks that way too).
  const int root = model.joint("root");
  if (root < 0) throw std::runtime_error("no root joint in the fox's model");
  hips_ = model.joint("hips");
  head_ = model.joint("head");
  if (hips_ < 0 || head_ < 0) throw std::runtime_error("no hips or head joint in the fox's model");
  hipsRest_ = glm::vec3(model.restJoint(hips_)[3]);
  headRest_ = glm::vec3(model.restJoint(head_)[3]);
  model_ = &model;
  index_ = index;
  pxe::SkinnedModel::Pose rest, standing;
  model_->restPose(rest);
  model_->sample(clip_[kStandUp], model_->clipDuration(clip_[kStandUp]), standing, false);
  const glm::vec3 facing = (standing.r[static_cast<std::size_t>(root)] *
                            glm::inverse(rest.r[static_cast<std::size_t>(root)])) *
                           glm::vec3{0.f, 0.f, 1.f};
  standTurn_ = std::atan2(facing.x, facing.z);
  // The point it turns about as it walks: midway between its four paws as
  // it stands (at kSize, in the model's ground plane), so that turning
  // swings its fore and hind paws sideways as little as a body that does
  // not bend can.
  pivot_ = glm::vec2{0.f};
  int paws = 0;
  model_->skin(standing, glm::scale(glm::mat4{1.f}, glm::vec3{kSize}), joints_);
  for (const char* name : {"forepaw.L", "forepaw.R", "hindpaw.L", "hindpaw.R"}) {
    const int j = model_->joint(name);
    if (j < 0) continue;
    const glm::vec3 p = pxe::SkinnedModel::jointPoint(joints_, j, glm::vec3(model_->restJoint(j)[3]));
    pivot_ += glm::vec2{p.x, p.z};
    ++paws;
  }
  if (paws > 0) pivot_ /= static_cast<float>(paws);
  // Half a check along the walk, a corner of the body moves at most as far
  // as the pivot plus its turn at the tightest; grown by that, the boxes
  // every kCheckStep cover the whole walk between them.
  float swing = 0.f;  // the farthest corner from the pivot
  for (const float x : {kBodyBack, kBodyFront})
    for (const float z : {kBodyLeft, kBodyRight}) swing = std::max(swing, glm::length(glm::vec2{x, z} - pivot_));
  const float grow = 0.5f * kCheckStep * (1.f + swing / kMinTurn);
  boxHalf_ = glm::vec2{0.5f * (kBodyFront - kBodyBack), 0.5f * (kBodyRight - kBodyLeft)} + grow;
  boxCorner_ = glm::length(boxHalf_);
  // How far each phase of the walk is from standing: the sum of every
  // joint's turn between the two.
  walkGap_.assign(kPhases, 0.f);
  const float walkLength = model_->clipDuration(clip_[kWalkOut]);
  for (int k = 0; k < kPhases; ++k) {
    model_->sample(clip_[kWalkOut], walkLength * static_cast<float>(k) / kPhases, walkPose_);
    for (std::size_t j = 0; j < walkPose_.r.size(); ++j)
      walkGap_[static_cast<std::size_t>(k)] +=
          2.f * std::acos(std::min(1.f, std::fabs(glm::dot(walkPose_.r[j], standing.r[j]))));
  }
  state_ = kRest;
  clock_ = 0.f;
  fade_ = kFade;
  skin();
}

void Fox::lie(const pxe::HeightMap& ground, glm::vec2 xz, float& y, glm::quat& tilt, glm::vec3& normal) {
  const float h = ground.heightAt(xz);
  y = h > pxe::HeightMap::kNone ? h : 0.f;
  tilt = glm::quat{1.f, 0.f, 0.f, 0.f};
  normal = glm::vec3{0.f, 1.f, 0.f};
  if (groundNormal(ground, xz, 0.6f * kFootprint, normal)) tilt = tiltTo(normal);
}

glm::mat4 Fox::world(glm::vec2 xz, float y, glm::quat tilt, float yaw) {
  return glm::scale(glm::rotate(glm::translate(glm::mat4{1.f}, glm::vec3{xz.x, y, xz.y}) * glm::mat4_cast(tilt), yaw,
                                glm::vec3{0.f, 1.f, 0.f}),
                    glm::vec3{kSize});
}

void Fox::spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw) {
  pos_ = xz;
  // The ground's smooth height under the middle of the curl, and its slope
  // across the curl: the curl rests on the ground at its lowest point, so on
  // a slope it is tilted to lie along it rather than float over the downhill
  // side. The standing fox, which reaches about 1.6 m out, keeps the same
  // tilt: a plane through the ground is the same plane whether it is sampled
  // 0.41 m out or 1.6 m, so long as the ground under the fox is planar, and
  // one tilt for every clip keeps the joins seamless (the hand-placed beds
  // are chosen where that holds, see scene.hpp; findBeds' beds keep every
  // part of the fox out of the ground).
  lie(scene.ground, xz, y_, tilt_, bedNormal_);
  yaw_ = yaw;
  // Where it always comes back to.
  bedPos_ = pos_;
  bedY_ = y_;
  bedYaw_ = yaw_;
  bedTilt_ = tilt_;
  // Asleep, at the start of the first rest, so it first wakes at least
  // kRestMin after it is put down.
  rng_.seed(kSeed + static_cast<std::uint32_t>(index_));
  walkRng_.seed(kWalkSeed + static_cast<std::uint32_t>(index_));
  walking_ = forceWalk_ = false;
  hold_ = false;
  enter(kRest, false);
  skin();
}

void Fox::setFires(const std::vector<glm::vec2>& centres, float reach) {
  fires_ = centres;
  fireReach_ = reach;
}

void Fox::beginRest() {
  const float u = draw01(rng_);
  restFor_ = kRestMin + (kRestMax - kRestMin) * u;
  rested_ = 0.f;
}

void Fox::enter(State next, bool fade) {
  if (fade) {
    // The pose the clip it leaves ended on. The clips are built to meet
    // exactly, so this only smooths over a join that does not.
    model_->sample(clip_[state_], model_->clipDuration(clip_[state_]), fadeFrom_, false);
    fade_ = 0.f;
  } else {
    fade_ = kFade;
  }
  state_ = next;
  clock_ = 0.f;
  if (next == kRest) beginRest();
}

void Fox::startLeg(State leg) {
  // From the pose it stands in (the look-around's end) into the walk, and
  // at the far end back to standing as the next clip starts: skin() fades
  // between them with the speed.
  model_->sample(clip_[state_], model_->clipDuration(clip_[state_]), fadeFrom_, false);
  const State after = leg == kWalkOut ? kLookOut : kLieDown;
  model_->sample(clip_[after], 0.f, fadeTo_, false);
  fade_ = kFade;
  state_ = leg;
  clock_ = 0.f;
}

float Fox::stateLength() const {
  if (state_ == kWalkOut) return route_.timeOut;
  if (state_ == kWalkBack) return route_.timeBack;
  return model_->clipDuration(clip_[state_]);
}

void Fox::update(const pxe::Scene& scene, float dt, std::span<const Fox> foxes) {
  if (!model_ || dt <= 0.f) return;
  // Through as many joins as dt spans, each at its exact moment, so one long
  // step (a capture's) ends where many short ones would. The walk is laid
  // out in full before it starts and timed in closed form, so it too lands
  // in the same place however the time is cut up.
  for (;;) {
    const float length = stateLength();
    const float left = length - clock_;
    if (state_ == kRest) {
      if (dt < left) {
        clock_ += dt;
        rested_ += dt;
        fade_ = std::min(fade_ + dt, kFade);
        break;
      }
      // The loop wraps: wake now if this rest is over, else breathe on.
      dt -= left;
      rested_ += left;
      fade_ = std::min(fade_ + left, kFade);
      clock_ = 0.f;
      if (!hold_ && rested_ >= restFor_) {
        enter(kStandUp, true);
        planWalk(scene, foxes);
      }
      continue;
    }
    if (dt < left) {
      clock_ += dt;
      fade_ = std::min(fade_ + dt, kFade);
      break;
    }
    dt -= left;
    switch (state_) {
      case kStandUp:
        enter(kLookAround, true);
        break;
      case kLookAround:
        if (walking_) {
          startLeg(kWalkOut);
        } else {
          enter(kLieDown, true);
        }
        break;
      case kWalkOut:
        // Stopped at the target, in the look-around's first pose.
        enter(kLookOut, false);
        break;
      case kLookOut:
        startLeg(kWalkBack);
        break;
      case kWalkBack:
        // Home, facing the way it stood, in Fox_LieDown's first pose.
        walking_ = false;
        enter(kLieDown, false);
        break;
      default:
        enter(kRest, true);
        break;
    }
  }
  place(scene);
  skin();
}

float Fox::untilWalkDecided() const {
  if (!model_ || hold_) return std::numeric_limits<float>::infinity();
  if (state_ != kRest) {
    // At least the rest of this state, and a whole rest after the wake.
    return std::max(stateLength() - clock_, 0.f) + kRestMin;
  }
  // The first time the loop wraps with the rest over.
  const float length = stateLength();
  float t = length - clock_, rested = rested_ + t;
  while (rested < restFor_) {
    t += length;
    rested += length;
  }
  return t;
}

void Fox::holdRest(float seconds) {
  if (!model_) return;
  hold_ = true;
  walking_ = false;
  enter(kRest, false);
  pos_ = bedPos_;
  y_ = bedY_;
  yaw_ = bedYaw_;
  tilt_ = bedTilt_;
  // As it always slept in captures: just the place in the loop.
  const float length = model_->clipDuration(clip_[kRest]);
  clock_ = length > 0.f ? std::fmod(std::max(seconds, 0.f), length) : 0.f;
  skin();
}

void Fox::wake(const pxe::Scene& scene, std::span<const Fox> foxes) {
  if (!model_) return;
  hold_ = false;
  enter(kStandUp, false);
  planWalk(scene, foxes);
  place(scene);
  skin();
}

void Fox::place(const pxe::Scene& scene) {
  float s = -1.f;
  if (state_ == kWalkOut) s = legDistance(clock_, route_.out, route_.timeOut);
  if (state_ == kLookOut) s = route_.out;
  if (state_ == kWalkBack) s = route_.out + legDistance(clock_, route_.total - route_.out, route_.timeBack);
  if (s < 0.f) {
    // In its bed, exactly as it was put there.
    pos_ = bedPos_;
    y_ = bedY_;
    yaw_ = bedYaw_;
    tilt_ = bedTilt_;
    return;
  }
  // Along the walk: on the ground under it, leaning to its slope.
  routeAt(route_, s, pos_, yaw_);
  const float h = scene.ground.heightAt(pos_);
  if (h > pxe::HeightMap::kNone) y_ = h;
  tilt_ = routeTilt(route_, s);
}

// --- The walk -----------------------------------------------------------

void Fox::planWalk(const pxe::Scene& scene, std::span<const Fox> foxes) {
  walking_ = false;
  const bool forced = forceWalk_;
  forceWalk_ = false;
  // Every wake draws whether it walks, forced or not, so the draws after it
  // are the same either way.
  const bool wants = draw01(walkRng_) < 0.5f || forced;
  if (!wants) return;
  const int tries = forced ? kForcedTries : kTries;
  for (int i = 0; i < tries; ++i) {
    // Uniform over the ring round the bed.
    const float a = kTwoPi * draw01(walkRng_);
    const float r = std::sqrt(kMinReach * kMinReach + (kMaxReach * kMaxReach - kMinReach * kMinReach) * draw01(walkRng_));
    const glm::vec2 target = bedPos_ + r * glm::vec2{std::cos(a), std::sin(a)};
    Route route;
    if (!layRoute(target, route) || !routeClear(scene, route) || !routeFree(route, foxes)) continue;
    route.timeOut = legTime(route.out);
    route.timeBack = legTime(route.total - route.out);
    route.phaseOut = startPhase(route.out);
    route.phaseBack = startPhase(route.total - route.out);
    route_ = std::move(route);
    walking_ = true;
    const float look = model_->clipDuration(clip_[kLookAround]);
    const float down = model_->clipDuration(clip_[kStandUp]) + look + route_.timeOut + look + route_.timeBack;
    std::fprintf(stderr,
                 "pxe: fox %d walks to a point %.1f m from its bed (target %d): %.1f m out in %.1f s, "
                 "%.1f m back in %.1f s; it lies down %.1f s into its wake\n",
                 index_, r, i + 1, route_.out, route_.timeOut, route_.total - route_.out, route_.timeBack, down);
    return;
  }
  std::fprintf(stderr, "pxe: fox %d stays in its bed: none of %d walks was clear\n", index_, tries);
}

bool Fox::layRoute(glm::vec2 target, Route& route) const {
  // The pivot's place and heading as it stands in its bed, and the frame of
  // the loop: x along the heading, y across it toward the target's side
  // (the side the loop goes round).
  const glm::vec2 heading{std::sin(bedYaw_ + standTurn_), std::cos(bedYaw_ + standTurn_)};
  const glm::vec2 left{heading.y, -heading.x};
  const glm::vec2 bed = bedPos_ + glm::vec2{std::cos(bedYaw_), -std::sin(bedYaw_)} * pivot_.x +
                        glm::vec2{std::sin(bedYaw_), std::cos(bedYaw_)} * pivot_.y;
  const glm::vec2 rel = target - bed;
  const float x = glm::dot(rel, heading), across = glm::dot(rel, left);
  const float side = across >= 0.f ? 1.f : -1.f;
  const float y = std::max(std::fabs(across), 1e-3f);
  const float cap = 2.f * glm::length(target - bedPos_) + kSpare;
  // The loop is the outline round two circles both touching the line the
  // fox leaves along: a rear one that it comes home round, touching that
  // line at the bed, and a front one through the target, touching it `t`
  // further on. Of those no longer than `cap`, the one with the widest
  // tightest turn.
  float best = -1.f, bestLength = 0.f, bestT = 0.f, rearR = 0.f, frontR = 0.f;
  float frontTurn = 0.f, targetTurn = 0.f, straight = 0.f;
  for (const float rear : {1.f, 1.25f, 1.5f, 2.f, 2.5f}) {
    const float r = rear * kMinTurn;
    for (int k = 0; k <= 32; ++k) {
      const float t = std::max(x, 0.f) * static_cast<float>(k) / 32.f;
      const float f = ((x - t) * (x - t) + y * y) / (2.f * y);
      if (f < kMinTurn) continue;
      // The other line touching both circles, over the top of them.
      const glm::vec2 between{t, f - r};
      const float apart = glm::length(between);
      if (apart <= std::fabs(r - f) + 1e-4f) continue;
      const glm::vec2 e = between / apart, perp{-e.y, e.x};
      const float c = (r - f) / apart, sn = std::sqrt(std::max(0.f, 1.f - c * c));
      const glm::vec2 n1 = e * c + perp * sn, n2 = e * c - perp * sn;
      const glm::vec2 top = n1.y > n2.y ? n1 : n2;
      const float topLine = std::sqrt(std::max(0.f, apart * apart - (r - f) * (r - f)));
      // Round the front circle from where the bottom line leaves it to the
      // top line, and the target somewhere on that.
      const float around = std::fmod(std::atan2(top.y, top.x) + 0.5f * kPi + 2.f * kTwoPi, kTwoPi);
      const float toTarget = std::fmod(std::atan2(y - f, x - t) + 0.5f * kPi + 2.f * kTwoPi, kTwoPi);
      if (toTarget > around) continue;
      const float length = t + topLine + f * around + r * (kTwoPi - around);
      if (length > cap) continue;
      const float score = std::min(r, f);
      if (score > best + 1e-4f || (score > best - 1e-4f && length < bestLength)) {
        best = score;
        bestLength = length;
        bestT = t;
        rearR = r;
        frontR = f;
        frontTurn = around;
        targetTurn = toTarget;
        straight = topLine;
      }
    }
  }
  if (best < 0.f) return false;
  // Turning toward +y is turning left, which is toward decreasing angle in
  // the x-z plane.
  const float turn = -side;
  const float start = std::atan2(heading.y, heading.x);
  route = Route{};
  const std::vector<Piece> out{{bestT, 0.f}, {frontR * targetTurn, turn / frontR}};
  if (!layLeg(out, bed, start, target, start + turn * targetTurn, route)) return false;
  route.out = route.length.back();
  const std::vector<Piece> back{
      {frontR * (frontTurn - targetTurn), turn / frontR}, {straight, 0.f}, {rearR * (kTwoPi - frontTurn), turn / rearR}};
  if (!layLeg(back, route.point.back(), route.heading.back(), bed, start + turn * kTwoPi, route)) return false;
  route.total = route.length.back();
  // Start on the bed's own yaw exactly.
  float yaw = 0.f;
  glm::vec2 at{0.f};
  routeAt(route, 0.f, at, yaw);
  route.yawOffset = bedYaw_ - yaw;
  return true;
}

bool Fox::layLeg(const std::vector<Piece>& pieces, glm::vec2 from, float heading, glm::vec2 to, float toHeading,
                 Route& route) const {
  // Laid out as Dubins would (straights and arcs), the yaw rate would jump
  // where they meet; so the turning is averaged over kEase, which turns each
  // join into an even ramp, and then bent by three gentle waves along the
  // leg (zero at its ends), sized to land on `to` facing `toHeading`.
  double total = 0.0;
  for (const Piece& p : pieces) total += p.length;
  if (total <= 1e-3) return false;
  const int steps = std::max(8, static_cast<int>(std::ceil(total / kRouteStep)));
  const double ds = total / steps;
  std::vector<double> raw(static_cast<std::size_t>(steps)), eased(static_cast<std::size_t>(steps));
  {
    std::size_t piece = 0;
    double end = pieces[0].length;
    for (int i = 0; i < steps; ++i) {
      const double s = (i + 0.5) * ds;
      while (s > end && piece + 1 < pieces.size()) end += pieces[++piece].length;
      raw[static_cast<std::size_t>(i)] = pieces[piece].curvature;
    }
    const int half = std::max(1, static_cast<int>(std::lround(0.5 * kEase / ds)));
    std::vector<double> sum(static_cast<std::size_t>(steps + 1), 0.0);
    for (int i = 0; i < steps; ++i) sum[static_cast<std::size_t>(i + 1)] = sum[static_cast<std::size_t>(i)] + raw[static_cast<std::size_t>(i)];
    for (int i = 0; i < steps; ++i) {
      const int a = std::max(0, i - half), b = std::min(steps, i + half + 1);
      eased[static_cast<std::size_t>(i)] = (sum[static_cast<std::size_t>(b)] - sum[static_cast<std::size_t>(a)]) / (b - a);
    }
  }
  auto curvature = [&](int i, const double (&c)[3]) {
    const double u = (i + 0.5) / steps;
    return eased[static_cast<std::size_t>(i)] + c[0] * std::sin(kPi * u) + c[1] * std::sin(2.0 * kPi * u) +
           c[2] * std::sin(3.0 * kPi * u);
  };
  // Where it ends up: x, z and heading.
  auto land = [&](const double (&c)[3], double (&end)[3]) {
    double px = from.x, pz = from.y, h = heading;
    for (int i = 0; i < steps; ++i) {
      const double k = curvature(i, c);
      const double mid = h + 0.5 * k * ds;
      px += ds * std::cos(mid);
      pz += ds * std::sin(mid);
      h += k * ds;
    }
    end[0] = px - to.x;
    end[1] = pz - to.y;
    end[2] = h - toHeading;
  };
  double c[3] = {0.0, 0.0, 0.0}, miss[3];
  bool landed = false;
  for (int iter = 0; iter < 10; ++iter) {
    land(c, miss);
    if (std::fabs(miss[0]) < 1e-5 && std::fabs(miss[1]) < 1e-5 && std::fabs(miss[2]) < 1e-6) {
      landed = true;
      break;
    }
    double jac[3][3];
    for (int k = 0; k < 3; ++k) {
      double nudged[3] = {c[0], c[1], c[2]}, m[3];
      nudged[k] += 1e-4;
      land(nudged, m);
      for (int r = 0; r < 3; ++r) jac[r][k] = (m[r] - miss[r]) / 1e-4;
    }
    const double det = jac[0][0] * (jac[1][1] * jac[2][2] - jac[1][2] * jac[2][1]) -
                       jac[0][1] * (jac[1][0] * jac[2][2] - jac[1][2] * jac[2][0]) +
                       jac[0][2] * (jac[1][0] * jac[2][1] - jac[1][1] * jac[2][0]);
    if (std::fabs(det) < 1e-12) return false;
    for (int k = 0; k < 3; ++k) {
      double m[3][3];
      for (int r = 0; r < 3; ++r)
        for (int q = 0; q < 3; ++q) m[r][q] = q == k ? miss[r] : jac[r][q];
      c[k] -= (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
               m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])) /
              det;
    }
  }
  if (!landed) return false;
  for (int i = 0; i < steps; ++i)
    if (std::fabs(curvature(i, c)) > 1.0 / kMinTurn + 1e-4) return false;
  // Lay it down, picking up where the route so far ends.
  double px = from.x, pz = from.y, h = heading;
  const double start = route.length.empty() ? 0.0 : route.length.back();
  if (route.length.empty()) {
    route.length.push_back(0.f);
    route.point.push_back(from);
    route.heading.push_back(heading);
  }
  for (int i = 0; i < steps; ++i) {
    const double k = curvature(i, c);
    const double mid = h + 0.5 * k * ds;
    px += ds * std::cos(mid);
    pz += ds * std::sin(mid);
    h += k * ds;
    route.length.push_back(static_cast<float>(start + (i + 1) * ds));
    route.point.push_back(glm::vec2{static_cast<float>(px), static_cast<float>(pz)});
    route.heading.push_back(static_cast<float>(h));
  }
  return true;
}

void Fox::routeAt(const Route& route, float s, glm::vec2& pos, float& yaw) const {
  s = std::clamp(s, 0.f, route.total);
  const auto it = std::upper_bound(route.length.begin(), route.length.end(), s);
  const std::size_t j = std::min(static_cast<std::size_t>(std::max<std::ptrdiff_t>(it - route.length.begin() - 1, 0)),
                                 route.length.size() - 2);
  const float span = route.length[j + 1] - route.length[j];
  const float f = span > 0.f ? std::clamp((s - route.length[j]) / span, 0.f, 1.f) : 0.f;
  const glm::vec2 point = glm::mix(route.point[j], route.point[j + 1], f);
  const float heading = route.heading[j] + (route.heading[j + 1] - route.heading[j]) * f;
  // A model yaw faces its standing way, standTurn_ round from +z, along the
  // heading (unwrapped: it turns a whole turn by the end).
  yaw = kPi * 0.5f - heading - standTurn_ + route.yawOffset;
  // The route is the pivot's path; the model's origin is back from it.
  pos = point - glm::vec2{std::cos(yaw), -std::sin(yaw)} * pivot_.x - glm::vec2{std::sin(yaw), std::cos(yaw)} * pivot_.y;
}

glm::quat Fox::routeTilt(const Route& route, float s) const {
  if (s <= 0.f || s >= route.total || route.normal.size() < 2) return bedTilt_;
  const float at = s / route.normalStep;
  const std::size_t i = std::min(static_cast<std::size_t>(at), route.normal.size() - 2);
  const float f = std::clamp(at - static_cast<float>(i), 0.f, 1.f);
  return tiltTo(glm::normalize(glm::mix(route.normal[i], route.normal[i + 1], f)));
}

bool Fox::routeClear(const pxe::Scene& scene, Route& route) const {
  const pxe::HeightMap& ground = scene.ground;
  const pxe::HeightMap& water = scene.waterTop;
  const int count = std::max(1, static_cast<int>(std::ceil(route.total / kCheckStep)));
  route.normalStep = route.total / static_cast<float>(count);
  std::vector<glm::vec3> normals(static_cast<std::size_t>(count + 1), glm::vec3{0.f, 1.f, 0.f});
  const int across = static_cast<int>(std::ceil((kBodyFront - kBodyBack) / kCheckStep));
  const int wide = static_cast<int>(std::ceil((kBodyRight - kBodyLeft) / kCheckStep));
  route.body.clear();
  // Every kCheckStep along the walk, and the target, where it stops to look.
  auto clear = [&](float s, glm::vec3* normal) {
    glm::vec2 pos{0.f};
    float yaw = 0.f;
    routeAt(route, s, pos, yaw);
    if (glm::length(pos - bedPos_) > kMaxReach + 1e-3f) return false;
    // The model's ground axes at this yaw: x its heading, z its right.
    const glm::vec2 ax{std::cos(yaw), -std::sin(yaw)}, az{std::sin(yaw), std::cos(yaw)};
    // The body, a box in those axes, clear of every campfire's dirt patch.
    for (const glm::vec2& fire : fires_) {
      const glm::vec2 toFire = fire - pos;
      const glm::vec2 local{glm::dot(toFire, ax), glm::dot(toFire, az)};
      const glm::vec2 nearest{std::clamp(local.x, kBodyBack, kBodyFront), std::clamp(local.y, kBodyLeft, kBodyRight)};
      if (glm::length(local - nearest) < fireReach_ + kFireClearance) return false;
    }
    // All of it over ground it could walk on: on the map, within a step of
    // the ground where it stands, not in deep water.
    const float here = ground.heightAt(pos);
    if (here <= pxe::HeightMap::kNone) return false;
    for (int i = 0; i <= across; ++i) {
      const float x = kBodyBack + (kBodyFront - kBodyBack) * static_cast<float>(i) / across;
      for (int k = 0; k <= wide; ++k) {
        const float z = kBodyLeft + (kBodyRight - kBodyLeft) * static_cast<float>(k) / wide;
        const glm::vec2 p = pos + ax * x + az * z;
        // A lone empty cell inside the lawn (a seam of its triangles that
        // missed the cell's middle) takes the height round it, as heightAt
        // gives it; off the map, nothing round it has one either.
        float top = ground.cellTop(p);
        if (top <= pxe::HeightMap::kNone) top = ground.heightAt(p);
        if (top <= pxe::HeightMap::kNone || std::fabs(top - here) > Hero::kStep) return false;
        const float wet = water.cellTop(p);
        if (wet > pxe::HeightMap::kNone && wet > top + kWade) return false;
      }
    }
    if (normal && !groundNormal(ground, pos, 0.6f * kFootprint, *normal)) *normal = glm::vec3{0.f, 1.f, 0.f};
    route.body.push_back({pos.x, pos.y, yaw});
    return true;
  };
  for (int k = 0; k <= count; ++k)
    if (!clear(route.normalStep * static_cast<float>(k), &normals[static_cast<std::size_t>(k)])) return false;
  if (!clear(route.out, nullptr)) return false;
  // The lean: the slope under it averaged along the way (a tent of
  // kLeanWindow either side), easing from the bed's tilt as it leaves and
  // back to it as it comes home.
  const int reach = std::max(1, static_cast<int>(std::lround(kLeanWindow / route.normalStep)));
  route.normal.assign(normals.size(), glm::vec3{0.f, 1.f, 0.f});
  for (int k = 0; k <= count; ++k) {
    glm::vec3 sum{0.f};
    for (int j = std::max(0, k - reach); j <= std::min(count, k + reach); ++j)
      sum += normals[static_cast<std::size_t>(j)] * static_cast<float>(reach + 1 - std::abs(j - k));
    const float s = route.normalStep * static_cast<float>(k);
    const float settle = smooth01(std::min(s, route.total - s) / kLeanSettle);
    route.normal[static_cast<std::size_t>(k)] = glm::normalize(glm::mix(bedNormal_, glm::normalize(sum), settle));
  }
  return true;
}

Fox::BodyBox Fox::bodyBox(glm::vec3 at) const {
  const glm::vec2 ax{std::cos(at.z), -std::sin(at.z)}, az{std::sin(at.z), std::cos(at.z)};
  return BodyBox{glm::vec2{at.x, at.y} + ax * (0.5f * (kBodyBack + kBodyFront)) + az * (0.5f * (kBodyLeft + kBodyRight)),
                 ax, az};
}

bool Fox::overlaps(const BodyBox& a, const BodyBox& b) const {
  // Two boxes overlap unless one of their four axes parts them.
  const glm::vec2 d = b.centre - a.centre;
  if (glm::dot(d, d) >= 4.f * boxCorner_ * boxCorner_) return false;
  for (const glm::vec2 axis : {a.ax, a.az, b.ax, b.az}) {
    const float ra = boxHalf_.x * std::fabs(glm::dot(a.ax, axis)) + boxHalf_.y * std::fabs(glm::dot(a.az, axis));
    const float rb = boxHalf_.x * std::fabs(glm::dot(b.ax, axis)) + boxHalf_.y * std::fabs(glm::dot(b.az, axis));
    if (std::fabs(glm::dot(d, axis)) >= ra + rb) return false;
  }
  return true;
}

bool Fox::touches(const BodyBox& a, glm::vec2 centre, float radius) const {
  // A box and a circle overlap when the box's nearest point is inside it.
  const glm::vec2 d = centre - a.centre;
  const float x = std::clamp(glm::dot(d, a.ax), -boxHalf_.x, boxHalf_.x);
  const float z = std::clamp(glm::dot(d, a.az), -boxHalf_.y, boxHalf_.y);
  return glm::length(d - a.ax * x - a.az * z) < radius;
}

bool Fox::routeFree(const Route& route, std::span<const Fox> foxes) const {
  // The body at each check as a box on the ground, grown so the boxes cover
  // the whole walk between them (build()).
  std::vector<BodyBox> mine, theirs;
  for (const glm::vec3& at : route.body) mine.push_back(bodyBox(at));
  for (const Fox& other : foxes) {
    if (&other == this || !other.model_) continue;
    // Its bed: curled up in it, and standing in it as it wakes (the body as
    // it sets off on a walk, which starts there).
    const BodyBox bed = bodyBox({other.bedPos_.x, other.bedPos_.y, other.bedYaw_});
    theirs.clear();
    if (other.walking_)
      for (const glm::vec3& at : other.route_.body) theirs.push_back(bodyBox(at));
    for (const BodyBox& b : mine) {
      if (touches(b, other.bedPos_, kFootprint) || overlaps(b, bed)) return false;
      for (const BodyBox& t : theirs)
        if (overlaps(b, t)) return false;
    }
  }
  return true;
}

bool Fox::bedFree(glm::vec2 xz, float yaw, std::span<const Fox> foxes) const {
  // As routeFree, the other way about: every box of every route out now
  // against the curl and the standing body in the bed.
  const BodyBox standing = bodyBox({xz.x, xz.y, yaw});
  for (const Fox& other : foxes) {
    if (&other == this || !other.model_ || !other.walking_) continue;
    for (const glm::vec3& at : other.route_.body) {
      const BodyBox t = bodyBox(at);
      if (touches(t, xz, kFootprint) || overlaps(t, standing)) return false;
    }
  }
  return true;
}

float Fox::walkSpeed() const { return kStride * kSize / model_->clipDuration(clip_[kWalkOut]); }

float Fox::legTime(float length) const {
  const float v = walkSpeed();
  return length / v + std::min(kRamp, length / v);
}

float Fox::legDistance(float t, float length, float time) const {
  // Speeding up as smoothstep over the ramp, on at full speed, and slowing
  // the same way, to stop at `length` exactly.
  const float v = walkSpeed();
  const float ramp = std::min(kRamp, length / v);
  if (t <= 0.f) return 0.f;
  if (t >= time) return length;
  auto eased = [&](float u) { return v * ramp * (u * u * u - 0.5f * u * u * u * u); };
  if (t < ramp) return eased(t / ramp);
  if (t > time - ramp) return length - eased((time - t) / ramp);
  return 0.5f * v * ramp + v * (t - ramp);
}

float Fox::startPhase(float length) const {
  // The phase it sets off on, so that both its first and its last steps are
  // as near standing as they can be: the walk fades in and out over them.
  const float cycles = length / (kStride * kSize);
  auto gap = [&](float phase) {
    phase -= std::floor(phase);
    const float at = phase * kPhases;
    const int i = static_cast<int>(at) % kPhases;
    const float f = at - std::floor(at);
    return walkGap_[static_cast<std::size_t>(i)] * (1.f - f) + walkGap_[static_cast<std::size_t>((i + 1) % kPhases)] * f;
  };
  float best = 0.f, bestGap = 1e9f;
  for (int k = 0; k < kPhases; ++k) {
    const float phase = static_cast<float>(k) / kPhases;
    const float g = std::max(gap(phase), gap(phase + cycles));
    if (g < bestGap) {
      bestGap = g;
      best = phase;
    }
  }
  return best;
}

void Fox::skin() {
  if (!model_) return;
  if (state_ == kWalkOut || state_ == kWalkBack) {
    // Fox_Walk on the ground covered, so its planted paws keep pace with the
    // ground, faded from and back to standing as it speeds up and slows.
    const bool out = state_ == kWalkOut;
    const float length = out ? route_.out : route_.total - route_.out;
    const float time = out ? route_.timeOut : route_.timeBack;
    float phase = (out ? route_.phaseOut : route_.phaseBack) + legDistance(clock_, length, time) / (kStride * kSize);
    phase -= std::floor(phase);
    model_->sample(clip_[state_], phase * model_->clipDuration(clip_[state_]), pose_);
    const float fade = std::min(kWalkFade, 0.5f * time);
    const float walk = smooth01(std::min(clock_, time - clock_) / fade);
    if (walk < 1.f) pxe::SkinnedModel::blend(clock_ < 0.5f * time ? fadeFrom_ : fadeTo_, pose_, walk, pose_);
  } else {
    // The rest loops; the waking clips play once each.
    model_->sample(clip_[state_], clock_, pose_, state_ == kRest);
    if (fade_ < kFade) pxe::SkinnedModel::blend(fadeFrom_, pose_, fade_ / kFade, pose_);
  }
  model_->skin(pose_, world(pos_, y_, tilt_, yaw_), joints_);
}

void Fox::pose(std::vector<pxe::ActorDraw>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) const {
  if (!model_) return;
  actors.push_back({&model_->mesh(), joints_});
  // Its vertices point at slots firstMaterial() + i (SkinnedModel::load).
  const auto& mats = model_->materials();
  const int first = model_->firstMaterial();
  for (int i = 0; i < static_cast<int>(mats.size()) && first + i < pxe::kActorMaterials; ++i)
    materials[first + i] = mats[static_cast<std::size_t>(i)];
}

Blocker Fox::body() const {
  if (!model_ || state_ == kRest || state_ == kLieDown) return {pos_, pos_, kFootprint};
  const glm::vec3 hips = pxe::SkinnedModel::jointPoint(joints_, hips_, hipsRest_);
  const glm::vec3 head = pxe::SkinnedModel::jointPoint(joints_, head_, headRest_);
  return {{hips.x, hips.z}, {head.x, head.z}, kBodyRadius};
}

Fox::Sweep Fox::sweep(const pxe::SkinnedModel& model) {
  Sweep out;
  // The fox, every vertex in every 1/60 s of its five clips and at each
  // clip's end, at kSize in the model (y up, the curl facing +z), as the
  // middles of the voxels it passes through.
  std::vector<glm::vec3> swept;
  {
    pxe::SkinnedModel::Pose pose;
    std::vector<glm::mat4> joints;
    std::vector<glm::vec3> points;
    for (const char* name : kClips) {
      const int clip = model.clip(name);
      if (clip < 0) throw std::runtime_error(std::string("no ") + name + " clip in the fox's model");
      const float length = model.clipDuration(clip);
      const int frames = static_cast<int>(std::ceil(length * 60.f - 1e-3f));
      for (int f = 0; f <= frames; ++f) {
        model.sample(clip, std::min(static_cast<float>(f) / 60.f, length), pose, false);
        model.skin(pose, glm::scale(glm::mat4{1.f}, glm::vec3{kSize}), joints);
        model.vertexPoints(joints, points);
        swept.insert(swept.end(), points.begin(), points.end());
      }
    }
  }
  glm::vec3 lo{1e9f}, hi{-1e9f};
  for (const glm::vec3& p : swept) {
    lo = glm::min(lo, p);
    hi = glm::max(hi, p);
  }
  const glm::ivec3 cells = glm::ivec3(glm::floor((hi - lo) / kVoxel)) + 1;
  auto voxelOf = [&](glm::vec3 p) {
    const glm::ivec3 c = glm::clamp(glm::ivec3(glm::floor((p - lo) / kVoxel)), glm::ivec3{0}, cells - 1);
    return (static_cast<std::uint64_t>(c.z) * static_cast<std::uint64_t>(cells.y) + static_cast<std::uint64_t>(c.y)) *
               static_cast<std::uint64_t>(cells.x) +
           static_cast<std::uint64_t>(c.x);
  };
  // Every vertex's place, sorted by voxel: each voxel is its middle and its
  // run of vertices, which are checked themselves only where the middle is
  // too near to say.
  constexpr int kIndexBits = 24;
  if (swept.size() >= (std::size_t{1} << kIndexBits)) throw std::runtime_error("too many fox vertices to sweep");
  std::vector<std::uint64_t> order(swept.size());
  for (std::size_t i = 0; i < swept.size(); ++i) order[i] = (voxelOf(swept[i]) << kIndexBits) | i;
  std::sort(order.begin(), order.end());
  out.vertices.resize(swept.size());
  for (std::size_t i = 0; i < order.size(); ++i) {
    const std::uint64_t v = order[i] >> kIndexBits;
    out.vertices[i] = swept[order[i] & ((std::uint64_t{1} << kIndexBits) - 1)];
    if (out.voxels.empty() || (order[i - 1] >> kIndexBits) != v) {
      const auto w = static_cast<std::uint64_t>(cells.x), h = static_cast<std::uint64_t>(cells.y);
      const glm::ivec3 c{static_cast<int>(v % w), static_cast<int>(v / w % h), static_cast<int>(v / (w * h))};
      out.voxels.push_back({lo + (glm::vec3{c} + 0.5f) * kVoxel, static_cast<std::uint32_t>(i), 0});
    }
    ++out.voxels.back().count;
  }
  // The voxels grouped into blocks of kBlock a side.
  constexpr int kBlock = 12;
  const glm::ivec3 blocksOf = (cells + kBlock - 1) / kBlock;
  out.blocks.resize(static_cast<std::size_t>(blocksOf.x) * blocksOf.y * blocksOf.z);
  for (std::size_t i = 0; i < out.voxels.size(); ++i) {
    const glm::vec3 p = out.voxels[i].centre;
    const glm::ivec3 c = glm::ivec3(glm::floor((p - lo) / kVoxel)) / kBlock;
    Sweep::Block& b = out.blocks[(static_cast<std::size_t>(c.z) * blocksOf.y + c.y) * blocksOf.x + c.x];
    b.lo = glm::min(b.lo, p - kVoxelSlack);
    b.hi = glm::max(b.hi, p + kVoxelSlack);
    b.voxels.push_back(static_cast<std::uint32_t>(i));
    out.fox.push_back(p);
    out.reach = std::max(out.reach, glm::length(glm::vec2{p.x, p.z}) + kVoxelSlack);
    out.radius = std::max(out.radius, glm::length(p) + kVoxelSlack);
  }
  std::erase_if(out.blocks, [](const Sweep::Block& b) { return b.voxels.empty(); });
  return out;
}

// The bed rules over a scene laid out as `layout`: everything standing in
// it on a grid of metre cells seen from above, and the beds taken so far,
// each as its fox seen from above.
class Fox::Beds {
 public:
  Beds(const Sweep& sweep, const SceneLayout& layout) : sweep_(sweep), layout_(layout) {
    const std::vector<glm::vec3>& solids = layout.solids;
    for (const glm::vec3& p : solids) {
      gridLo_ = glm::min(gridLo_, glm::vec2{p.x, p.z});
      gridHi_ = glm::max(gridHi_, glm::vec2{p.x, p.z});
    }
    gw_ = solids.empty() ? 0 : static_cast<int>((gridHi_.x - gridLo_.x) / kCell) + 1;
    gh_ = solids.empty() ? 0 : static_cast<int>((gridHi_.y - gridLo_.y) / kCell) + 1;
    grid_.resize(static_cast<std::size_t>(gw_) * gh_);
    for (std::size_t i = 0; i + 2 < solids.size(); i += 3) {
      const glm::vec3 a = glm::min(glm::min(solids[i], solids[i + 1]), solids[i + 2]);
      const glm::vec3 b = glm::max(glm::max(solids[i], solids[i + 1]), solids[i + 2]);
      triLo_.push_back(a);
      triHi_.push_back(b);
      for (int z = static_cast<int>((a.z - gridLo_.y) / kCell); z <= static_cast<int>((b.z - gridLo_.y) / kCell); ++z)
        for (int x = static_cast<int>((a.x - gridLo_.x) / kCell); x <= static_cast<int>((b.x - gridLo_.x) / kCell); ++x)
          grid_[static_cast<std::size_t>(z) * gw_ + x].push_back(static_cast<std::uint32_t>(i / 3));
    }
  }

  // The fox lying in a bed, placed in the world.
  struct Placed {
    glm::vec2 xz{0.f};
    float y = 0.f;
    glm::mat3 turn{1.f};
    glm::vec3 at(glm::vec3 p) const { return glm::vec3{xz.x, y, xz.y} + turn * p; }
  };
  Placed place(glm::vec2 xz, float yaw) const {
    Placed b;
    b.xz = xz;
    glm::quat tilt;
    glm::vec3 normal;
    lie(layout_.ground, xz, b.y, tilt, normal);
    // As world(), less the scale the points already have.
    b.turn = glm::mat3(glm::rotate(glm::mat4_cast(tilt), yaw, glm::vec3{0.f, 1.f, 0.f}));
    return b;
  }

  // Open lawn under all of the curl.
  bool onLawn(glm::vec2 xz) const {
    bool lawn = layout_.lawn.heightAt(xz) > pxe::HeightMap::kNone;
    for (int k = 0; k < 8 && lawn; ++k) {
      const float a = kTwoPi * static_cast<float>(k) / 8.f;
      lawn = layout_.lawn.heightAt(xz + kFootprint * glm::vec2{std::cos(a), std::sin(a)}) > pxe::HeightMap::kNone;
    }
    return lawn;
  }

  // The curl's yaw toward the nearest campfire within kFaceFire, if any.
  void faceFire(glm::vec2 xz, float& yaw) const {
    float nearest = kFaceFire;
    for (const glm::vec2& fire : layout_.campfires) {
      const glm::vec2 to = fire - xz;
      if (glm::length(to) >= nearest) continue;
      nearest = glm::length(to);
      yaw = std::atan2(to.x, to.y);
    }
  }

  bool clearOfScene(const Placed& b) const {
    const glm::vec3 centre = b.at(glm::vec3{0.f});
    const float far = sweep_.radius + kBedClearance;
    const int x0 = std::max(0, static_cast<int>((b.xz.x - far - gridLo_.x) / kCell));
    const int x1 = std::min(gw_ - 1, static_cast<int>((b.xz.x + far - gridLo_.x) / kCell));
    const int z0 = std::max(0, static_cast<int>((b.xz.y - far - gridLo_.y) / kCell));
    const int z1 = std::min(gh_ - 1, static_cast<int>((b.xz.y + far - gridLo_.y) / kCell));
    std::vector<std::uint32_t> near;
    for (int z = z0; z <= z1; ++z)
      for (int x = x0; x <= x1; ++x)
        for (std::uint32_t t : grid_[static_cast<std::size_t>(z) * gw_ + x])
          if (glm::length(glm::clamp(centre, triLo_[t], triHi_[t]) - centre) < far) near.push_back(t);
    std::sort(near.begin(), near.end());
    near.erase(std::unique(near.begin(), near.end()), near.end());
    const glm::mat3 back = glm::transpose(b.turn);
    const float sure = kBedClearance + kVoxelSlack, never = kBedClearance - kVoxelSlack;
    for (std::uint32_t t : near) {
      // The triangle in the model, and the voxels near it: those whose
      // middles are clearly clear or clearly not say so; the rest are
      // checked vertex by vertex.
      glm::vec3 c[3];
      for (int k = 0; k < 3; ++k) c[k] = back * (layout_.solids[3 * t + static_cast<std::size_t>(k)] - centre);
      const glm::vec3 tlo = glm::min(glm::min(c[0], c[1]), c[2]), thi = glm::max(glm::max(c[0], c[1]), c[2]);
      for (const Sweep::Block& k : sweep_.blocks) {
        if (boxGap(k.lo, k.hi, tlo, thi) >= kBedClearance) continue;
        for (std::uint32_t i : k.voxels) {
          const Sweep::Voxel& v = sweep_.voxels[i];
          const glm::vec3 q = closestOnTriangle(v.centre, c[0], c[1], c[2]);
          const float d2 = glm::dot(q - v.centre, q - v.centre);
          if (d2 >= sure * sure) continue;
          if (d2 < never * never) return false;
          for (std::uint32_t j = v.first; j < v.first + v.count; ++j) {
            const glm::vec3 r = closestOnTriangle(sweep_.vertices[j], c[0], c[1], c[2]);
            if (glm::dot(r - sweep_.vertices[j], r - sweep_.vertices[j]) < kBedClearance * kBedClearance) return false;
          }
        }
      }
    }
    return true;
  }

  bool onGround(const Placed& b) const {
    for (const glm::vec3& p : sweep_.fox) {
      const glm::vec3 w = b.at(p);
      const float g = layout_.ground.heightAt({w.x, w.z});
      if (g > pxe::HeightMap::kNone && w.y - kVoxelSlack < g - kSink) return false;
    }
    return true;
  }

  bool clearOfFoxes(const Placed& b) {
    bool looked = false;
    for (const Taken& t : taken_) {
      if (glm::length(t.xz - b.xz) >= 2.f * sweep_.reach + foxNear() + 0.1f) continue;
      if (!looked) outline(b);
      looked = true;
      for (std::size_t k = 0; k < sweep_.blocks.size(); ++k) {
        // The plan cells its voxels' middles can fall in, those within the
        // window (which reaches past foxNear). The distance changes by no
        // more than the cells' spacing, so if the one in the middle of them
        // is far enough by more than the farthest is from it, all are; else
        // each voxel is looked at.
        const glm::vec4& r = outline_[k];
        const int x0 = std::max(0, static_cast<int>(std::floor((r.x - t.lo.x) / kPlan)));
        const int z0 = std::max(0, static_cast<int>(std::floor((r.y - t.lo.y) / kPlan)));
        const int x1 = std::min(t.w - 1, static_cast<int>(std::floor((r.z - t.lo.x) / kPlan)));
        const int z1 = std::min(t.h - 1, static_cast<int>(std::floor((r.w - t.lo.y) / kPlan)));
        if (x0 > x1 || z0 > z1) continue;
        const int xm = (x0 + x1) / 2, zm = (z0 + z1) / 2;
        const float spread = kPlan * glm::length(glm::vec2{static_cast<float>(std::max(xm - x0, x1 - xm)),
                                                           static_cast<float>(std::max(zm - z0, z1 - zm))});
        if (t.dist[static_cast<std::size_t>(zm) * t.w + xm] - spread >= foxNear() + kRounding) continue;
        for (std::uint32_t i : sweep_.blocks[k].voxels) {
          const glm::vec3 w = b.at(sweep_.fox[i]);
          const int x = static_cast<int>(std::floor((w.x - t.lo.x) / kPlan));
          const int z = static_cast<int>(std::floor((w.z - t.lo.y) / kPlan));
          if (x < 0 || z < 0 || x >= t.w || z >= t.h) continue;  // the window reaches past foxNear
          if (t.dist[static_cast<std::size_t>(z) * t.w + x] < foxNear()) return false;
        }
      }
    }
    return true;
  }

  // The bed is taken: later beds keep clear of its fox.
  void take(const Placed& b) {
    seen(b);
    Taken t;
    t.xz = b.xz;
    glm::vec2 pl{1e9f}, ph{-1e9f};
    for (const glm::vec2& p : plan_) {
      pl = glm::min(pl, p);
      ph = glm::max(ph, p);
    }
    const float pad = foxNear() + 2.f * kPlan;
    t.lo = glm::floor((pl - pad) / kPlan) * kPlan;
    t.w = static_cast<int>(std::ceil((ph.x + pad - t.lo.x) / kPlan));
    t.h = static_cast<int>(std::ceil((ph.y + pad - t.lo.y) / kPlan));
    t.dist.assign(static_cast<std::size_t>(t.w) * t.h, std::numeric_limits<float>::max());
    for (const glm::vec2& p : plan_) {
      const int x = static_cast<int>(std::floor((p.x - t.lo.x) / kPlan));
      const int z = static_cast<int>(std::floor((p.y - t.lo.y) / kPlan));
      t.dist[static_cast<std::size_t>(z) * t.w + x] = 0.f;
    }
    // Squared distances along each row, then down each column.
    std::vector<float> line;
    for (int z = 0; z < t.h; ++z) {
      const auto row = t.dist.begin() + static_cast<std::ptrdiff_t>(z) * t.w;
      line.assign(row, row + t.w);
      squaredDistance(line, t.w, kPlan);
      std::copy(line.begin(), line.end(), row);
    }
    line.resize(static_cast<std::size_t>(t.h));
    for (int x = 0; x < t.w; ++x) {
      for (int z = 0; z < t.h; ++z) line[static_cast<std::size_t>(z)] = t.dist[static_cast<std::size_t>(z) * t.w + x];
      squaredDistance(line, t.h, kPlan);
      for (int z = 0; z < t.h; ++z)
        t.dist[static_cast<std::size_t>(z) * t.w + x] = std::sqrt(line[static_cast<std::size_t>(z)]);
    }
    taken_.push_back(std::move(t));
  }

  // How near a bed's fox may come to another's before it is checked cell by
  // cell (clearOfFoxes skips beds farther apart than this and both reaches).
  float foxNear() const {
    // Both seen from above (no nearer than in 3D): each vertex its voxel's
    // middle, then that middle's plan cell's.
    return kBedClearance + 2.f * (kVoxelSlack + kPlanSlack);
  }

 private:
  static constexpr float kCell = 1.f;
  // More than the rounding in placing a point or in a distance (m).
  static constexpr float kRounding = 1e-3f;
  // A taken bed's fox seen from above: the distance from every cell of a
  // window round it to the nearest cell it covers.
  struct Taken {
    glm::vec2 xz{0.f}, lo{0.f};
    int w = 0, h = 0;
    std::vector<float> dist;
  };
  // Each block of the fox in a bed, seen from above, into outline_: the xz
  // rectangle (lo, hi) its corners fall in, and so every voxel's middle in
  // it, kRounding wider all round for the arithmetic's rounding.
  void outline(const Placed& b) {
    outline_.clear();
    for (const Sweep::Block& k : sweep_.blocks) {
      glm::vec2 lo{1e9f}, hi{-1e9f};
      for (int c = 0; c < 8; ++c) {
        const glm::vec3 w = b.at({c & 1 ? k.hi.x : k.lo.x, c & 2 ? k.hi.y : k.lo.y, c & 4 ? k.hi.z : k.lo.z});
        lo = glm::min(lo, glm::vec2{w.x, w.z});
        hi = glm::max(hi, glm::vec2{w.x, w.z});
      }
      outline_.push_back({lo - kRounding, hi + kRounding});
    }
  }
  // The fox in a bed, seen from above, into plan_.
  void seen(const Placed& b) {
    plan_.clear();
    for (const glm::vec3& p : sweep_.fox) {
      const glm::vec3 w = b.at(p);
      plan_.push_back({w.x, w.z});
    }
  }

  const Sweep& sweep_;
  const SceneLayout& layout_;
  glm::vec2 gridLo_{1e9f}, gridHi_{-1e9f};
  int gw_ = 0, gh_ = 0;
  std::vector<std::vector<std::uint32_t>> grid_;
  std::vector<glm::vec3> triLo_, triHi_;
  std::vector<Taken> taken_;
  std::vector<glm::vec2> plan_;
  std::vector<glm::vec4> outline_;
};

std::vector<FoxBed> Fox::findBeds(const Sweep& sweep, const SceneLayout& layout, int count) {
  std::vector<FoxBed> found;
  if (count <= 0 || layout.lawn.empty() || layout.ground.empty()) return found;
  Beds beds(sweep, layout);
  // Every kBedStep over the lawn where beds are looked for, in a fixed
  // shuffled order; each spot draws a yaw, used when no campfire is near
  // enough to face.
  std::vector<glm::vec2> spots;
  const glm::vec2 from = glm::ceil(glm::max(layout.bedsLo, layout.lawn.min()) / kBedStep);
  const glm::vec2 to = glm::floor(glm::min(layout.bedsHi, layout.lawn.max()) / kBedStep);
  for (int z = static_cast<int>(from.y); z <= static_cast<int>(to.y); ++z)
    for (int x = static_cast<int>(from.x); x <= static_cast<int>(to.x); ++x)
      spots.push_back(glm::vec2{static_cast<float>(x), static_cast<float>(z)} * kBedStep);
  std::mt19937 rng(kBedSeed);
  for (std::size_t i = spots.size(); i > 1; --i)
    std::swap(spots[i - 1], spots[std::min(i - 1, static_cast<std::size_t>(draw01(rng) * static_cast<float>(i)))]);
  for (const glm::vec2& xz : spots) {
    if (static_cast<int>(found.size()) >= count) break;
    float yaw = kTwoPi * draw01(rng);
    if (!beds.onLawn(xz)) continue;
    beds.faceFire(xz, yaw);
    const Beds::Placed b = beds.place(xz, yaw);
    if (!beds.clearOfFoxes(b) || !beds.clearOfScene(b) || !beds.onGround(b)) continue;
    beds.take(b);
    found.push_back({xz, yaw});
  }
  return found;
}

bool Fox::findBed(const Sweep& sweep, const SceneLayout& layout, glm::vec2 at, glm::vec2 toCamera,
                  std::span<const Fox> foxes, FoxBed& bed) const {
  if (!model_ || layout.lawn.empty() || layout.ground.empty()) return false;
  Beds beds(sweep, layout);
  // The foxes already lying near enough to matter.
  for (const Fox& other : foxes) {
    if (&other == this || !other.model_) continue;
    if (glm::length(other.bedPos_ - at) >= kAddReach + 2.f * sweep.reach + beds.foxNear() + 0.1f) continue;
    beds.take(beds.place(other.bedPos_, other.bedYaw_));
  }
  // findBeds' spots within reach, nearest first (and, as far, in rows of
  // increasing z, then x, so the order is fixed).
  std::vector<glm::vec2> spots;
  const glm::vec2 from = glm::ceil((at - kAddReach) / kBedStep);
  const glm::vec2 to = glm::floor((at + kAddReach) / kBedStep);
  for (int z = static_cast<int>(from.y); z <= static_cast<int>(to.y); ++z)
    for (int x = static_cast<int>(from.x); x <= static_cast<int>(to.x); ++x) {
      const glm::vec2 xz = glm::vec2{static_cast<float>(x), static_cast<float>(z)} * kBedStep;
      if (glm::length(xz - at) <= kAddReach) spots.push_back(xz);
    }
  std::stable_sort(spots.begin(), spots.end(),
                   [&](glm::vec2 a, glm::vec2 b) { return glm::length(a - at) < glm::length(b - at); });
  // Standing, it faces standTurn_ round from the curl's yaw.
  const float facingCamera = std::atan2(toCamera.x, toCamera.y) - standTurn_;
  for (const glm::vec2& xz : spots) {
    if (!beds.onLawn(xz)) continue;
    float yaw = facingCamera;
    beds.faceFire(xz, yaw);
    if (!bedFree(xz, yaw, foxes)) continue;
    const Beds::Placed b = beds.place(xz, yaw);
    if (!beds.clearOfFoxes(b) || !beds.clearOfScene(b) || !beds.onGround(b)) continue;
    bed = {xz, yaw};
    return true;
  }
  return false;
}

}  // namespace game
