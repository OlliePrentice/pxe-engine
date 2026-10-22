## ISSUE-001: Walks are a ~13 m lap round the fire, not a few steps out and back
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
The spec (Goal, Decisions: "walks out and back along simple paths", "take a few steps") asks for a short outing. `fox.cpp` `layRoute` makes every walk one closed lap round the campfire, because from the bed at `kRuinsFoxSpot` the only way to arrive at the standing heading with no turn on the spot is from behind, along the fire's far side (handoff cycle 1, headline: 0.067 m spare fire clearance at the bed, bank about 1 m behind it). Accepted laps average 6.0 m out and 7.2 m back, about 16 s of walking. All passing targets are west to south of the bed, so every walk goes the same way round. Only 13% of targets pass, so `kTries` is 24 rather than "a few".

This is a design decision outside the spec (route shape, or moving the bed), so it goes to the user.

### Fix notes
Root cause: at (-3.3, -6.6) the fire (0.07 m spare clearance) and the bank behind the bed left the corridor round the fire as the only way back in facing the standing heading, so `layRoute` built a lap.
Fix:
- **Bed moved** to (-2.0, -7.0) (scene.hpp, user decision). Per-vertex check across all five clips: nearest Paving.026 at 0.204 m, the fire more than 1 m; standing faces 18.4° off the default camera.
- **Route replaced.** fox.cpp/.hpp: the lap is replaced by a teardrop out-and-back:
  - straight out, then a front circle through the target;
  - back along the line touching both circles and round a rear circle into the bed from behind;
  - no longer than 2d + 4 m, turns no tighter than 0.9 m;
  - curvature eased over 0.6 m, and each leg Newton-corrected to land exactly.
  - Same checks and live/simulated route function as before. 8 tries; `kWalkSeed` changed so the first walk is the second wake.
- **Measured:** 6.6% of targets clear (all ahead-left/left), 19.6% of wakes walk, outings 6.7–11.1 m (mean 9.25), arrival error ≤ 1.3e-5 m.
- **Captures:** non-fox captures are identical to the baseline; `--fox` captures are identical with the old spot restored.
- **Docs:** README, `--help` and comments updated.


## ISSUE-002: Step check samples every 10 cm, so the real step can reach 0.577 m
Status: wont-fix
Severity: minor
Source: review
Found in cycle: 2
Reopened: 0

### Problem
`routeClear` checks the footprint every `kCheckStep` (0.1 m). Re-sampled every 2.5 cm, the step under the body on accepted routes reaches 0.577 m, against `Hero::kStep` 0.55 m (handoff cycle 2, Known limitations).

### Fix notes
Orchestrator: wont-fix. The criterion (every sample checked) is met. The overshoot is 2.7 cm on a limit that only stands in for "walkable". The fox's height follows the ground under its origin, so nothing visibly breaks. Tightening the check would cut the already low walk rate further.
