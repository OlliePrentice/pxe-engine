## Cycle 1
Build: pass. Fresh Debug directory, rebuilt against fox revision 5: `cmake -S . -B <fresh> -DCMAKE_BUILD_TYPE=Debug && cmake --build <fresh> --parallel`. The only warnings are the existing stb `sprintf` deprecation in main.cpp and the linker's duplicate-libglfw note.

REQ-001 (leg IK flips in Fox_StandUp and Fox_LieDown, raised against revision 4) is fixed in fox revision 5. I re-verified it in-engine; see "Revision 5 re-check" below. No code change was needed for revision 5.

### Changes
- game/src/fox.hpp, game/src/fox.cpp: the state machine.
  - States are Rest, StandUp, LookAround and LieDown. `build()` looks up all four clips by name, and throws if any is missing or empty.
  - Rest loops `Fox_Rest`. On entry, a rest length is drawn uniformly from 20-40 s (`kRestMin`/`kRestMax`). StandUp starts only when the loop wraps with that much rest done, so the rest is rounded up to the next wrap. The first wake is therefore at least 20 s after spawn.
  - The one-shots play once each and are sampled with `loop = false`: StandUp, then LookAround, then LieDown, then Rest at time 0.
  - `update(dt)` steps through every join that dt spans, each at its exact moment, so one long step gives the same result as many short ones.
  - Each join crossfades over `kFade = 0.2 s` with `SkinnedModel::blend`. It fades from the outgoing clip's end pose, sampled at its duration, into the incoming clip. The Rest-to-Rest wrap has no fade, so the loop is exactly as before.
  - RNG: `std::mt19937` with a fixed seed (`kSeed`), reseeded in `spawn()`. The rest length is computed from the raw 32-bit draw rather than a `std::` distribution, so it is the same on every platform. With this seed, a normal run wakes at 28.0 s, 71.5 s, 111.0 s, 158.5 s, and so on.
  - New `holdRest(seconds)` for captures: asleep at that point in the loop, never waking. It is the same pose as the old `update(params.time)`.
  - New `wake(seconds)`: starts StandUp with no fade, then runs `update(seconds)`.
  - No input, awareness, movement or collision.
- Slope tilt in `spawn()`: unchanged. I kept one tilt, sampled 0.41 m out, for all clips:
  - A plane fitted to planar ground is the same plane whether it is sampled 0.41 m or 1.6 m out.
  - One tilt for every clip keeps the joins seamless. A tilt per state would itself need blending.
  - At the new spot, standing in LookAround, all four paws are 9 mm above `scene.ground.heightAt`, the same as the 4 mm pad gap on flat ground at 2.4x. The ground there is planar to the millimetre.
  - The spawn comment explains this.
- game/src/scene.hpp: `kRuinsFoxSpot` moved from (-6.65, -6.95) to **(-3.3, -6.6)**, on the pond side (+x) of the fire, 1.80 m from it and slightly behind it in the default view. The comment records the facing and clearance.
  - The grass clearing follows automatically: main.cpp passes `kRuinsFoxSpot` with the curl's `kFootprint` (0.68 m) as before. It drops 20 sprites.
- game/src/main.cpp:
  - `--fox-wake SECS` (`Options::foxWake`, clamped to 0 or more). It implies `--fox` in captures, but `--no-fox` still wins (`foxShown`).
  - After `spawn()`, `fox.wake(SECS)` runs when it is given. That applies to captures and also to play, where the fox starts that far into waking, so the sequence can be seen without waiting.
  - In captures without it, the fox is `holdRest(params.time)`: asleep, with the breath phase from `--time`/`--settle` as before. `--fox-wake` is independent of `--time` and `--settle`.
  - Added to `--help`.
- README.md: the fox paragraph now describes the wake cycle, the four clips, the fixed seed, the new spot, and `--fox-wake` (its meaning, 0/1.5/5.5/7.5 s landmarks, implying `--fox`, and use in play). Also updated the usage block and the build_fox.py clip list.

