# Replan 1: the "Undersuit seen" check on the left forearm

## What failed
The designer (cycle 1) returned NEEDS INPUT before building: Approach §1's binding check "at least 8 `Arms_Undersuit` vertices per forearm are `seen` from the eye at rest" can't pass on the left forearm with the left vambrace as the spec fixes it (`BRACER_S` (−0.160, −0.020), ±70° about `back`, the gauge kept at its wrist end).

Evidence, `scripts/undersuit_visibility.py` on base 1657933, re-run by the orchestrator with the same output:
- At rest the only forearm surface in view and facing the eye behind each cuff is s −0.030 to −0.0655 m. On the left it spans −56° to +50° from `back`, wholly inside the plate's ±70° and s range. Rings of sleeve vertices there: 0 of 156 seen on the left.
- On the right (no plate today) it spans −50° to +56°; 20 ring vertices are seen, at s −0.0385 to −0.0635.

So the check contradicts the left vambrace's size and placement, which the user set (Decisions, and the spec revision from `desert-arms-pose`'s unblock).

## Alternatives
- **(a) Apply the check to the right forearm only, and place the right vambrace so it passes.** Keeps every Decision: the left forearm still has its undersuit, strips and gaps ("the undersuit shows in the gaps between strips and toward the elbow"), they are just under the plate or out of view. Chosen.
- (b) Shorten the left plate so its wrist end is at s ≤ −0.066. Takes the gauge and toggles off the plate, against Decisions ("carries the gauge at its wrist end") and the user's revision. Rejected.
- (c) Narrow the left plate below ±56°. Breaks the ±70° Decision and the 140° ± 10° check. Rejected.
- (d) Drop the check on both sides. Loses the only proof that the undersuit reads on screen at all. Rejected.

## Proposal
Changes only Approach §1 (the binding checks and the right vambrace's placement). Goal, Decisions, Layering, acceptance criteria, Assets and Out of scope are unchanged.

- The check becomes: "Undersuit seen: at least 8 `Arms_Undersuit` vertices on the **right** forearm are `seen` from the eye at rest. On the left the plate covers every in-view part of the forearm, so the left forearm is not checked for it."
- The right vambrace's placement (the spec doesn't fix it): its plate's wrist end goes toward the elbow far enough that the right check passes, with strips leaving a gap of undersuit behind the right cuff. Prefer the wrist end at s −0.045 to −0.066 so part of the right plate is still in view; if the check can't pass there, move it further toward the elbow, wholly out of view if need be, and say so in `assets.md`.
- Attachment note (no spec change needed): with `BRACER_SIT` unchanged so the gauge doesn't move, the left plate may stand off the strips and be held by its straps; in the attachment chain it touches the straps rather than the arm tube.
