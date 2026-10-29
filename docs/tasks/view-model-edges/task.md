# Edges for the view model
Type: feature
Status: complete
Cycle: 2

## Goal
The perspective edge pass reads the view model (`FrameParams::viewModel`) as surfaces just past the near plane, because the view model is drawn squeezed into the nearest 1% of the depth range. So no outline is drawn where one part of the view model stands in front of another. The view model's outline against the scene is also mostly a light rim, which gets lost against bright sand and sky. Make the perspective edge pass read a view-model pixel's true depth. Then ink every view-model silhouette in dark ink: against the scene, and inside the view model wherever a neighbour is more than 1 cm further away. Intricate parts stacked in depth (a dial on its housing, a cable over a plate, a finger over the palm) then read as separate pieces. Only view-model pixels change. Nothing else in either render mode changes, so the ruins game (orthographic) and every perspective frame without a view model stay byte-identical.

## Decisions
All from the planning interview with the user, 2026-10-29.
- **Engine, perspective mode only** (Q7). The change goes in the perspective mode's own edge pass (`engine/shaders/edge_perspective.frag`) and frame (`engine/src/renderer_perspective.cpp`). The user asked that nothing here touches the other game:
  - the ruins game runs the orthographic mode, with its own edge pass `edge.frag`, which this task doesn't touch;
  - no shared shader (`mesh.frag`, `common.glsl`) or shared renderer code (`renderer.cpp`) changes;
  - no game-specific shaders and no rework of how the engine provides shaders. The architecture's mode split already isolates the change (user, 2026-10-29: build that plan only if needed, and it isn't).
- **Only view-model pixels change** (Q7). With no view model in the frame, the edge pass computes exactly what it does today. `example-first-person` stays byte-identical.
- **Edge rules for a view-model pixel** (Q12):
  - **Against the scene or the sky:** always an outline in dark ink.
  - **Inside the view model:** an outline in dark ink where a neighbour is further away by more than `kViewModelStep` = 1 cm, plus the existing allowance for a surface seen edge-on (`min(abs(onPlane - d), d)`).
  - **Light rims:** none on silhouettes. A light rim stays only where today's crease rule gives one: an upward face (`n.y > 0.6`) at a crease whose neighbour faces less upward (`nq.y < n.y - 0.3`).
  - **Crease ink on creased materials:** as today.
