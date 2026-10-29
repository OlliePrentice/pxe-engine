# The arms walk with the player
Type: feature
Status: open
Cycle: 1

## Goal
The desert player's arms are held still in their rest pose whatever the player does. Give them a walk: two looping clips, `Arms_Walk` and `Arms_Sprint`, authored in `build_arms.py`. The game plays them with a phase driven by the distance the eye actually moves across the ground, blended in by speed. So the arms keep step at any speed and settle back to the exact rest pose when the player stops. The camera doesn't move: only the arms do. A new `--walk <seconds>` capture flag (with `--sprint`) lets the run see the walk, since nobody plays during it.

## Decisions
All from the planning interview with the user, 2026-10-29.
- **The motion is a first-person carry** (Q18), not a natural swing from the shoulder:
  - both hands bob up and down twice per stride cycle;
  - they counter-swing fore and aft in turn, the left forward as the right goes back;
  - the forearms rock slightly at the elbow.
- **Authored clips, phase by distance** (Q19). The clips are authored in the glb, not built procedurally in code, so later animations build on the rig.
- **The view stays still** (Q20). No head bob. The camera and the scene in every capture are unchanged.
- **Starting and stopping** (Q21):
  - the walk's weight over rest follows the eye's actual ground speed, eased with a 0.15 s time constant;
  - standing still shows the rest pose exactly, with no breathing or idle sway.
- **What counts as moving** (Q22): the eye's horizontal movement over the ground, in any direction.
  - Walking backwards or sideways swings the arms.
  - Pressing into the play-area edge or the trunk without moving doesn't.
  - Sliding along the trunk does.
  - The eye easing up and down over the dunes doesn't.
- **Capture flags** (Q23):
  - `--walk <seconds>` simulates walking straight ahead from the spawn for that long, in fixed 1/60 s steps, before capturing;
  - `--sprint` makes that walk a sprint;
  - without them, `--capture` is unchanged.
- **Walk amplitudes** (Q24), as each hand joint's offset from rest in engine coordinates (`y` up, `z` the view's forward):
  - vertical bob ±1.5 cm, twice per cycle;
  - fore and aft counter-swing ±2.5 cm, once per cycle;
  - the elbow rocking ±4°;
  - the wrist trailing the swing by up to 6°;
  - the fingers still.
- **Sprint** (Q25):
  - bob ±2.5 cm;
  - counter-swing ±5 cm;
  - elbow ±8°;
  - hands lifted 1.5 cm on average;
  - every finger and thumb joint curled 25° more than rest, into a loose fist.

  The sprint blends in by speed: none at `kWalkSpeed` (3.5 m/s), full at `kSprintSpeed` (6 m/s).
- **Stride** (Q26):
  - **Cycle length:** walking covers 2.0 m per cycle (two steps) and sprinting 3.0 m. In between, the cycle length blends with the sprint weight, so the phase never jumps.
  - **Clip length:** both clips are 1.0 s long and sampled at phase × 1.0 s.
  - **Shared timing:** at phase 0 the left hand is furthest forward. Phases 0 and 0.5 are the low points of the bob (the footfalls).
- **Framing while moving** (Q27):
  - nothing in view above the lower 35%, near clearance of 0.15 m or more, and forearms leaving through the bottom edge, at every sampled frame of both clips;
  - the gauge and compass readability checks apply at rest only.
- **What the clips may move** (Q28, Q29):
  - The standing animation rules apply (`game/docs/art-style.md` "Animation", via `desert/docs/art-style.md`): the tag-prefixed names `Arms_Walk` and `Arms_Sprint`, 30 fps with every joint keyed on every frame (31 keys, the last equal to the first), linear interpolation, and no joint turning more than 45° between adjacent keys.
  - Only `shoulder`, `upper_arm`, `forearm` and `hand` move, plus the finger and thumb joints in `Arms_Sprint`. Every other joint is keyed at exactly its rest transform. Every translation and scale key equals rest.
  - Each moving rotation stays within the attachment test poses' ranges from rest: wrists 30° about each axis, fingers 60°, elbows opened 30°. The test poses gain an elbow closed by 10°.
  - Attachment is checked at 8 evenly spaced frames of each clip too.
- **Tuning values** are the numbers above. Changing them later is its own task.

## Layering
- Engine: none. `SkinnedModel::clip`, `sample`, `blend` and `clipDuration` already do what's needed.
- Game (`desert/`):
  - `desert::Player` records the ground distance the eye moved in its last update.
  - `desert::Arms` owns the clips, the phase and the eased weights. It advances them from that distance and poses the arms.
  - `main.cpp` drives both, and adds `--walk` and `--sprint`.
