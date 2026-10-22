## Cycle 1
Build: pass. Fresh Debug directory: `cmake -S . -B <fresh> -DCMAKE_BUILD_TYPE=Debug && cmake --build <fresh> --parallel`, exit 0. The only warnings are the existing stb `sprintf` deprecation (main.cpp) and the linker's duplicate-libglfw note. Against fox revision 6, unchanged.

### Headline: every walk is a lap round the campfire
The route check makes a plain out-and-back walk impossible from this bed. I mapped every position and heading near the bed against the task's check, with a scratch tool using the game's own `scene.ground`/`waterTop`, the union footprint and the 0.96 + 0.2 m fire rule. Two things hem it in:
- **The fire, on its right.** With the union footprint (right edge +0.573 m), the bed has 1.227 m from the fire's centre against the 1.16 m needed: 0.067 m spare. Yawing more than about 15° either way near the bed swings the body's front or rear corner into that clearance.
- **A bank behind it.** The rise is more than 0.55 m under the brush once the fox's origin is more than about 1.0 m behind the bed.

A backward reachability search showed the consequences. It ran paths of minimum turn radius R backward out of the bed at the standing heading, through every check:
- With R = 1.0 or 1.2 m, the only way in is a nearly straight last metre. Before that, the path comes from behind-right, along the strip between the fire and the bank.
- With R = 1.5 m, nothing gets in at all.

So to arrive facing the standing heading with no turn on the spot, the fox must come home along the fire's far side. I made each walk one smooth lap round the fire that bulges out through the random target:
- Out: bed → target, then a look around there.
- Back: target → on round → bed, from behind.

The standing heading is exactly tangent to the circle round the fire through the bed, because the curl faces the fire and the stand turns a quarter-turn. This shape needs no special casing. The orchestrator may want to confirm this reading of "walks back" with the user. A short out-and-back would need a turn-on-the-spot clip, a different bed or a relaxed check.

