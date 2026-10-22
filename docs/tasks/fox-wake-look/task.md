# Fox wakes and looks around
Type: feature
Status: complete
Cycle: 1

## Goal
The sleeping fox by the campfire (task `fox-npc`) periodically wakes. After a random 20–40 s of rest it stands up, turning as it rises so it ends side-on to the fire, looks around, then lies back down into its curl and rests again, forever. It runs on a timer: there's no awareness of anything and it never leaves its spot. This makes the fox feel alive, and gives it the reusable clips (stand up, look, lie down) it will need when it gets AI.

## Decisions
- Four clips, strung together in code: `Fox_Rest` (unchanged), `Fox_StandUp` (about 1.5 s), `Fox_LookAround` (about 4 s), `Fox_LieDown` (about 2 s). Separate clips rather than one long one, so they can be reused when the fox gets AI.
- Seamless joins: `Fox_StandUp`'s first frame equals `Fox_Rest`'s first frame, and `Fox_LieDown`'s last frame equals `Fox_Rest`'s first frame. `Fox_LookAround` starts on `Fox_StandUp`'s last frame and ends on `Fox_LieDown`'s first frame. The game starts StandUp only as the rest loop wraps, and goes back into `Fox_Rest` at time 0. A short crossfade (about 0.15–0.25 s) is a safety net at each join, using `SkinnedModel::blend`.
- Timing: rest for a uniformly random 20–40 s (rounded up to the next wrap of `Fox_Rest`), then StandUp → LookAround → LieDown → Rest, repeating. The first wake is at least 20 s after load.
- Facing when standing (user chose option a): the fox turns as it stands, so standing it is side-on to the fire, its body running along the ring rather than its head over the stones. The turn is in the clips (root rotation), and LieDown turns it back so the curl faces the fire as now. The designer picks the turning direction that keeps the standing fox clear; the developer checks clearance.
- Clearance: the whole standing fox at 2.4×, through all four clips, stays at least 0.2 m clear of the fire's stones, logs and dirt patch and the scene's objects, as the curl already does.
- Spot moves (user decision after the asset review): the fox turns a quarter-turn left as it stands (asset revision 4), which at the old spot (-6.65, -6.95) left it facing away from the default camera. `kRuinsFoxSpot` moves to the pond side of the fire (+x of it), so that, curled, it still faces the fire, and standing it faces the default camera or three-quarters toward it, so its face shows during the look-around. If no clear spot exists on that side, report back rather than compromising clearance. The grass clearing moves with the spot.
- Size stays at 2.4× (user confirmed): standing, about 1 m at the shoulder and about 2.7 m nose to tail, about a third of the hero's height.
- Look-around: ears up and alert, head turning one way then the other, a sniff, the tail low with a gentle sway. It may glance toward the fire. One clip; variety comes only from the random rest time.
- No awareness: purely a timer. No reaction to the hero, sword, fire, rain or time of day; no movement off the spot; no collision.
- The existing slope tilt in `Fox::spawn` applies to the standing fox too.
- Grass: the cleared circle under the curl stays as it is. Standing legs and tail in the grass are fine.
- Captures stay deterministic: in `--fox` captures the fox is asleep, as now. A new `--fox-wake SECS` shows the fox SECS into its wake sequence (0 = starting to stand), so stand, look and lie-down poses can be captured, mirroring `--hero-attack`.
- Asset pipeline as in `fox-npc`: built in the user's live Blender session through the Blender MCP, with `tools/blender/build_fox.py` as the source of truth, rebuilt and exported headless.

