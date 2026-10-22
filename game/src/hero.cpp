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
constexpr float kGravity = 30.f;
constexpr float kFoot = 0.15f;   // half the footprint it stands on (m, at size 1)
constexpr float kAhead = 0.2f;   // how far ahead it looks for walls

// The cuts, after Hyper Light Drifter: forehand, backhand and a heavier
// third. Frames (30 a second) match ATTACKS in build_hooded_hero.py - the
// wind-up reached, the sweep through, the blade held out until, the
// length - and the lunge: the ground it covers at size 1 (m) and over how
// long, starting with the wind-up, fast at first and easing off.
struct Cut {
  const char* clip;
  float wind, sweep, held, length;
  float lunge, lungeTime;
};
constexpr Cut kCuts[3] = {
    {"HH_Slash1", 2.f / 30, 3.f / 30, 7.f / 30, 12.f / 30, 0.38f, 0.1f},
    {"HH_Slash2", 2.f / 30, 3.f / 30, 7.f / 30, 12.f / 30, 0.38f, 0.1f},
    {"HH_Slash3", 3.f / 30, 5.f / 30, 10.f / 30, 16.f / 30, 0.65f, 0.13f},
};
constexpr float kBufferFrom = 0.03f;  // seconds into a cut before a press counts toward the next
constexpr float kChainAfter = 2.f / 30;  // the next cut starts this long after the sweep
constexpr float kComboGrace = 0.25f;  // after a cut ends, a press still carries on the combo
constexpr float kChargeAfter = 0.3f;  // the button held this long after a cut starts a charge
constexpr float kChargeTime = 0.55f;  // charging this long readies the spin
constexpr float kChargePace = 0.25f;  // of the walk, while charging
constexpr float kSpinTurn = 0.26f;    // seconds the spin takes to turn round
constexpr float kSpinLunge = 1.0f;    // m at size 1
constexpr float kFadeIn = 0.03f;      // seconds to ease into a cut
constexpr float kFadeOut = 0.12f;     // and back out of it at the end
constexpr float kCut = 0.06f;         // easing out of a pose cut short
// The smear: how long each moment of the sweep shows, and then shatters.
constexpr float kSmearAge = 0.09f;
constexpr int kSmearPoints = 48;
constexpr int kShards = 96;
// The hero's actor-material slots are 0-7 (other characters take theirs from
// 8 up): its model's materials from 0, and the smear's white-hot core in the
// last of them.
constexpr int kCoreSlot = 7;

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
  for (int i = 0; i < 3; ++i) slashClips_[i] = model_.clip(kCuts[i].clip);
  chargeClip_ = model_.clip("HH_Charge");
  spinClip_ = model_.clip("HH_Spin");
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
  // How much of each joint a sword move takes: all the upper body; the legs,
  // hips and root as much as it stands still; cape and flaps half.
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
  // Fixed-size meshes for the smear (a strip) and the fragments (two crossed
  // quads each); unused parts fold away to nothing.
  trailVerts_.assign(kSmearPoints * 2, pxe::Vertex{});
  for (int i = 0; i + 1 < kSmearPoints; ++i) {
    const auto a = static_cast<std::uint32_t>(2 * i);
    trailIndices_.insert(trailIndices_.end(), {a, a + 1, a + 3, a, a + 3, a + 2});
  }
  shardVerts_.assign(kShards * 8, pxe::Vertex{});
  for (int i = 0; i < kShards * 2; ++i) {
    const auto a = static_cast<std::uint32_t>(4 * i);
    shardIndices_.insert(shardIndices_.end(), {a, a + 1, a + 2, a, a + 2, a + 3});
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
  spin_ = yawRate_ = 0.f;
  phase_ = clock_ = runBlend_ = lastSpeed_ = fall_ = fadeLeft_ = 0.f;
  act_ = kNone;
  actT_ = held_ = comboGrace_ = 0.f;
  combo_ = 0;
  queued_ = charged_ = false;
  trail_.clear();
  shards_.clear();
  turn_.reset(yaw);
  lagSpeed_.reset(0.f);
  lagYaw_.reset(yaw);
  lean_.reset(0.f);
  hoodLag_.reset(0.f);
}