### Changes
- game/src/fox.hpp, game/src/fox.cpp: the walk.
  - **States.** New `kWalkOut`, `kLookOut` (Fox_LookAround at the target) and `kWalkBack`. A walking wake is StandUp → LookAround → WalkOut → LookOut → WalkBack → LieDown → Rest. A non-walking wake runs the same code path as before: StandUp → LookAround → LieDown with the same crossfades. `build()` also requires `Fox_Walk`.
  - **Decision.** At each wake's start (`planWalk`, called where StandUp begins in `update()` and in `wake()`):
    - one draw from a separate stream `walkRng_` (seed `kWalkSeed` 20261024, reseeded in `spawn()`), walking if < 0.5;
    - the draw happens even when forced, so the stream stays aligned.
    - The rest RNG (`rng_`) is untouched, so rest lengths are drawn exactly as before.
    - With this seed, wake 1 stays (so it is identical to before), and wakes 2 and 4 walk.
  - **Targets.** `bedPos_ + r(cos a, sin a)`, with `a = 2π·u` and `r = sqrt(2² + (5² − 2²)·u)`, uniform over the 2–5 m ring. All draws are raw 32-bit values, platform-stable. Up to `kTries` = 24 targets, or `kForcedTries` = 256 for `--fox-walk`. Then it gives up and that wake doesn't walk; stderr says so.
  - **Route (`layRoute`).** A closed polar curve about the fire, tracing the path of the walk pivot (below):
    - P(l) = fire + exp(y(l))·(cos(a0 + turn·l), sin(a0 + turn·l)), for l from 0 to 2π.
    - y(l) is two cubic Hermites in log-radius: bed radius → target radius → bed radius. They meet at the target with matched slope and curvature.
    - The slope at the bed is set so the curve leaves and arrives exactly along the standing heading. `turn` (the direction round the fire) comes from that heading.
    - It is a C1 closed loop: one heading at the target for the stop and restart, and smooth everywhere it moves.
    - An arc-length table (2048 steps, the target on a step) maps distance to l.
    - A lap is rejected if any step turns tighter than `kMinTurn` = 1.2 m.
  - **Route check (`routeClear`).** Every 0.1 m along the whole lap, and at the target itself, through `routeAt()`, the same function the live walk uses:
    - The model origin must be within 5 m of the bed.
    - The union footprint box from assets.md (x −1.359..+1.423, z −0.314..+0.573, rotated to the model's yaw) must be ≥ 0.96 + 0.2 m from the fire's centre. That is an exact box-to-point distance.
    - At a 29×10 grid over the box (≤ 0.1 m spacing):
      - `cellTop` must be on the map and within `Hero::kStep` (0.55 m) of `heightAt` where the fox stands. I check drops as well as rises: walkable both ways, as the hero treats them.
      - There must be no water deeper than 0.15 m over the ground (the hero's rule).
    - An empty `cellTop` cell inside the lawn takes `heightAt` (the cells round it); see Known limitations.
  - **Live motion.** Each leg is timed in closed form:
    - It speeds up as smoothstep over `kRamp` = 1.6 s, cruises, and slows the same way, stopping exactly at the leg's length. Legs too short for that ramp at half the time.
    - Speed = `kStride` (0.405 m, assets.md) × `kSize` / `clipDuration(Fox_Walk)` = 0.810 m/s. Nothing is hard-coded that ignores the clip.
    - Fox_Walk's phase = start phase + distance / (0.405 × 2.4), so the clip advances by ground covered.
    - Position, yaw, height and lean are pure functions of the leg clock, so a capture's single long `update` lands where 60 Hz steps do.
  - **Fade.** Pose fades from standing to Fox_Walk over the first `kWalkFade` = 1.0 s of each leg, and back over the last 1.0 s, while the speed ramps (generous, per the 43°/0.3 m first-frame difference):
    - It fades from the outgoing clip's end pose (LookAround's).
    - It fades into the next clip's first frame (LookAround or LieDown).
    - At both ends of a leg the weight is exactly 0, so legs join their neighbours pose-for-pose with no extra crossfade.
  - **Start phase (`startPhase`).** `build()` measures every 1/72 of Fox_Walk against standing (the sum of joint rotation differences). Each leg starts on the phase that makes the worse of its first and last pose nearest standing.
  - **Standing heading from the clip.** `standTurn_` is the root's turn at the end of Fox_StandUp, relative to rest: 90.000023°. Walking yaw = path heading − `standTurn_`. The lap's start heading is matched to `bedYaw_` exactly (`yawOffset`, which is ~1e-7).
  - **Pivot.** The path is steered at `pivot_`: the four paws' midpoint as it stands, from the clip, at (0.198, 0.120) m in model x/z at 2.4×. The origin is placed back from it. That is where a rigid body's turn swings the paws sideways least.
  - **Ground.** Height = `scene.ground.heightAt(origin)` every update. Lean = the ground normal (the spawn's own 4-point formula), sampled every 0.1 m along the lap. It is averaged with a ±0.6 m tent along the route, and blended from the bed's normal over the first and last 1 m. Exactly `bedTilt_` at both ends.
  - **Spawn refactor.** The spawn tilt code moved into helpers (`groundNormal`, `tiltTo`) with identical arithmetic. Spawn also records the bed placement (`bedPos_`, `bedY_`, `bedYaw_`, `bedTilt_`, `bedNormal_`). Outside a walk, `place()` restores exactly those values. Arrival at LieDown uses the bed values exactly.
  - **API.** `update(scene, dt)`, `wake(scene, seconds)`, `setFire(centre, reach)` (without it there are no walks), `forceWalk()`.
  - **Log.** Each planned walk prints one stderr line: target distance, try number, metres and seconds out and back, and when it lies down. That is useful for `--fox-wake`.
- game/src/hero.hpp, game/src/hero.cpp: `kStep` (0.55 m) moved from hero.cpp's anonymous namespace to `Hero::kStep`, so the fox uses the same step height. Same value and same uses.
- game/src/scene.hpp:
  - New `kCampfireReach` = 0.96 m: the dirt patch's maximum rim, from `addCampfire`'s 0.82 + 0.14. scene.cpp is untouched.
  - The `kRuinsFoxSpot` comment notes why walks go round the fire.
- game/src/main.cpp:
  - `--fox-walk` (`Options::foxWalk`): `fox.forceWalk()` after spawn, so the `--fox-wake` wake, or in play the first wake, walks. It implies `--fox` in captures, as `--fox-wake` does.
  - `fox.setFire(kRuinsCampfire, kCampfireReach)`.
  - `update` and `wake` now take the scene.
  - `--help` updated.
- README.md: the fox paragraph describes the walk (the lap, the check, the pass rate, speed and fades, ground following, the second seed). Also `--fox-walk` and the walking `--fox-wake` landmarks, `Fox_Walk` in the build_fox.py clip list, and the usage block.

### Verification
- **Pixel-identical to a baseline build of the pre-change working tree.** The baseline is a snapshot of HEAD plus the uncommitted rev-6 asset, built separately. Identical by byte-compare of the PNGs:
  - default; `--hero --hero-attack 0.12`; `--scene hills --fox`; `--candles --fox`;
  - `--fox` alone, and with `--time 2.6`, `--time 30`, `--time 57.3 --hour 22`, `--settle 3`, `--hide-grass --time 1.1`;
  - `--fox-walk` alone (asleep, the same as `--fox`).
  - `--fox-wake` 0, 0.9, 1.15 (`--hide-grass`), 2.5, 3 (`--hour 21`), 5, 5.6, 6.9, 7.5, 9 and 40 are also identical, because the seed keeps the first wake as before.
- **Arrival.** The route function itself (no snapping) over 2,593 accepted laps gives these errors against the bed:
  - start and end position: worst 2.4e-7 m;
  - heading: worst 4.8e-7 rad.
  - On arrival, placement switches to the exact bed values: a ~1e-7 change. Every walk join measured 0.0000–0.0004 m largest vertex step between frames (60 Hz).
- **Continuity and determinism**, on a forced walk stepped at 60 Hz (StandUp → LookAround → WalkOut → LookOut → WalkBack → LieDown → Rest):
  - No vertex jumps at any join.
  - A single `update(T)`, 60 Hz steps and random 5–55 ms steps agree to ≤ 0.23 mm and ≤ 1e-4 rad at every T sampled. This is float accumulation of the clock.
- **Paws** (contact points from assets.md, through `jointPoint`, 60 Hz, within each paw's stance window from assets.md):
  - Fully walking:
    - along the heading: 0.048 m/s mean, 0.097 max;
    - sideways: 0.133 m/s mean, 0.292 max.
  - The sideways part is the rigid body turning on a lap of radius about 1.6–2.5 m (ω × about 0.42 m). The along part is the inner and outer paws on the curve.
  - During the 1 s fades, up to 0.75 m/s. That is the 0.3 m standing-to-walk pose difference being crossfaded.
- **Route-check statistics** (20,000 uniform targets, real `layRoute` + `routeClear`):
  - 45.5% can be laid without a turn tighter than 1.2 m.
  - **13.0% are clear.** Almost all of them are west to south of the bed, in front of and beyond the fire. Targets on the pond side (east) never pass: the lap would turn too tightly, run more than 5 m out, or hit the bank.
  - Chance a walking wake finds a clear target within 24 tries: 96.4% (within 8: 67%; 16: 89%).
  - Over 4,000 simulated wakes, **48.7% walk** (3.8% of walk decisions find no route).
  - Accepted laps average 6.0 m out and 7.2 m back, about 19.4 s of walking. The tightest turn is 1.56 m.
  - Re-sampled every 5 cm: nearest the dirt patch 0.200 m, furthest from the bed 5.00 m, largest step under the body 0.540 m.
- **Cost.**
  - `planWalk`: 0.63 ms mean and 4.5 ms worst at -O2; 2.3 ms mean and 14.5 ms worst at -O0 (the Debug build). Once, at the moment a wake starts.
  - A clear lap costs about 0.33 ms to check at -O2.
- **Captures.** The forced walk's sequence (`--fox-walk --fox-wake` 5–31.5) shows it:
  - walking off toward the camera;
  - round the fire's front and west side;
  - looking around west of the fire;
  - up behind it;
  - in from behind to the bed, facing the camera, straight into lying down.

### Issues addressed
- None (cycle 1).

### How to playtest
- **Without waiting.**
  - `./build/game/pxe --fox-walk` makes the first wake (at about 28 s) take a walk: target 1, 4.6 m from the bed. It sets off about 33.5 s in and lies down about 59 s in.
  - `--fox-walk --fox-wake 0` starts it standing up at once. It sets off at 5.5 s, stops to look at about 14.3 s, sets off again at 18.3 s and lies down at 31.0 s. The game prints these times.
- **Unforced.** Plain `./build/game/pxe`:
  - The first wake (28 s) stays in its bed as before.
  - The second wake (about 71.5 s) walks: 9.0 m out to a target 2.8 m away, then 3.2 m back. It lies down 27.7 s into that wake.
- **Watch for:**
  - no turning on the spot;
  - the turn starting only as it moves off;
  - the walk fading in as it speeds up, and out as it slows;
  - the fox following the ground (it rises over the slope behind the fire) and leaning smoothly;
  - the look-around at the target;
  - the return from behind the bed, already facing the camera, then lying down with no snap and the curl exactly as before.
- **Clearance.** Q/E to rotate. The body should stay clear of the fire's stones and dirt (at least 0.2 m from the dirt patch) the whole lap.
- **Captures.**
  - `--capture a.png --fox-walk --fox-wake 10`: walking in front of the fire.
  - `--fox-walk --fox-wake 16`: looking around west of the fire.
  - `--fox-walk --fox-wake 26`: coming round behind the fire.
  - Without `--fox-walk`, the second wake starts 43.5 s after `--fox-wake 0`: `--fox-wake 55` shows it walking in front of the fire, and `--fox-wake 62` shows it looking around behind the fire.
- **Edge cases.**
  - `--fox-walk --no-fox` shows no fox.
  - `--fox-walk` alone in a capture shows it asleep, as `--fox`.
  - `--scene hills --fox-walk` shows no fox.

### Known limitations
- **Walks are laps round the fire, not out-and-back.** This is forced by the bed and the check (see the headline). The orchestrator may want to confirm with the user. Targets that pass are all west to south of the bed, so it never wanders toward the pond.
- **Paws slip on curves.** Planted paws slide about 0.13 m/s sideways on average while fully walking (up to 0.29 m/s) on the curves of the lap, and up to 0.75 m/s while the pose fades in and out.
  - The first is the rigid walk clip turning: a spine bend in proportion to the curvature would remove most of it. I didn't add one, because a bent body no longer matches the rigid footprint box the route check uses.
  - The second is crossfading the 0.3 m standing-to-walk difference; a start/stop clip would remove it.
- **`kTries` = 24** is more than "a few", because only about 1 target in 8 passes. With 8 tries only 33% of wakes would walk; with 24, 48.7% do.
- **Planning hitch.** It runs on the frame a wake starts: up to about 15 ms in the Debug build, about 4.5 ms optimised.
- **It walks over the low rock west of the fire.** The rock is about 0.2–0.3 m high, within the step. Its height follows `heightAt` under its origin, so its body rides up over the rock rather than stepping round it. The brush and legs may clip the rock edge briefly.
- **Engine bug, out of scope.** `HeightMap::build` leaves isolated empty cells where a lawn triangle's diagonal passes through a cell centre: x = z near (−10.7, −10.7), (−9.1, −9.1), (−6.3, −6.3) and (−5.1, −5.1).
  - The hero's `cellTop` treats them as walls.
  - The route check treats them as the ground round them (via `heightAt`); otherwise routes were accepted or rejected by sampling luck.
  - The hero is unchanged.
- **The lean ignores width.** It uses the spawn's 0.41 m 4-point normal, smoothed along the route, not a fit over the whole 2.8 m body. On uneven ground the long body's ends can sit a little above or into the slope.
- **`--candles` and `--extra-fires`** don't know about the walk, so a test candle or fire could stand on the lap. They are test options, unchanged so existing captures don't change.
- **The orchestrator's SecondOrder suggestion isn't used.** Smoothing the lean over distance at plan time keeps it deterministic and frame-rate independent, and makes it exactly the bed's tilt at both ends.

## Cycle 2 (stopped: no bed spot meets all the requirements)
Build: pass (incremental Debug; the working tree builds). ISSUE-001 is not fixed: the bed has not moved.

### What is in the tree
- game/src/fox.hpp, game/src/fox.cpp: the lap is replaced by the requested teardrop route generator. `kRuinsFoxSpot` is unchanged, and the comments, README and `--help` still describe the lap. At the current bed no teardrop passes the check, so **as it stands the fox never walks**; this is work in progress.
- The generator builds a Dubins-style loop:
  - out straight along the standing heading, then round a front circle through the target;
  - back round that circle, along the line touching both circles, and round a rear circle that meets the bed along the heading.
  - It picks the widest-turning loop no longer than 2d + 4 m.
  - It averages the curvature over 0.6 m, so the yaw rate ramps rather than jumps.
  - A 3-term Newton correction makes each leg land exactly (end error ≤ 1e-5 m).
  - It is laid out every 1 cm and drives both the route check and the live walk.
- `kTries` 8, `kMinTurn` 0.9 m. Everything else from cycle 1 is kept.
- The other cycle 1 changes are unchanged.

### Why no spot qualifies
**1. The length cap caps the pass rate even on open ground.**
- Leaving and coming home along the same heading means turning a whole turn. Even the shortest such loop is about 4.3 × the turn radius longer than 2d.
- Over uniform 2–5 m targets, the fraction that any loop can serve within 2d + 4 m is:

| Minimum turn radius | Fraction of targets |
|---|---|
| 1.2 m | 8% |
| 1.0 m | 19% |
| 0.9 m | 29% |
| 0.8 m | 34% |
| 0.6 m | 38% |

- No radius serves more than 50%: targets behind the bed can never be served.
- The generator as built lays a loop for 20% (0.9 m), 35% (0.7 m) or 37% (0.5 m) of targets.
- With 8 tries, even perfectly open ground caps walking wakes at about 0.5 × (1 − 0.8⁸) = 42% at 0.9 m.

**2. The bed rules leave one small pocket.**
- Method: the same per-vertex search as fox-wake-look, extended to all five clips. Every vertex is sampled every 1/60 s with the spawn tilt, at exact point-to-triangle distance against every ruins.glb node except Ground, plus the game-built campfire. The grid is 0.25 m, within 45° of +x of the fire, 1.5–5 m from it.
- Spots ≥ 0.2 m clear exist only on the lawn north-east of the fire, between the bank and the paving: x about −3.75..−0.75, z −7..−9.5.
- Almost all of the pond side fails, because the bed would overlap or touch paving: Paving.020, .026, .030 and .014 are the limiting objects.
- Of the 87 clear spots, most fail the route check while standing in the bed itself: the bank rises more than 0.55 m under the brush.

**3. The best spots in that pocket fall well short.**
- Turn radius 0.9 m, 8 tries:

| Spot | Targets clear | Wakes walking |
|---|---|---|
| (−2.0, −7.0) | 5.9% | 19% |
| (−1.75, −7.25) | 4.9% | 17% |
| (−2.75, −6.75) | 4.6% | 16% |

- Passing targets are only ahead or ahead-left; none to the right (the fire's side) or behind.
- At (−2.0, −7.0), of the routes that could be laid, the first failing check was the fire clearance for 338 and the bank for 76; 194 were clear.
- Changing the turn radius or the cap barely helps:
  - 0.7 m radius: 23–25% walking;
  - 0.5 m radius: 21–24% walking;
  - a 2d + 6 m cap: 4–20% walking (terrain-limited).

**4. Dropping the scene-object rule doesn't reach half either.**
- Requiring the bed to clear only the fire, the best spot in the camera-facing sector is (−0.5, −7.5), 4.6 m from the fire.
- There, 10.6% of targets are clear, so 30% of wakes walk.
- Its passing targets spread from ahead-right to ahead-left.

### Options needing a decision (numbers at (−2.0, −7.0) unless noted)
- **Draw targets only from the half ahead of the bed.** Targets behind can never meet the cap. This roughly doubles the per-target pass rate: about 32% of wakes walk with 8 tries, about 43% with 16.
- **More tries.** 24 tries give about 38%.
- **Relax the bed's scene-object clearance for flat paving.** About 30% at (−0.5, −7.5).
- **Accept fewer walks.** About 19% of wakes at (−2.0, −7.0) under the current rules.
- **A turn-on-the-spot clip.** Without the whole-turn loop, the cap is easy to meet.

## Cycle 2 (completed, after the user accepted fewer walks)
Build: pass. Fresh Debug directory with its own FetchContent deps: `cmake -S . -B <fresh> -DCMAKE_BUILD_TYPE=Debug && cmake --build <fresh> --parallel`, exit 0. The only warnings are the existing stb `sprintf` deprecation and the duplicate-libglfw linker note. The repo's build/ was only built incrementally; its configuration was not touched.

### Changes
- game/src/scene.hpp:
  - `kRuinsFoxSpot` moves (-3.3, -6.6) → **(-2.0, -7.0)**.
  - The comment now records the spot, its facing and margins, and why walks go ahead and to the left. The hemmed-in/lap wording is gone.
- game/src/fox.hpp, game/src/fox.cpp: the lap is replaced by the teardrop out-and-back (built in the stopped part of this cycle, above).
  - **Shape:** straight out along the standing heading, then onto a front circle through the target. Back round that circle, along the line touching both circles, and round a rear circle that meets the bed from behind along the heading.
  - **Selection:** of the loops no longer than 2d + 4 m (`kSpare`), the one whose tightest turn is widest. A 0.9 m minimum (`kMinTurn`).
  - **Smoothing:** curvature is averaged over `kEase` = 0.6 m, so the yaw rate ramps and never jumps.
  - **Landing:** three sine terms per leg, solved by Newton, land each leg exactly on the target and then on the bed (position and heading).
  - **Layout:** laid every 1 cm. `routeAt()` reads that table for both the route check and the live walk.
  - **Unchanged rules:** p = 0.5 per wake; up to `kTries` = 8 targets uniform over the 2–5 m ring (256 for `--fox-walk`); every check from cycle 1 (footprint, step, water, off-map, fire, 5 m); the separate walk RNG; phase by distance; ramps and fades; ground follow; exact bed placement on return.
  - **`kWalkSeed` 20261024 → 20261022.** At the new bed the old seed's first walk would have been on wake 8. With 20261022, wakes 1–12 go: stay, walk, stay, walk, stay, stay, stay, walk, stay, walk, stay, stay. Wake 1 does draw a walk, but none of its 8 targets is clear, so it stays put exactly as before.
- game/src/main.cpp: the `--fox-walk` help says "short walk out and back… about one wake in five". The spawn comment no longer says the walks go round the fire.
- README.md: the fox paragraph now covers:
  - the new place (about 3 m from the fire, behind it, between the bank and the paving);
  - the teardrop out-and-back with no turn on the spot;
  - the 8 tries, the 2d + 4 m cap and the 0.9 m turn;
  - about one target in fifteen passing (ahead or to its left only), so about one wake in five walks;
  - walks of about 7–11 m;
  - the first unforced walk on the second wake (about 71.5 s into play).

### Bed verification (the per-vertex method from fox-wake-look, now with all five clips)
- **Method.** Every one of the 1984 skinned vertices, at 2.4×, with the spawn tilt, every 1/60 s through Rest, StandUp, LookAround, LieDown and Walk (772 poses). Exact point-to-triangle distance to every ruins.glb node except Ground, plus the campfire rebuilt with the game's `addCampfire` (dirt patch, stones, logs).
- **Facing.** The curl faces the fire (yaw −71.6°). Standing, it faces (0.316, 0.949): **18.4° off straight at the yaw-180 camera**. It is 3.162 m from the fire.
- **Margins:**
  - Paving.026: **0.204 m** (LieDown 1.22 s);
  - Paving.030: 0.309 m (LieDown 1.55 s);
  - the fire's dirt patch, stones and logs, and every other object: more than 1.0 m.
- **Ground.** Standing paws are 10 mm (hind) and 16 mm (fore) over `heightAt` (9 mm on flat ground). The curl's underside is 0.001–0.102 m over it. Nothing goes below the ground (deepest −0.000 m). So the spawn tilt suits it.
- **Grass clearing.** It follows the spot: main.cpp passes `kRuinsFoxSpot` as `foxSpot`; 20 sprites are dropped at the new spot.

### Route statistics (real `layRoute` + `routeClear`, 20,000 uniform targets)
- **Pass rate.**
  - 20.4% of targets can be laid within the cap and turn limit.
  - **6.6% are clear.**
  - **All clear targets are ahead-left (0–45° left of the heading: 512) or left (45–90°: 817).** None are ahead-right, right or behind: the fire is to its right, the bank behind.
- **Walk rate.** **19.6%** of 4,000 simulated wakes walk. 8 tries find a clear target for 41% of the wakes that want a walk.
- **Outing length.** Mean 9.25 m (3.75 out, 5.53 back), min 6.69 m, **max 11.13 m**. It never exceeds 2d + 4 (the largest margin used is 4.00 m). About 14.7 s of walking.
- **Turns.** Tightest 0.91 m (limit 0.9).
- **Re-sampled every 2.5 cm on accepted walks:**
  - nearest the dirt patch 0.988 m;
  - furthest from the bed 3.46 m;
  - largest step under the body **0.577 m**. That is 2.7 cm over the 0.55 m limit, between the 10 cm samples the check uses: the step is measured against the ground under the fox, which moves between samples. Checking every 5 cm didn't change the figure, so I kept 10 cm.
- **Cost.** `planWalk` takes 0.35 ms mean and 2.3 ms worst (-O2).

### Arrival, joins, slip
- **Arrival.** The route's end against the bed, over 1,253 accepted walks: **position ≤ 1.3e-5 m, heading 0** (mod 2π; the start yaw is matched to the bed's exactly). Placement then switches to the exact bed values.
- **Joins.** On a forced walk at 60 Hz, the largest vertex step between frames at each join is:

| Join | Step |
|---|---|
| LookAround → WalkOut | 0.0004 m |
| WalkOut → LookOut | 0.0002 m |
| LookOut → WalkBack | 0.0000 m |
| WalkBack → LieDown | 0.0020 m (the first frame of LieDown's own motion) |

- **Determinism.** A single long `update`, 60 Hz steps and random steps agree to ≤ 0.11 mm and ≤ 1.1e-4 rad.
- **Paw slip.** Planted contact points (stance windows from assets.md), fully walking:
  - along the heading: 0.081 m/s mean, 0.112 max;
  - sideways: 0.225 m/s mean, 0.537 max.
  - Sideways is higher than cycle 1 (0.133 / 0.292) because the teardrop turns as tight as 0.9 m.
  - In the 1 s fades: up to 0.77 m/s.

### Capture comparisons
Three builds:
- the pre-task baseline (snapshot build in its own scratch directory);
- this tree;
- this tree with only `kRuinsFoxSpot` set back to (-3.3, -6.6), built in its own directory with its own deps.

| Captures | This tree vs baseline | Old-spot build vs baseline |
|---|---|---|
| Without the fox: default, `--hero --hero-attack 0.12`, `--scene hills --fox`, `--candles`, `--hour 22 --time 5` | identical | identical |
| `--fox` with: nothing, `--time 2.6`, `--time 30`, `--time 57.3 --hour 22`, `--settle 3`, `--hide-grass --time 1.1`, `--candles` | differ (bed move) | identical |
| `--fox-wake` 0, 2.5, 5, 6.9, 7.5, 9, 40 | differ (bed move) | identical |

### Issues addressed
- ISSUE-001: walks are short teardrop out-and-backs from the moved bed, not laps round the fire.

### How to playtest
- **Plain play** (`./build/game/pxe`):
  - The fox sleeps right of and behind the fire, next to the near pillar, facing the fire.
  - Wake 1 (about 28 s) stays in its bed, as before; stderr says none of its 8 walks was clear.
  - **Wake 2 (about 71.5 s) walks:** 3.5 m out, a look around, 5.8 m back. It lies down about 24 s after it starts waking.
- **Forced:** `--fox-walk` makes the first wake walk (target 19 of up to 256). With `--fox-walk --fox-wake 0`: it sets off at 5.5 s, stops at about 10.5 s, sets off again at 14.5 s and lies down at 23.5 s.
- **Watch for:**
  - it leaving straight on, curving left onto its target, and stopping to look;
  - a teardrop curl back that comes in behind the bed;
  - no turn on the spot and no snap into Fox_LieDown;
  - the walk fading in and out with the speed;
  - the lean following the ground (it walks onto the paving between the pillars).
- **Camera.** **From the default view the near pillar hides most of the walk.** Press Q/E to rotate: from yaw 225 or 270 the whole walk across the paving between the pillars is visible. Check its outline stays clear of the fire's stones.
- **Captures.**
  - `--capture a.png --fox-walk --fox-wake 12 --yaw 225`: looking around at its target.
  - `--fox-walk --fox-wake 18 --yaw 270`: on the way back.
  - Without `--fox-walk`, the second wake starts 43.5 s after `--fox-wake 0`; `--fox-wake 52 --yaw 225` shows it walking.

### Known limitations
- **Few walks, one way.** About one wake in five walks, always ahead or to its left. This is the user's accepted outcome.
- **Hidden by the pillar.** At the default camera most of the walk is behind the near pillar; it's visible after rotating (Q/E).
- **Tight clearance.** Paving.026 is 0.204 m from the lying-down fox, just over the 0.2 m rule.
- **Step sampling.** Between the 10 cm route samples, the step under its body can reach 0.577 m against the 0.55 m rule.
- **Paw slip on turns.** Sideways slip while turning is up to 0.54 m/s (0.9 m turns, rigid walk clip, no spine bend), and up to 0.77 m/s during the standing/walk fades.
- **Unchanged from cycle 1:** the planning cost on the wake's first frame (now ≤ about 2.3 ms optimised); the engine's empty heightmap cells along x = z; `--candles`/`--extra-fires` ignoring the walk.
