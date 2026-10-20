#include "hero.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <string>

namespace game {
namespace {

constexpr float kPi = 3.14159265f;

// Ground covered by one full cycle (two steps) at size 1, matched to the
// clips' strides so the feet stay put; with each clip's length that sets the
// walking and running speeds.
constexpr float kWalkCycle = 1.4f;
constexpr float kRunCycle = 2.5f;
constexpr float kAccel = 9.f;    // how fast it reaches the speed asked for (1/s)
constexpr float kStop = 11.f;    // and comes to rest
constexpr float kStep = 0.55f;   // highest step it climbs or drops (m): the ruins' stairs are up to half a metre
constexpr float kGravity = 30.f;
constexpr float kFoot = 0.15f;   // half the footprint it stands on (m, at size 1)
constexpr float kAhead = 0.2f;   // how far ahead it looks for walls

// The sword combo, after Hyper Light Drifter: forehand, backhand, then a
// heavier finisher. Each cut holds its wind-up a beat, then lunges forward
// through the strike; a press any time after the wind-up is kept and cuts
// the next one from the follow-through. Frames (at 30 a second) match
// ATTACKS in build_hooded_hero.py: wind-up held to, strike lands, follow
// through, length. Lunge: speed (m/s at size 1, easing off) and how long.
struct AttackSpec {
  const char* clip;
  float wind, strike, follow, length;
  float lunge, lungeTime;
};
constexpr AttackSpec kAttacks[3] = {
    {"HH_Attack1", 4.f / 30, 6.f / 30, 8.f / 30, 16.f / 30, 7.f, 0.14f},
    {"HH_Attack2", 4.f / 30, 6.f / 30, 8.f / 30, 16.f / 30, 7.f, 0.14f},
    {"HH_Attack3", 6.f / 30, 9.f / 30, 11.f / 30, 24.f / 30, 10.f, 0.18f},
};
constexpr float kBufferFrom = 0.05f;  // seconds in before a press counts toward the next cut
constexpr float kComboGrace = 0.3f;   // after a cut ends, a press still continues the combo
constexpr float kFadeIn = 0.04f;     // seconds to ease into the attack
constexpr float kFadeOut = 0.2f;     // and back out of it at the end
constexpr float kCut = 0.08f;        // easing out of a pose cut short
constexpr float kSmearAge = 0.1f;    // seconds of the swing the smear shows
constexpr int kSmearPoints = 40;     // points along it, at most

float smooth01(float x) {
  x = std::clamp(x, 0.f, 1.f);
  return x * x * (3.f - 2.f * x);
}

}  // namespace

void Hero::build() {
  model_ = pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/characters/hooded_hero.glb");
  idleClip_ = model_.clip("HH_Idle");
  walkClip_ = model_.clip("HH_Walk");
  runClip_ = model_.clip("HH_Run");
  for (int i = 0; i < 3; ++i) attackClips_[i] = model_.clip(kAttacks[i].clip);
  spine_ = model_.joint("spine");
  chest_ = model_.joint("chest");
  hips_ = model_.joint("hips");
  hood_ = model_.joint("hood");
  head_ = model_.joint("head");
  // The cape's chains, at their angles round the back (from straight back
  // toward the left), and the axis that swings each one out from the body.
  // The model faces +z with its left at +x.
  const struct {
    const char* name;
    float degrees;
  } chains[] = {{"R2", -100.f}, {"R1", -50.f}, {"C", 0.f}, {"L1", 50.f}, {"L2", 100.f}};
  for (const auto& c : chains) {
    Chain chain;
    for (int i = 0; i < 3; ++i) chain.joints[i] = model_.joint("cape." + std::string(c.name) + "." + std::to_string(i + 1));
    const float a = c.degrees * kPi / 180.f;
    chain.out = {std::cos(a), 0.f, std::sin(a)};
    cape_.push_back(chain);
  }
  // How much of each joint the attack takes: all the upper body; the legs,
  // hips and root only as much as it stands still; cape and flaps half.
  lower_.assign(static_cast<std::size_t>(model_.jointCount()), 0.f);
  for (const char* name : {"root", "hips", "thigh.L", "thigh.R", "shin.L", "shin.R", "foot.L", "foot.R", "toe.L", "toe.R"}) {
    const int j = model_.joint(name);
    if (j >= 0) lower_[static_cast<std::size_t>(j)] = 1.f;
  }
  for (const Chain& c : cape_)
    for (int j : c.joints)
      if (j >= 0) lower_[static_cast<std::size_t>(j)] = 0.5f;
  for (const char* name : {"tabard.L.1", "tabard.L.2", "tabard.R.1", "tabard.R.2"}) {
    const int j = model_.joint(name);
    if (j >= 0) lower_[static_cast<std::size_t>(j)] = 0.5f;
  }
  mask_.assign(lower_.size(), 0.f);
  // The blade at rest: its bone runs from the grip along it.
  sword_ = model_.joint("sword");
  if (sword_ >= 0) {
    const glm::mat4 rest = model_.restJoint(sword_);
    grip_ = glm::vec3(rest[3]);
    const glm::vec3 along = glm::normalize(glm::vec3(rest[1]));
    bladeBase_ = grip_ + along * 0.08f;
    bladeTip_ = grip_ + along * 0.8f;
  }
  const auto& mats = model_.materials();
  for (int i = 0; i < static_cast<int>(mats.size()); ++i)
    if (mats[i].glow && glow_ < 0) glow_ = i;
  trailVerts_.assign(kSmearPoints * 2, pxe::Vertex{});
  for (int i = 0; i + 1 < kSmearPoints; ++i) {
    const auto a = static_cast<std::uint32_t>(2 * i);
    trailIndices_.insert(trailIndices_.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  model_.restPose(pose_);
  last_ = pose_;
  model_.skin(pose_, glm::mat4{1.f}, verts_);
  mesh_.update(verts_, model_.indices());
  built_ = true;
}

void Hero::spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw) {
  pos_ = xz;
  const float h = scene.ground.cellTop(xz);
  y_ = h > pxe::HeightMap::kNone ? h : 0.f;
  vel_ = glm::vec2{0.f};
  yaw_ = yaw;
  yawRate_ = 0.f;
  phase_ = clock_ = runBlend_ = lastSpeed_ = fall_ = fadeLeft_ = 0.f;
  attackT_ = -1.f;
  combo_ = 0;
  queued_ = false;
  comboGrace_ = 0.f;
  trail_.clear();
  turn_.reset(yaw);
  lagSpeed_.reset(0.f);
  lagYaw_.reset(yaw);
  lean_.reset(0.f);
  hoodLag_.reset(0.f);
}

void Hero::update(const pxe::Scene& scene, float dt, const HeroInput& input) {
  if (!built_ || dt <= 0.f) return;
  clock_ += dt;
  dt_ = dt;

  // The combo. A press starts the first cut, or is kept (from early in a
  // cut) for the next, which starts from the follow-through. Each cut turns
  // to the stick as it starts. Pushing the stick after a cut's
  // follow-through cancels its recovery; a press soon after still carries on
  // the combo, which ends after the finisher.
  const float tilt = std::min(glm::length(input.move), 1.f);
  const glm::vec2 dir = tilt > 1e-4f ? input.move / glm::length(input.move) : glm::vec2{0.f};
  auto start = [&](int which) {
    if (attackT_ >= 0.f) {
      from_ = last_;  // cut short: ease out of where it was
      fadeLeft_ = kCut;
    }
    combo_ = which;
    attackT_ = 0.f;
    queued_ = false;
    if (tilt > 0.2f) {
      const float want = std::atan2(dir.x, dir.y);
      yaw_ = turn_.value() + std::remainder(want - turn_.value(), 2.f * kPi);
      turn_.reset(yaw_);
    }
  };
  comboGrace_ = std::max(0.f, comboGrace_ - dt);
  if (attackT_ >= 0.f) {
    const AttackSpec& a = kAttacks[combo_];
    attackT_ += dt;
    if (input.attack && attackT_ >= kBufferFrom && combo_ < 2) queued_ = true;
    if (queued_ && attackT_ >= a.follow) {
      start(combo_ + 1);
    } else if (attackT_ >= a.length || (attackT_ >= a.follow && tilt > 0.3f)) {
      if (attackT_ < a.length) {
        from_ = last_;
        fadeLeft_ = kCut;
      }
      attackT_ = -1.f;
      comboGrace_ = combo_ < 2 ? kComboGrace : 0.f;
    }
  } else if (input.attack && attackClips_[0] >= 0) {
    start(comboGrace_ > 0.f ? combo_ + 1 : 0);
  }
  const AttackSpec& spec = kAttacks[combo_];
  const bool swinging = attackT_ >= 0.f && attackT_ < spec.follow;
  const float walkSpeed = kWalkCycle * size_ / std::max(model_.clipDuration(walkClip_), 1e-3f);
  const float runSpeed = kRunCycle * size_ / std::max(model_.clipDuration(runClip_), 1e-3f);

  // Speed eases toward what the stick asks for, so it sets off and pulls up
  // over a few frames rather than jumping. Through a swing the stick is
  // ignored: it holds still through the wind-up, then lunges, easing off.
  const glm::vec2 target = swinging ? glm::vec2{0.f} : dir * tilt * (input.walk ? walkSpeed : runSpeed);
  const float rate = glm::length(target) > glm::length(vel_) ? kAccel : kStop;
  vel_ += (target - vel_) * (1.f - std::exp(-dt * (swinging ? 25.f : rate)));
  if (attackT_ >= spec.wind && attackT_ < spec.wind + spec.lungeTime) {
    const float u = (attackT_ - spec.wind) / spec.lungeTime;
    vel_ = glm::vec2{std::sin(yaw_), std::cos(yaw_)} * spec.lunge * size_ * (1.f - u) * (1.f - u);
  }

  // One axis at a time, so it slides along a wall met at an angle. It steps
  // up anything within a knee of its feet and treats a bigger rise, deep
  // water and the edge of the world as walls.
  const pxe::HeightMap& ground = scene.ground;
  const float foot = kFoot * size_;
  auto groundAt = [&](glm::vec2 p) {
    float top = pxe::HeightMap::kNone;
    for (const glm::vec2 o : {glm::vec2{-foot, -foot}, glm::vec2{foot, -foot}, glm::vec2{-foot, foot}, glm::vec2{foot, foot}})
      top = std::max(top, ground.cellTop(p + o));
    return top;
  };
  auto blocked = [&](glm::vec2 from, glm::vec2 to) {
    const glm::vec2 d = to - from;
    const glm::vec2 ahead = glm::length(d) > 1e-6f ? glm::normalize(d) * kAhead * size_ : glm::vec2{0.f};
    for (const glm::vec2 probe : {to, to + ahead}) {
      const float there = groundAt(probe);
      if (there <= pxe::HeightMap::kNone || there - y_ > kStep) return true;
      const float water = scene.waterTop.cellTop(probe);
      if (water > pxe::HeightMap::kNone && water > there + 0.15f) return true;
    }
    return false;
  };
  const glm::vec2 from = pos_;
  for (int axis = 0; axis < 2; ++axis) {
    glm::vec2 next = pos_;
    next[axis] += vel_[axis] * dt;
    if (blocked(pos_, next)) {
      vel_[axis] = 0.f;
      continue;
    }
    pos_ = next;
  }
  // Feet onto each step's top quickly; a drop deeper than a step is a fall.
  const float top = groundAt(pos_);
  if (top > pxe::HeightMap::kNone) {
    if (fall_ > 0.f || top < y_ - kStep) {
      fall_ += kGravity * dt;
      y_ -= fall_ * dt;
      if (y_ <= top) y_ = top, fall_ = 0.f;
    } else {
      y_ += (top - y_) * std::min(1.f, dt * 25.f);
    }
  }
  const float moved = glm::length(pos_ - from);
  const float speed = moved / dt;

  // Facing turns smoothly toward the way it moves.
  const float prevYaw = yaw_;
  if (glm::length(vel_) > 0.05f * size_ && attackT_ < 0.f) {
    float want = std::atan2(vel_.x, vel_.y);
    want = turn_.value() + std::remainder(want - turn_.value(), 2.f * kPi);
    yaw_ = turn_.update(dt, want);
  } else {
    yaw_ = turn_.update(dt, turn_.value());
  }
  yawRate_ += ((yaw_ - prevYaw) / dt - yawRate_) * (1.f - std::exp(-dt * 12.f));

  // One phase for both cycles, advanced by the ground covered over the
  // stride of the blend between them, so walk and run stay in step.
  const float runTarget = std::clamp((speed - walkSpeed) / std::max(runSpeed - walkSpeed, 1e-3f), 0.f, 1.f);
  runBlend_ += (runTarget - runBlend_) * (1.f - std::exp(-dt * 8.f));
  const float cycle = (kWalkCycle + (kRunCycle - kWalkCycle) * runBlend_) * size_;
  phase_ = std::fmod(phase_ + moved / cycle, 1.f);

  lagSpeed_.update(dt, speed);
  lagYaw_.update(dt, yaw_);
  const float accel = (speed - lastSpeed_) / dt;
  lastSpeed_ = speed;
  lean_.update(dt, std::clamp(accel * 0.03f / size_, -0.25f, 0.3f));
  hoodLag_.update(dt, speed / size_);
}

void Hero::pose(std::vector<const pxe::Mesh*>& actors, std::vector<const pxe::Mesh*>& effects,
                pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) {
  if (!built_) return;
  const float speed = glm::length(vel_);
  const float walkSpeed = kWalkCycle * size_ / std::max(model_.clipDuration(walkClip_), 1e-3f);

  // The clips, blended: walk and run on the shared phase, then idle under
  // them as it slows to a stop.
  model_.sample(walkClip_, phase_ * model_.clipDuration(walkClip_), pose_);
  if (runBlend_ > 0.001f) {
    model_.sample(runClip_, phase_ * model_.clipDuration(runClip_), run_);
    pxe::SkinnedModel::blend(pose_, run_, runBlend_, pose_);
  }
  const float moving = smooth01(speed / (0.6f * walkSpeed));
  if (moving < 0.999f) {
    model_.sample(idleClip_, clock_, idle_);
    pxe::SkinnedModel::blend(idle_, pose_, moving, pose_);
  }

  // The attack over it all: the whole body standing, the upper body moving.
  if (attackT_ >= 0.f) {
    const AttackSpec& a = kAttacks[combo_];
    const float w = smooth01(attackT_ / kFadeIn) * smooth01((a.length - attackT_) / kFadeOut);
    // The whole body through the swing; the legs give way to the stride after.
    const float legs = attackT_ < a.follow ? w : w * (1.f - 0.9f * moving);
    for (std::size_t j = 0; j < mask_.size(); ++j) mask_[j] = lower_[j] >= 1.f ? legs : lower_[j] > 0.f ? 0.5f * w : w;
    model_.sample(attackClips_[combo_], attackT_, attack_, false);
    pxe::SkinnedModel::blend(pose_, attack_, mask_, pose_);
  }
  // A pose cut short eases into the new one.
  if (fadeLeft_ > 0.f) {
    pxe::SkinnedModel::blend(from_, pose_, 1.f - smooth01(fadeLeft_ / kCut), pose_);
    fadeLeft_ = std::max(0.f, fadeLeft_ - dt_);
  }
  last_ = pose_;

  // Over the clips: lean into speeding up (and back pulling up), bank into turns.
  const glm::vec3 X{1.f, 0.f, 0.f}, Z{0.f, 0.f, 1.f};
  const float lean = lean_.value();
  model_.turn(pose_, spine_, X, lean * 0.5f);
  model_.turn(pose_, chest_, X, lean * 0.4f);
  const float bank = std::clamp(-yawRate_ * speed / size_ * 0.025f, -0.18f, 0.18f);
  model_.turn(pose_, hips_, Z, bank);
  model_.turn(pose_, head_, Z, -bank * 0.6f);

  // Follow-through: the cape trails out as it speeds up and swings in as it
  // stops, and lags behind a turn; the hood's point does the same.
  const float surge = std::clamp((speed - lagSpeed_.value()) / size_ * 0.12f, -0.3f, 0.5f);
  const float swing = std::clamp(std::remainder(yaw_ - lagYaw_.value(), 2.f * kPi), -0.6f, 0.6f);
  static constexpr float kLift[3] = {0.8f, 0.45f, 0.35f};
  static constexpr float kSide[3] = {0.45f, 0.3f, 0.25f};
  for (const Chain& c : cape_) {
    for (int i = 0; i < 3; ++i) {
      model_.turn(pose_, c.joints[i], c.out, surge * kLift[i]);
      model_.turn(pose_, c.joints[i], Z, -swing * kSide[i]);
    }
  }
  model_.turn(pose_, hood_, X, -std::clamp((speed / size_ - hoodLag_.value()) * 0.15f, -0.3f, 0.4f));
  model_.turn(pose_, hood_, Z, -swing * 0.3f);

  const glm::mat4 world = glm::scale(
      glm::rotate(glm::translate(glm::mat4{1.f}, glm::vec3{pos_.x, y_, pos_.y}), yaw_, glm::vec3{0.f, 1.f, 0.f}),
      glm::vec3{size_});
  model_.skin(pose_, world, verts_);
  mesh_.update(verts_, model_.indices());
  actors.push_back(&mesh_);
  smear(effects);
  const auto& mats = model_.materials();
  for (int i = 0; i < pxe::kActorMaterials && i < static_cast<int>(mats.size()); ++i) materials[i] = mats[i];
}

void Hero::smear(std::vector<const pxe::Mesh*>& effects) {
  // Through the swing, the blade's base and tip each frame; older ones age out.
  for (Sample& s : trail_) s.age += dt_;
  trail_.erase(std::remove_if(trail_.begin(), trail_.end(), [](const Sample& s) { return s.age > kSmearAge; }),
               trail_.end());
  if (sword_ < 0 || glow_ < 0) return;
  const AttackSpec& a = kAttacks[combo_];
  if (attackT_ >= a.wind - 0.5f / 30.f && attackT_ <= a.follow + 3.f / 30.f)
    trail_.push_back({model_.jointPoint(sword_, grip_), model_.jointPoint(sword_, bladeBase_),
                      model_.jointPoint(sword_, bladeTip_), 0.f});
  if (trail_.size() < 2) return;

  // A fan between the blade's places, each gap filled with in-between
  // blades swung round the grip (t3ssel8r's smear), narrowing to the tip as
  // it ages, so it trails off in a crescent. Two points a sample: the
  // smear's inner and outer edge.
  const float surface = static_cast<float>(pxe::kActorSurface + glow_);
  std::vector<glm::vec3> inner, outer;
  for (std::size_t i = 0; i + 1 < trail_.size(); ++i) {
    const Sample& a = trail_[i];
    const Sample& b = trail_[i + 1];
    const int steps = 4;
    for (int k = 0; k < steps || (i + 2 == trail_.size() && k == steps); ++k) {
      const float f = static_cast<float>(k) / steps;
      const glm::vec3 grip = glm::mix(a.grip, b.grip, f);
      const glm::vec3 da = a.tip - a.grip, db = b.tip - b.grip;
      const float len = glm::mix(glm::length(da), glm::length(db), f);
      const glm::vec3 dir = glm::normalize(glm::mix(glm::normalize(da), glm::normalize(db), f));
      const float baseLen = glm::mix(glm::length(a.base - a.grip), glm::length(b.base - b.grip), f);
      const float age = glm::mix(a.age, b.age, f) / kSmearAge;  // 0 now .. 1 gone
      const float from = glm::mix(baseLen, len, 0.25f + 0.7f * age);
      inner.push_back(grip + dir * from);
      outer.push_back(grip + dir * len);
    }
  }
  const int n = std::min(static_cast<int>(inner.size()), kSmearPoints);
  const int skip = static_cast<int>(inner.size()) - n;
  for (int i = 0; i < kSmearPoints; ++i) {
    const int k = skip + std::min(i, n - 1);  // unused points fold onto the newest
    const glm::vec3 p = inner[static_cast<std::size_t>(k)], q = outer[static_cast<std::size_t>(k)];
    trailVerts_[static_cast<std::size_t>(2 * i)] = {p.x, p.y, p.z, 0.f, 1.f, 0.f, surface};
    trailVerts_[static_cast<std::size_t>(2 * i + 1)] = {q.x, q.y, q.z, 0.f, 1.f, 0.f, surface};
  }
  trailMesh_.update(trailVerts_, trailIndices_);
  effects.push_back(&trailMesh_);
}

}  // namespace game
