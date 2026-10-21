#pragma once

#include "pxe/mesh.hpp"
#include "pxe/scene.hpp"
#include "pxe/skinned.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace game {

// A red fox curled up asleep, built, rigged and animated in Blender
// (tools/blender/build_fox.py) and loaded from assets/characters/fox.glb.
//
// Set dressing for now: it lies where it is put and breathes through its
// Fox_Rest loop. Every 20-40 s, on a timer, it wakes: it stands up
// (Fox_StandUp, turning a quarter-turn left on the spot so it stands side-on
// to the way the curl faces), looks around (Fox_LookAround), lies back down
// into the curl (Fox_LieDown) and rests again, forever. The turns are in the
// clips. It has no AI, never leaves its spot, takes no input, and nothing
// touches it. Its rig is a full quadruped's (spine, neck, head, ears, four
// legs, a tail chain), ready for walk and run clips later.
//
// Its colours take the actor-material slots from kFirstMaterial up, clear of
// the hero's 0-7, so the two characters never share a colour.
class Fox {
 public:
  static constexpr int kFirstMaterial = 8;
  // Drawn at 2.4 times a real fox's size (1.5 times the hero's 1.6), so it
  // reads at pixel scale; the model itself is real size.
  static constexpr float kSize = 2.4f;
  // At kSize the curl spans x +-0.523 m and z +-0.427 m round its origin
  // (assets.md); a circle this wide covers it all, for clearing the grass
  // under it and keeping it clear of the fire.
  static constexpr float kFootprint = 0.68f;

  // The rest before each wake is drawn from this range (seconds), then
  // rounded up to the next time the Fox_Rest loop wraps.
  static constexpr float kRestMin = 20.f;
  static constexpr float kRestMax = 40.f;
  // Each join between clips is crossfaded over this long, a safety net: the
  // clips already meet pose for pose.
  static constexpr float kFade = 0.2f;

  // Load the model (needs the GL context); throws if it is missing or lacks
  // any of its four clips.
  void build();
  bool built() const { return built_; }
  // Lie at xz on the ground, the curl's face toward `yaw` (radians; 0 faces
  // +z), tilted to lie along the slope there, at the start of a rest. The
  // rests are drawn from a fixed seed, so every run wakes at the same times.
  void spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw);
  // On through the rest loop and, when a rest is over, the wake sequence.
  void update(float dt);
  // Captures: hold it asleep `seconds` into the rest loop, never waking.
  void holdRest(float seconds);
  // Start the wake sequence now (no crossfade) and run it on `seconds`:
  // 0 is the first frame of Fox_StandUp. Past its end (StandUp, LookAround
  // and LieDown, 7.5 s in all) it rests again as usual.
  void wake(float seconds);
  // Skin this frame's pose and hand the figure to the frame (actors), its
  // colours into its own slots of `materials`.
  void pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]);

 private:
  pxe::SkinnedModel model_;
  pxe::Mesh mesh_;
  pxe::SkinnedModel::Pose pose_;
  std::vector<pxe::Vertex> verts_;
  bool built_ = false;

  // Where it is in its cycle; each state plays its clip of clip_[state].
  enum State { kRest, kStandUp, kLookAround, kLieDown, kStateCount };
  int clip_[kStateCount] = {-1, -1, -1, -1};
  // Go into `next` at its clip's start, crossfading from the pose the clip
  // before it ended on (when `fade`).
  void enter(State next, bool fade);
  // A fresh rest: how long, drawn from rng_.
  void beginRest();

  State state_ = kRest;
  float clock_ = 0.f;      // seconds into the current state's clip
  float rested_ = 0.f;     // seconds of the current rest so far
  float restFor_ = 0.f;    // ...and how long it lasts before it may wake
  bool hold_ = false;      // holdRest: asleep for good
  // The crossfade at the last join: the pose it started from, and how far
  // into it the new clip is (kFade or more: done).
  pxe::SkinnedModel::Pose fadeFrom_;
  float fade_ = kFade;
  // Its rests come from this seed, so captures and runs are repeatable.
  static constexpr std::uint32_t kSeed = 20261021u;
  std::mt19937 rng_{kSeed};

  glm::vec2 pos_{0.f};
  float y_ = 0.f;
  float yaw_ = 0.f;
  glm::quat tilt_{1.f, 0.f, 0.f, 0.f};  // up turned to the ground's normal
};

}  // namespace game
