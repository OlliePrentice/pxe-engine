## Cycle 1
Build: pass (fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning)

### Changes
- engine/shaders/edge_perspective.frag: `uViewModelDepth`, `kViewModelStep`, `viewModelAt`; `viewDepth` stretches depths below `uViewModelDepth` back over the whole range; `edges()` gets the view-model rules (dark silhouette ink against scene/sky and against a part more than 1 cm further away, light rim only at an upward crease, `min(mix(col, uInk, 0.65), col * 0.8)` before rims and crease ink, a scene pixel skips view-model neighbours); header comment gains the binding paragraph.
- engine/src/renderer_perspective.cpp: the edge pass sets `uViewModelDepth` (0 with no view model, `kViewModelDepth` otherwise). Nothing else.
- docs/tasks/view-model-edges/scripts/diff_box.py: differing-pixel count and bounding box of two PNGs (PIL; run with `python3 -I`).
- docs/tasks/view-model-edges/: base-day/night.png, edges-day/night.png, edges-at-trunk.png, diff-day.txt, diff-night.txt.

### Verification
1. Fresh check build: pass, only the sprintf warning.
2. `tools/ortho-check.sh 37c2178`: `ortho-check: 6/6 cases identical against 37c2178`.
3. `tools/perspective-check.sh 37c2178`: `perspective-check: 1/3 cases identical`: `example-first-person` PASS (identical); `desert-default` and `desert-night` differ (the intended change).
4. `git diff 37c2178 -- <shared shaders, renderer files, engine/include, game, desert, examples>`: no output.
5. grep for `uViewModelDepth|kViewModelStep|viewModelAt`: matches only in `engine/shaders/edge_perspective.frag` and `engine/src/renderer_perspective.cpp`.
6. diff_box.py (base vs edges):
   - day: `differing pixels: 1827`, `columns 32..904, rows 425..539`
   - night: `differing pixels: 1827`, `columns 32..904, rows 425..539`
   Every differing pixel is in rows 351-539 (first row 425). Two runs of each edges capture are byte-identical (`cmp`). Base captures came from `/var/folders/.../pxe-render-check/37c2178f.../build/release/desert/pxe_desert`.
7. Trunk: with `kSpawn` temporarily `{0.f, -0.71f}`, `edges-at-trunk.png` shows the bark behind and between the hands with no new light or dark line along the arms' outline beyond their own thin ink. The edit was reverted with `git checkout`, `desert/src/main.cpp` touched, rebuilt; a fresh `--capture` byte-matches `edges-day.png`. `git status` shows no change to `desert/`.

### What the captures show (edges-day.png vs base-day.png, zoomed)
- The light rim on the tops of both gloves and along the forearms is gone and replaced by a dark outline against the sand. The glove leather is already dark brown, so the ink (`col * 0.8` at most, mixed 0.65 toward the ink colour) is close to the glove's own colour: a thin darker edge, not a black line.
- Inner dark outlines appear where the fingers and thumb cross the hand, round the gauge's bezel and along the bracer straps and the wrap bands' edges.
- No ink across the middle of a glove's back or of a wrap band. A few light lines remain on the hands' backs: they are the existing crease rule (an upward face at a crease whose neighbour faces less upward), kept by the spec.
- Night is the same geometry in the night palette.

### kViewModelStep tuning
None. `kViewModelStep` stays at 0.01 m: no ink across a continuous surface was seen.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: perspective edge pass reads the view model's true depth and has its own edge rules (`edge_perspective.frag`); `uViewModelDepth` uniform set in `renderer_perspective.cpp`. No new public API.
- Game: none; `desert/` and `game/` unchanged.
- Misplaced code touched: none

### How to playtest
- `build/release/desert/pxe_desert`: look down at both arms against the sand. They should be outlined in dark, with no light rim on the glove tops.
- Raise and lower the view (mouse) so the arms pass over sky and horizon sand: the outline should hold on both.
- Walk up to the palm trunk (about 0.7 m from its centre, or walk into it) and watch the bark round the hands: no flicker or extra lines at the arms' outline.
- Sprint, and look at the gauge and bracer: inner outlines should be steady, not shimmering.
- Ruins game (`build/release/game/pxe`) is unchanged by construction (ortho check 6/6).

### Docs
- docs/engine-guide.md: in the view-model section, say the perspective edge pass reads view-model pixels at their true depth and inks every view-model silhouette dark (against the scene or sky, and where a part is more than 1 cm in front of another), with a light rim only at an upward crease, and that the scene never outlines against the view model. Known limit: while a view model is drawn, a scene surface within about a millimetre past the near plane would be read as the view model's (the desert keeps the eye 0.3 m from the palm and 1.7 m above the sand).
- docs/architecture.md: the "A view model under the perspective camera" row can mention its edges are in `engine/shaders/edge_perspective.frag`.

### Known limitations
- Texels outside the target (a neighbour past the frame edge) are read with `texelFetch` as before; with a view model they may read as depth 0, so a view-model pixel on the very edge of the frame (arms at the bottom row) may not outline against the frame edge. Not seen as an issue in the captures.

## Cycle 2
Build: pass (fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning)

### Changes
- engine/shaders/edge_perspective.frag: moved the `uViewModelDepth` / `kViewModelStep` block, unchanged, to directly after `uniform vec2 uHazeSpan;`, so `uNear` and `uFar` sit together again. `viewModelAt` stays where it was.

### Issues addressed
- ISSUE-001: uniform block moved after the camera uniform group; fixed.

### Verification
- Fresh desert captures (day, `--hour 21`) are byte-identical (`cmp`) to the committed `edges-day.png` and `edges-night.png`.
- `tools/ortho-check.sh 37c2178`: 6/6 cases identical.
- `tools/perspective-check.sh 37c2178`: 1/3 identical, as in cycle 1: `example-first-person` PASS; `desert-default` and `desert-night` differ (the intended change).

### Layering
- Engine: none new (a declaration moved within one shader).
- Game: none.
- Misplaced code touched: none

### How to playtest
- As in cycle 1; nothing visible changes.

### Docs
- As in cycle 1.

### Known limitations
- As in cycle 1.