- **Ink weight** (Q16): a view-model silhouette uses the treatment creased slate already gets, `min(mix(col, uInk, 0.65), col * 0.8)`. Silhouette ink comes before rims and crease ink on the same pixel.
- **Tuning** (Q12): `kViewModelStep` starts at 0.01 m. The run may change it within 0.005–0.02 m, and only if a capture shows ink across the middle of one continuous surface (a glove's back, a wrap band) where no part stands in front of another. Record any change in the handoff.
- **A scene pixel never outlines against the view model.** A scene pixel ignores its view-model neighbours, which are always in front of it. They are not "behind", not a crease partner and not a bark-stroke partner.

## Layering
- Engine:
  - `engine/shaders/edge_perspective.frag`: the view model's true depth and its edge rules.
  - `engine/src/renderer_perspective.cpp`: the edge pass's `uViewModelDepth` uniform.
  - Named for what they do. No game content.
- Game: none. `desert/` and `game/` don't change. The desert's captures change because it draws a view model.

## Approach
### 1. `engine/src/renderer_perspective.cpp`
In the edge pass's uniform block, after `u1f(progEdge_, "uNear", camera.nearPlane);` (binding):
```cpp
  // The view model's pixels are those drawn into [0, kViewModelDepth] of
  // the depth range; with none in the frame, no pixel is.
  u1f(progEdge_, "uViewModelDepth", params.viewModel.empty() ? 0.f : static_cast<float>(kViewModelDepth));
```
Nothing else in the file changes. The view-model draw block and `kViewModelDepth` stay as they are.

### 2. `engine/shaders/edge_perspective.frag`
Add the uniform and constant after `uniform float uNear;`'s group, next to the other camera uniforms (binding text):
```glsl
// The view model (pxe::FrameParams::viewModel) is drawn into [0,
// uViewModelDepth] of the depth range, in front of the whole scene: a pixel
// whose depth is below it is the view model's, and its true depth is its
// depth stretched back over the whole range. 0 when the frame has none.
uniform float uViewModelDepth;
// Metres of depth step that count as an outline inside the view model.
const float kViewModelStep = 0.01;

bool viewModelAt(ivec2 p) { return texelFetch(uDepth, p, 0).r < uViewModelDepth; }
```
`viewDepth` reads the true depth (binding shape):
```glsl
float viewDepth(ivec2 p) {
    float w = texelFetch(uDepth, p, 0).r;
    if (w < uViewModelDepth) w /= uViewModelDepth;
    float z = w * 2.0 - 1.0;
    return 2.0 * uNear * uFar / (uFar + uNear - z * (uFar - uNear));
}
```
In `edges()` (sketch, follow its shape; the existing scene branch must stay exactly as it is):
```glsl
    bool vm = viewModelAt(p);
    float apart = vm ? kViewModelStep : kSilhouette * max(1.0, d * uPixelAngle / uRefTexel);
    ...
    for (int i = 0; i < 8; ++i) {
        ...
        bool vmq = viewModelAt(q);
        // A scene pixel ignores the view model in front of it.
        if (!vm && vmq) continue;
        ...  // dq, rq, facing, onPlane, tol, nq4, nq as today
        // A view-model pixel's neighbour in the scene or sky is always behind it.
        bool behind = (vm && !vmq) || dq - d > tol;
        ...  // meshNeighbour, crease as today
        if (vm) {
            // The view model: dark outlines on every silhouette, light rims
            // only at upward creases.
            if (n.y > 0.6 && crease && nq.y < n.y - 0.3) rim = true;
            if (behind && !diagonal) ink = true;
        } else if (!timbered(surface) && n.y > 0.6 && (behind || (crease && nq.y < n.y - 0.3))) rim = true;
        else if (behind && !diagonal) ink = true;
        ...  // bark stroke and crease ink as today
    }
    // A view model's silhouette is inked as a slate crease is, before any rim.
    if (vm && ink) return min(mix(col, uInk, 0.65), col * 0.8);
    ...  // the rest as today: bark rim, dark regions, stroke, rim, ink, creaseInk
```
Update the file's header comment. Add one paragraph after the "Dark regions" paragraph (binding text):
```
// The view model (pxe::FrameParams::viewModel): its pixels are read at
// their true depth, though it is drawn squeezed in front of the scene. Each
// of its silhouettes, against the scene and where one of its parts stands
// more than kViewModelStep in front of another, is inked dark, and it takes
// a light rim only at an upward crease. The scene never outlines against
// it. A frame with no view model is drawn exactly as before.
```
Nothing else changes: `distanceHaze`, `worldAt` and `main` stay as they are. They call `viewDepth`, so the view model now gets its true distance, which is far short of the haze.

A known limit, for the engine guide: while a view model is drawn, a scene surface within about a millimetre past the near plane would be read as the view model's. The desert keeps the eye 0.3 m from the palm and 1.7 m above the sand.

### 3. Verification script
Write `docs/tasks/view-model-edges/scripts/diff_box.py`: given two PNGs of the same size, it prints the number of differing pixels and their bounding box (first and last column and row, rows counted from the top), using PIL. Run it with `python3 -I`.

## Verification
Run in the task's worktree:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. `tools/ortho-check.sh <cycle base>`: all 6 cases identical.
3. `tools/perspective-check.sh <cycle base>`:
   - `example-first-person` identical;
   - `desert-default` and `desert-night` differ, the intended change.

   Record the summary in the handoff.
4. `git diff <cycle base> -- engine/shaders/edge.frag engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/mesh.vert engine/src/renderer.cpp engine/src/renderer_orthographic.cpp engine/src/renderer_internal.hpp engine/include game desert examples`: no output.
5. `grep -rn "uViewModelDepth\|kViewModelStep\|viewModelAt" engine game desert examples`: matches only in `engine/shaders/edge_perspective.frag` and `engine/src/renderer_perspective.cpp`.
6. Captures, into `docs/tasks/view-model-edges/`:
   - `build/release/desert/pxe_desert --capture docs/tasks/view-model-edges/edges-day.png`;
   - `... --hour 21 --capture docs/tasks/view-model-edges/edges-night.png`;
   - the same two from the base binary (the directory `tools/perspective-check.sh` prints), as `base-day.png` and `base-night.png`.

   For each pair, run `python3 -I docs/tasks/view-model-edges/scripts/diff_box.py base-day.png edges-day.png` (and night). Every differing pixel must be in row 351 or below, the lower 35% of the 540-row view, where the arms are. Record the output in the handoff.

   `edges-day.png` should show:
   - a dark outline round both arms against the sand, including the tops of the gloves, which carried a light rim before;
   - dark inner outlines where the thumbs and fingers cross the hands, and round the gauge's bezel and the bracer's straps;
   - no ink across the middle of a glove's back or a wrap band.

   Two runs of each capture are identical (`cmp`).
7. **The trunk, checked by a temporary edit.** Never commit it. Set `kSpawn` in `desert/src/main.cpp` to `{0.f, -0.71f}`, rebuild, and capture `edges-at-trunk.png`. The bark around the hands shows no new light or dark line along the arms' outline beyond the arms' own ink. Revert, `touch desert/src/main.cpp`, rebuild, and confirm a fresh `--capture` byte-matches `edges-day.png`.

## Acceptance criteria
- [x] `edge_perspective.frag` reads a pixel's true depth from `viewDepth`, stretching depths below `uViewModelDepth` back over the whole range, and has the binding uniform, constant and `viewModelAt` above. Evidence: `viewDepth`, `uViewModelDepth`, `kViewModelStep` and `viewModelAt` in `engine/shaders/edge_perspective.frag`, binding text.
- [x] A view-model pixel is outlined against any scene or sky neighbour, and against a view-model neighbour more than `kViewModelStep` (0.01 m, or a recorded value within 0.005–0.02) plus the edge-on allowance further away. Its silhouettes use `min(mix(col, uInk, 0.65), col * 0.8)`, before rims and crease ink. Evidence: `edges()`: `apart = vm ? kViewModelStep : ...`, `behind = (vm && !vmq) || dq - d > tol`, early `return min(mix(col, uInk, 0.65), col * 0.8)` before the rim and crease-ink returns; `kViewModelStep` kept at 0.01.
- [x] A view-model pixel takes a light rim only at an upward crease, never on a silhouette. Evidence: `edges()` `if (vm)` branch sets `rim` only on `crease && nq.y < n.y - 0.3`, and `crease` needs `!behind`; glove-top rims gone in `edges-day.png`.
- [x] A scene pixel skips its view-model neighbours. A scene pixel's own rules, with no view-model neighbour, are unchanged. Evidence: `if (!vm && vmq) continue;`; the scene branch is unchanged text; `example-first-person` identical; `edges-at-trunk.png` shows no new line on the bark.
- [x] `renderer_perspective.cpp` sets `uViewModelDepth` to 0 with no view model and to `kViewModelDepth` with one, and nothing else in the file changes. Evidence: `git diff 37c2178 -- engine/src/renderer_perspective.cpp` is the 3 binding lines.
- [x] The header comment gains the binding paragraph. Evidence: header of `edge_perspective.frag`, after the "Dark regions" paragraph.
- [x] Verification 2–5 pass: ortho 6/6, `example-first-person` identical, the diff empty, the grep only in the two files. Evidence: my runs: `ortho-check: 6/6`, `perspective-check: 1/3` with `example-first-person` PASS, protected-file diff 0 lines, grep only in the two files.
- [x] The captures and `diff_box.py` output are in the task folder. Every differing pixel is in rows 351–539. The captures show what Verification 6 describes and are deterministic. Evidence: `diff-day.txt`/`diff-night.txt`: 1827 pixels, rows 425..539; captures inspected; cycle 2 fresh captures `cmp`-identical to the committed ones.
- [x] The engine names no game content. Evidence: review of the diff; the new comments speak of the view model only.
- [x] Project builds cleanly from a fresh build directory with CMake. Evidence: my fresh `check` build in cycle 2: exit 0, only the stb `sprintf` warning.

## Assets
None.

## Out of scope
- Any change to the orthographic mode, its edge pass, or the shared shaders and renderer code
- Game-specific shaders, or a mechanism for games to supply shaders
- The view model's lighting, haze or depth squeeze (`kViewModelDepth` and its draw block)
- Edge rules for scene surfaces
- Any change to `desert/` or `game/`

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-29: base 37c2178
- Cycle 1 review: build clean, ortho 6/6, example-first-person identical, desert differs only in rows 425-539; captures checked. One minor issue: ISSUE-001.
- Cycle 2, 2026-10-29: base 9308ab5. Outstanding: ISSUE-001
- Cycle 2 review: fresh build clean; the block moved unchanged; fresh day and night captures byte-match cycle 1's. ISSUE-001 resolved. Complete.

## Playtest checklist
- `build/release/desert/pxe_desert`: look down at both arms against the sand. Each arm has a thin dark outline; the glove tops no longer carry a light rim.
- Move the mouse so the arms pass over the sky and the horizon sand: the outline holds against both.
- Look at the left gauge and the bracer straps, and where the thumbs and fingers cross the hands: dark inner lines, and no ink across the middle of a glove's back or a wrap band.
- Walk into the palm trunk until the hands are against the bark: no flicker or extra light or dark line on the bark along the arms' outline.
- Sprint and turn quickly: the inner outlines stay steady, not shimmering.
- `tools/build.sh release desert -- --hour 21`: the same at night.
- The ruins game is unchanged (ortho check 6/6); no need to playtest it for this task.
