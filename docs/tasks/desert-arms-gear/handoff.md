## Cycle 2
Build: pass

### Changes
- desert/src/arms.hpp: slot comment lists the new names in order (Arms_Gunmetal, Arms_Rust, Arms_Undersuit in place of Arms_Brass, Arms_Paint, Arms_Amber).
- desert/src/arms.cpp: `kBrass` is now `kGunmetal = 3`, `kPaint` is `kRust = 12` and `kAmber` is `kUndersuit = 13` (both `[[maybe_unused]]`). The crease line is `i == kWrap || i == kGlove || i == kGunmetal || i == kWorn || i == kGrime`. No other comment in `desert/src` mentioned brass, amber, paint or a bracer, so none needed fixing.
- docs/tasks/desert-arms-gear/gear-{day,golden,night,sprint}.png: the item 7 captures. scripts/gear_captures.txt is the cases file for `tools/captures.sh`.
- The designer's commit 27c6762 (`arms.glb`, `build_arms.py`, `art-style.md`, previews) is the asset side; I changed none of it.

### Issues addressed
- None (no `issues.md` this cycle).

### Layering
- Engine: none.
- Game: `desert/src/arms.cpp` still owns the `Arms` object's use of the glb and sets the crease flag per slot.
- Misplaced code touched: none.

### Verification (from the worktree, cycle base 14ce794)
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: exit 0. The only warning is stb_image_write's `sprintf`.
2. Not run by me (item 2 is the designer's); `arms.glb` is as the designer committed it.
3. `python3 tools/glb.py info desert/assets/characters/arms.glb`: materials `0 Arms_Wrap, 1 Arms_Glove, 2 Arms_Strap, 3 Arms_Gunmetal, 4 Arms_Glow, 5 Arms_Steel, 6 Arms_Copper, 7 Arms_Rubber, 8 Arms_Thread, 9 Arms_Worn, 10 Arms_Grime, 11 Arms_Dial, 12 Arms_Rust, 13 Arms_Undersuit, 14 Arms_Screen`; 39 joints; `Arms_Walk` and `Arms_Sprint` both 1.000 s, 117 channels, keys {31}, LINEAR; total triangles 24208 (within 18,000-30,000).
4. `tools/ortho-check.sh 14ce794`: 6/6 identical. `tools/perspective-check.sh 14ce794`: `example-first-person` PASS (identical), `desert-default` and `desert-night` differ, as expected.
5. `git diff 14ce794 -- engine game examples desert/src/main.cpp desert/src/player.* desert/src/world.* desert/src/settings.*`: no output.
6. `git grep -n -e Arms_Brass -e Arms_Paint -e Arms_Amber -e kBrass -e kPaint -e kAmber -e band_crease_problems -- desert`: no output.
7. `tools/captures.sh --twice`: 4 cases, every one deterministic (cmp-identical on a second run). Descriptions below.
8. `git diff 14ce794 -- desert/docs/art-style.md`: the three changes of section 2 and nothing else.

### Captures against `desert/docs/concept/wanderers.png`
The arms are first person at the bottom of the frame, so only the hands, the wrists and the wrist devices are in view at rest. The forearms, most of the wrap strips and the tubes are mostly out of view; the sprint capture shows more of them.
- **gear-day.png (rest, noon):** the gloves are grey-brown and chunky, with a dark gunmetal plate over the back of each hand and raised lighter knuckle guards at the finger roots, in the concept's heavy-glove manner. Stitching shows as pale dashes along the plate and fingers. At the left wrist the vambrace's gunmetal plate carries the gauge: a dark teal panel with `47%▮` in cyan dots (the concept's cyan screen on the forearm), a steel-screwed bezel and a toggle. At the right wrist a dark-faced compass with a cyan ring of ticks, needle and lamp sits on a brown strap, with its gunmetal housing and lid behind it. Dark undersuit and khaki wrap strips show only as a thin band at the wrist edge, so the frayed cloth is not really readable at rest, and no tube is in view. The only bright colour is cyan; everything else is dusty brown, grey or dark. The concept's weathered look comes through in the colours, less so in detail, because the view is so cropped.
- **gear-golden.png (hour 18):** the same view in warm dusk light. The gloves and wrap turn a darker reddish brown, the plates go near-black, and the pale stitching and steel edge lines stand out. The gauge and compass stay bright cyan against the dark metal. No other bright colour.
- **gear-night.png (hour 21):** the gloves, plates and wrists are almost black with faint blue edge light on the stitching and steel edges. The gauge readout (`47%▮`), the compass ticks, needle and lamp stay full cyan, so the cyan parts still glow at night.
- **gear-sprint.png (`--walk 3 --sprint`):** the hands swing up and forward, and more of the arms is in view. The left forearm shows the dark gunmetal vambrace under the gauge, a brown buckled strap and a steel buckle at the wrist, and two cyan tubes leaving the bottom-left edge diagonally from the gauge along the forearm, as in the concept's cyan tubing. The right hand shows the plate over the hand's back with its stitched, bevelled segments and knuckle guards, and the compass on a strap at the right wrist. The khaki strips and undersuit are still mostly below the frame; a thin khaki and dark band shows at the right cuff. Scratches on the vambrace are too small to see at 960x540. Cyan appears only in the gauge, compass and tubes.

### How to playtest
- `tools/build.sh release desert`, then look down at the hands. The gunmetal back plates, knuckle guards, the cyan gauge on the left wrist and the compass on the right should read clearly.
- Walk and sprint (hold shift): the two cyan tubes from the gauge should swing into view at the bottom-left, and the plates should stay on the hands. Known: the plates slide a few mm over the glove's pinky side when the wrist turns strongly (see limitations).
- `--hour 21`: cyan readout, compass marks, lamp and tubes should still glow while everything else is dark.
- Tab menu (arm hold of a clip at a phase) shows the pose at any stride point.
- Nothing in the keyboard, mouse or gamepad handling changed.

### Docs
- `README.md`'s arms description and `docs/architecture.md`'s arms row (the crease list) need the new gear: crease ink on wrap, glove, gunmetal, worn and grime (as the spec's "For the run's docs step" says).
- `docs/engine-guide.md`: none.

### Known limitations
- The designer's note applies: the hand plates and knuckle guards are rigid to `hand.<S>`, but the glove's pinky side blends toward the forearm, so in strong wrist poses the plate slides over the glove by up to about 6 mm at the pinky guard.
- I did not run verification item 2 (the headless build's checks); that was the designer's cycle.
- The frayed strips, scratches and the undersuit are mostly or wholly out of the rest view; they show only in poses where the forearms rise.
