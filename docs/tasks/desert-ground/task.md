# The desert's ground after the concept art
Type: feature
Status: complete
Cycle: 1

## Goal
Rework the desert's ground toward `desert/docs/concept/wanderers.png`:
- shadowed dune faces that stay warm and sandy, with a clear light/shadow edge along the crests;
- wind ripples near the player that fade with distance, aligned with the dunes' wind;
- taller, more varied dunes with sharper downwind faces, so silhouettes layer into the haze;
- a golden-hour palette of warm beige-ochre lit faces against a pale, dusty sky.

The shading controls go in the settings menu. A `--terrain-report` flag proves the palm and spawn constraints still hold.

## Decisions
All from the planning interview with the user, 2026-10-30.
- **Warm shadows** come from the desert's light values, with no engine change. These replace `desert-lighting`'s starting fill colours and bounce strengths. sRGB colour × strength:

  | | day | golden | twilight | night |
  |---|---|---|---|---|
  | sky fill | (170,180,200) × 0.30 | (200,175,160) × 0.30 | (150,125,140) × 0.22 | engine default (70,85,130) × 0.12 |
  | bounce strength | 0.20 | 0.18 | 0.05 (as `desert-lighting`) | 0.03 (as `desert-lighting`) |

  The bounce colours stay as `desert-lighting` set them.
- **The crest edge:**
  - the fine grid goes from 1 m to **0.5 m** cells over the middle 240 m (`kFineCell` 0.5);
  - normals come from `duneHeight`'s exact slope (analytic gradient), not a ±0.5 m average;
  - sand gets terminator sharpness **0.35** (`Scene::terminator`, from `perspective-surface-detail`).
- **Ripples** on `Sand`: wavelength 0.25 m, depth 0.35, heading the main layer's wind (200°), fade from 6 m to 20 m, absent above 22° slope.
- **Dunes** (`kRidges`), four layers:

  | Layer | Heading | Spacing | Height | Windward share |
  |---|---|---|---|---|
  | main | 200° | 110 m | 6.5 m | 0.75 |
  | second | 214° | 48 m | 2.4 m | 0.75 |
  | small | 186° | 26 m | 0.8 m | 0.75 |
  | long hills (new) | 196° | 260 m | 5.0 m | 0.75 |

  Each layer's swell ranges over 50–100% of its height (was 70–100%). Meander, meander length and swell length stay as today for the three existing layers. The long hills use meander 0.08, meander length 400 m, swell length 500 m and seed 3. Crest to trough comes to about 4–12 m. The play area, ground size, coarse cell and far plane don't change.
- **The palm and spawn stay**: the palm at the origin, the spawn at (0, −25) facing it. Only the four layers' `phase` values may be tuned, until every constraint holds:
  - **palm slope:** the ground at the palm is at most 15° steep, and rises away from the spawn (windward). That is, the gradient at (0, 0) dotted with the unit direction from the palm toward the spawn is ≤ 0;
  - **dune behind:** along the line from the palm directly away from the spawn, between 5 and 60 m, the ground somewhere rises at least 2 m above the palm's base;
  - **sight line:** from the spawn eye (ground + 1.7 m at (0, −25)) to the point 0.5 m up the trunk (palm base + 0.5 m at (0, 0)), the ground stays below the line at every 0.25 m sample, by at least 0.1 m.
- **`--terrain-report`** prints those three constraints with ok/FAIL and the dune stats, and exits 1 on any FAIL. It needs no window or GL: it reads `duneHeight` and `groundHeight` only. It's listed in `--help`.
- **Palette:**
  - sand albedo (212,180,136), set by the desert (the engine default stays (214,176,118));
  - golden sky horizon (236,212,176) and zenith (150,160,175);
  - day horizon (226,214,190) and zenith (150,170,190).
- **Menu "Ground" section** (shading only, all live): the sand albedo colour picker, terminator sharpness, and every ripple value (wavelength, depth, heading, fade start, fade end, slope cutoff). "Print values" adds them. Dune shapes stay constants.
- **Art-style doc:** the Terrain section and the sand palette bullet are updated: ripples are now in, and the dunes taller.