### Spot, facing and clearance (method)
- **Default camera yaw.** `buildRuins` sets `scene.yawDegrees = 180`. `Camera::basis` gives flat forward (sin 180, cos 180) = (0, -1), so the view looks toward -z, and straight at the camera is world +z.
- **Facing.** The spawn yaw is atan2 toward the fire, and the model's +Z maps to world (sin yaw, cos yaw). Standing, the fox faces model +X (assets.md), which is world (cos yaw, -sin yaw). This reproduces the designer's (0.50, -0.87) at the old spot.
  - At (-3.3, -6.6), yaw is -70.6 degrees. Curled, it faces (-0.943, 0.333), toward the fire. Standing, it faces (0.333, 0.943), which is **19.4 degrees off straight at the camera**, three-quarters toward it, turned to screen right. The right side is to the fire.
  - The `--fox-wake` captures confirm it: standing, its face and chest point down-screen toward the viewer.
- **Clearance tool.** A scratch C++ tool, not in the repo. It compiles game/src/scene.cpp into itself and links the engine library.
  - It loads ruins.glb with `pxe::loadGltf`.
  - It rebuilds the campfire with the game's own `addCampfire` (same `fireRng(11)`, same lawn `GroundIndex`). This gives the actual dirt patch, stones and logs, not radius bounds.
  - It builds the same `HeightMap` as the game, and reproduces `Fox::spawn`'s height, tilt and yaw exactly.
  - Obstacles are every triangle of every ruins.glb node except `Ground` (the lawn and its dirt), plus the fire's dirt patch, stones and logs. That includes water, paving, rock and trunk.
  - The fox is all 1984 skinned vertices at 2.4x, in every 1/60 s sample of all four clips plus each clip's end: 698 poses. That is twice the key rate, so half-key poses are included. Distance is exact point-to-triangle in 3D.
- **Search.** A 0.1 m grid over x -4.5..-1.5, z -8.5..-3.5, keeping only offsets from the fire within 45 degrees of +x, which puts the standing facing within 45 degrees of the camera.
  - Directly +x of the fire (facing 0 degrees), the paving to the fox's right (Paving.020) limits the clearance to 0.23 m at best, at 1.6 m from the fire.
  - At facings under about 15 degrees, the best is 0.32-0.37 m.
  - I picked (-3.3, -6.6) for a wider margin at 19 degrees.
  - The +z side (in front of the fire) is tighter, 0.25 m at best.
- **Margins at (-3.3, -6.6), every vertex and every 1/60 s sample:**

  | Obstacle | Margin | When |
  |---|---|---|
  | Paving.020 | 0.421 m | LieDown 1.23 s |
  | Paving.026 | 0.462 m | LieDown 1.28 s |
  | Fire dirt patch | 0.466 m | LieDown 1.87 s |
  | Fire stones | 0.805 m | |
  | Fire logs, Rock, PondTrunk, water and all else | > 1.0 m | |

  All margins clear the required 0.2 m.
