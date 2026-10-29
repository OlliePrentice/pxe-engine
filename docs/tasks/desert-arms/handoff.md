## Cycle 1
Build: fail as the spec is written (renderer_perspective.cpp does not compile); pass with one extra engine line the spec does not name. Nothing code-side is committed.

### Spec infeasible
The spec's `## Approach` step 2 draws the view model between `glDepthRange(0.0, kViewModelDepth)` and `glDepthRange(0.0, 1.0)`, and says "No other engine file changes". The engine loads its GL entry points itself (`engine/src/gl.hpp`, the `PXE_GL_EACH` table) and `glDepthRange` is not in it. With the spec's code exactly as written:

    engine/src/renderer_perspective.cpp:254:5: error: use of undeclared identifier 'glDepthRange'
    engine/src/renderer_perspective.cpp:265:5: error: use of undeclared identifier 'glDepthRange'

No other call works either: `glDepthRangef` is not loaded either, and neither is declared without a table entry. The binding block can't compile without `engine/src/gl.hpp` changing, which the spec doesn't name, so I stopped as the rules require and did not add it.

What I tried: I added one line to the `PXE_GL_EACH` table, `M(PFNGLDEPTHRANGEPROC, glDepthRange)`, after the `glDepthMask` entry, and implemented all of steps 2 and 3 as written. Everything verified (below). Then I discarded all of it so the branch stays as the cycle left it. The full work is saved outside the repo, in `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/1c2863cb-0adb-420a-85d6-394fa258a7e4/scratchpad/desert-arms-cycle1/`: `tracked-changes.patch` (every tracked change including the gl.hpp line), `arms.hpp`, `arms.cpp`, and the six captures.

Evidence with that one line added (build: only the stb sprintf warning, no new warnings):
- `tools/ortho-check.sh db97273`: 6/6 identical.
- `tools/perspective-check.sh db97273`: `example-first-person` PASS; `desert-default` and `desert-night` FAIL "png differs" (the intended change); 1/3 identical.
- `git diff db97273 -- engine/src/renderer.cpp engine/src/renderer_orthographic.cpp engine/shaders game/`: no output.
- Captures: day (16.5 h) twice byte-identical; morning, golden, night and the two clipping captures made.
- arms-day / morning / golden / night: both gloved hands in the lower corners, centres at about 450 px and 510 px of 960 (a quarter in from each side), knuckles about 20% up from the bottom, forearms leaving through the bottom edge, nothing above the lower 35% (the highest point is at about y 425 of 540, which is 21% up from the bottom; the limit is 35%), outline round the arms, light creases on the glove backs and wrap bands, left bracer with three teal bars at the bottom left, still bright teal at night (emissive). The right wrap is pale in day and bluish at night; in all four the arms darken with the light as the sand does.
- arms-at-trunk (kSpawn = {0, -0.71}): both hands and forearms whole in front of the bark, not cut. The trunk fills the middle of the view and the hands overlap its base.
- arms-looking-down (pitch 1.2): both arms whole, unchanged in the view, in front of the plain sand filling the rest of the view.
- Both temporary edits were reverted with the rest.

Alternatives, not built: (a) add `glDepthRange` to `gl.hpp`'s table, name it in the spec (a one-line, output-neutral change that the orthographic check also covers); (b) get the same squeeze without it by scaling clip z in the view model's projection, which departs from the binding block.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none committed.
- Game: none committed.
- Misplaced code touched: none

### How to playtest
- Nothing to playtest this cycle.

### Docs
- None this cycle.

### Known limitations
- The cycle's code is not on the branch; see Spec infeasible.

