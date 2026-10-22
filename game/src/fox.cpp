#include "fox.hpp"

#include "hero.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
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

}  // namespace

void Fox::build() {
  const std::string path = std::string(PXE_ASSET_DIR) + "/characters/fox.glb";
  model_ = pxe::SkinnedModel::load(path, kFirstMaterial);
  // The curl and the waking are all in the clips: the rest pose alone is
  // the fox standing in its bind pose. Looking round at the walk's target is
  // Fox_LookAround again; both legs of the walk are Fox_Walk.
  const char* names[kStateCount] = {"Fox_Rest", "Fox_StandUp", "Fox_LookAround", "Fox_LieDown",
                                    "Fox_Walk", "Fox_LookAround", "Fox_Walk"};
  for (int s = 0; s < kStateCount; ++s) {
    clip_[s] = model_.clip(names[s]);
    if (clip_[s] < 0) throw std::runtime_error(std::string("no ") + names[s] + " clip in " + path);
    if (model_.clipDuration(clip_[s]) <= 0.f) throw std::runtime_error(std::string(names[s]) + " is empty in " + path);
  }
  // The way it faces standing, against the way the curl faces: the turn its
  // root has made by the end of Fox_StandUp (a quarter-turn left; Fox_Walk's
  // root is the same, so it walks that way too).
  const int root = model_.joint("root");
  if (root < 0) throw std::runtime_error("no root joint in " + path);
  pxe::SkinnedModel::Pose rest, standing;
  model_.restPose(rest);
  model_.sample(clip_[kStandUp], model_.clipDuration(clip_[kStandUp]), standing, false);
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
  model_.skin(standing, glm::scale(glm::mat4{1.f}, glm::vec3{kSize}), verts_);
  for (const char* name : {"forepaw.L", "forepaw.R", "hindpaw.L", "hindpaw.R"}) {
    const int j = model_.joint(name);
    if (j < 0) continue;
    const glm::vec3 p = model_.jointPoint(j, glm::vec3(model_.restJoint(j)[3]));
    pivot_ += glm::vec2{p.x, p.z};
    ++paws;
  }
  if (paws > 0) pivot_ /= static_cast<float>(paws);
  // How far each phase of the walk is from standing: the sum of every
  // joint's turn between the two.
  walkGap_.assign(kPhases, 0.f);
  const float walkLength = model_.clipDuration(clip_[kWalkOut]);
  for (int k = 0; k < kPhases; ++k) {
    model_.sample(clip_[kWalkOut], walkLength * static_cast<float>(k) / kPhases, walkPose_);
    for (std::size_t j = 0; j < walkPose_.r.size(); ++j)
      walkGap_[static_cast<std::size_t>(k)] +=
          2.f * std::acos(std::min(1.f, std::fabs(glm::dot(walkPose_.r[j], standing.r[j]))));
  }
  model_.sample(clip_[kRest], 0.f, pose_);
  model_.skin(pose_, glm::mat4{1.f}, verts_);
  mesh_.update(verts_, model_.indices());
  built_ = true;
}

void Fox::spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw) {
  pos_ = xz;
  // The ground's smooth height under the middle of the curl, and its slope
  // across the curl: the curl rests on the ground at its lowest point, so on
  // a slope it is tilted to lie along it rather than float over the downhill
  // side. The standing fox, which reaches about 1.6 m out, keeps the same
  // tilt: a plane through the ground is the same plane whether it is sampled
  // 0.41 m out or 1.6 m, so long as the ground under the fox is planar, and
  // one tilt for every clip keeps the joins seamless (kRuinsFoxSpot is chosen
  // where that holds; see scene.hpp).
  const pxe::HeightMap& ground = scene.ground;
  const float h = ground.heightAt(xz);
  y_ = h > pxe::HeightMap::kNone ? h : 0.f;
  yaw_ = yaw;
  tilt_ = glm::quat{1.f, 0.f, 0.f, 0.f};
  bedNormal_ = glm::vec3{0.f, 1.f, 0.f};
  if (groundNormal(ground, xz, 0.6f * kFootprint, bedNormal_)) tilt_ = tiltTo(bedNormal_);
  // Where it always comes back to.
  bedPos_ = pos_;
  bedY_ = y_;
  bedYaw_ = yaw_;
  bedTilt_ = tilt_;
  // Asleep, at the start of the first rest, so it first wakes at least
  // kRestMin after it is put down.
  rng_.seed(kSeed);
  walkRng_.seed(kWalkSeed);
  walking_ = forceWalk_ = false;
  hold_ = false;
  enter(kRest, false);
}

void Fox::setFire(glm::vec2 centre, float reach) {
  fire_ = centre;
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
    model_.sample(clip_[state_], model_.clipDuration(clip_[state_]), fadeFrom_, false);
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
  // at the far end back to standing as the next clip starts: pose() fades
  // between them with the speed.
  model_.sample(clip_[state_], model_.clipDuration(clip_[state_]), fadeFrom_, false);
  const State after = leg == kWalkOut ? kLookOut : kLieDown;
  model_.sample(clip_[after], 0.f, fadeTo_, false);
  fade_ = kFade;
  state_ = leg;
  clock_ = 0.f;
}

float Fox::stateLength() const {
  if (state_ == kWalkOut) return route_.timeOut;
  if (state_ == kWalkBack) return route_.timeBack;
  return model_.clipDuration(clip_[state_]);
}

void Fox::update(const pxe::Scene& scene, float dt) {
  if (!built_ || dt <= 0.f) return;
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
        planWalk(scene);
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
}