- **Cross-check.** The same tool at the old spot gives Rock 0.310 m (designer: 0.262 untilted), dirt patch 0.644 m (designer: 0.473, which used the rim's 0.96 m maximum radius rather than the actual ragged rim) and PondTrunk 0.763 m.
- **Asset revision.** The margins above are re-measured against revision 5, and are the same as against revision 4 to the millimetre. The limiting poses are the body and brush swinging through LieDown, not the legs the fix changed. The grid search was run against revision 4. The designer reports revision 5's footprint and 16-gon identical, and the chosen spot's margins are unchanged, so I didn't re-run the search.
- **Ground at the spot, revision 5.** The deepest any vertex goes below `heightAt`, over every sample of all four clips, is 6 mm (StandUp 0.18 s). Against revision 4 it was 116 mm, a forepaw in LieDown at 0.45 s. Standing, the four paws are all 9 mm above the ground.

### Revision 5 re-check (REQ-001)
I measured through `pxe::SkinnedModel::sample`/`skin` at 2.4x with my own scratch tools, the same ones that found the fault:
- **Largest per-key joint turn:** 23.9 degrees, the forepaws in StandUp 19-20 (a stepping paw). No other joint turns over 20 degrees per key. Revision 4 had turns of 64-176 degrees at six key pairs.
- **Largest mid-key departure from the key chord** (any vertex, half-key):

  | Clip | Revision 5 | Where | Revision 4 |
  |---|---|---|---|
  | StandUp | 0.039 m | keys 4-5, the head and brush arcing out of the curl | 0.750 m |
  | LieDown | 0.018 m | | 0.174 m |
  | LookAround | 0.001 m | | |
  | Rest | 0.002 m | | |

- **Lowest vertex at 240 Hz:** -4 mm (StandUp about 0.45 s), against -115 mm before.
- **Captures at the former flip points** (`--fox-wake` 0.7333, 0.75, 0.7667, 0.8667, **0.8833**, 0.9, 1.15, 5.8167, 5.95 and 6.25, with `--hide-grass`): the legs are continuous through every one. At 0.8833 the front legs are extended and planted. In revision 4 they folded into the chest there.
- These all agree with the designer's figures.

### Verification
- Pixel-identical to a baseline build of the pre-task tree:
  - Default capture.
  - `--hero --hero-attack 0.12`.
  - `--scene hills --fox`.
- `--fox` rest logic: built a variant with only `kRuinsFoxSpot` set back to the old spot. Its `--fox` captures are pixel-identical to the baseline for `--fox`, `--time 2.6`, `--time 30` (past the first wake time: captures still sleep), `--time 57.3 --hour 22`, `--settle 3` and `--hide-grass --time 1.1`.
- With the real build, `--fox` captures differ from before only where the spot moved (expected): the fox and its cleared grass are now right of the fire instead of left.
- `--fox-wake` captures at 0, 0.5, 0.9, 1.5, 2.5, 3.8, 5, 6.3, 6.9, 7.5 and 9 s show the expected sequence (checked first against revision 4). At 7.5 and 9 s it is back in the curl, identical to the rest pose.
- Re-checked with the fresh revision 5 build:
  - The default, `--hero --hero-attack 0.12` and `--scene hills --fox` captures are still pixel-identical to the pre-task baseline.
  - The six sleeping `--fox` captures listed above are pixel-identical to the revision 4 build (Fox_Rest is unchanged). The chain to the baseline therefore still holds: the only `--fox` differences from before this task are the spot move.

### Issues addressed
- None (cycle 1).

### How to playtest
- `./build/game/pxe`, default ruins view. The fox is curled to the right of the campfire and a little behind it, its face to the fire.
- **First wake.** At about 28 s after start (the seeded first wake) it should do the following, with no hitch at any of the three joins or when it settles back into the breathing loop:
  - lift its head and swing round as it stands, a quarter-turn to its left, ending side-on to the fire and facing toward you and slightly right;
  - look left (away from the fire, toward screen right), then right toward the fire, then sniff the ground;
  - lie back down into the same curl facing the fire.
- **Later wakes.** The next wakes come at about 71.5 s, 111 s and 158.5 s (it varies 20-40 s rest each time). `[`/`]` change only the day's speed, not the fox's, so these are real seconds.
- **Without waiting.** `--fox-wake 0` starts it standing up immediately. `--fox-wake 1.5` starts it standing, and `--fox-wake 5.5` starts it lying down.
- **Legs (REQ-001, fixed in revision 5).** Watch the front legs around 0.7-1.2 s into standing and 0.3-0.5 s into lying down, and the hind legs around 0.75 s into each. Revision 4 popped there. Now the legs should step and fold smoothly, the elbows staying behind the front paws and no paw dipping into the ground. Standing up is fast at the start: the brush and head sweep out of the curl in about 0.2 s. That is the clip's timing, not a pop.
- **Clearance.** Q/E to rotate: check its shadow and outline while standing. It must not intersect the fire ring or the paving to its right. Standing legs and tail in the grass are expected.
- **Captures.**
  - `--capture a.png --fox` sleeps, even with `--time 30`.
  - `--capture a.png --fox-wake 2.5` looks left, and implies the fox.
  - `--fox-wake 2.5 --no-fox` shows no fox.
  - `--scene hills --fox-wake 1` shows no fox.

### Known limitations
- The standing fox faces 19 degrees right of straight at the camera, not straight at it. A spot with a smaller angle has less margin from the paving: 0.23 m at 0 degrees. I chose the margin.
- `--candles` keeps lawn candles 2.5 m from the campfire. The standing fox reaches up to about 3.4 m from it, so with `--candles` a lawn candle could stand inside the standing fox. `--extra-fires` still ignores the fox. Both are test options and were left unchanged so existing test captures don't change.
- In `--fox` captures, `--time` still sets the breath phase. `--fox-wake` ignores `--time` and `--settle`.
