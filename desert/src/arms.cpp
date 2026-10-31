#include "arms.hpp"

#include "player.hpp"

#include <glm/vec4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace desert {
namespace {

// The glb's materials, as offsets from Arms::kFirstMaterial.
constexpr int kWrap = 0;
constexpr int kGlove = 1;
[[maybe_unused]] constexpr int kStrap = 2;
constexpr int kGunmetal = 3;
[[maybe_unused]] constexpr int kGlow = 4;
[[maybe_unused]] constexpr int kSteel = 5;
[[maybe_unused]] constexpr int kCopper = 6;
[[maybe_unused]] constexpr int kRubber = 7;
[[maybe_unused]] constexpr int kThread = 8;
constexpr int kWorn = 9;
constexpr int kGrime = 10;
[[maybe_unused]] constexpr int kDial = 11;
[[maybe_unused]] constexpr int kRust = 12;
[[maybe_unused]] constexpr int kUndersuit = 13;
[[maybe_unused]] constexpr int kScreen = 14;

// Metres walked per cycle of each clip (two steps), and how fast the walk's
// weights follow the speed: a time constant, seconds.
constexpr float kWalkCycle = 2.0f;
constexpr float kSprintCycle = 3.0f;
constexpr float kEase = 0.15f;
// Below this the walk's weight is taken as none, and the arms are drawn in
// the rest pose itself.
constexpr float kAtRest = 1e-3f;

}  // namespace

void Arms::load() {
  model_ = pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/characters/arms.glb", kFirstMaterial);
  model_.restPose(rest_);
  walkClip_ = model_.clip("Arms_Walk");
  sprintClip_ = model_.clip("Arms_Sprint");
  if (walkClip_ < 0 || sprintClip_ < 0) throw std::runtime_error("arms.glb lacks the Arms_Walk or Arms_Sprint clip");
}

void Arms::walk(float moved, float dt, float walkSpeed, float sprintSpeed) {
  if (dt <= 0.f) return;
  const float speed = moved / dt;
  const float k = 1.f - std::exp(-dt / kEase);
  const float moving = std::clamp(speed / walkSpeed, 0.f, 1.f);
  const float sprinting = std::clamp((speed - walkSpeed) / (sprintSpeed - walkSpeed), 0.f, 1.f);
  moving_ += (moving - moving_) * k;
  sprinting_ += (sprinting - sprinting_) * k;
  if (moving == 0.f && moving_ < kAtRest) moving_ = 0.f;
  const float cycle = kWalkCycle + (kSprintCycle - kWalkCycle) * sprinting_;
  phase_ = std::fmod(phase_ + moved / cycle, 1.f);
}

void Arms::hold(bool sprint, float phase) {
  held_ = true;
  heldSprint_ = sprint;
  heldPhase_ = std::clamp(phase, 0.f, 1.f);
}

void Arms::release() { held_ = false; }

void Arms::pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params) {
  // The model faces +Z with +X on its left and +Y up, the way every
  // character faces at rest, so its axes map onto the view's: +Z forward,
  // +X the view's left, +Y up. The origin is the eye.
  glm::mat4 eye{1.f};
  eye[0] = glm::vec4(-view.right, 0.f);
  eye[1] = glm::vec4(view.up, 0.f);
  eye[2] = glm::vec4(view.forward, 0.f);
  eye[3] = glm::vec4(view.eye, 1.f);
  if (held_) {
    const int clip = heldSprint_ ? sprintClip_ : walkClip_;
    model_.sample(clip, heldPhase_ * model_.clipDuration(clip), pose_);
    model_.skin(pose_, eye, joints_);
  } else if (moving_ == 0.f) {
    model_.skin(rest_, eye, joints_);
  } else {
    // The clips at the stride's phase, the sprint blended over the walk by
    // its weight, and that over the rest pose by the walk's.
    model_.sample(walkClip_, phase_ * model_.clipDuration(walkClip_), walkPose_);
    model_.sample(sprintClip_, phase_ * model_.clipDuration(sprintClip_), sprintPose_);
    pxe::SkinnedModel::blend(walkPose_, sprintPose_, sprinting_, pose_);
    pxe::SkinnedModel::blend(rest_, pose_, moving_, pose_);
    model_.skin(pose_, eye, joints_);
  }

  params.viewModel.push_back({&model_.mesh(), joints_});
  const auto& mats = model_.materials();
  for (int i = 0; i < static_cast<int>(mats.size()) && kFirstMaterial + i < pxe::kActorMaterials; ++i) {
    params.actorMaterials[kFirstMaterial + i] = mats[static_cast<std::size_t>(i)];
    params.actorMaterials[kFirstMaterial + i].crease = i == kWrap || i == kGlove || i == kGunmetal || i == kWorn || i == kGrime;
  }
}

}  // namespace desert
