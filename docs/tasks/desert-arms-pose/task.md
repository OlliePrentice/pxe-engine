# Arms: pointing forward, closer to the body, chunkier gloves
Type: feature
Status: open
Cycle: 1

## Goal
Re-pose the desert player's arms (`desert/tools/blender/build_arms.py`, `desert/assets/characters/arms.glb`) toward `desert/docs/concept/wanderers.png`:
- **Pointing:** the hands point mostly forward, with a firm, almost straight wrist, palms facing each other and thumbs up as if loosely holding something.
- **Placement:** the arms sit closer to the body and a little higher, so less forearm shows.
- **Gloves:** 15% chunkier.

The gauge and the compass move toward the hand until they are back in view. The walk clips are regenerated from the new rest pose with today's motion, the sprint's lift and bob lowered just enough to keep it in frame; the motion itself is reworked in `desert-arms-motion`. The gear and palette are `desert-arms-gear`.

## Decisions
All from the planning interview with the user, 2026-10-30, revised by the unblock of replan 1 the same day (see Spec revisions). Every value below was run through the build's whole `check()` and `clip_checks()` in the unblock trial, and all pass.
- **Pointing:** `HAND_YAW` goes from 16.0 (inward) to **−8.0** (8° outward), and `HAND_PITCH` from 16.0 to **4.0** (a firm, almost straight wrist). `HAND_ROLL` stays 90.0: palms facing, thumbs up. With palms facing, the fingers still curl toward the other hand, and in a first-person view a forward-pointing hand converges toward the screen centre. The planning trial showed that this yaw and pitch read most forward.
- **Placement:** `WRIST` goes from (0.215, −0.370, −0.155) to **(0.200, −0.330, −0.150)**: 4 cm closer, 0.5 cm higher, 1.5 cm inward. The planned z −0.140 was lowered by the agreed walk fallback (below). `SHOULDER`, `CLAVICLE`, `UPPER_LEN`, `FORE_LEN` and `ELBOW_SWING` don't change.
- **Chunkier gloves:** every glove width and thickness grows by 1.15. That covers `PALM`'s half width and its dorsal and volar heights, each finger's base width and thickness in `FINGERS`, and `THUMB_W` and `THUMB_T`. Lengths, curls, splays and `palm_front` reaches don't change. **`PALM`'s first (wrist) ring is spared from the bulk**, so the glove tapers into the cuff (user, unblock: at 1.15 the wrist ring poked out of the cuff at any cuff size up to 1.20). **The cuff keeps its shipped size** (no cuff bulk; user, unblock: at 1.10 it hid gauge readout dots, and with the taper it no longer needs to grow). The stitch rows follow the wider gloves; the 12 triangles that adds are accepted (user, unblock).
- **The gauge back into view:** at the new placement the gauge's readout leaves the bottom of the view. The agreed search (`GAUGE_S` toward the wrist in 5 mm steps from −0.0535 to −0.035, then `GAUGE_TURN` from 0.55 up) was run in the unblock trial: the first passing pair at wrist z −0.150 is **`GAUGE_S` −0.035, `GAUGE_TURN` 0.55**. These are fixed constants; the build has no search.
- **The bracer follows the gauge by extending its plate** (user, unblock): at `GAUGE_S` −0.035 the toggles sit past the plate's wrist end, so `BRACER_S`'s wrist end moves from −0.040 to **−0.020**, the first 5 mm step at which every check passes. The plate's elbow end, the straps, canister, coil and cables don't move. The plate grows from 0.120 to 0.140 m; its wrist end reaches under the cuff.
- **The compass slides toward the hand** (user, unblock: at the new wrist about a third of its face was below the bottom edge). `COMPASS_STRAP` moves along the cuff in 5 mm steps, all three values together, until every check passes: the first passing step is **+0.020 m**, so `COMPASS_STRAP` goes from (−0.0355, −0.031, −0.0265) to **(−0.0155, −0.011, −0.0065)**. `COMPASS_TURN` and the mount angle don't change.
- **Walk fallback:** the agreed fallback (the wrist's z lowers in 5 mm steps until every walk frame stays at or below NDC y −0.30) was run in the unblock trial: `Arms_Walk` first passes at **z −0.150** (top −0.314). `Arms_Walk`'s values stay as shipped.
- **The sprint is damped now** (user, unblock: at every allowed wrist z its top broke −0.30, at −0.217 for z −0.150). Its `lift` and then its `bob` were lowered in 5 mm steps until every clip check passed: **`Arms_Sprint` `lift` 0.015 → 0.0, `bob` 0.025 → 0.020** (top −0.313), and `CLIP_TARGETS["Arms_Sprint"]` follows: `mean` 0.0, `bob` 0.020. Every other sprint value (cycle, swing, elbow, wrist, fist) stays. `desert-arms-motion` replaces the clips next anyway.
- **Every other check** in the build stays binding, with today's values: framing, `KNUCKLE_Y` (−0.70, −0.40), `HAND_X` (0.40, 0.60), clearance 0.15 m, limbs leaving through the bottom edge, compass, readout, small parts, attachment in the rest and test poses, covered, closed, crease, clip checks, and glb read-back.
- **Materials, joints, clips' names and lengths, and slot order stay as shipped.**
- **The values are fixed constants.** The build keeps no gauge, wrist, compass or sprint search: the trial ran them, and `docs/tasks/desert-arms-pose/scripts/build_arms-unblock.patch` holds the result. If the build at these values fails a check, the task stops as blocked.

## Layering
- Engine: none.
- Game: none. `desert::Arms` loads whatever the glb holds, and the joints and clips keep their names.
- Assets: `build_arms.py` rebuilds `arms.glb` and its previews.

## Approach
Designer only. `docs/tasks/desert-arms-pose/scripts/build_arms-unblock.patch` (binding) is the whole change to `desert/tools/blender/build_arms.py` apart from the docstring; it applies to this branch's file at the cycle base (`git apply --check` passes). It passed every check headless (`PXE_SKIP_EXPORT=1`). The cycle 1 patch (`build_arms-pose.patch`) and its `choose_gauge` / `build_checked` searches are superseded: don't apply it. What the patch does:
1. **Constants:** `HAND_YAW = -8.0`, `HAND_PITCH = 4.0`, `WRIST = v(0.200, -0.330, -0.150)`, `BRACER_S = (-0.160, -0.020)`, `GAUGE_S = -0.035` (`GAUGE_TURN` stays 0.55), `COMPASS_STRAP = (-0.0155, -0.011, -0.0065)` (its comment now says "the post's line", since the middle value is no longer a cuff ring), `CLIPS["Arms_Sprint"]` `bob=0.020, lift=0.0`, `CLIP_TARGETS["Arms_Sprint"]` `mean=0.0, bob=0.020`.
2. **The glove scale.** `GLOVE_BULK = 1.15` beside the hand constants, applied where the widths are read: in `Side`'s finger frames (`w0`, `t0`), in `gloves()`'s `PALM` loop for every ring but the first, and to `THUMB_W` / `THUMB_T` in the thumb's `_segments` call. The tables keep their shipped numbers. No cuff bulk.
3. **The module docstring** (not in the patch): the held pose's numbers (wrists 0.33 m ahead, hands pointing slightly outward, a firm wrist, gloves chunkier and tapering into the cuff), and the clips paragraph's "Arms_Sprint lifts the hands": the sprint no longer lifts them; it bobs 20 mm, swings wider and curls the fists.
4. Rebuild headless (`--python-exit-code 1`) and keep the new previews: the asset changed. Build step by step in the user's live Blender through the MCP first, so the user can watch, as `desert/docs/art-style.md` says.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py` exits 0 with every check passing. Run it twice into `PXE_DESERT_OUT` scratch directories: the two `arms.glb` are `cmp`-identical.
3. `python3 tools/glb.py info desert/assets/characters/arms.glb`: 15 materials as shipped; 39 joints; `Arms_Walk` and `Arms_Sprint`, 1.0 s, 31 LINEAR keys.
4. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: `example-first-person` identical, and both desert cases differ.
5. `git diff <cycle base> -- engine game examples desert/src`: no output.
6. Captures into `docs/tasks/desert-arms-pose/`, each rendered twice and `cmp`-identical:
   - `pxe_desert --capture pose-day.png`
   - `--hour 18 --capture pose-golden.png`
   - `--walk 2 --capture pose-walk.png`
   - `--walk 3 --sprint --capture pose-sprint.png`

   Compare with `docs/tasks/desert-arms-revision/revision-day.png` in the handoff:
   - hands point more forward, palms facing, thumbs up, wrists straighter;
   - less forearm shows;
   - gloves visibly bulkier;
   - the gauge's readout and the compass face wholly in view and readable.
7. assets.md records the measured framing, readout, compass and clip values next to the trial's expected values below.
8. `git diff <cycle base> -- desert/tools/blender/build_arms.py` matches `build_arms-unblock.patch` apart from the docstring: `git apply --check -R docs/tasks/desert-arms-pose/scripts/build_arms-unblock.patch` passes on the result.

## Acceptance criteria
- [ ] `HAND_YAW` −8.0, `HAND_PITCH` 4.0, `HAND_ROLL` 90.0. `WRIST` is (0.200, −0.330, −0.150). `GLOVE_BULK` 1.15 is applied as Approach §2 says, sparing `PALM`'s first ring; `CUFF` is applied unscaled.
- [ ] `GAUGE_S` −0.035, `GAUGE_TURN` 0.55, `BRACER_S` (−0.160, −0.020), `COMPASS_STRAP` (−0.0155, −0.011, −0.0065), as constants with no search in the build.
- [ ] `Arms_Sprint` has `lift` 0.0 and `bob` 0.020 in `CLIPS`, and `mean` 0.0 and `bob` 0.020 in `CLIP_TARGETS`; every other `CLIPS` and `CLIP_TARGETS` value is as shipped.
- [ ] Every existing build check passes with today's values. Materials, joints, clip names and lengths, and key counts are as shipped.
- [ ] Verification 2–8 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms** (revised): `desert/assets/characters/arms.glb`, rebuilt by `desert/tools/blender/build_arms.py`, with all six previews rewritten.
  - Format, units, axes, origin, rig (39 joints), materials (15, as shipped), clips: unchanged in kind.
  - Budget: 12,000–20,000 triangles, as today (the gear task raises it). The unblock trial built 16,396. The stitch rows following the bulk add 12; accepted.
  - Animations: `Arms_Walk`, `Arms_Sprint`, regenerated. Every value in `CLIPS` is as shipped except `Arms_Sprint`'s `lift` 0.0 and `bob` 0.020.

  **Terms**, binding: as `desert-arms-revision` (hand roll, hand frame `f`, `n`, `t`). Yaw turns `f` about engine +Y (up); a negative yaw turns the left hand's fingertips toward +X (outward, the player's left). Pitch turns `f` down about the hand's lateral axis.

  **Expected values**, from the unblock trial (`build_arms-unblock.patch` with `PXE_SKIP_EXPORT=1`; not binding beyond the checks themselves):
  - highest NDC y in view at rest −0.410; nearest in view 0.258 m;
  - index knuckle top −0.469; hand centre (±0.494, −0.686); forearm exit |NDC x| 0.714;
  - readout: 38 dots, NDC x −0.842 to −0.681, y −0.952 to −0.857, at least 4.56 × 4.70 px, facing at least 0.954;
  - compass: centre NDC (0.637, −0.864), 0.276 m ahead, dial 50.9 px, lamp 5.8 px, 0 of 216 face vertices blocked, all in view;
  - clips: `Arms_Walk` top −0.314, `Arms_Sprint` top −0.313;
  - 16,396 triangles.

  `docs/tasks/desert-arms-pose/unblock-trial-eye.png` is the trial's eye view.

