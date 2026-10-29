## Cycle 1
Build: pass

### Changes
- desert/src/arms.cpp: the anonymous namespace names all 14 material offsets (`kWrap` 0 to `kAmber` 13), those the file never reads marked `[[maybe_unused]]`. `pose()` sets crease on `kWrap`, `kGlove`, `kBrass`, `kWorn` and `kGrime` only.
- desert/src/arms.hpp: the `kFirstMaterial` comment lists slots 0-13.
- docs/tasks/desert-arms-detail/: the eight captures (below), this handoff, friction.md.
- The designer's asset, art-style.md and assets.md (commit d7178a7) are untouched.

### Verification
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` deprecation warning.
2. Not mine (designer's headless rebuild, run in the asset commit).
3. `tools/ortho-check.sh 505b604`: 6/6 identical (game-default, game-hills, game-night-foxes, game-night-fires, game-self-test, example-ortho).
4. `tools/perspective-check.sh 505b604`: example-first-person PASS; desert-default and desert-night FAIL "png differs", as expected (1/3 identical).
5. `git diff 505b604 -- engine game examples desert/src/main.cpp desert/src/player.* desert/src/world.*`: no output.
6. `grep -n crease desert/src/arms.cpp`: one line, 49: `crease = i == kWrap || i == kGlove || i == kBrass || i == kWorn || i == kGrime;`.
7. Captures from a release build, each run twice and `cmp` identical (day, morning, golden, night). I viewed all four, plus 4x crops of the two hands in detail-day:
   - Hands: same place as `docs/tasks/desert-arms/arms-day.png`. A pixel diff against arms-day has its bounding box starting at row 425, so nothing differs above it and nothing is above row 351.
   - Gauge: three teal bars where they were, on the brass plate at bottom left.
   - Left bracer wrist end: four steel screws (two on the face, two at the cut corners, so two or more seen), a round knurled steel dial with a red index mark beside the bezel's inner side, three toggles with red tips above the plate, and the canister with its valve wheel at the far left edge.
   - Right, bottom right: the compass, a grey steel ring and ivory face with 12 red ticks and a red needle, an amber lamp at the lower rim, two dark side buttons, the open steel lid up and left, on a strap over the cuff.
   - Gloves: stitch dashes along the finger sides, the thumb and round the cuff, knuckle pads and worn patches on fingertips and back of hand, a banded cuff strap, wraps with grime bands, which show in the morning, golden and night captures (the grime and wrap are mostly cut by the bottom edge).
   - Night and golden: the teal bars and the amber lamp stay bright against the dark gloves, which are in shadow. The night and golden gloves read as dark silhouettes with the stitching and lamp still visible.
8. Temporary `kSpawn {0.f, -0.71f}` -> `detail-at-trunk.png`: the trunk's near face fills the middle; both hands, cuffs, the bracer and the compass are whole and drawn in front of the bark. Temporary spawn pitch `1.2f` -> `detail-looking-down.png`: plain sand, both arms whole and unchanged in the view. Both edits reverted, `touch desert/src/main.cpp`, rebuilt, and a fresh `--capture` byte-matches `detail-day.png`. `git status` shows only arms.cpp and arms.hpp modified.
9. Previews are the designer's (in the asset commit).

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none.
- Game: `desert::Arms` (desert/src/arms.*), owned by `main.cpp` as before; it sets the actor materials and crease ink for slots 0-13.
- Misplaced code touched: none

### How to playtest
- `tools/build.sh release desert`. Look down at the hands: stitching, knuckle pads, grime bands on the wraps.
- Left wrist: the three teal bars in the same place, bezel screws, a knurled dial and toggles at the wrist end. Right wrist: the sun compass, face tilted toward you, with its amber lamp. Tilt the view down until the arms leave the screen bottom, and walk to the palm trunk to check the arms stay in front.
- `--hour 21` and `--hour 18.5`: the teal bars and the amber lamp should keep glowing.
- Frame time: the arms are now about 15,800 triangles (budget 12,000-20,000); check the game still runs smoothly.

### Docs
- engine-guide: none. architecture.md: none. The designer's art-style.md already carries the view-model rule.

### Known limitations
- Two of the four bezel screws are on the cut corner and are not seen, as the designer's friction note records (the spec allows 2 of 4).
- At the bottom edge the bracer's plate corner and the right wrist's housing rim are cut by the edge, which the spec permits.
