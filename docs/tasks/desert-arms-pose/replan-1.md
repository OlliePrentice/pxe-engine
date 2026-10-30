# desert-arms-pose: replan 1

## What failed
The designer (cycle 1, commit 5f8386c) implemented the spec as `scripts/build_arms-pose.patch` and ran the build's whole `check()` and `clip_checks()` over it with `scripts/pose_trials.py`. Four binding checks fail, and the spec's own fallbacks can't recover any of them. The orchestrator reran `pose_trials.py` headless on a scratch copy with the patch applied and got the same gauge-search numbers.

1. **Gauge readout.** No candidate in the Decisions' search passes. At −0.0535 the screen leaves the bottom edge; from −0.0485 on, the 1.10 cuff hides readout dots (3, 11, 19, then 19–24 at −0.035 with `GAUGE_TURN` 0.55–0.80). With `CUFF_BULK` 1.0 the search passes at −0.0385, 0.55. Separately, from −0.0535 on the toggles sit past the bracer plate's wrist end (s −0.040), and the spec doesn't say whether the plate extends or the whole bracer shifts.
2. **Compass face out of view.** Even in the planning trial's own pose (no bulk, shipped gauge), 66 of 216 compass face vertices are below the bottom edge (72 with the bulk); centre at NDC (0.707, −0.938). The trial measured only "blocked 0 of 216", not "in view". The spec says the compass doesn't move.
3. **Palm outside the cuff.** With `GLOVE_BULK` 1.15, two rear-ring vertices of each palm sit outside the cuff at (±0.199, −0.320, −0.113), in all 10 test poses and at every `CUFF_BULK` from 1.10 to 1.20. Without the glove bulk it passes.
4. **Sprint framing.** The clip limit (top NDC y −0.30) fails for `Arms_Sprint` at every allowed wrist z: −0.175 / −0.196 / −0.217 / −0.238 at z −0.140 / −0.145 / −0.150 / −0.155. Linear extrapolation needs z ≈ −0.185, below the shipped −0.155. (`Arms_Walk` passes from −0.150.)

Minor: the bulk adds 12 triangles (stitch rows follow the wider gloves), where the spec says "the bulk adds no triangles". Still within the 12,000–20,000 budget.

## Why it can't be replanned within Approach and Verification
Every fix changes a Decision the user agreed: the compass's placement ("The compass doesn't move"), the cuff or glove bulk, the wrist's fallback range, or `CLIPS` as shipped. So the user decides.

## Options and recommendation
Per failure, the alternatives considered and what I'd pick:

1. **Compass** (Decisions: "The compass doesn't move")
   - (a) Move it toward the hand along the cuff (`COMPASS_STRAP` s less negative) until the face is wholly in view. **Recommended**: it keeps the new pose and moves only gear, as the gauge already does.
   - (b) A larger `COMPASS_TURN` / another mount angle: turning doesn't raise the centre much (it is at −0.94), likely not enough on its own.
   - (c) Keep the wrist higher than −0.330: gives up part of "a little higher, less forearm".
2. **Sprint framing** (Decisions: walk fallback, `CLIPS` as shipped)
   - (a) Widen the wrist fallback down to ≈ −0.185: undoes "a little higher". Rejected.
   - (b) Lower `Arms_Sprint`'s `lift`/`bob` now so its top stays at −0.30. **Recommended**: `desert-arms-motion` reworks the sprint next anyway; this is the smallest change that keeps the pose.
   - (c) Relax the clip limit for the sprint only (e.g. −0.17): the hands would rise to about a third of the way up the screen mid-sprint until `desert-arms-motion`.
3. **Palm vs cuff** (Decisions: every glove width ×1.15; lengths unchanged)
   - (a) Spare `PALM`'s first (wrist) ring from the bulk, so the glove tapers into the cuff. **Recommended**: keeps every length and the cuff as agreed, and the visible hand still gets the full bulk.
   - (b) Lengthen the cuff forward (its front ring's s): changes a length.
4. **Gauge vs cuff** (Decisions: `CUFF_BULK` 1.10, the gauge search)
   - (a) `CUFF_BULK` 1.0: the gauge passes at −0.0385, 0.55. **Recommended if 3(a) is taken**, because the cuff then no longer needs to grow to cover the bulkier palm; to be confirmed by the trial.
   - (b) Add `GAUGE_TURN` steps at −0.0485 (3 dots hidden, screen 2 px below the edge): may not pass.
   - (c) Another placement.
   - Bracer at gauge positions past −0.0535: extend the plate's wrist end so the toggles stay on it (**recommended**: it moves the least), or shift the whole bracer.
5. **Triangles**: accept the 12 extra triangles from stitches following the bulk (**recommended**), or keep stitch counts fixed.

## Before re-queuing
Whatever the answers, rerun `scripts/pose_trials.py` (with the patch updated to the answers) and require every `check()` and `clip_checks()` result to pass before the spec is revised. The original planning trial measured a subset of the checks, which is how these four got through.

## Spec sections that would change
- `## Decisions`: Placement (compass), Chunkier gloves (palm wrist ring, cuff bulk), The gauge back into view (search and bracer), Walk fallback (sprint clip or limit).
- `## Approach`: §2 (bulk application), §3 (search), §4 (fallback), a new step for the compass.
- `## Assets`: expected values, triangles note; Animations line if `CLIPS` changes.
- `## Acceptance criteria`: the first three criteria.
- `## Out of scope`: "Moving the shoulders or the compass"; "any change to the motion" if 2(b).
