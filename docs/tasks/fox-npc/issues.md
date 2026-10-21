# Issues: fox-npc

## ISSUE-001: Grass hides the lower third of the fox
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Lawn grass clumps and tufts rooted in front of and under the curled fox draw over its lower third and break up its outline (cycle 1 handoff, "Known limitations"). The user decided to flatten the grass: when the fox is shown, drop grass rooted under its curled footprint, the same way grass under stone and fire pits is dropped. Pass the fox's spot (and footprint radius) into scene building, e.g. through `BuildOptions`, set only when the fox will be shown. Runs without the fox must produce exactly the same grass as before. Use the revision-3 footprint at 2.4× (see assets.md once updated).

### Fix notes
Root cause: grass is scattered over the whole lawn, and only the parts under stone, trunks and in fire pits were dropped afterwards. Nothing knew about the fox, and blades are depth-tested as if upright, so the ones rooted under its low curl drew over it.
Change: I reused the fire-pit mechanism. `game::BuildOptions` gains `foxSpot` and `foxRadius` (default 0). In `buildRuins`, straight after the fire-pit `erase_if` and before the leaf clusters are added, a second `erase_if` drops every lawn sprite (clumps and tufts) rooted within `foxRadius` of `foxSpot` when `foxRadius > 0`. It logs the count, which is 17 at the current spot. Dropping happens after scattering, so the RNG streams and the rest of the lawn are untouched.
Single source of truth: the spot is `game::kRuinsFoxSpot` in `game/src/scene.hpp`, next to `kRuinsCampfire`. The radius is `game::Fox::kFootprint` = 0.68 m, the assets.md circle at 2.4x. main.cpp now loads the fox before building the scene and sets `build.foxSpot`/`build.foxRadius` only when `fox.built()`. A new `foxShown(opt)` decides that: never in hills, the bench or the self-test; captures only with `--fox`; play unless `--no-fox`. The same `fox` object is spawned after the scene is built and used by both the capture path and the play loop. As a result, a missing fox model also leaves the grass alone.
Verified against a HEAD build with captures: default ruins, `--hero --hero-attack 0.12`, `--scene hills`, and `--candles --extra-fires 6` are all pixel-identical. `--scene hills --fox` is identical to HEAD's hills. `--fox` differs only in a 34x21 px box at the fox. `--ui-shot` play runs drop the grass by default and not with `--no-fox` or in the hills.

## ISSUE-002: Fox must be 1.5× bigger, and placed clear of the fire at that size
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
The user wants the fox 1.5× bigger overall. `kSize` in `game/src/fox.cpp` is 1.6 and must become 2.4 (the asset stays at real size). At 2.4× with the revision-3 curl, `kFoxSpot` in `game/src/main.cpp` (-6.4, -6.5) may put the curl into the fire ring, dirt patch or the rock to the left. Re-check clearance as in cycle 1, and move the spot so the whole curl is at least 0.2 m clear, staying facing the fire. Update the comments and README text that say 1.6 times and "about a metre and a half".

### Fix notes
Root cause: the scale was set to the art-style character scale (1.6x). The spot was chosen for the cycle-1 footprint at that scale.
Change: `Fox::kSize` is now 2.4 (a public constant in `fox.hpp`, replacing the file-local one in fox.cpp). Clearance was re-checked the cycle-1 way. Headless Blender sampled every non-ground triangle of `ruins.glb` near the fire that sits above the ground. For candidate spots at 15-degree steps and radii 1.85-2.1 m round the fire, I measured the horizontal distance to the nearest sample. The campfire pieces are generated in code, so I bounded them from `addCampfire`: dirt disc up to 0.96 m, stones about 0.8 m, logs about 0.54 m.
The old spot (-6.4, -6.5) at 1.49 m would have put the 0.68 m curl circle 0.15 m into the dirt patch. The new spot is `kRuinsFoxSpot` (-6.65, -6.95): 1.90 m from the fire, left of it and a little behind it in the default view, the same side as before.
- The whole curl circle is 0.26 m clear of the dirt patch, and further clear of the stones and logs.
- It is 0.46 m clear of the rock, the nearest scene object (1.14 m to its above-ground surface).
- Every other scene object (pond trunk, paving, water, bushes) is further away than the rock.
It still faces the fire (yaw = atan2 toward `kRuinsCampfire`).
At this size the lawn's slope (about 8%, rising toward -z) would leave the downhill rim about 5 cm above the ground. Revision 3's curl rests at y = 0 and no longer sinks. So `Fox::spawn` now tilts the curl to the ground's normal, taken from `heightAt` 0.41 m either side in x and z, and keeps the centre on `heightAt`.
Updated the comments in fox.hpp, scene.hpp and main.cpp, and the README paragraph: about two metres from the fire, 2.4 times real size, grass cleared under it.
