#pragma once

#include "hero.hpp"
#include "scene.hpp"

#include "pxe/renderer.hpp"
#include "pxe/scene.hpp"
#include "pxe/skinned.hpp"

#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <random>
#include <span>
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
// keep to clear ground, so few targets can be reached (from the first fox's
// bed, only ahead and to its left): about one wake in five walks. The whole
// walk is checked before it sets off, with the same route it then walks.
//
// Several foxes share one model (loadModel) and keep out of each other's
// way by reservation: a fox sets off only on a walk whose whole route keeps
// its body clear of every other fox's bed and of every route another fox
// has set off on and not yet lain down from, so foxes never meet and never
// have to give way mid-walk. Beyond that they have no AI: no awareness of
// anything else, the hero included, and no input. The hero is pushed out of
// their bodies (body()); they never react to it.
//
// Every fox's colours take the actor-material slots from kFirstMaterial up,
// clear of the hero's 0-7, so the hero and the foxes never share a colour.
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

  // Standing or walking, a circle this wide round the line from its hips to
  // its head covers its body, but for the brush: the walking body is 0.58 m
  // across (assets.md). The hero is kept out of it.
  static constexpr float kBodyRadius = 0.3f;

  // The model every fox is drawn from, loaded once (needs the GL context);
  // throws if it is missing.
  static pxe::SkinnedModel loadModel();
  // Take up `model` (loadModel's, which must outlive it) as fox number
  // `index`, which picks its seeds; throws if the model lacks any of its
  // five clips.
  void build(const pxe::SkinnedModel& model, int index);
  bool built() const { return model_ != nullptr; }
  // Lie at xz on the ground, the curl's face toward `yaw` (radians; 0 faces
  // +z), tilted to lie along the slope there, at the start of a rest. The
  // rests and walks are drawn from fixed seeds, kSeed and kWalkSeed plus its
  // index, so every run wakes and walks alike.
  void spawn(const pxe::Scene& scene, glm::vec2 xz, float yaw);
  // The scene's campfires: their middles (xz), and how far their dirt
  // patches reach. Its walks keep clear of them all.
  void setFires(const std::vector<glm::vec2>& centres, float reach);
  // On through the rest loop and, when a rest is over, the wake sequence
  // (with any walk, which follows the ground in `scene` and keeps clear of
  // `foxes` - all the foxes, this one among them).
  void update(const pxe::Scene& scene, float dt, std::span<const Fox> foxes);
  // Captures: hold it asleep `seconds` into the rest loop, never waking.
  void holdRest(float seconds);
  // Start the wake sequence now (no crossfade), at the first frame of
  // Fox_StandUp, deciding on any walk against `foxes`; update() runs it on
  // (StandUp, LookAround and LieDown, 7.5 s in all, or longer with a walk),
  // and then it rests again.
  void wake(const pxe::Scene& scene, std::span<const Fox> foxes);
  // The next wake takes a walk, if any route is clear (--fox-walk).
  void forceWalk() { forceWalk_ = true; }
  // How long until it may next decide on a walk, at the soonest (update()
  // decides at each wake): for running several foxes on together in long
  // steps that each see the others as they are when they decide.
  float untilWalkDecided() const;
  // Hand this moment's figure to the frame (actors: its joint matrices must
  // outlive the frame's draw), its colours into its own slots of `materials`.
  void pose(std::vector<pxe::ActorDraw>& actors, pxe::ActorMaterial (&materials)[pxe::kActorMaterials]) const;
  // The ground its body takes up this moment, for keeping the hero out:
  // the kFootprint circle round its curl while it rests or lies down, and
  // otherwise kBodyRadius round its hips-to-head line as it stands and walks.
  Blocker body() const;

  // The fox through every frame of its five clips, at kSize in the model (y
  // up, the curl facing +z), for checking beds: every vertex of every pose,
  // sorted into the voxels it passes through. Built once per model (sweep()),
  // and shared by every bed search, findBeds' and findBed's.
  struct Sweep {
    struct Voxel {
      glm::vec3 centre;
      std::uint32_t first, count;  // its vertices in `vertices`
    };
    // The voxels in blocks, each with bounds round all its vertices, so a
    // triangle need only look at the voxels near it.
    struct Block {
      glm::vec3 lo{1e9f}, hi{-1e9f};
      std::vector<std::uint32_t> voxels;
    };
    std::vector<glm::vec3> vertices;  // by voxel
    std::vector<Voxel> voxels;
    std::vector<Block> blocks;
    std::vector<glm::vec3> fox;  // every voxel's middle
    float reach = 0.f;           // how far any vertex reaches from the origin, seen from above
    float radius = 0.f;          // and at all
  };
  // Sweep `model` (loadModel's); throws if it lacks any of the five clips.
  static Sweep sweep(const pxe::SkinnedModel& model);

  // The beds for `count` foxes in a scene laid out as `layout` (with `sweep`
  // as sweep()'s), tried over its bed area in a fixed order so every run
  // finds the same: each on open lawn (lawn under all of the curl, and no
  // part of the fox sunk into the ground), with the whole fox through every
  // frame of its five clips, at kSize and tilted to the slope, at least
  // kBedClearance from everything standing in the scene (every campfire's
  // dirt patch, stones and logs among it) and from the fox in every other
  // bed. Each curl faces the nearest campfire within kFaceFire, or else a
  // yaw drawn from the same fixed seed. Fewer than `count` if no more fit.
  static std::vector<FoxBed> findBeds(const Sweep& sweep, const SceneLayout& layout, int count);
  static constexpr float kBedClearance = 0.2f;
  static constexpr float kFaceFire = 6.f;
  // A bed for this fox (built, not yet spawned) as near `at` (world xz) as
  // one fits, among `foxes` as they are now (this fox not among them): the
  // spots of findBeds' 0.25 m grid within kAddReach of it, nearest first, under
  // findBeds' rules, clear of the fox in each of their beds, and with its
  // curl and its standing body in the bed clear of every route one of them
  // is out on. The curl faces the nearest campfire within kFaceFire, or else
  // turns so that the standing fox faces `toCamera` (xz). False if none fits.
  bool findBed(const Sweep& sweep, const SceneLayout& layout, glm::vec2 at, glm::vec2 toCamera,
               std::span<const Fox> foxes, FoxBed& bed) const;
  static constexpr float kAddReach = 2.f;
  // Where it lies, as spawn() put it.
  glm::vec2 bedPos() const { return bedPos_; }
  float bedYaw() const { return bedYaw_; }

 private:
  const pxe::SkinnedModel* model_ = nullptr;
  int index_ = 0;
  pxe::SkinnedModel::Pose pose_;
  std::vector<glm::mat4> joints_;  // pose_, placed
  int hips_ = -1, head_ = -1;      // the line body() runs along...
  glm::vec3 hipsRest_{0.f}, headRest_{0.f};  // ...between these joints' places at rest

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
  // This moment's pose, placed (pose_, joints_).
  void skin();
  // Where it lies on the ground at xz: its height there, and its tilt to the
  // ground's slope (the normal it is turned to).
  static void lie(const pxe::HeightMap& ground, glm::vec2 xz, float& y, glm::quat& tilt, glm::vec3& normal);
  // The model's place in the world, at kSize.
  static glm::mat4 world(glm::vec2 xz, float y, glm::quat tilt, float yaw);

  State state_ = kRest;
  float clock_ = 0.f;      // seconds into the current state's clip
  float rested_ = 0.f;     // seconds of the current rest so far
  float restFor_ = 0.f;    // ...and how long it lasts before it may wake
  bool hold_ = false;      // holdRest: asleep for good
  // The crossfade at the last join: the pose it started from, and how far
  // into it the new clip is (kFade or more: done).
  pxe::SkinnedModel::Pose fadeFrom_;
  float fade_ = kFade;
  // Its rests come from this seed (plus its index), so captures and runs are
  // repeatable.
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
    // Its body as checked, every kCheckStep and at the target: where the
    // model's origin is (x, z) and its yaw; what other foxes keep clear of.
    std::vector<glm::vec3> body;
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
  void planWalk(const pxe::Scene& scene, std::span<const Fox> foxes);
  // Lay out the loop through `target`; false if none fits (no turn tighter
  // than kMinTurn, no longer than 2 x the target's distance + kSpare).
  bool layRoute(glm::vec2 target, Route& route) const;
  // Lay one leg from (from, heading) to (to, toHeading) along `pieces`, its
  // turning eased and then corrected to arrive exactly; appended to `route`.
  // False if it cannot be made to arrive, or turns tighter than kMinTurn.
  bool layLeg(const std::vector<Piece>& pieces, glm::vec2 from, float heading, glm::vec2 to, float toHeading,
              Route& route) const;
  // Whether every sample of the walk is on walkable ground, clear of the
  // fires and within kMaxReach of the bed; fills in the lean and the body.
  bool routeClear(const pxe::Scene& scene, Route& route) const;
  // Whether its body along the walk keeps clear of every other fox's bed
  // (curled, and standing in it) and of every route another has set off on
  // and not yet lain down from.
  bool routeFree(const Route& route, std::span<const Fox> foxes) const;
  // The body as routeFree checks it: a box on the ground with its middle, its
  // axes (x the heading, z its right) and boxHalf_ along them, for the model's
  // origin at x, z and its yaw.
  struct BodyBox {
    glm::vec2 centre, ax, az;
  };
  BodyBox bodyBox(glm::vec3 at) const;
  // Whether two boxes overlap, and a box and a circle.
  bool overlaps(const BodyBox& a, const BodyBox& b) const;
  bool touches(const BodyBox& a, glm::vec2 centre, float radius) const;
  // Whether a bed at xz, its curl facing `yaw`, keeps clear of every route
  // one of `foxes` is out on: its kFootprint circle and its standing body box.
  bool bedFree(glm::vec2 xz, float yaw, std::span<const Fox> foxes) const;
  // The bed rules over a laid-out scene, for findBeds and findBed.
  class Beds;
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

  std::vector<glm::vec2> fires_;  // the campfires' middles...
  float fireReach_ = 0.f;        // ...and how far their dirt patches reach
  float standTurn_ = 0.f;   // the standing heading, from the curl's yaw (radians)
  glm::vec2 pivot_{0.f};    // what the walk steers: model x (heading) and z (right) at kSize
  // A body box's half sizes, grown so that the boxes every kCheckStep cover
  // the whole walk between them (routeFree), and how far its corners reach.
  glm::vec2 boxHalf_{0.f};
  float boxCorner_ = 0.f;
  bool forceWalk_ = false;
  bool walking_ = false;    // this wake walks, along route_, from the wake until it lies down
  Route route_;
  pxe::SkinnedModel::Pose fadeTo_, walkPose_;
  // How far each phase of Fox_Walk is from standing (sum of joint turns).
  std::vector<float> walkGap_;
  // The walks come from a stream of their own (plus its index), so the rests
  // are drawn exactly as before. With this seed the first fox's first wake
  // stays put (so it wakes exactly as before this walk was added) and its
  // second walks; after that about one wake in five does.
  static constexpr std::uint32_t kWalkSeed = 20261022u;
  std::mt19937 walkRng_{kWalkSeed};
};

}  // namespace game