void Fox::holdRest(float seconds) {
  if (!built_) return;
  hold_ = true;
  walking_ = false;
  enter(kRest, false);
  pos_ = bedPos_;
  y_ = bedY_;
  yaw_ = bedYaw_;
  tilt_ = bedTilt_;
  // As it always slept in captures: just the place in the loop.
  const float length = model_.clipDuration(clip_[kRest]);
  clock_ = length > 0.f ? std::fmod(std::max(seconds, 0.f), length) : 0.f;
}

void Fox::wake(const pxe::Scene& scene, float seconds) {
  if (!built_) return;
  hold_ = false;
  enter(kStandUp, false);
  planWalk(scene);
  update(scene, seconds);
  place(scene);
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

void Fox::planWalk(const pxe::Scene& scene) {
  walking_ = false;
  const bool forced = forceWalk_;
  forceWalk_ = false;
  // Every wake draws whether it walks, forced or not, so the draws after it
  // are the same either way.
  const bool wants = draw01(walkRng_) < 0.5f || forced;
  if (!wants || fireReach_ < 0.f) return;
  const int tries = forced ? kForcedTries : kTries;
  for (int i = 0; i < tries; ++i) {
    // Uniform over the ring round the bed.
    const float a = kTwoPi * draw01(walkRng_);
    const float r = std::sqrt(kMinReach * kMinReach + (kMaxReach * kMaxReach - kMinReach * kMinReach) * draw01(walkRng_));
    const glm::vec2 target = bedPos_ + r * glm::vec2{std::cos(a), std::sin(a)};
    Route route;
    if (!layRoute(target, route) || !routeClear(scene, route)) continue;
    route.timeOut = legTime(route.out);
    route.timeBack = legTime(route.total - route.out);
    route.phaseOut = startPhase(route.out);
    route.phaseBack = startPhase(route.total - route.out);
    route_ = std::move(route);
    walking_ = true;
    const float look = model_.clipDuration(clip_[kLookAround]);
    const float down = model_.clipDuration(clip_[kStandUp]) + look + route_.timeOut + look + route_.timeBack;
    std::fprintf(stderr,
                 "pxe: the fox walks to a point %.1f m from its bed (target %d): %.1f m out in %.1f s, "
                 "%.1f m back in %.1f s; it lies down %.1f s into its wake\n",
                 r, i + 1, route_.out, route_.timeOut, route_.total - route_.out, route_.timeBack, down);
    return;
  }
  std::fprintf(stderr, "pxe: the fox stays in its bed: none of %d walks was clear\n", tries);
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
  // Every kCheckStep along the walk, and the target, where it stops to look.
  auto clear = [&](float s, glm::vec3* normal) {
    glm::vec2 pos{0.f};
    float yaw = 0.f;
    routeAt(route, s, pos, yaw);
    if (glm::length(pos - bedPos_) > kMaxReach + 1e-3f) return false;
    // The model's ground axes at this yaw: x its heading, z its right.
    const glm::vec2 ax{std::cos(yaw), -std::sin(yaw)}, az{std::sin(yaw), std::cos(yaw)};
    // The body, a box in those axes, clear of the fire's dirt patch.
    const glm::vec2 toFire = fire_ - pos;
    const glm::vec2 local{glm::dot(toFire, ax), glm::dot(toFire, az)};
    const glm::vec2 nearest{std::clamp(local.x, kBodyBack, kBodyFront), std::clamp(local.y, kBodyLeft, kBodyRight)};
    if (glm::length(local - nearest) < fireReach_ + kFireClearance) return false;
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

float Fox::walkSpeed() const { return kStride * kSize / model_.clipDuration(clip_[kWalkOut]); }

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

void Fox::pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) {
  if (!built_) return;
  if (state_ == kWalkOut || state_ == kWalkBack) {
    // Fox_Walk on the ground covered, so its planted paws keep pace with the
    // ground, faded from and back to standing as it speeds up and slows.
    const bool out = state_ == kWalkOut;
    const float length = out ? route_.out : route_.total - route_.out;
    const float time = out ? route_.timeOut : route_.timeBack;
    float phase = (out ? route_.phaseOut : route_.phaseBack) + legDistance(clock_, length, time) / (kStride * kSize);
    phase -= std::floor(phase);
    model_.sample(clip_[state_], phase * model_.clipDuration(clip_[state_]), pose_);
    const float fade = std::min(kWalkFade, 0.5f * time);
    const float walk = smooth01(std::min(clock_, time - clock_) / fade);
    if (walk < 1.f) pxe::SkinnedModel::blend(clock_ < 0.5f * time ? fadeFrom_ : fadeTo_, pose_, walk, pose_);
  } else {
    // The rest loops; the waking clips play once each.
    model_.sample(clip_[state_], clock_, pose_, state_ == kRest);
    if (fade_ < kFade) pxe::SkinnedModel::blend(fadeFrom_, pose_, fade_ / kFade, pose_);
  }
  const glm::mat4 world =
      glm::scale(glm::rotate(glm::translate(glm::mat4{1.f}, glm::vec3{pos_.x, y_, pos_.y}) * glm::mat4_cast(tilt_), yaw_,
                             glm::vec3{0.f, 1.f, 0.f}),
                 glm::vec3{kSize});
  model_.skin(pose_, world, verts_);
  mesh_.update(verts_, model_.indices());
  actors.push_back(&mesh_);
  // Its vertices point at slots firstMaterial() + i (SkinnedModel::load).
  const auto& mats = model_.materials();
  const int first = model_.firstMaterial();
  for (int i = 0; i < static_cast<int>(mats.size()) && first + i < pxe::kActorMaterials; ++i)
    materials[first + i] = mats[static_cast<std::size_t>(i)];
}

}  // namespace game
