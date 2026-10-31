# Replan 1: the sprint can't be reached as specified
Date: 2026-10-31. Cycle 1, base 0a63800. Designer commit db6e434.

## What failed, with the evidence
`Arms_Walk` builds and passes every check at k 1.0. `Arms_Sprint` passes at no k in `CLIP_SCALES`, which the spec's Decisions ("If none passes, the task stops as blocked") send to the user.

1. **Reach.** Only rotations are keyed, so the wrist is at most |S−C| + |S−W(e)| from the clavicle C. |S−C| = 0.1626 m, and |S−W| = 0.3865 m at rest, 0.3307 m at 15° closed and 0.2829 m at 26.8°. At φ 0 (k 1) the target is +50 mm forward and +18 mm up, 0.4912 m from C. With the elbow closed the specified 26.8° the arm reaches only 0.4455 m, so it is 46.7 mm short. The most the elbow can close and still reach it is 15.5°, and only with the chain locked straight. The fallback scales the elbow angles and lift/drive but not the swing or draw-back, so the gap is still 34.8, 23.2, 11.7 and 0.4 mm at k 0.9–0.6, and at k 0.5 the solve still misses frames 0–3 and 28–29. Source: `scripts/sprint_reach.py`, `scripts/sprint_reach.txt`, `scripts/build_fail.txt`.
2. **Attachment.** With both elbows closed 18° or more, the end vertices of `R.tube.1` show outside the `R.arm` sleeve (10° and 15° are clean). The pump reaches about 27°. Source: `scripts/elbow_close_attachment.py` / `.txt`. Fixing it is a gear change, which Out of scope rules out.
3. **Conflicting check.** The `desert-arms-walk` joint-range check `ELBOW_SPAN` (elbows closed at most 10°) contradicts the sprint's 15° hold. This spec doesn't list it among the checks to keep, so the designer dropped it, and the per-clip attachment check at 8 phases now covers each clip's range.

I checked the claim against the script output and the geometry: the reach bound is a hard limit of a rotation-only chain, so no solver change gets around it.

## Alternatives considered
- **(a) Cut the forward elbow close** to about 13° or less at the front (hold + front pump), and allow more close only behind the body. This keeps the forward swing but loses most of the "more bent" read at the front, which is the main thing a sprint shows. The designer measured hold 10° and pump (+4, −8): it passes only at k 0.6, with the elbow at 1–8°, which makes a weak pump.
- **(b) Keep the 27° pump but move the forward peak up and back instead of forward.** With 27° closed, the wrist can be no further from C than about its rest distance, so the front of the swing has to go up (towards the chest) rather than out. This is closer to a real sprint arm, but it changes the binding swing, draw-back and drive numbers, and it still needs (d), because 27° breaks the tube.
- **(c) Also scale the swing and draw-back in the fallback.** Easy, but it shrinks the swing that makes the clip read as a sprint, and it still needs elbows at 17° or less because of the tube.
- **(d) Revise the right tube** so its end stays in the sleeve with the elbows closed up to about 30°. This is a gear change, out of scope today.

## Proposal (needs the user)
Recommended: **(b) + (d)**. Revise the right tube's end so it stays in the sleeve up to 30° closed. Then re-plan the sprint's numbers with the forward peak driven up rather than out (for example a smaller forward swing with more lift at the front), choosing the numbers with `scripts/sprint_reach.py` so every frame keeps at least 10 mm of reach margin before they're written into Decisions. Drop `ELBOW_SPAN` for clips (the attachment check covers it), and keep the 10° closed-elbow test pose, or raise it to the sprint's maximum.

If the user doesn't want a gear change, then **(a)**: the sprint's elbow stays at 17° or less everywhere, and the "more bent" read comes mostly from the lift and the fists.

## Sections that would change
- **Decisions:** the sprint's numbers (held pose, swing, drive, elbow pump), possibly the fallback's scaled terms, and the `ELBOW_SPAN` check.
- **Approach:** CLIPS' sprint dict, CLIP_TARGETS, clip_checks, and for (d) the tube in `build_arms.py`.
- **Assets / Out of scope:** for (d), the right tube's geometry.
- **Acceptance criteria / Verification:** "rest pose, mesh and skin unchanged" no longer holds under (d). The rest-pose comparison would exclude `R.tube.1`, and the perspective checks could differ wherever the tube is in view.

The walk is done and passes at k 1.0 on branch `desert-arms-motion` (db6e434), so it can be kept as it is in the replanned task.
