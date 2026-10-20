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
  model_.restPose(pose_);
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
  phase_ = clock_ = runBlend_ = lastSpeed_ = fall_ = 0.f;
  turn_.reset(yaw);
  lagSpeed_.reset(0.f);
  lagYaw_.reset(yaw);
  lean_.reset(0.f);
  hoodLag_.reset(0.f);
}

void Hero::update(const pxe::Scene& scene, float dt, const HeroInput& input) {
  if (!built_ || dt <= 0.f) return;
  clock_ += dt;
  const float walkSpeed = kWalkCycle * size_ / std::max(model_.clipDuration(walkClip_), 1e-3f);
  const float runSpeed = kRunCycle * size_ / std::max(model_.clipDuration(runClip_), 1e-3f);

  // Speed eases toward what the stick asks for, so it sets off and pulls up
  // over a few frames rather than jumping.
  const float tilt = std::min(glm::length(input.move), 1.f);
  const glm::vec2 dir = tilt > 1e-4f ? input.move / glm::length(input.move) : glm::vec2{0.f};
  const glm::vec2 target = dir * tilt * (input.walk ? walkSpeed : runSpeed);
  const float rate = glm::length(target) > glm::length(vel_) ? kAccel : kStop;
  vel_ += (target - vel_) * (1.f - std::exp(-dt * rate));

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
  if (glm::length(vel_) > 0.05f * size_) {
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

void Hero::pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) {
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
  const auto& mats = model_.materials();
  for (int i = 0; i < pxe::kActorMaterials && i < static_cast<int>(mats.size()); ++i) materials[i] = mats[i];
}

}  // namespace game
