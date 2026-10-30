# Arms: pointing forward, closer to the body, chunkier gloves
Type: feature
Status: blocked
Cycle: 1

## Goal
Re-pose the desert player's arms (`desert/tools/blender/build_arms.py`, `desert/assets/characters/arms.glb`) toward `desert/docs/concept/wanderers.png`:
- **Pointing:** the hands point mostly forward, with a firm, almost straight wrist, palms facing each other and thumbs up as if loosely holding something.
- **Placement:** the arms sit closer to the body and a little higher, so less forearm shows.
- **Gloves:** 15% chunkier.

The gauge moves toward the wrist until it is back in view. The walk clips are regenerated from the new rest pose with today's motion; the motion itself is reworked in `desert-arms-motion`. The gear and palette are `desert-arms-gear`.

## Decisions
All from the planning interview with the user, 2026-10-30.
- **Pointing:** `HAND_YAW` goes from 16.0 (inward) to **−8.0** (8° outward), and `HAND_PITCH` from 16.0 to **4.0** (a firm, almost straight wrist). `HAND_ROLL` stays 90.0: palms facing, thumbs up. With palms facing, the fingers still curl toward the other hand, and in a first-person view a forward-pointing hand converges toward the screen centre. The planning trial showed that this yaw and pitch read most forward.
- **Placement:** `WRIST` goes from (0.215, −0.370, −0.155) to **(0.200, −0.330, −0.140)**: 4 cm closer, 1.5 cm higher, 1.5 cm inward. `SHOULDER`, `CLAVICLE`, `UPPER_LEN`, `FORE_LEN` and `ELBOW_SWING` don't change.
- **Chunkier gloves:** every glove width and thickness grows by 1.15. That covers `PALM`'s half width and its dorsal and volar heights, each finger's base width and thickness in `FINGERS`, and `THUMB_W` and `THUMB_T`. Lengths, curls, splays and `palm_front` reaches don't change. The cuff (`CUFF` radii) grows by 1.10, so it still covers the wrap.
- **The gauge back into view:** at the new placement the gauge's readout leaves the bottom of the view. First, `GAUGE_S` moves toward the wrist in 5 mm steps: −0.0535, −0.0485, −0.0435, −0.0385, then −0.035. At each step the build checks the readout. If none passes, `GAUGE_TURN` steps from 0.55 up to 0.80 in 0.05 steps at `GAUGE_S` −0.035. The first combination where every readout check passes is used, and recorded in assets.md. If none passes, the task stops as blocked. The bracer and its straps, toggles, canister, coil and cables move with the gauge only as far as they must to stay on the plate; the build's attachment checks decide. The compass doesn't move.
- **Walk fallback:** the clips are regenerated unchanged (`CLIPS`, `CLIP_TARGETS`). If a walk or sprint frame breaks today's limit (NDC y −0.30), the wrist's z lowers in 5 mm steps (−0.145, −0.150, −0.155). The first value where every clip check passes is used and recorded. If none passes, the task stops as blocked.
- **Every other check** in the build stays binding, with today's values: framing, `KNUCKLE_Y` (−0.70, −0.40), `HAND_X` (0.40, 0.60), clearance 0.15 m, limbs leaving through the bottom edge, compass, readout, small parts, attachment in the rest and test poses, covered, closed, crease, clip checks, and glb read-back.
- **Materials, joints, clips' names and lengths, and slot order stay as shipped.**

## Layering
- Engine: none.
- Game: none. `desert::Arms` loads whatever the glb holds, and the joints and clips keep their names.
- Assets: `build_arms.py` rebuilds `arms.glb` and its previews.

## Approach
Designer only.
1. **`build_arms.py` constants:** `HAND_YAW = -8.0`, `HAND_PITCH = 4.0`, `WRIST = v(0.200, -0.330, -0.140)`, with their comments updated ("the hand turns this far outward", "a firm, almost straight wrist").
2. **The glove scale.** Add `GLOVE_BULK = 1.15` and `CUFF_BULK = 1.10` beside the hand constants. Apply them where the widths are read, not by editing every number. That means `PALM`'s columns 2–4, `FINGERS`' `w0` and `t0`, `THUMB_W` and `THUMB_T`, and `CUFF`'s radii, so the tables stay readable. Anything derived from those widths (stitch rows, pads, patches, straps over the cuff) follows them.
3. **The gauge search** (Decisions), written as a loop in the build. It is the same mechanism as the old `HAND_ROLL` fallback: try each candidate, run the readout's checks, and keep the first that passes. Print the chosen `GAUGE_S` and `GAUGE_TURN` in the arms measurements.
4. **The wrist fallback** (Decisions), the same kind of loop round the whole build and the clip checks. Print the chosen wrist z.
5. **The module docstring:** the held pose's numbers (wrists 0.33 m ahead, hands pointing slightly outward, a firm wrist, gloves chunkier).
6. Rebuild headless (`--python-exit-code 1`) and keep the new previews: the asset changed. Build step by step in the user's live Blender through the MCP first, so the user can watch, as `desert/docs/art-style.md` says.

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
7. assets.md records the measured framing values next to the trial's expected values below, plus the chosen `GAUGE_S`, `GAUGE_TURN` and wrist z.

## Acceptance criteria
- [ ] `HAND_YAW` −8.0, `HAND_PITCH` 4.0, `HAND_ROLL` 90.0. `WRIST` is (0.200, −0.330, z), with z −0.140 or the first fallback that passes. `GLOVE_BULK` 1.15 and `CUFF_BULK` 1.10 are applied as §2 says.
- [ ] `GAUGE_S` and `GAUGE_TURN` are the first passing pair from the Decisions' search, and recorded.
- [ ] Every existing build check passes with today's values. Materials, joints and clip names, lengths and keys are as shipped.
- [ ] Verification 2–7 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms** (revised): `desert/assets/characters/arms.glb`, rebuilt by `desert/tools/blender/build_arms.py`, with all six previews rewritten.
  - Format, units, axes, origin, rig (39 joints), materials (15, as shipped), clips: unchanged in kind.
  - Budget: 12,000–20,000 triangles, as today (the gear task raises it). The bulk adds no triangles.
  - Animations: `Arms_Walk`, `Arms_Sprint`, regenerated, every value in `CLIPS` as shipped.

  **Terms**, binding: as `desert-arms-revision` (hand roll, hand frame `f`, `n`, `t`). Yaw turns `f` about engine +Y (up); a negative yaw turns the left hand's fingertips toward +X (outward, the player's left). Pitch turns `f` down about the hand's lateral axis.

  **Expected values**, from the planning trial (`HAND_YAW` −8, `HAND_PITCH` 4, `WRIST` (0.200, −0.330, −0.140), before the glove bulk; not binding):
  - highest NDC y in view at rest −0.375;
  - nearest in view 0.25 m;
  - index knuckle top −0.434;
  - hand centre (±0.494, −0.631);
  - forearm exit |NDC x| 0.744;
  - compass face vertices blocked 0 of 216, compass centre 0.259 m ahead.

  At `GAUGE_S` −0.0585, 7 readout pieces fell outside the view's bottom edge.

## Out of scope
- Any material, colour or gear change (`desert-arms-gear`), and any change to the motion (`desert-arms-motion`).
- Moving the shoulders or the compass; joints renamed or added.
- Engine or game code.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 010f23d
- Cycle 1, 2026-10-30: designer returned NEEDS INPUT (5f8386c): four binding checks fail at the spec's values and its fallbacks can't recover them (gauge readout, compass in view, palm vs cuff, sprint framing); reproduced by the orchestrator. Fixes change Decisions: blocked, see replan-1.md.