## Acceptance criteria
- [x] `tools/blender/build_fox.py` builds `Fox_StandUp`, `Fox_LookAround` and `Fox_LieDown` alongside the unchanged `Fox_Rest`, and its `check()` attachment test covers sampled frames of all four clips — `build_fox.py` clip builders plus `check()`/`check_motion()` run on all four clips in every headless build
- [x] `assets/characters/fox.glb` contains the four clips, with the joins matching as specified (documented in assets.md with measured join differences) — assets.md revision 5: four clips, joins bit-identical (designer and developer both measured)
- [x] `game::Fox` has a state machine (rest → stand up → look around → lie down → rest) in `Fox::update`, with a random 20–40 s rest that starts StandUp at a `Fox_Rest` wrap; the first wake is at least 20 s after spawn — `fox.cpp` `Fox::update`/`enter`/`beginRest`, kRestMin/Max 20/40, StandUp only at a Rest wrap, first rest starts at spawn
- [x] Each join crossfades briefly with `SkinnedModel::blend`; one-shot clips are sampled with `loop = false` — `Fox::enter` samples the outgoing end pose, `Fox::pose` blends over `kFade` 0.2 s; one-shots sampled with loop=false
- [x] Standing, the fox is side-on to the fire (from the clips' root rotation), and the curl still faces the fire after lying down — root quarter-turn left in the clips (assets.md); LieDown ends on Fox_Rest frame 0
- [x] `kRuinsFoxSpot` is on the pond side of the fire, and the standing fox's facing at the default ruins camera yaw points toward the camera (within about 45° of straight at it); the method is recorded in the handoff — `scene.hpp` (-3.3,-6.6), +x of the fire; standing faces 19.4° off the yaw-180 default camera (handoff)
- [x] The whole fox at 2.4× through all four clips stays at least 0.2 m clear of the fire's stones, logs and dirt patch and scene objects (the method and margins are recorded in the handoff) — handoff cycle 1: every vertex of 698 poses with tilt, nearest 0.421 m (Paving.020)
- [x] The fox has no input, awareness, movement off its spot or collision — `fox.hpp/.cpp`: timer only, no input, movement or collision
- [x] In captures the fox rests as before unless `--fox-wake SECS` is given, which poses it SECS into the wake sequence; the flag is in `--help` and the README — `main.cpp` `holdRest(params.time)` unless `--fox-wake`, then `Fox::wake`; in `printUsage` and the README
- [x] Existing captures without `--fox` are unchanged, and `--fox` captures without `--fox-wake` are unchanged from before this task — handoff: default, hero and hills captures pixel-identical; `--fox` captures differ only by the user-approved spot move (identical with the old spot restored)
- [x] The README's fox paragraph describes the wake cycle — README fox paragraph and usage updated
- [x] Project builds cleanly from a fresh build directory with CMake — orchestrator fresh Debug build: exit 0, only the existing stb/glfw warnings

## Assets
- **fox** (revision 4 of the `fox-npc` asset): add three clips to the existing fox. Don't change its mesh, materials, joint names or `Fox_Rest`.
  - Source: `tools/blender/build_fox.py`; export `assets/characters/fox.glb`; preview `assets/characters/fox.png` plus key poses of the new clips
  - Format, units, axes, origin: unchanged (glb, real size, +Y up, faces +Z in the engine, origin at ground centre); the engine draws it at 2.4×
  - Animations:
    - `Fox_StandUp`, about 1.5 s, one-shot: from `Fox_Rest` frame 0 up to standing on all four paws, turning (root rotation) so the standing body is side-on to the original facing; pick the direction (left or right) that keeps the standing fox clear of the fire, which is about 1.9 m ahead along +Z at 2.4×
    - `Fox_LookAround`, about 4 s, one-shot: standing, ears up, head turning one way then the other, a sniff, the tail low with a gentle sway; starts and ends on StandUp's last pose
    - `Fox_LieDown`, about 2 s, one-shot: from the standing pose, turning back and settling into exactly `Fox_Rest` frame 0
  - Paws stay planted on the ground while standing (no sliding or floating), and the attachment `check()` passes on all clips
  - Report the standing footprint at 2.4× (extent along and across the facing, through all clips) in assets.md for the developer's clearance check

## Out of scope
- Any reaction to the hero or the world (AI)
- Walking, or moving off its spot
- Look-around variants
- Changes to the mesh, materials or `Fox_Rest`
- Collision

## Cycle log
- Cycle 1: 2026-10-21 started. Asset revision 4 accepted (four clips, exact joins). Spot moves to the pond side of the fire so the standing fox faces the camera.
- Cycle 1: developer blocked on asset REQ-001 (leg IK flips between keys); fixed in fox revision 5 and confirmed in-engine. Fresh build passes, review found no issues. Complete.
