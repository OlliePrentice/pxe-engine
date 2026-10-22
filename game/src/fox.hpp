#pragma once

#include "pxe/mesh.hpp"
#include "pxe/scene.hpp"
#include "pxe/skinned.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <random>
#include <vector>

namespace game {

// A red fox curled up asleep, built, rigged and animated in Blender
// (tools/blender/build_fox.py) and loaded from assets/characters/fox.glb.
//
// It lies where it is put and breathes through its Fox_Rest loop. Every
// 20-40 s, on a timer, it wakes: it stands up (Fox_StandUp, turning a
// quarter-turn left on the spot so it stands side-on to the way the curl
// faces, the fire on its right), looks around (Fox_LookAround), lies back
// down into the curl (Fox_LieDown) and rests again, forever. The turns are in
// the clips.
//
// Now and then, after the first look around, it takes a short walk: out to
// a random point 2-5 m from its bed (Fox_Walk, played in place while it is
// moved along the ground), a look around there, and back into its bed in a
// teardrop loop, arriving from behind already facing the way it stood, so it
// lies down just as on a wake without a walk; it never turns on the spot.
// Each wake has an even chance of wanting a walk, but a walk must leave and
// come home facing the same way within 2 x the target's distance + 4 m and
// keep to clear ground, so few targets can be reached (from its bed, only
// ahead and to its left): about one wake in five walks. The whole walk is
// checked before it sets off, with the same route it then walks. It has no
// AI: no awareness of anything, no input, and nothing touches it.
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
  // any of its five clips.
  void build();
  bool built() const { return built_; }
  // Lie at xz on the ground, the curl's face toward `yaw` (radians; 0 faces
  // +z), tilted to lie along the slope there, at the start of a rest. The
  // rests and walks are drawn from fixed seeds, so every run wakes and walks
  // alike.
  void spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw);
  // The campfire it sleeps by: the middle (xz) and how far its dirt patch
  // reaches. Its walks keep clear of it. Without one it never walks.
  void setFire(glm::vec2 centre, float reach);
  // On through the rest loop and, when a rest is over, the wake sequence
  // (with any walk, which follows the ground in `scene`).
  void update(const pxe::Scene& scene, float dt);
  // Captures: hold it asleep `seconds` into the rest loop, never waking.
  void holdRest(float seconds);
  // Start the wake sequence now (no crossfade) and run it on `seconds`:
  // 0 is the first frame of Fox_StandUp. Past its end (StandUp, LookAround
  // and LieDown, 7.5 s in all, or longer with a walk) it rests again.
  void wake(const pxe::Scene& scene, float seconds);
  // The next wake takes a walk, if any route is clear (--fox-walk).
  void forceWalk() { forceWalk_ = true; }
  // Skin this frame's pose and hand the figure to the frame (actors), its
  // colours into its own slots of `materials`.
  void pose(std::vector<const pxe::Mesh*>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]);

 private:
  pxe::SkinnedModel model_;
  pxe::Mesh mesh_;
  pxe::SkinnedModel::Pose pose_;
  std::vector<pxe::Vertex> verts_;
  bool built_ = false;

  // Where it is in its cycle; each state plays its clip of clip_[state]. A
  // walking wake goes LookAround, WalkOut, LookOut (Fox_LookAround at the
  // walk's target), WalkBack, then LieDown.
  enum State { kRest, kStandUp, kLookAround, kLieDown, kWalkOut, kLookOut, kWalkBack, kStateCount };
  int clip_[kStateCount] = {-1, -1, -1, -1, -1, -1, -1};
  // Go into `next` at its clip's start, crossfading from the pose the clip
  // before it ended on (when `fade`).
  void enter(State next, bool fade);
  // Set off on a leg of the walk (kWalkOut or kWalkBack) from standing.
  void startLeg(State leg);
  // A fresh rest: how long, drawn from rng_.
  void beginRest();
  // How long the current state lasts: its clip, or a leg of the walk.
  float stateLength() const;
  // Where it is and how it is turned and tilted this moment (pos_, y_,
  // yaw_, tilt_): in its bed, or along the walk.
  void place(const pxe::Scene& scene);

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
  // Its bed: where spawn() put it, exactly; it always comes back to this.
  glm::vec2 bedPos_{0.f};
  float bedY_ = 0.f;
  float bedYaw_ = 0.f;
  glm::quat bedTilt_{1.f, 0.f, 0.f, 0.f};
  glm::vec3 bedNormal_{0.f, 1.f, 0.f};

  // --- The walk ---------------------------------------------------------
  // Fox_Walk's ground covered per gait cycle at real size (assets.md: 0.405
  // m), which with the clip's length and kSize sets the walking speed, so
  // the planted paws stay put.
  static constexpr float kStride = 0.405f;
  // Speeding up from standing, and slowing to a stop, each take kRamp; the
  // pose fades from standing into the walk over the first kWalkFade of it,
  // and back over the last, generously: no walking pose is near standing
  // (the closest is 43 degrees and 0.3 m off), and while it fades it is
  // still slow.
  static constexpr float kRamp = 1.6f;
  static constexpr float kWalkFade = 1.f;
  // Targets: uniformly spread over the ring this far from the bed (m), the
  // route never more than kMaxReach from it nor longer than twice the
  // target's distance plus kSpare, and this many targets tried before it
  // gives up on a walk (kForcedTries for --fox-walk).
  static constexpr float kMinReach = 2.f;
  static constexpr float kMaxReach = 5.f;
  static constexpr float kSpare = 4.f;
  static constexpr int kTries = 8;
  static constexpr int kForcedTries = 256;
  // How tightly it may turn (radius, m): a walk that leaves and comes home
  // facing the same way turns a whole turn, which with kSpare to spare
  // allows little more. Turning sets in and eases off over kEase (m), so
  // the yaw rate never jumps. And the clearance it keeps from the fire's
  // dirt patch.
  static constexpr float kMinTurn = 0.9f;
  static constexpr float kEase = 0.6f;
  static constexpr float kFireClearance = 0.2f;
  // The whole fox walking and looking around, at kSize, in the model's
  // ground plane: x along its heading, z to its right (assets.md: the union
  // of Fox_Walk and Fox_LookAround, the head swung out both sides).
  static constexpr float kBodyBack = -1.359f, kBodyFront = 1.423f;
  static constexpr float kBodyLeft = -0.314f, kBodyRight = 0.573f;
  // The route is checked (footprint) and its lean worked out every this
  // far along it (m), and laid out every kRouteStep.
  static constexpr float kCheckStep = 0.1f;
  static constexpr float kRouteStep = 0.01f;

  // A walk, the path of pivot_: a teardrop loop from the bed through the
  // target and back. Out, it goes straight on along the standing heading
  // and curves onto the target; back, it carries on round, heads home past
  // the bed's side and curves in behind it, arriving along the standing
  // heading. Laid out as heading and place every kRouteStep along it.
  struct Route {
    std::vector<float> length;      // metres along it at each step
    std::vector<glm::vec2> point;   // where the pivot is
    std::vector<float> heading;     // which way it heads (radians in the ground's x-z plane, unwrapped)
    float out = 0.f, total = 0.f;   // length to the target, and there and back
    std::vector<glm::vec3> normal;  // ground normal for the lean every kCheckStep, smoothed
    float normalStep = 0.f;
    float yawOffset = 0.f;          // so its heading at the start is bedYaw_ exactly
    float phaseOut = 0.f, phaseBack = 0.f;  // Fox_Walk's phase at the start of each leg
    float timeOut = 0.f, timeBack = 0.f;    // each leg's length in seconds
  };
  // One stretch of a leg as first laid out: this long, turning this much
  // per metre (radians, + toward increasing angle in the x-z plane).
  struct Piece {
    float length, curvature;
  };
  // Decide at the start of a wake whether it walks, and if so where.
  void planWalk(const pxe::Scene& scene);
  // Lay out the loop through `target`; false if none fits (no turn tighter
  // than kMinTurn, no longer than 2 x the target's distance + kSpare).
  bool layRoute(glm::vec2 target, Route& route) const;
  // Lay one leg from (from, heading) to (to, toHeading) along `pieces`, its
  // turning eased and then corrected to arrive exactly; appended to `route`.
  // False if it cannot be made to arrive, or turns tighter than kMinTurn.
  bool layLeg(const std::vector<Piece>& pieces, glm::vec2 from, float heading, glm::vec2 to, float toHeading,
              Route& route) const;
  // Whether every sample of the walk is on walkable ground, clear of the
  // fire and within kMaxReach of the bed; fills in the lean.
  bool routeClear(const pxe::Scene& scene, Route& route) const;
  // A point `s` metres along the walk: where the model's origin is, and its
  // yaw there.
  void routeAt(const Route& route, float s, glm::vec2& pos, float& yaw) const;
  glm::quat routeTilt(const Route& route, float s) const;
  // Metres covered `t` seconds into a leg `length` long taking `time`; how
  // long a leg takes; and the Fox_Walk phase a leg should start on.
  float legDistance(float t, float length, float time) const;
  float legTime(float length) const;
  float startPhase(float length) const;
  float walkSpeed() const;

  glm::vec2 fire_{0.f};
  float fireReach_ = -1.f;  // < 0: no fire, no walks
  float standTurn_ = 0.f;   // the standing heading, from the curl's yaw (radians)
  glm::vec2 pivot_{0.f};    // what the walk steers: model x (heading) and z (right) at kSize
  bool forceWalk_ = false;
  bool walking_ = false;    // this wake walks, along route_
  Route route_;
  pxe::SkinnedModel::Pose fadeTo_, walkPose_;
  // How far each phase of Fox_Walk is from standing (sum of joint turns).
  std::vector<float> walkGap_;
  // The walks come from a stream of their own, so the rests are drawn
  // exactly as before. With this seed the first wake stays put (so it wakes
  // exactly as before this walk was added) and the second walks; after that
  // about one wake in five does.
  static constexpr std::uint32_t kWalkSeed = 20261022u;
  std::mt19937 walkRng_{kWalkSeed};
};

}  // namespace game