## Cycle 2
Build: pass. Fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`: the only warning is stb_image_write's `sprintf` deprecation. The cycle 1 work (saved patch, `arms.hpp`, `arms.cpp`) was reapplied and checked against revision 1 line by line.

### Changes
- `engine/src/gl.hpp`: `M(PFNGLDEPTHRANGEPROC, glDepthRange)` in `PXE_GL_EACH`, after the `glDepthMask` entry (the revision-1 addition; the loader had no `glDepthRange`).
- `engine/include/pxe/renderer.hpp`: `FrameParams::viewModel` (`std::vector<ActorDraw>`) after `actorEffects`, with the binding comment.
- `engine/src/renderer_perspective.cpp`: `kViewModelDepth = 0.01` in the anonymous namespace (binding text); the actor tint and material block also runs when `viewModel` is non-empty; after the window-pane block and before `endPass()`, the view-model block (depth range `[0, 0.01]`, depth test LESS, depth write on, back faces culled, the mesh program with `uViewProj`, `uMirror = identity`, `uClipBelow = -1e9`, `drawActors`, depth range restored to `[0, 1]`). `identity` and the other names were already in this function. `viewModel` is in no shadow pass, static-map logic or lightning map.
- `desert/src/arms.hpp`, `desert/src/arms.cpp` (new): `desert::Arms` as in the binding interface. `load()` reads `characters/arms.glb` into slots 0-4 and takes the rest pose. `pose()` builds the binding eye transform, skins, pushes the draw to `params.viewModel`, copies the materials into `actorMaterials[kFirstMaterial + i]` and sets `crease` on Wrap, Glove and Brass only. The five slot offsets are named constants in the anonymous namespace; `kStrap` and `kGlow` are `[[maybe_unused]]` (only the three crease slots are read).
- `desert/CMakeLists.txt`: `src/arms.cpp` added to `pxe_desert`.
- `desert/src/main.cpp`: includes `arms.hpp`; one `desert::Arms arms` loaded after `renderer.upload`; in the capture loop and the play loop `params` is non-const, the `PerspectiveFrame` is taken once, `arms.pose(view, params)` is called, and the same `view` is rendered. The header and `frameParams` comments updated. `frameParams` itself is unchanged.
- `desert/src/player.hpp`: the eye comment reworded as specified; no constant changed.
- `docs/tasks/desert-arms/arms-day.png`, `arms-morning.png`, `arms-golden.png`, `arms-night.png`, `arms-at-trunk.png`, `arms-looking-down.png`: the verification captures.

### Issues addressed
- none (no issues.md for this task; cycle 2 is the code half under spec revision 1)

### Verification
1. Fresh check build: only the stb_image_write `sprintf` warning. Pass.
2. `tools/ortho-check.sh db97273`: 6/6 identical (game-default, game-hills, game-night-foxes, game-night-fires, game-self-test, example-ortho).
3. `tools/perspective-check.sh db97273`: `example-first-person` PASS; `desert-default` FAIL "png differs"; `desert-night` FAIL "png differs"; summary `1/3 cases identical against db97273`. The two failures are the intended change.
4. `git diff db97273 -- engine/src/renderer.cpp engine/src/renderer_orthographic.cpp engine/shaders game/`: no output (0 bytes).
5. Captures, from the release build. `--capture` day and `--hour 21 --capture` night were each run twice and `cmp` reports identical. What each shows:
   - `arms-day.png` (16.5 h): the two gloved hands in the lower corners, the left centred at about x 235 and the right at about x 725 of 960 (about a quarter of the width in from each side). Knuckles at about y 430-440 of 540 (about 18-20% up from the bottom), forearms leaving through the bottom edge. Nothing above y 425 (the lower 21%; the limit is 35%). Both hands are dark leather with a thin light outline round the arms and light crease lines on the glove backs and fingers; the right forearm shows two pale cloth wrap bands with a darker band edge, and the left a leather strap and the brass bracer. The left bracer at the bottom-left corner shows three bright teal bars in a recessed window. The palm and its shadow are visible behind, unaffected by the arms.
   - `arms-morning.png` (10 h): same framing; brighter, warmer light on the bracer (more saturated brass), the palm's shadow falls to the left of the trunk; the three teal bars unchanged.
   - `arms-golden.png` (18.5 h): same framing; the whole scene is dim purple, the gloves and bracer darkened with the sand, the wraps pale violet; the three teal bars still bright.
   - `arms-night.png` (21 h): same framing; the gloves near black, the right wrap pale blue, the bracer dark olive; the three teal bars still bright (emissive), the only bright thing on the arms.
6. Clipping:
   - `arms-at-trunk.png` (temporary `kSpawn = {0.f, -0.71f}`): the trunk's near face fills the middle of the view; both hands, the forearms and the bracer are whole and drawn in front of the bark, the hands overlapping the trunk's base. Nothing is cut or hidden by the trunk.
   - `arms-looking-down.png` (temporary pitch `1.2f`): the view is plain sand; both arms are whole and unchanged in the view, in front of the sand.
   - Both temporary edits were reverted and `main.cpp` was `touch`ed and rebuilt; a fresh `--capture` byte-matches `arms-day.png`, so the binary is the committed source. `git diff db97273 -- desert/src/main.cpp` holds only this task's changes (the header and `frameParams` comments, the include, the `Arms` object, `params` non-const, `view`, `arms.pose`); no `kSpawn` or pitch line.
7. Designer previews and the build script's checks: the assets landed in `1c07899` (cycle 1 review). Not re-run this cycle; no asset file changed.

### Layering
- Engine: `FrameParams::viewModel` (`engine/include/pxe/renderer.hpp`), its draw in the perspective frame (`engine/src/renderer_perspective.cpp`), and the `glDepthRange` loader entry (`engine/src/gl.hpp`). The orthographic frame, `renderer.cpp` and every shader are unchanged. The engine names no arms, gloves or player.
- Game: `desert::Arms` (`desert/src/arms.hpp`, `arms.cpp`) owns the model, rest pose and joint matrices; `main.cpp` owns one `Arms`.
- Misplaced code touched: none

### How to playtest
- Run `build/release/desert/pxe_desert`. Both gloved hands rise from the bottom corners; the left wrist has the brass bracer with three teal bars. Turn the mouse and look up and down: the arms stay fixed to the view with no bob or lag.
- Walk up to the palm's trunk until the body-radius stops you (about 0.7 m from its centre) and turn so the trunk fills the view: the hands stay whole in front of the bark. Look straight down: the arms stay whole in front of the sand.
- Stand in the palm's shadow: the arms darken with the sand. Try `--hour 21`: the bars glow while the rest goes dark.
- Edge cases: the arms are in no shadow map, so they cast no shadow on the sand. The arms have an outline against the scene.

### Docs
- `docs/engine-guide.md`: `FrameParams::viewModel` (perspective mode only, characters attached to the view, drawn after the scene and sky into a squeezed depth range, in no shadow map, joints must last until the frame is drawn, materials from `actorMaterials`). Note a use: a game's view model is built per frame from the frame's `PerspectiveFrame`.
- `docs/architecture.md`: the "Where things live" row for perspective-mode frame passes gets the view model; `desert/` gets the arms.
- `README.md`: the desert game's features gain the player's arms.

### Known limitations
- The depth squeeze means the edge pass reads the view model as surfaces just past the near plane, so it is outlined against the scene and gets no haze; the glove outline is light grey rather than dark in the captures (as specified).
- The arms are drawn through the same field of view as the scene (no separate view-model FOV), as out of scope.
- Keyboard, mouse and gamepad play is not exercised by the captures; the playtest list above covers it.
