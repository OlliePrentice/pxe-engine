#include "fox.hpp"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace game {

void Fox::build() {
  const std::string path = std::string(PXE_ASSET_DIR) + "/characters/fox.glb";
  model_ = pxe::SkinnedModel::load(path, kFirstMaterial);
  // The curl and the waking are all in the clips: the rest pose alone is
  // the fox standing in its bind pose.
  const char* names[kStateCount] = {"Fox_Rest", "Fox_StandUp", "Fox_LookAround", "Fox_LieDown"};
  for (int s = 0; s < kStateCount; ++s) {
    clip_[s] = model_.clip(names[s]);
    if (clip_[s] < 0) throw std::runtime_error(std::string("no ") + names[s] + " clip in " + path);
    if (model_.clipDuration(clip_[s]) <= 0.f) throw std::runtime_error(std::string(names[s]) + " is empty in " + path);
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
  const float reach = 0.6f * kFootprint;
  const float x0 = ground.heightAt(xz - glm::vec2{reach, 0.f}), x1 = ground.heightAt(xz + glm::vec2{reach, 0.f});
  const float z0 = ground.heightAt(xz - glm::vec2{0.f, reach}), z1 = ground.heightAt(xz + glm::vec2{0.f, reach});
  if (x0 > pxe::HeightMap::kNone && x1 > pxe::HeightMap::kNone && z0 > pxe::HeightMap::kNone &&
      z1 > pxe::HeightMap::kNone) {
    const glm::vec3 normal = glm::normalize(glm::vec3{(x0 - x1) / (2.f * reach), 1.f, (z0 - z1) / (2.f * reach)});
    const glm::vec3 axis = glm::cross(glm::vec3{0.f, 1.f, 0.f}, normal);
    if (glm::length(axis) > 1e-5f) tilt_ = glm::angleAxis(std::acos(std::clamp(normal.y, -1.f, 1.f)), glm::normalize(axis));
  }
  // Asleep, at the start of the first rest, so it first wakes at least
  // kRestMin after it is put down.
  rng_.seed(kSeed);
  hold_ = false;
  enter(kRest, false);
}

void Fox::beginRest() {
  // From the raw 32-bit draw (std::mt19937's sequence is fixed by the
  // standard; the distributions' are not), so every platform rests alike.
  const float u = static_cast<float>(rng_()) / 4294967296.f;
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

void Fox::update(float dt) {
  if (!built_ || dt <= 0.f) return;
  // Through as many joins as dt spans, each at its exact moment, so one long
  // step (a capture's) ends where many short ones would.
  for (;;) {
    const float length = model_.clipDuration(clip_[state_]);
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
      if (!hold_ && rested_ >= restFor_) enter(kStandUp, true);
      continue;
    }
    if (dt < left) {
      clock_ += dt;
      fade_ = std::min(fade_ + dt, kFade);
      break;
    }
    dt -= left;
    enter(state_ == kStandUp ? kLookAround : state_ == kLookAround ? kLieDown : kRest, true);
  }
}

void Fox::holdRest(float seconds) {
  if (!built_) return;
  hold_ = true;
  enter(kRest, false);
  // As it always slept in captures: just the place in the loop.
  const float length = model_.clipDuration(clip_[kRest]);
  clock_ = length > 0.f ? std::fmod(std::max(seconds, 0.f), length) : 0.f;
}

void Fox::wake(float seconds) {
  if (!built_) return;
  hold_ = false;
  enter(kStandUp, false);
  update(seconds);
}

void Fox::pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) {
  if (!built_) return;
  // The rest loops; the waking clips play once each.
  model_.sample(clip_[state_], clock_, pose_, state_ == kRest);
  if (fade_ < kFade) pxe::SkinnedModel::blend(fadeFrom_, pose_, fade_ / kFade, pose_);
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
