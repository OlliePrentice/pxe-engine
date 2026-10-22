# Fox takes a walk
Type: feature
Status: complete
Cycle: 2

## Goal
On about half of its wakes, after standing up and looking around, the fox walks a few steps to a random point 2–5 m from its bed, stops and looks around there, then walks back and lies down in its bed, facing the fire as now. This makes the fox feel alive without AI. It's still a timer-driven routine with no awareness of anything. It needs a new walk clip and moving the fox in code, with speed tied to the stride so the paws don't slide.

## Decisions
- Builds on `fox-wake-look`: the bed is `kRuinsFoxSpot` (-3.3, -6.6), the curl faces the fire, and `Fox_StandUp` leaves it standing a quarter-turn left of the curl's facing (the turn is on the `root` joint).
- Sequence on a walking wake: StandUp → LookAround → walk out → LookAround (at the destination) → walk back → LieDown → Rest. A non-walking wake is as now: StandUp → LookAround → LieDown.
- About half of wakes walk (each wake decides with probability 0.5 from the fox's seeded RNG).
- It always returns to its bed to lie down, so the grass cleared at load and the bed's fire clearance stay valid. User agreed; foxes have a favourite spot.
- Distance: the destination is a uniformly random point 2–5 m from the bed, never further than 5 m (at 2.4× a stride is about 1.4 m, so about 2–4 steps each way).
- One stop per outing, with a look-around there (reusing `Fox_LookAround`).
- No pathfinding. It walks out and back along simple paths, turning smoothly as it walks (never spinning on the spot). Before committing to an outing, the code simulates the whole route out and back with the same steering it will use, and accepts it only if every sample along the way passes:
  - the fox's whole standing body (the footprint from assets.md, rotated to its heading) is on walkable ground: no rise above the hero's step height (`kStep`) relative to where it stands, no water, not off the map;
  - the body stays at least 0.2 m clear of the fire's dirt patch (0.96 m round the campfire).
  If a target fails, it tries another (a few attempts, all from the seeded RNG). If none pass, that wake doesn't walk.
- Arriving home: it curves its approach so it reaches the bed already facing the standing direction (the quarter-turn left of the fire-facing curl), then plays `Fox_LieDown` straight away. No turn-on-the-spot clip.
- Gait: one unhurried walk, no trot or run. A new looping clip `Fox_Walk`, played in place (no forward root motion) while the code moves the fox. Ground speed = the clip's stride length per cycle × 2.4 / its duration, so the paws don't slide. It eases from standing into the walk and back to a stop, with crossfades and speed ramping together, as the hero does.
- Ground: while away from the bed, the fox's height follows the ground every frame, and it leans smoothly toward the slope under it (smoothed, so no jitter over bumps). Back in the bed, it ends at the bed's existing spawn height and tilt, so `Fox_LieDown` and the curl are unchanged.
- No awareness or interaction: no reaction to the hero, sword, fire, rain or time of day; no collision with the hero; the hero still walks through it.
- Captures: everything stays deterministic from the fixed seed. `--fox-wake SECS` covers the walking part of a wake too. A new `--fox-walk` forces the next wake (in captures, the one `--fox-wake` starts; in play, the first) to include a walk, so it can be captured and playtested without luck. It's in `--help` and the README.
- Asset review (orchestrator, cycle 1): revision 6 accepted. The stride came out at 0.405 m real (0.972 m at 2.4×) per gait cycle, not the 1.4 m estimate above, because the standing forelegs are nearly straight. The ground speed is 0.810 m/s at 2.4×, and 2–5 m is about 2–5 cycles each way. The 2–5 m distance is unchanged. "Elbows behind the forepaws" means the leg bends on the correct side of the shoulder-to-paw line (the standing bend direction), not that the elbow is behind the wrist along the heading. Late in each front stance the elbow passes ahead of the wrist, as a real fox's does. Checks must test bend direction.
- Bed moves (user decision after cycle 1 review, ISSUE-001): at `kRuinsFoxSpot` (-3.3, -6.6) the fire and a bank hem the bed in, so every walk had to be a ~13 m lap round the fire. The bed moves to more open lawn near the campfire where a short out-and-back fits: it leaves at the standing heading, walks out to the target, looks, and curves back (a teardrop loop) into the bed from behind, without going round the fire. The bed keeps the fox-wake-look rules: the curl faces the fire; standing, it faces within about 45° of the default ruins camera (yaw 180); the whole fox through all clips stays at least 0.2 m clear of the fire's stones, logs and dirt patch and every scene object; the grass clearing moves with it. If no spot meets all of these, report back rather than compromising.
- Fewer walks (user decision after cycle 2's spot search): no spot meets every bed rule and still lets about half of wakes walk under the 2d + 4 m cap, because without a turn on the spot every walk needs a whole-turn loop. The user accepted fewer walks: the bed moves to (-2.0, -7.0), between the bank and the paving, all current rules kept (each wake still decides with p = 0.5, up to 8 targets drawn uniformly over the 2-5 m ring, then it gives up). About 19% of wakes walk, only ahead or ahead-left of the bed.
- Asset pipeline as before: built in the user's live Blender session through the Blender MCP, with `tools/blender/build_fox.py` as the source of truth, rebuilt and exported headless.

## Acceptance criteria
- [x] `tools/blender/build_fox.py` builds a looping `Fox_Walk` clip, in place, with the same root quarter-turn as the standing pose, and its `check()`/`check_motion()` cover it; the other four clips are unchanged — `build_fox.py` Fox_Walk builder; check()/check_motion() run on all five clips (assets.md rev 6)
- [x] assets.md documents `Fox_Walk`'s duration, its stride length per cycle (real size), and how close its first frame is to the standing pose — assets.md rev 6: 1.200 s, stride 0.405 m real, first frame 43.1°/0.299 m from standing
- [x] `Fox::update` has the walking sequence (out → look → back) on walking wakes, decided with probability 0.5 from the seeded RNG; non-walking wakes are unchanged — `fox.cpp` planWalk (walkRng_ draw < 0.5) and the kWalkOut/kLookOut/kWalkBack states; non-walking wakes use the cycle-0 path, and captures are identical
- [x] Destinations are uniformly random 2–5 m from the bed, never further than 5 m — `fox.cpp` planWalk: r = sqrt(2² + (5² − 2²)u), uniform over the ring; routeClear rejects any sample beyond kMaxReach
- [x] Before an outing, the whole route out and back is simulated with the same steering, and every sample's rotated footprint is checked for step height, water, off-map and the 0.2 m fire clearance; failing targets are retried a few times and then the walk is skipped — `fox.cpp` layRoute + routeClear (rotated union footprint, Hero::kStep, water/off-map, fire 0.96 + 0.2 m), shared with routeAt for the live walk; kTries 8, then skip
- [x] Ground speed comes from the stride length × 2.4 / clip duration (no hard-coded speed that ignores the clip), with eased starts and stops crossfaded with the clip — `fox.cpp` walkSpeed() = kStride × kSize / clip duration; legDistance eases over kRamp, with the pose blend over kWalkFade
- [x] The fox turns only while walking (smooth steering), and arrives at the bed facing the standing direction, so `Fox_LieDown` starts with no turn on the spot or snap — `fox.cpp` route yaw from curvature (min radius 0.9 m, eased); arrival error ≤ 1.3e-5 m, heading 0 (handoff cycle 2)
- [x] Away from the bed, height follows the ground each frame and the lean to the slope is smoothed; back at the bed, the height and tilt match spawn — `fox.cpp` place(): ground height each frame; lean smoothed along the route (routeTilt), with bedY_/bedTilt_ exactly at the bed
- [x] The fox always lies down in its bed; `Fox_Rest` and the grass clearing are unchanged — Both legs end at bedPos_; Fox_Rest untouched; grass clearing keyed off kRuinsFoxSpot (scene.cpp unchanged)
- [x] No interaction with the hero or the world beyond the ground checks — `fox.cpp` takes only the scene's ground/water queries; no hero, input or collision
- [x] `--fox-walk` forces the next wake to walk; `--fox-wake SECS` covers the walk; both are deterministic and in `--help` and the README — `main.cpp` --fox-walk → forceWalk(); wake(scene, secs) runs the walk; printUsage and the README updated
- [x] Captures without `--fox` are pixel-identical to before this task; `--fox` captures without `--fox-wake`/`--fox-walk` differ only by the user-approved bed move (identical with the old spot restored) — Handoff cycle 2: 5 non-fox captures byte-identical; 14 --fox/--fox-wake captures identical with the old spot restored
- [x] The bed is on open lawn near the campfire, with the curl facing the fire, the standing fox facing within about 45° of the yaw-180 default camera, and the whole fox through all clips at least 0.2 m clear of the fire and scene objects (method and margins in the handoff) — `scene.hpp` kRuinsFoxSpot (-2, -7): curl faces the fire, standing 18.4° off the camera, nearest Paving.026 0.204 m (handoff cycle 2)
- [x] Walks are short outings, not laps round the fire: the route out is close to direct, the return is a teardrop back into the bed, and the whole walk is no longer than about 2 × the target distance + 4 m; at most 8 targets are tried per walking wake (about 19% of wakes walk, per the user's decision) — `fox.cpp` layRoute teardrop ≤ 2d + 4 m; outings 6.7–11.1 m; kTries 8; 19.6% of wakes walk (the user-accepted rate)
- [x] The README fox paragraph describes the walk — README fox paragraph describes the teardrop walk
- [x] Project builds cleanly from a fresh build directory with CMake — Orchestrator fresh Debug build, cycle 2: no errors, only the existing stb/glfw warnings

## Assets
- **fox** (revision 6 of the fox asset): add one clip. Don't change the mesh, materials, joint names or the four existing clips.
  - Source: `tools/blender/build_fox.py`; export `assets/characters/fox.glb`; preview `assets/characters/fox_walk.png` with key poses of the cycle, from the side and from the game angle
  - Format, units, axes, origin: unchanged (glb, real size, +Y up, origin at ground centre); the engine draws it at 2.4×
  - Animations:
    - `Fox_Walk`, looping seamlessly, an unhurried red-fox walk: a four-beat lateral-sequence gait, head carried low-ish and level, the brush held out low behind with a slight sway, ears up. Two strides per cycle; pick a natural duration (about 1–1.3 s at real size).
    - In place: no net forward root translation. Forward is the standing facing, so it carries the same root quarter-turn as the end of `Fox_StandUp`.
    - During each stance phase, the planted paw moves backward in model space at a constant speed equal to stride length ÷ cycle duration, so with the engine moving it forward the paw stays still on the ground. Report that speed and the stride length.
    - The first frame is as close to the standing pose (`Fox_StandUp`'s last frame) as a mid-stride pose allows, so the engine's crossfade is short and clean.
  - `check()` and `check_motion()` pass on `Fox_Walk`: attachment, no joint turning more than 45° between keys, nothing more than 1 cm below the ground at half-keys, and elbows and knees bending the right way.

## Out of scope
- Pathfinding, multiple stops, or wandering further than 5 m
- Trot or run gaits, and turn-on-the-spot clips
- Lying down anywhere but the bed
- Any reaction to the hero or the world (AI), and collision with the hero
- Changes to the existing four clips, the mesh or the bed position

## Cycle log
- Cycle 1: 2026-10-22 started. Asset revision 6 (Fox_Walk) accepted.
- Cycle 2: 2026-10-22 started. User chose to move the bed so walks can be short out-and-backs. Outstanding: ISSUE-001
- Cycle 2: developer stopped: no bed spot met every rule at ~50% walking. User accepted fewer walks (~19%) with the bed at (-2.0, -7.0).
- Cycle 2: complete. Fresh build passes; ISSUE-001 resolved; ISSUE-002 wont-fix.