## Layering
- Engine: none. It uses `Scene::terminator` and `Scene::ripples` from `perspective-surface-detail`, plus the light and sky fields.
- Game (`desert`):
  - `world.cpp`: dunes, grid, normals, values;
  - `main.cpp`: `--terrain-report`;
  - `settings.cpp`: the Ground section and print lines;
  - `art-style.md`.

## Approach
1. **`desert/src/world.hpp`.** `kFineCell = 0.5f`, and update its comment ("a 0.5 m grid"). `kFineHalf`, `kGroundHalf` and `kCoarseCell` stay; the coarse stitch's `ratio` becomes 8. Update the top comment: dunes 4–12 m.
2. **`desert/src/world.cpp`.**
   - `kRidges` holds the four layers from Decisions. The swell line becomes `0.75f + 0.25f * std::sin(...)`. Rewrite the comment above `kRidges` with the chosen phases and the constraints they meet.
   - Replace `duneNormal`'s finite difference with the analytic gradient (binding math). Per layer, with wind `w = (sin heading, cos heading)` and crest `c = (w.z, −w.x)`:
     ```
     a = across = x·w.x + z·w.z,   s = along = x·c.x + z·c.z
     u = a/λ + m·sin(2π s/L + 1.3 seed + 0.4) + phase   (wrapped to 0..1)
     ∂u/∂a = 1/λ,   ∂u/∂s = m·(2π/L)·cos(2π s/L + 1.3 seed + 0.4)
     swell = 0.75 + 0.25·sin(2π s/S + 2.1 seed + 1),   ∂swell/∂s = 0.25·(2π/S)·cos(...)
     P(u): windward (u < k): t = u/k, P = t²(1.5 − 0.5t), P′ = (3t − 1.5t²)/k
           downwind:          v = 1 − (u − k)/(1 − k), P = v², P′ = −2v/(1 − k)
     ∂h/∂a = H·swell·P′·∂u/∂a,   ∂h/∂s = H·(∂swell/∂s·P + swell·P′·∂u/∂s)
     ∂h/∂x = ∂h/∂a·w.x + ∂h/∂s·c.x,   ∂h/∂z = ∂h/∂a·w.z + ∂h/∂s·c.z
     normal = normalize(−Σ∂h/∂x, 1, −Σ∂h/∂z)
     ```
     Add a file-local `glm::vec2 duneGradient(float x, float z)` beside `duneHeight` and use it in `duneNormal`. Drop `kNormalStep`, and update the comments that mention the toon ramp.
   - In `buildWorld`, after the existing light and sky setup:
     - the sand albedo;
     - the fill colours and strengths and the bounce strengths from Decisions (overwriting `desert-lighting`'s values for those);
     - the four sky colours;
     - `scene.terminator[pxe::kSurfSand] = 0.35f`;
     - `scene.ripples = {pxe::kSurfSand, 0.25f, 0.35f, 200.f, 6.f, 20.f, 22.f}`.

     Give them a short comment block naming the concept art.
3. **`desert/src/main.cpp`.**
   - `--terrain-report` in `Options`, `parseArgs` and `printUsage`: `"  --terrain-report         print the dunes' stats and check the palm and spawn constraints, then exit\n"`. It can't combine with `--capture`, `--window-png`, `--menu` or `--walk`; any of them is a bad argument.
   - It runs before the window is made. It prints exactly these lines (binding labels; numbers `%.2f`), then returns 0, or 1 on any FAIL:
     ```
     terrain: play area height 0.00 .. 0.00 m (crest to trough 0.00 m)
     terrain: steepest slope in play area 0.00 deg
     terrain: palm slope 0.00 deg, rises away from spawn: ok|FAIL
     terrain: dune behind palm: rises 0.00 m within 5-60 m: ok|FAIL
     terrain: sight line from spawn eye to palm: clears by 0.00 m: ok|FAIL
     ```
   - Height range and slope: `duneHeight` and `duneGradient` sampled every 0.5 m over the play area (|x|, |z| ≤ `kPlayHalf`). `duneGradient` needs declaring in `world.hpp` for this.
   - The sight line uses `groundHeight` every 0.25 m.
4. **`desert/src/settings.cpp`.**
   - Section **"Ground"** after "Sky and dust":
     - `ColorEdit3("Sand", &scene.albedo[pxe::kSurfSand].x)`;
     - "Terminator sharpness" (0–1);
     - "Ripple wavelength" (0.05–1 m, logarithmic);
     - "Ripple depth" (0–1);
     - "Ripple heading" (0–360°);
     - "Ripples full to" (0–30 m);
     - "Ripples gone by" (1–60 m, kept at least 1 m past "full to");
     - "Ripple slope cutoff" (0–45°).

     The disabled text: "Dune shapes are fixed in world.cpp".
   - "Print values" appends `scene.albedo[pxe::kSurfSand]`, `scene.terminator[pxe::kSurfSand]` and every `scene.ripples` field, as C++ statements in the existing style.
5. **`desert/docs/art-style.md`.** Make exactly these replacements (the text as `desert-lighting` leaves it):
   - The Terrain section's two bullets
     > - The dunes are generated in the game's code, not in Blender: long, smooth, wind-aligned ridges with a sharper crest on the downwind face, built on a 1 m grid with smooth normals, so the light falls off continuously across each slope's curve.
     > - No ripples, rocks or scrub until a task adds them.

     become
     > - The dunes are generated in the game's code, not in Blender: wind-aligned ridges in four layers, 4–12 m crest to trough, each with a long windward slope up to a sharp crest and a steep downwind face, plus long low hills that layer the far silhouettes into the haze. Built on a 0.5 m grid with exact normals, and sand's terminator sharpened, so the light falls off continuously across each slope but turns to shadow in a clear line along the crests.
     > - Shadowed faces stay warm and sandy: the desert's sky fill is warm and the sand bounces its own warmth back up. Never a muddy mauve.
     > - Wind ripples (shading only, about 0.25 m apart) cross the sand near the player, aligned with the main dunes' wind, fading out by 20 m and absent on steep downwind faces. No rocks or scrub until a task adds them.
   - In "Palette", the Sand bullet's opening
     > - **Sand:** a warm ochre albedo, about sRGB (214, 176, 118) (`Scene::albedo[kSurfSand]`, the engine's default).

     becomes
     > - **Sand:** a warm beige-ochre albedo, about sRGB (212, 180, 136), set by the desert (`Scene::albedo[kSurfSand]`).

     The rest of that bullet stays.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `build/release/desert/pxe_desert --terrain-report` exits 0, every constraint line reads ok, and crest to trough is between 4 and 12 m. Paste the output into the handoff. `--terrain-report --capture x.png` exits 1 with "bad argument".
3. `git diff <cycle base> -- engine game examples`: no output. `tools/ortho-check.sh <cycle base>`: 6/6. `tools/perspective-check.sh <cycle base>`: `example-first-person` identical, desert cases differ.
4. Captures into `docs/tasks/desert-ground/`, each rendered twice and `cmp`-identical:
   - `pxe_desert --capture ground-1630.png`
   - `--hour 18 --capture ground-golden.png`
   - `--hour 12 --capture ground-noon.png`
   - `--hour 7 --capture ground-morning.png`
   - `--hour 21 --capture ground-night.png`
   - `--walk 4 --capture ground-walk.png`

   Describe each against the concept art in the handoff:
   - shadowed faces warm and sandy, not mauve;
   - crisp shadow lines along the crests;
   - ripples near the eye, fading by 20 m and absent on downwind faces;
   - taller dunes layering into the haze;
   - the palm visible from the spawn with a dune rising behind it;
   - a pale, dusty golden sky.
5. `git grep -n kNormalStep -- desert`: no output. `git diff <cycle base> -- desert/docs/art-style.md` shows exactly the two replacements.
6. By reading the code:
   - `duneGradient` implements the binding math;
   - `groundHeight` still matches the drawn triangles at the new `kFineCell`, because both read the same grid and diagonal;
   - the Ground section and print lines are as specified.
7. Playtest checklist for the user, in the handoff:
   - walk over several crests and confirm the eye follows the taller dunes smoothly;
   - ripples appear and fade as you move;
   - drag every Ground control and see it take effect at once;
   - compare golden hour with the concept art.

## Acceptance criteria
- [x] `kRidges` has the four binding layers. Only the phases differ from the table, and they're chosen so `--terrain-report` passes. The swell is 50–100%. `kFineCell` is 0.5. Normals come from the analytic gradient. (world.cpp `kRidges`: table values, phases 0.875/0.30/0.55/0.25; swell 0.75+0.25 sin; world.hpp `kFineCell` 0.5; `duneNormal` uses `duneGradient`)
- [x] `buildWorld` sets the Decisions' palette, fill, bounce, sky, terminator and ripple values. (world.cpp `buildWorld`, block after the bounce colours)
- [x] `--terrain-report` prints the binding lines, checks the three constraints, needs no window, and is listed in `--help`. (main.cpp `terrainReport`, run before the window; `printUsage`; orchestrator ran it: exit 0, all ok, crest to trough 9.38 m)
- [x] The Ground section and print lines are as specified. (settings.cpp "Ground" section after "Sky and dust"; `printLighting` appends albedo, terminator and every ripples field)
- [x] `desert/docs/art-style.md` has exactly the two replacements. (`git diff c5d362b -- desert/docs/art-style.md`: the two replacements only)
- [x] Verification 2–6 pass. (orchestrator: --terrain-report exit 0, combined with --capture exit 1 "bad argument"; engine/game/examples diff empty; ortho 6/6; perspective example identical, desert cases differ; no kNormalStep; six captures twice-identical per handoff; duneGradient matches the binding math line by line; groundHeight reads kFineCell)
- [x] Project builds cleanly from a fresh build directory with CMake (orchestrator check build: only the stb sprintf warning)

## Assets
None.

## Out of scope
- Engine changes; live dune editing; rocks, scrub or footprints; moving the palm or the spawn; changing the play area or the far plane.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base c5d362b
- Cycle 1 review, 2026-10-30: build clean, Verification 2-6 pass, code matches the spec; no issues. Observations for the user (spec outcomes, not defects): `--hour 18` is mostly twilight under the engine's phase weights, so ground-golden.png is dark brown under a lilac sky, and the golden values read best around 17-17.6 h; from the spawn the layered silhouettes are weak (one crest behind the palm), clearer in ground-walk.png; ripples show some moire banding at 4-12 m; the palm constraint's "windward" wording contradicts its numeric test (the face seen from the spawn is the downwind face), the numeric test was followed.

## Playtest checklist
- Walk over several crests (behind the palm, up the crest about 22 m behind it and beyond): the eye follows the taller dunes smoothly, no jitter at crest edges.
- Ripples appear near the eye and fade out between 6 and 20 m as you move, not on steep faces; check for moire banding at mid-distance.
- Tab, "Ground": drag every control (Sand, Terminator sharpness, wavelength, depth, heading, full to, gone by, slope cutoff) and see it take effect at once. Drag "Ripples full to" past "gone by": the other is pushed 1 m clear.
- Lighting's "Print values": the terminal also shows the `scene.albedo`, `scene.terminator` and `scene.ripples.*` lines.
- Set the hour to about 17.6 and compare with desert/docs/concept/wanderers.png; try 7, 12 and 18 too (18 reads as twilight, not golden).
- Shadowed faces stay warm and sandy, never mauve; crest edges read as a clear light/shadow line.
- `pxe_desert --terrain-report` prints five lines, all ok, and exits 0.
