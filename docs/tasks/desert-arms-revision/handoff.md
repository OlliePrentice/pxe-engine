## Cycle 1
Build: pass

### Changes
- desert/src/arms.hpp: slot comment now "Actor-material slots 0-14: ... Arms_Paint, Arms_Amber, Arms_Screen, in the glb's material order."
- desert/src/arms.cpp: added `[[maybe_unused]] constexpr int kScreen = 14;` after `kAmber`. The material loop and crease list are unchanged (the screen takes no crease ink).
- The designer's part (build_arms.py, arms.glb, previews, art-style.md; commit 7388e7a) is untouched.
- docs/tasks/desert-arms-revision/revision-day.png, revision-night.png, revision-sprint.png: Verification 8 captures.

### Verification
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: all targets built; the only warning is stb_image_write's `sprintf`.
5. `tools/ortho-check.sh 8b57c1b`: 6/6 identical. `tools/perspective-check.sh 8b57c1b`: `example-first-person` PASS; `desert-default` (the spec's desert-day) and `desert-night` differ, as expected, because the arms changed.
6. `git diff 8b57c1b -- engine game examples desert/src/main.cpp desert/src/player.* desert/src/world.*`: empty (0 bytes).
7. `git grep` for the removed bar/`PXE_ARMS_BEFORE` names in `desert`: no output.
8. Each capture rendered twice: `cmp`-identical (day, night, sprint).
   - `revision-day.png` vs `desert-arms-detail/detail-day.png`: the hands are no longer drooped fists. Palms face each other, fingers pointing forward and slightly inward, thumbs on top. The left bracer's gauge is now a dark green-grey panel reading `47` then `%` and a solid block cursor in bright green (the `%` is small, about 4-5 px dots, so it reads as a blocky 4-7-%-block). The glove and strap stitching is a darker brown, quieter than the pale tan. Compass, right wrist and everything behind the cuffs look as before; the compass face is fully visible.
   - `revision-night.png`: the same pose; the readout is just as bright green on its dark panel (emissive), against the dark night gloves.
   - `revision-sprint.png` vs `desert-arms-walk/sprint.png`: the arms are swung forward as before, the fingers curl into loose fists with the palms facing inward, the gauge shows the green readout (it was three cyan bars), and the stitching is darker. The highest pixel of the hands is about row 390, well below row 351.
Verifications 2, 3 and 4 are the designer's (recorded in assets.md); not rerun here.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none
- Game: `desert::Arms` already owns the glb's materials; only the slot comment and a constant changed.
- Misplaced code touched: none

### How to playtest
- Run `build/release/desert/pxe_desert`. Standing still: palms face each other, thumbs on top. The left gauge reads `47%` and a block cursor in green, by day and with `--hour 21`.
- Hold W then W+Shift: walk and sprint swing as before, fingers curl in the sprint, nothing reaches the top of the view.

### Docs
- README.md: the arms paragraph's "gauge glowing with three teal bars" becomes an LCD readout (47% and a cursor in green on a dark backlit screen).
- docs/architecture.md: the arms row's "actor slots 0-13" becomes "0-14".

### Known limitations
- None. (Detail: the `%` glyph is 3x5 with 2.4 mm dots, so it is blocky at 960x540, as designed.)
