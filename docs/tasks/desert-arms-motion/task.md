# Arms: a natural walk and a pumping sprint
Type: feature
Status: open
Cycle: 1

## Goal
Replace the arms' mechanical walk and sprint clips (`Arms_Walk`, `Arms_Sprint` in `desert/tools/blender/build_arms.py`) with more natural motion.
- **Walk:** the arms swing from the shoulder in opposition to the stride, along a pendulum arc. The elbows give a little, the hands follow through at the wrist, and a sharp small dip marks each footfall.
- **Sprint:** a different, pumping motion. Arms are held more bent and driven up and forward, then back and out, with a harder footfall and the hands in loose fists.

Cycle lengths, phase convention, key layout and the game code stay as they are.

## Decisions
All from the planning interview with the user, 2026-10-30.
- **Same frame of the clips.** Each clip is 1.0 s at 30 fps: 31 linear keys, the last equal to the first. `Arms_Walk` is 2.0 m per cycle and `Arms_Sprint` 3.0 m. The left hand is furthest forward at phase 0, in opposition to the stride. Only `shoulder`, `upper_arm`, `forearm` and `hand` turn (and the digits in the sprint's fist). Every other joint, and every translation and scale, is keyed at exactly rest. The wrist targets are reached by the existing least-turn solve of the shoulder and upper arm (`Builder.carry`).
- **Notation** (binding): φ is the phase, 0..1. `sgn` is +1 for the left arm and −1 for the right. `c = sgn·cos 2πφ` is the side's swing, +1 at its forward peak. `s = |sin 2πφ|`. Offsets are the wrist's from rest in **engine** axes: x the player's left, y up, z forward.
- **Walk** (binding numbers):
  - Swing: `dz = 0.035·c`.
  - Pendulum arc and footfall bob: `dy = 0.006·c² − 0.012·(1 − s)²`. The bob's low points are the footfalls at φ 0 and 0.5.
  - Inward drift on the forward swing: up to 6 mm toward the body's middle, `|dx| = 0.006·max(c, 0)`, toward −x for the left arm and +x for the right.
  - Elbow give: the forearm closes by `e = 6°·max(sgn·cos 2π(φ − 0.05), 0)`, peaking 0.05 cycle after the side's forward peak.
  - Hand follow-through: the hand turns about the **engine x axis** (the body's left-right axis) at the wrist by `h = 5°·sgn·cos 2π(φ − 0.15)`. A positive h turns the fingertips **up**, so the hand peaks 0.15 cycle after the swing.
  - Fingers stay relaxed as at rest.
- **Sprint** (binding numbers):
  - Held pose: elbows closed by a constant 15°, hands lifted 15 mm and drawn back 20 mm.
  - Swing: `dz = −0.020 + 0.070·c`.
  - Drive and footfall: `dy = 0.015 + 0.025·max(c, 0) − 0.022·(1 − s)³`.
  - Out on the back swing: up to 10 mm away from the body's middle, `|dx| = 0.010·max(−c, 0)`, toward +x for the left arm and −x for the right.
  - Elbow pump: `e = 15° + (c′ > 0 ? 12°·c′ : 8°·c′)` with `c′ = sgn·cos 2π(φ − 0.03)`. It closes a further 12° at the front and opens 8° at the back, peaking 0.03 cycle late.
  - Hand follow-through: `h = 8°·sgn·cos 2π(φ − 0.10)`, axis and sign as the walk's.
  - Fist: every finger and thumb joint curled 25° toward the palm, as today.
- **Frame limits:**
  - Rest and every walk frame: nothing in view above NDC y **−0.30**, as today.
  - Every sprint frame: **−0.10** (user decision, relaxed from −0.30 for the pumping hands).

  Near clearance (0.15 m) and limbs leaving through the bottom edge stay binding at every frame.
- **Automatic fallback.** If a clip breaks its frame limit, the build multiplies that clip's **vertical and angle terms** by a scale `k` and retries, for `k` = 0.9, 0.8, 0.7, 0.6 and 0.5. The scaled terms are the walk's 0.006 arc, the sprint's 0.015 lift and 0.025 drive, every elbow angle (give, hold, pump) and the hand amplitude. Swing, bob, drift/out, the sprint's draw-back, and all timings stay. The first `k` that passes is used and recorded in assets.md. If none passes, the task stops as blocked.
- **Unchanged:** the rest pose, the geometry, the materials and the game code. `desert::Arms` keeps its 2.0 m and 3.0 m cycles.

## Layering
- Engine: none.
- Game: none. The clip names and cycle lengths are as `desert/src/arms.cpp` expects.
- Assets: `build_arms.py`'s clips and checks. `arms.glb` and its clip preview sheets are rebuilt.

## Approach
Designer only.

### 1. `build_arms.py`
- **`CLIPS`** becomes the Decisions' numbers, one dict per clip, every term named. Sketch:
  ```python
  CLIPS = {
      "Arms_Walk": dict(cycle=2.0, swing=0.035, back=0.0, lift=0.0, arc=0.006, drive=0.0, bob=0.012, bob_pow=2,
                        drift=0.006, out=0.0, hold=0.0, give=6.0, pump=(0.0, 0.0), elbow_lag=0.05,
                        hand=5.0, hand_lag=0.15, fist=0.0),
      "Arms_Sprint": dict(cycle=3.0, swing=0.070, back=0.020, lift=0.015, arc=0.0, drive=0.025, bob=0.022, bob_pow=3,
                          drift=0.0, out=0.010, hold=15.0, give=0.0, pump=(12.0, 8.0), elbow_lag=0.03,
                          hand=8.0, hand_lag=0.10, fist=25.0),
  }
  FRAME_TOP_CLIP = {"Arms_Walk": -0.30, "Arms_Sprint": -0.10}
  CLIP_SCALES = (1.0, 0.9, 0.8, 0.7, 0.6, 0.5)
  ```
- **`Builder.carry(spec, phase, S, guess, k)`** builds the target from the binding formulas. Engine (x, y, z) maps to Blender (x, −z, y), so `target = sd.W + Vector((dx, -dz, dy))`, with dx's sign per Decisions. The two fixed turns:
  - **Forearm:** closes by `e` degrees about the existing elbow hinge (`sd.au.cross(sd.af)`). Closing is the opposite sign of today's `opened`, so the rotation vector is `hinge · radians(e)`. Check that sign against the test pose `elbow_closed`, which closes by a positive angle.
  - **Hand:** turns by h about Blender +X, which is engine x. Fingertips up is a rotation of −h about +X, since the fingers point along −Y, so the rotation vector is `(1, 0, 0) · radians(−h)`.

  Then the existing least-turn solve of the shoulder and upper arm, and the existing fist.
- **`clip_keys` / `animate`** take `k`. Around them, a loop over `CLIP_SCALES` per clip runs `clip_checks` for that clip and keeps the first `k` that passes. Print `m["clip_scale"] = {"Arms_Walk": k, "Arms_Sprint": k}`.
- **`CLIP_TARGETS`** is kept apart from `CLIPS`, as today, so a slip in the motion's numbers fails the build. It holds each clip's independent expected values. **`clip_checks`** (binding) samples each clip's 30 keys, plus half-keys for framing as now, and checks, for each side, the hand joint head's offset from rest in engine axes:
  - z: its max is `−back + swing` and its min `−back − swing`, each within `AMP_TOL` (3 mm). The max comes at the side's forward phase (left 0, right 0.5) within `TIMING_TOL` (1/30).
  - y: at φ 0 and 0.5 it equals the formula's value (with `k` applied) within 3 mm. The side's minimum y over the cycle comes within 1/30 of φ 0 or 0.5.
  - The forearm's turn about the hinge from rest is most closed at the side's forward phase + `elbow_lag`, within 1/30.
  - The hand's turn about engine x relative to the forearm is most fingertips-up at the forward phase + `hand_lag`, within 1/30.
  - Framing at every sampled frame: `FRAME_TOP_CLIP[clip]`, `CLEARANCE`, and limbs leaving through the bottom edge.
  - As today: attachment at `ATTACH_FRAMES` phases, `STEP_MAX`, seamless loops, rest-keyed joints, and only the allowed joints turning.

  Remove `CLIP_TARGETS`' old keys (mean, bob, swing) and today's `SOLVE_WEIGHT` only if unused; keep it if the solve still uses it.
- **The module docstring's clip paragraph** describes the new motion in the Decisions' words.
- Rebuild headless and keep the new previews (`arms_walk.png`, `arms_sprint.png`, and the rest, which may be byte-identical if nothing else changed).

### 2. `desert/docs/art-style.md`
Make exactly these replacements:
- Replace
  > - nothing in view above the lower 35% (NDC y ≤ -0.30);

  with
  > - nothing in view above the lower 35% (NDC y ≤ -0.30), at rest and in every clip except a sprint, whose frames may reach NDC y -0.10 for its pumping hands;
- Replace
  > The framing rules (lower 35%, near clearance, limbs leaving through the bottom edge) hold at every sampled frame;

  with
  > The framing rules (lower 35%, or NDC y -0.10 in a sprint clip, near clearance, limbs leaving through the bottom edge) hold at every sampled frame;

Everything else stays word for word, including the clips' lengths, cycle metres and phase convention.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. The headless build exits 0 with every check passing. Two runs into scratch `PXE_DESERT_OUT` directories give `cmp`-identical glbs. assets.md records `clip_scale` for both clips.
3. **Rest pose unchanged.** Run the build's clip read-back, which compares the glb with clips against the one just exported. Then compare the new glb's mesh and skin with the cycle base's using a small script that reads both through `tools/glb.py` or Blender: the rest vertices and inverse bind matrices must be within 1e-6. Commit the script to `docs/tasks/desert-arms-motion/scripts/`.
4. `python3 tools/glb.py info desert/assets/characters/arms.glb`: as the base apart from the animation data. Both clips are 1.0 s with 31 LINEAR keys.
5. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: **3/3 identical**, because the standing captures use the rest pose.
6. `git diff <cycle base> -- engine game examples desert/src`: no output.
7. Captures into `docs/tasks/desert-arms-motion/`, each rendered twice and `cmp`-identical:
   - `pxe_desert --walk 2 --capture walk-a.png`
   - `--walk 2.29 --capture walk-b.png`
   - `--walk 3 --sprint --capture sprint-a.png`
   - `--walk 3.25 --sprint --capture sprint-b.png`

   Describe in the handoff how the clip preview sheets (`arms_walk.png`, `arms_sprint.png`) show the arc, the elbow give, the hand lag and, in the sprint, the pumping drive and fists. Compare with `docs/tasks/desert-arms-gear/gear-sprint.png`.
8. `git diff <cycle base> -- desert/docs/art-style.md` shows exactly the two replacements.
9. Playtest checklist for the user, in the handoff:
   - walk (W) and watch the swing, arc, footfall dip and hand follow-through;
   - sprint (Shift+W) and see a distinctly different pump, with fists;
   - stop, and the arms settle to rest;
   - the menu's Arms preview (Walk, Sprint, Hold pose with a phase scrub) shows each clip standing still.

## Acceptance criteria
- [ ] `CLIPS`, `FRAME_TOP_CLIP` and `CLIP_SCALES` hold the Decisions' numbers. `carry()` implements the binding formulas, axes and signs.
- [ ] `clip_checks` runs every binding check in §1 against `CLIP_TARGETS`, kept apart from `CLIPS`. The fallback over `CLIP_SCALES` works as specified and is recorded.
- [ ] The rest pose, mesh and skin are unchanged (Verification 3). Perspective captures are identical to base.
- [ ] `desert/docs/art-style.md` has exactly the two replacements.
- [ ] Verification 2–8 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms** (revised clips only): `desert/assets/characters/arms.glb`'s `Arms_Walk` and `Arms_Sprint`, rebuilt by `build_arms.py`, with the preview sheets.
  - Format, units, axes, origin, rig, materials, mesh: unchanged from `desert-arms-gear`.
  - Animations: `Arms_Walk` (2.0 m per cycle) and `Arms_Sprint` (3.0 m per cycle), 1.0 s, 31 linear keys, values as Decisions.

  Axes, binding: engine x is the player's left (Blender +X), y up (Blender +Z), z forward (Blender −Y). The hand's follow-through turns about engine x through the wrist; positive h raises the fingertips. The elbow hinge is `au × af` of the rest pose, as today; positive e closes the elbow.

## Out of scope
- New clips (idle, look, jump), changes to the cycle lengths, or game code that drives the clips.
- The pose, gear or materials.
- Engine changes.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-31, base 0a63800.