- Assets: `build_arms.py` adds the two clips to `arms.glb`.

## Approach
This task runs after `desert-arms-detail`, whose rebuilt `build_arms.py` and `arms.glb` it extends. Asset first, then code.

### 1. The asset (designer)
Add the clips to `desert/tools/blender/build_arms.py` (Assets). The mesh, skin, materials and rest pose must come out exactly as `desert-arms-detail` left them. Two checks prove it:
- with the clips added, the glb's rest-pose vertex positions, read back, equal the previous glb's;
- the desert's `--capture` stays byte-identical (Verification 3).

Add a short "View models" bullet to the Animation part of `desert/docs/art-style.md`'s Characters section (binding text, after the "Test poses" bullet):
> - **View-model clips.** A view model's clips move only its limbs: its `root` and every joint it doesn't animate are keyed at exactly their rest transform, and every translation and scale key equals rest. The framing rules (lower 35%, near clearance, limbs leaving through the bottom edge) hold at every sampled frame; readability (the gauge, the compass) is checked at rest only. A locomotion clip's phase is driven by the distance walked: assets.md records each clip's cycle length in metres. The arms' `Arms_Walk` and `Arms_Sprint` are 1.0 s each, 2.0 m and 3.0 m per cycle, the left hand furthest forward at phase 0.

### 2. `desert/src/player.hpp`, `player.cpp`
Add to `Player`, after `pitch` (binding):
```cpp
  // How far the eye moved over the ground in the last update, metres:
  // across it only, after the trunk and the play area's edge held it.
  float moved = 0.f;
```
In `Player::update`, keep the xz before moving. After the clamp and `outsidePalm`, `moved = glm::length(xz - before)`. It is 0 when there is no input. The height easing doesn't count.

### 3. `desert/src/arms.hpp`, `arms.cpp`
Interface (binding):
```cpp
class Arms {
 public:
  static constexpr int kFirstMaterial = 0;  // (its slot comment as desert-arms-detail left it)

  // Needs the GL context. Throws std::runtime_error if the glb can't be read
  // or lacks the Arms_Walk or Arms_Sprint clip.
  void load();
  // Advances the walk by the ground distance the eye moved over the last
  // dt seconds (Player::moved, 0 while the player isn't moving).
  void walk(float moved, float dt);
  // Places the arms at `view`, walking or at rest, and adds them to the
  // frame: params.viewModel and their actor materials. Their joints stay
  // valid until the next pose().
  void pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params);

 private:
  pxe::SkinnedModel model_;
  pxe::SkinnedModel::Pose rest_, walkPose_, sprintPose_, pose_;
  std::vector<glm::mat4> joints_;
  int walkClip_ = -1;
  int sprintClip_ = -1;
  float phase_ = 0.f;      // 0..1 through the stride cycle
  float moving_ = 0.f;     // the walk's eased weight over rest, 0..1
  float sprinting_ = 0.f;  // the sprint's eased weight over the walk, 0..1
};
```
`arms.cpp` (binding constants and the shape of `walk`):
```cpp
// Metres walked per cycle of each clip (two steps), and how fast the walk's
// weights follow the speed: a time constant, seconds.
constexpr float kWalkCycle = 2.0f;
constexpr float kSprintCycle = 3.0f;
constexpr float kEase = 0.15f;
// Below this the walk's weight is taken as none, and the arms are drawn in
// the rest pose itself.
constexpr float kAtRest = 1e-3f;

void Arms::walk(float moved, float dt) {
  if (dt <= 0.f) return;
  const float speed = moved / dt;
  const float k = 1.f - std::exp(-dt / kEase);
  const float moving = std::clamp(speed / kWalkSpeed, 0.f, 1.f);
  const float sprinting = std::clamp((speed - kWalkSpeed) / (kSprintSpeed - kWalkSpeed), 0.f, 1.f);
  moving_ += (moving - moving_) * k;
  sprinting_ += (sprinting - sprinting_) * k;
  if (moving == 0.f && moving_ < kAtRest) moving_ = 0.f;
  const float cycle = kWalkCycle + (kSprintCycle - kWalkCycle) * sprinting_;
  phase_ = std::fmod(phase_ + moved / cycle, 1.f);
}
```
`pose()` (sketch):
- With `moving_ == 0.f`, skin `rest_`, exactly as today, so standing still is the rest pose to the bit.
- Otherwise:
  - sample both clips at `phase_ * model_.clipDuration(clip)` into `walkPose_` and `sprintPose_`;
  - `SkinnedModel::blend(walkPose_, sprintPose_, sprinting_, pose_)`;
  - `SkinnedModel::blend(rest_, pose_, moving_, pose_)`;
  - skin `pose_`.