## Out of scope
- Any material, colour or gear change (`desert-arms-gear`), and any change to the motion (`desert-arms-motion`) beyond the sprint's `lift` and `bob` (Decisions).
- Moving the shoulders; moving the compass other than along its strap's line (Decisions); joints renamed or added.
- Engine or game code.

## Spec revisions
- **Unblock of replan 1, 2026-10-30, user decisions** (replan-1.md's four failures). Compass: slides toward the hand along the cuff (replan option 1a). Sprint: `lift` and `bob` lowered now (2b). Palm vs cuff: `PALM`'s wrist ring spared from the bulk (3a). Gauge vs cuff: no cuff bulk (4a). Bracer: the plate's wrist end extends (4, plate). Triangles: the 12 extra from the stitches accepted (5). Compass fallback: 5 mm steps along the strap, blocking if none passes. The planner ran every search through the build's whole `check()` and `clip_checks()` (`scripts/unblock_trials.py`) and fixed the results as constants. Changed: Goal, Decisions (Placement z −0.150, Chunkier gloves, gauge, bracer, compass, walk fallback, sprint, fixed constants), Approach, Verification 7–8, Acceptance criteria, Assets, Out of scope. Knock-on: `desert-arms-gear`'s vambrace length range becomes 0.12–0.15 m (user), because this plate is now 0.140 m.

## Cycle log
- Cycle 1, 2026-10-30: base 010f23d
- Cycle 1, 2026-10-30: designer returned NEEDS INPUT (5f8386c): four binding checks fail at the spec's values and its fallbacks can't recover them (gauge readout, compass in view, palm vs cuff, sprint framing); reproduced by the orchestrator. Fixes change Decisions: blocked, see replan-1.md.
- Unblocked: 2026-10-30, replan 1 answered by the user; values fixed from a full-check trial (build_arms-unblock.patch)