float Hero::rand01() {
  seed_ = seed_ * 1664525u + 1013904223u;
  return static_cast<float>(seed_ >> 8) / 16777216.f;
}

void Hero::startSlash(int which, glm::vec2 dir, float tilt) {
  if (act_ != kNone) {
    from_ = last_;  // cut short: ease out of where it was
    fadeLeft_ = kCut;
  }
  act_ = kSlash;
  combo_ = which;
  actT_ = 0.f;
  queued_ = false;
  // Each cut snaps to the stick, as the Drifter's does.
  if (tilt > 0.2f) {
    yaw_ = turn_.value() + std::remainder(std::atan2(dir.x, dir.y) - turn_.value(), 2.f * kPi);
    turn_.reset(yaw_);
  }
}

void Hero::endAct(bool cut) {
  if (cut) {
    from_ = last_;
    fadeLeft_ = kCut;
  }
  if (act_ == kSlash) comboGrace_ = combo_ < 2 ? kComboGrace : 0.f;
  act_ = kNone;
  charged_ = false;
  spin_ = 0.f;
}

void Hero::update(const pxe::Scene& scene, float dt, const HeroInput& input) {
  if (!built_ || dt <= 0.f) return;
  clock_ += dt;
  dt_ = dt;
  const float tilt = std::min(glm::length(input.move), 1.f);
  const glm::vec2 dir = tilt > 1e-4f ? input.move / glm::length(input.move) : glm::vec2{0.f};
  held_ = input.attackHeld ? held_ + dt : 0.f;
  comboGrace_ = std::max(0.f, comboGrace_ - dt);

  // The sword. A press cuts at once, or (from just into a cut) queues the
  // next, which starts as soon as the sweep is through. Pushing the stick
  // once a cut's blade is out cancels its recovery; a press soon after still
  // carries the combo on. The button held on past a cut starts a charge; let
  // go once charged and it spins; let go sooner and the charge just ends.
  switch (act_) {
    case kNone:
      if (input.attack && slashClips_[0] >= 0) startSlash(comboGrace_ > 0.f ? (combo_ + 1) % 3 : 0, dir, tilt);
      break;
    case kSlash: {
      const Cut& c = kCuts[combo_];
      actT_ += dt;
      if (input.attack && actT_ >= kBufferFrom && combo_ < 2) queued_ = true;
      if (queued_ && actT_ >= c.sweep + kChainAfter) {
        startSlash(combo_ + 1, dir, tilt);
      } else if (input.attackHeld && held_ >= kChargeAfter && actT_ >= c.held && chargeClip_ >= 0) {
        from_ = last_;
        fadeLeft_ = kCut * 2.f;
        act_ = kCharge;
        actT_ = 0.f;
        charged_ = false;
      } else if (actT_ >= c.length) {
        endAct(false);
      } else if (actT_ >= c.held && tilt > 0.3f) {
        endAct(true);
      }
      break;
    }
    case kCharge:
      actT_ += dt;
      if (!charged_ && actT_ >= kChargeTime) {
        charged_ = true;
        // Ready: a flash of sparks off the blade.
        for (int i = 0; i < 14; ++i) spark(glm::vec3{0.f}, 0.35f * size_, 0.05f * size_, 0.25f, true);
      }
      if (!input.attackHeld) {
        if (charged_ && spinClip_ >= 0) {
          from_ = last_;
          fadeLeft_ = kCut;
          act_ = kSpin;
          actT_ = 0.f;
          spin_ = 0.f;
          if (tilt > 0.2f) {
            yaw_ = turn_.value() + std::remainder(std::atan2(dir.x, dir.y) - turn_.value(), 2.f * kPi);
            turn_.reset(yaw_);
          }
        } else {
          endAct(true);
        }
      }
      break;
    case kSpin:
      actT_ += dt;
      // One full turn, fast then slowing, to the figure's left.
      spin_ = 2.f * kPi * (1.f - std::pow(1.f - std::min(actT_ / kSpinTurn, 1.f), 2.f));
      if (actT_ >= model_.clipDuration(spinClip_)) endAct(false);
      break;
  }

  const float walkSpeed = kWalkCycle * size_ / std::max(model_.clipDuration(walkClip_), 1e-3f);
  const float runSpeed = kRunCycle * size_ / std::max(model_.clipDuration(runClip_), 1e-3f);
  // Speed eases toward what the stick asks for, so it sets off and pulls up
  // over a few frames rather than jumping. Through a cut or a spin the stick
  // is ignored and it lunges, fast then easing off; charging, it creeps.
  const bool locked = (act_ == kSlash && actT_ < kCuts[combo_].held) || act_ == kSpin;
  glm::vec2 target = dir * tilt * (input.walk ? walkSpeed : runSpeed);
  if (locked) target = glm::vec2{0.f};
  if (act_ == kCharge) target = dir * tilt * walkSpeed * kChargePace;
  const float rate = glm::length(target) > glm::length(vel_) ? kAccel : kStop;
  vel_ += (target - vel_) * (1.f - std::exp(-dt * (locked ? 30.f : rate)));
  const glm::vec2 facing{std::sin(yaw_), std::cos(yaw_)};
  auto lunge = [&](float from, float distance, float time) {
    // Speed falling off as (1 - u)^2 covers distance over time.
    if (actT_ < from || actT_ >= from + time) return;
    const float u = (actT_ - from) / time;
    vel_ = facing * (3.f * distance * size_ / time) * (1.f - u) * (1.f - u);
  };
  if (act_ == kSlash) lunge(kCuts[combo_].wind - 1.f / 30, kCuts[combo_].lunge, kCuts[combo_].lungeTime);
  if (act_ == kSpin) lunge(0.f, kSpinLunge, kSpinTurn);

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

  // Facing turns smoothly toward the way it moves - or, charging, toward
  // the stick, to aim the spin. Cuts hold the facing they snapped to.
  const float prevYaw = yaw_;
  if (act_ == kCharge && tilt > 0.2f) {
    const float want = turn_.value() + std::remainder(std::atan2(dir.x, dir.y) - turn_.value(), 2.f * kPi);
    yaw_ = turn_.update(dt, want);
  } else if (glm::length(vel_) > 0.05f * size_ && act_ == kNone) {
    const float want = turn_.value() + std::remainder(std::atan2(vel_.x, vel_.y) - turn_.value(), 2.f * kPi);
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
  lagYaw_.update(dt, yaw_ + spin_);
  const float accel = (speed - lastSpeed_) / dt;
  lastSpeed_ = speed;
  lean_.update(dt, act_ == kNone ? std::clamp(accel * 0.03f / size_, -0.25f, 0.3f) : 0.f);
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

  // The sword move over it all: the whole body through a cut, a charge or a
  // spin; once a cut's blade has been held out, the legs give way to the stride.
  if (act_ != kNone) {
    int clip = -1;
    float w = 1.f, legs = 1.f;
    bool loop = false;
    if (act_ == kSlash) {
      const Cut& c = kCuts[combo_];
      clip = slashClips_[combo_];
      w = smooth01(actT_ / kFadeIn) * smooth01((c.length - actT_) / kFadeOut);
      legs = actT_ < c.held ? w : w * (1.f - 0.9f * moving);
    } else if (act_ == kCharge) {
      clip = chargeClip_;
      loop = true;
      legs = 1.f - 0.6f * moving;
    } else {
      clip = spinClip_;
      w = smooth01((model_.clipDuration(spinClip_) - actT_) / kFadeOut);
      legs = w;
    }
    for (std::size_t j = 0; j < mask_.size(); ++j) mask_[j] = lower_[j] >= 1.f ? legs : lower_[j] > 0.f ? 0.5f * w : w;
    model_.sample(clip, actT_, actPose_, loop);
    pxe::SkinnedModel::blend(pose_, actPose_, mask_, pose_);
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
  // stops, and lags behind a turn (a spin flings it right out); the hood's
  // point does the same.
  const float surge = std::clamp((speed - lagSpeed_.value()) / size_ * 0.12f, -0.3f, 0.5f);
  const float swing = std::clamp(std::remainder(yaw_ + spin_ - lagYaw_.value(), 2.f * kPi), -0.6f, 0.6f);
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

  const glm::mat4 world = glm::scale(glm::rotate(glm::translate(glm::mat4{1.f}, glm::vec3{pos_.x, y_, pos_.y}),
                                                 yaw_ + spin_, glm::vec3{0.f, 1.f, 0.f}),
                                     glm::vec3{size_});
  model_.skin(pose_, world, verts_);
  mesh_.update(verts_, model_.indices());
  actors.push_back(&mesh_);
  const auto& mats = model_.materials();
  for (int i = 0; i < kCoreSlot && i < static_cast<int>(mats.size()); ++i) materials[i] = mats[i];
  // The smear's white-hot core, in the last of the hero's slots.
  materials[kCoreSlot] = {glm::vec3{0.93f, 1.f, 1.f}, true};
  this->effects(effects);
}

void Hero::spark(glm::vec3 at, float spread, float size, float life, bool white) {
  if (static_cast<int>(shards_.size()) >= kShards) return;
  // A spark off the blade (at is an offset from somewhere along it, or zero).
  glm::vec3 base = model_.jointPoint(sword_, glm::mix(bladeBase_, bladeTip_, 0.25f + 0.75f * rand01()));
  const glm::vec3 r{rand01() - 0.5f, rand01() * 0.8f, rand01() - 0.5f};
  shards_.push_back({base + at, r * spread * 6.f, size * (0.6f + 0.8f * rand01()), 0.f, life * (0.6f + 0.8f * rand01()), white});
}

void Hero::effects(std::vector<const pxe::Mesh*>& out) {
  if (sword_ < 0 || glow_ < 0) return;
  const float dt = dt_;
  const float coreSurface = static_cast<float>(pxe::kActorSurface + kCoreSlot);
  const float cyanSurface = static_cast<float>(pxe::kActorSurface + glow_);

  // Where the blade is through a sweep (a cut from just before its wind-up
  // to the blade held out; a spin through its turn), and old moments aging.
  // Each moment that ages out of the smear shatters: fragments along the
  // outer half of the blade, drifting a little and hanging in the air.
  for (Sample& s : trail_) s.age += dt;
  for (const Sample& s : trail_) {
    if (s.age <= kSmearAge || s.age - dt > kSmearAge) continue;
    for (int k = 0; k < 2; ++k) {
      if (static_cast<int>(shards_.size()) >= kShards) break;
      const float f = 0.45f + 0.55f * rand01();
      const glm::vec3 p = glm::mix(s.base, s.tip, f);
      const glm::vec3 drift = glm::normalize(s.tip - s.grip) * (0.15f + 0.3f * rand01()) + glm::vec3{0.f, 0.08f, 0.f};
      shards_.push_back({p, drift * size_ * 0.5f, (0.035f + 0.045f * rand01()) * size_, 0.f, 0.22f + 0.2f * rand01(), false});
    }
  }
  trail_.erase(std::remove_if(trail_.begin(), trail_.end(), [](const Sample& s) { return s.age > kSmearAge; }),
               trail_.end());
  bool sweeping = false;
  if (act_ == kSlash) {
    const Cut& c = kCuts[combo_];
    sweeping = actT_ >= c.wind - 1.f / 30 && actT_ <= c.held;
  } else if (act_ == kSpin) {
    sweeping = actT_ <= kSpinTurn + 0.04f;
  }
  if (sweeping)
    trail_.push_back({model_.jointPoint(sword_, grip_), model_.jointPoint(sword_, bladeBase_),
                      model_.jointPoint(sword_, bladeTip_), 0.f});
  // Charging: sparks run along the blade, quicker once it is ready.
  if (act_ == kCharge) {
    sparkClock_ += dt;
    const float every = charged_ ? 0.025f : 0.06f;
    while (sparkClock_ > every) {
      sparkClock_ -= every;
      spark(glm::vec3{0.f}, 0.05f * size_, 0.03f * size_, 0.18f, charged_ && rand01() < 0.5f);
    }
  }

  // The smear: a fan between the blade's places, each gap filled with
  // in-between blades swung round the grip (t3ssel8r's smear), narrowing to
  // the tip as it ages, so it trails off in a crescent - white-hot where the
  // blade has just been, cyan behind.
  if (trail_.size() >= 2) {
    std::vector<glm::vec3> inner, outer;
    std::vector<float> ages;
    for (std::size_t i = 0; i + 1 < trail_.size(); ++i) {
      const Sample& a = trail_[i];
      const Sample& b = trail_[i + 1];
      const int steps = 5;
      for (int k = 0; k < steps || (i + 2 == trail_.size() && k == steps); ++k) {
        const float f = static_cast<float>(k) / steps;
        const glm::vec3 grip = glm::mix(a.grip, b.grip, f);
        const glm::vec3 da = a.tip - a.grip, db = b.tip - b.grip;
        const float len = glm::mix(glm::length(da), glm::length(db), f);
        glm::vec3 dir = glm::mix(glm::normalize(da), glm::normalize(db), f);
        dir = glm::length(dir) > 1e-4f ? glm::normalize(dir) : glm::normalize(db);
        const float baseLen = glm::mix(glm::length(a.base - a.grip), glm::length(b.base - b.grip), f);
        const float age = glm::mix(a.age, b.age, f) / kSmearAge;  // 0 now .. 1 gone
        inner.push_back(grip + dir * glm::mix(baseLen, len, 0.15f + 0.8f * age));
        outer.push_back(grip + dir * len * (1.04f - 0.06f * age));
        ages.push_back(age);
      }
    }
    const int n = std::min(static_cast<int>(inner.size()), kSmearPoints);
    const int skip = static_cast<int>(inner.size()) - n;
    for (int i = 0; i < kSmearPoints; ++i) {
      const auto k = static_cast<std::size_t>(skip + std::min(i, n - 1));  // unused points fold onto the newest
      const float surface = ages[k] < 0.4f ? coreSurface : cyanSurface;
      const glm::vec3 p = inner[k], q = outer[k];
      trailVerts_[static_cast<std::size_t>(2 * i)] = {p.x, p.y, p.z, 0.f, 1.f, 0.f, surface};
      trailVerts_[static_cast<std::size_t>(2 * i + 1)] = {q.x, q.y, q.z, 0.f, 1.f, 0.f, surface};
    }
    trailMesh_.update(trailVerts_, trailIndices_);
    out.push_back(&trailMesh_);
  }

  // Fragments: each a small flat diamond crossed with an upright one,
  // drifting and slowing, shrinking away; white sparks cool to cyan.
  for (Shard& s : shards_) {
    s.age += dt;
    s.pos += s.vel * dt;
    s.vel *= std::exp(-dt * 6.f);
  }
  shards_.erase(std::remove_if(shards_.begin(), shards_.end(), [](const Shard& s) { return s.age > s.life; }),
                shards_.end());
  if (!shards_.empty()) {
    for (int i = 0; i < kShards; ++i) {
      pxe::Vertex* v = &shardVerts_[static_cast<std::size_t>(8 * i)];
      if (i >= static_cast<int>(shards_.size())) {
        for (int k = 0; k < 8; ++k) v[k] = pxe::Vertex{};
        continue;
      }
      const Shard& s = shards_[static_cast<std::size_t>(i)];
      const float left = 1.f - s.age / s.life;
      const float r = s.size * (0.3f + 0.7f * left);
      const float surface = s.white && s.age < 0.5f * s.life ? coreSurface : cyanSurface;
      const glm::vec3 c = s.pos;
      const glm::vec3 flat[4] = {c + glm::vec3{r, 0, 0}, c + glm::vec3{0, 0, r}, c + glm::vec3{-r, 0, 0}, c + glm::vec3{0, 0, -r}};
      const glm::vec3 up[4] = {c + glm::vec3{r, 0, 0}, c + glm::vec3{0, r, 0}, c + glm::vec3{-r, 0, 0}, c + glm::vec3{0, -r, 0}};
      for (int k = 0; k < 4; ++k) {
        v[k] = {flat[k].x, flat[k].y, flat[k].z, 0.f, 1.f, 0.f, surface};
        v[4 + k] = {up[k].x, up[k].y, up[k].z, 0.f, 1.f, 0.f, surface};
      }
    }
    shardMesh_.update(shardVerts_, shardIndices_);
    out.push_back(&shardMesh_);
  }
}

}  // namespace game