- The eye transform, `params.viewModel` and the materials stay as they are.

`load()` looks the clips up by name (`model_.clip("Arms_Walk")`, `"Arms_Sprint"`) and throws `std::runtime_error` if either is -1. `kWalkSpeed` and `kSprintSpeed` come from `player.hpp`.

### 4. `desert/src/main.cpp`
- **Options:** `Options` gains `float walk = 0.f;` and `bool sprint = false;`.
- **Parsing:**
  - `--walk <seconds>` takes 0 < seconds ≤ 10;
  - `--sprint` takes no value;
  - `--sprint` without `--walk` is a bad argument;
  - both are headless only: either one without `--capture` or `--window-png` is a bad argument.
- **Usage:** `printUsage` gains:
  ```
  "  --walk S                 walk straight ahead from the spawn for S seconds (0-10) first\n"
  "  --sprint                 with --walk: sprint instead\n"
  ```
- **Headless,** after `player.aim(camera)` and before the capture frames (binding shape):
  ```cpp
      // The walk before a capture: fixed 1/60 s steps straight ahead, so
      // a capture is the same on every run.
      const int steps = static_cast<int>(std::lround(opt.walk * 60.f));
      for (int s = 0; s < steps; ++s) {
        desert::PlayerInput in;
        in.move = {0.f, 1.f};
        in.sprint = opt.sprint;
        player.update(in, 1.f / 60.f);
        arms.walk(player.moved, 1.f / 60.f);
      }
      player.aim(camera);
  ```
  The capture frames then render as today, with no further updates.
- **Play loop:**
  - with the cursor held, after `player.update(...)`, call `arms.walk(player.moved, dt)`;
  - with it free, call `arms.walk(0.f, dt)`, so the arms settle when play pauses.
- **Comments:** the header comment and the `--help` text say the arms walk with the player.

## Verification
Run in the task's worktree:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. A headless rebuild of `build_arms.py` passes every check, including the clip checks (Assets). Two runs give byte-identical glbs.
3. `tools/perspective-check.sh <cycle base>`: **all 3 cases identical.** Standing still is the exact rest pose, and the mesh didn't change.
4. `tools/ortho-check.sh <cycle base>`: all 6 identical.
5. `git diff <cycle base> -- engine game examples desert/src/world.hpp desert/src/world.cpp`: no output.
6. `build/release/desert/pxe_desert --help` lists `--walk` and `--sprint`. Each of these exits 1 with "bad argument":
   - `--sprint --capture x.png`;
   - `--walk 0 --capture x.png`;
   - `--walk 11 --capture x.png`;
   - `--walk 2` alone.
7. Captures, into `docs/tasks/desert-arms-walk/`:
   - `--walk 2 --capture walk-low.png`: 7 m walked, phase about 0.5, a footfall with the right hand forward. Both hands lower than at rest, the right hand further forward than the left.
   - `--walk 2.29 --capture walk-left.png`: about 8 m, phase about 0. The left hand forward and the right back, mirrored from `walk-low.png`.
   - `--walk 2.14 --capture walk-mid.png`: about 7.5 m, phase about 0.75. The hands level with each other fore and aft, at the top of the bob.
   - `--walk 3 --sprint --capture sprint.png`: larger offsets than the walks, the hands raised, the fingers curled into loose fists.
   - `--walk 3 --hour 21 --capture walk-night.png`: the gauge and the lamp still glow.

   In each, nothing is above row 351 and the arms are whole. Two runs of each are identical (`cmp`). Compare each with `docs/tasks/desert-arms-detail/detail-day.png` in the handoff: the scene has moved forward because the eye walked, and the arms' offsets are as described.
8. **Stopping, by reading the code:** `walk()` snaps `moving_` to 0 only when the target is 0, and `pose()` uses `rest_` exactly when `moving_` is 0.

## Acceptance criteria
- [ ] `arms.glb` has the clips `Arms_Walk` and `Arms_Sprint` only, each 1.0 s with 31 keys per joint channel, linear. Its mesh, skin, materials and rest pose are unchanged from `desert-arms-detail` (rest-pose vertices read back equal)
- [ ] The build's clip checks pass (Assets): amplitudes and timing, framing and near clearance at every sampled frame, joint ranges, the 45° key-to-key limit, seamless loops, rest-keyed joints, attachment at 8 frames per clip, and the new closed-elbow test pose
- [ ] `Player::moved` is the horizontal distance after the edge and trunk held the eye, 0 with no input
- [ ] `desert::Arms` matches the binding interface and constants. `walk()` follows the binding shape, and `pose()` draws `rest_` exactly when `moving_` is 0 and otherwise blends walk→sprint by `sprinting_` and rest→that by `moving_`
- [ ] `main.cpp` parses and validates `--walk` and `--sprint` as specified, runs the fixed-step walk before a capture, and calls `arms.walk` every play frame (0 while the cursor is free)
- [ ] `desert/docs/art-style.md` has the binding "View-model clips" bullet, and nothing else in it changed
- [ ] Verification 3–6 pass: perspective 3/3 and ortho 6/6 identical, the diff empty, the flags validated
- [ ] The captures are in the task folder, deterministic, and show what Verification 7 describes
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms** (clips added): the rebuilt arms of `desert-arms-detail`, plus two clips. Nothing else in the glb changes.
  - Format, units and axes, origin, rig, materials, budget: as `desert-arms-detail`.
  - Animations: `Arms_Walk` and `Arms_Sprint`, both looping, 1.0 s, 30 fps, 31 keys, linear, every joint keyed on every frame. assets.md records each clip's cycle length (2.0 m and 3.0 m).

  **Measured points and terms**, binding:
  - **Hand offset:** the head of `hand.L` or `hand.R`, in engine coordinates, minus its rest position. `y` is vertical and `z` fore and aft (+ forward).
  - **Phase:** time / 1.0 s.
  - **Sampled frame:** each key, and each half-key between keys: 61 samples per clip, sampled as the engine does (linear, with rotations slerped the short way).

  **Motion guide** (how to get it; the checks below bind):
  - **Bob:** from shoulder and upper-arm rotation. Each hand's `y` offset is about -A·cos(4π·phase).
  - **Counter-swing:** the left hand's `z` offset is about +S·cos(2π·phase), and the right's is -S·cos(2π·phase).
  - **Elbow and wrist:** the forearm rocks at the elbow in time with the swing. The hand trails it at the wrist.
  - **Sprint lift and fist:** in `Arms_Sprint`, both hands sit 1.5 cm higher on average, and every finger and thumb joint is curled 25° beyond rest about the hinge the curl test pose uses, on every key.

  **Checks, binding, in the build** (on top of every `desert-arms-detail` check, which still runs in the rest pose):
  - **Amplitudes:** over the 61 samples, each hand's `y` and `z` offsets reach the following, within ±3 mm. `Arms_Walk`: `y` ±1.5 cm about 0, `z` ±2.5 cm. `Arms_Sprint`: `y` ±2.5 cm about a mean of +1.5 cm, `z` ±5 cm.
  - **Timing:**
    - the left hand's `z` offset peaks within 1/30 of phase 0;
    - the right's peaks within 1/30 of phase 0.5;
    - each hand's `y` offset is lowest within 1/30 of phases 0 and 0.5.
  - **Joints:**
    - only `shoulder`, `upper_arm`, `forearm` and `hand` (both sides) change between keys in `Arms_Walk`, plus the finger and thumb joints in `Arms_Sprint`;
    - every other joint's keys equal its rest transform exactly;
    - every translation and scale key equals rest.
  - **Ranges:**
    - each `hand` within 30° of rest about each of the twist, flex and sideways axes the test poses use;
    - each `forearm` between 10° closed and 30° opened;
    - fingers within 60° of rest.
  - **Steps:** no joint turns more than 45° between adjacent keys.
  - **Loops:** the last key equals the first exactly, for every channel.
  - **Framing at every sampled frame:** nothing in view above NDC y -0.30, near clearance of 0.15 m or more, and each forearm leaving through the bottom edge.
  - **Attachment:** the attachment check at 8 evenly spaced frames of each clip (phases 0, 1/8 … 7/8), plus a new test pose with each elbow closed by 10°.
  - **Read-back:** after export, the glb has exactly these two animations, each 1.0 s with 31 keys per channel and LINEAR interpolation. Its rest-pose vertex positions equal those of the glb before the clips were added (to 1e-7 m).

  **Previews**, beside the glb: `arms_walk.png`, a 2×4 sheet of the eye view at phases 0, 1/8 … 7/8 of `Arms_Walk`, and `arms_sprint.png`, the same for `Arms_Sprint`. The existing previews are rendered as before.

## Out of scope
- Head bob or any camera motion
- Idle breathing or sway, lag on turns, jump or landing motion
- Animated gadgets: needle, lamp, dial
- A held tool, grabbing, using the gauge or compass
- Footstep sounds
- Changing the mesh, materials, rest pose or framing rules of `desert-arms-detail`
- Engine changes

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-29, base 55838cc
