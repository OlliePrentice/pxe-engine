## Cycle 1
Build: pass

Verification (base 1722db0):
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: builds, the only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh 1722db0`: 6/6 identical.
3. `git diff 1722db0 -- mesh.frag common.glsl edge.frag renderer_orthographic.cpp shader.cpp game desert examples`: no output.
4. `tools/perspective-check.sh 1722db0`: desert-default, desert-night and example-first-person all ran and all differ (0/3 identical, as expected).
5. The three greps (`toon_[0][0]` in renderer_perspective.cpp, `uHazeSpan` in engine, `kBands` in backdrop.frag): no output.
6. Captures in this folder, each rendered twice (second time from another directory) and `cmp`-identical (`scripts/capture-all.sh` makes them). Everything below is how they look to me:
   - `d07.png` (hour 7): low warm sun from the left. The dune faces turned to it are warm orange-ochre and fall off smoothly into cooler brown on the far sides. The palm's long shadow runs back-left across the sand with a soft, dark edge about 2-3 px wide. Sky is a smooth pale blue-grey gradient to a warm cream-orange at the horizon. The sun is not in view (the view faces the palm). Distance fades smoothly into the horizon colour.
   - `d12.png` (hour 12): bright sand, near-flat light with a faint gradient. The palm's shadow is a short, soft-edged crown and trunk shadow at its foot. Sky is a smooth blue gradient, paler at the horizon. Horizon haze is soft and starts close in.
   - `d1630.png` (default 16.5): golden-hour sand, darker and redder-brown than noon. The sunward dune faces are lit, and the dune faces turned away are darker. A soft shadow of the palm runs right along the sand. Sky is a smooth blue-grey gradient with a pale horizon. A soft darker band on the far left dune is a dune face turning from the light, with no hard step.
   - `d1818.png` (hour 18.3): twilight. Sand is dark mauve-brown with a faint cool tint, sky a smooth violet gradient to a pink-mauve horizon. The shadow has vanished (the sun is on the horizon). The sun and its disc are not in view (they are behind the camera), so the disc is not shown in this capture.
   - `d21.png` (hour 21): night. A deep navy gradient sky and dark blue-brown sand. The moon's light gives a cool dim shadow of the palm, soft-edged, running back-left.
   - `walk.png` (`--walk 2`): the same golden-hour light seen after walking two seconds. The palm is closer and the crown is cut off at the top. The shadow is still soft. Haze fades smoothly into the horizon.
   - `example-fp.png` (`pxe_minimal --first-person`): green lawn lit smoothly. The box's lit face is warm tan and its shadowed side is a dark slate-blue (sky fill only). The sphere crown is shaded continuously from light green to dark green with no bands. Soft cast shadows from the box and tree stretch across the grass. Sky is a smooth blue gradient to a cream horizon with soft haze on the far hills.
   - In all of the desert captures, the palm and arms keep their outlines and rims, the arms are lit by the same model (their wraps are darker and browner than before: the stone-ramp tint is gone, as the spec accepted), and the glowing screen and dial parts stay full colour.
7. By reading: `mesh_perspective.frag` writes `oNormal` as `mesh.frag` does (`surface + 1` for scene surfaces; actors `kSurfStone` or `kSurfActorCrease`). `progMeshLit_` is linked only inside the `kPerspective` branch of `Renderer::init`. `git grep` finds no read of any new `Lighting` field outside `lighting.cpp` and `renderer_perspective.cpp`.

### Changes
- `engine/include/pxe/scene.hpp`, `engine/src/scene.cpp`: the binding `Scene` fields (per-phase sun/moon, sky fill and ground bounce colours and strengths, `albedo`, `hazeDensity`, `sunGlow`, `shadowPenumbra`, `exposure`) with the Decisions-table defaults; `albedo[s]` from the day mid band; the sky comment no longer says "flat bands".
- `engine/include/pxe/lighting.hpp`, `engine/src/lighting.cpp`: the binding `Lighting` fields, filled in `lightingAt(scene, hour)` with a file-local exact-sRGB `toLinear`.
- `engine/shaders/lit_perspective.glsl` (new), `engine/shaders/mesh_perspective.frag` (new), `engine/CMakeLists.txt`: the lit model, soft 16-tap shadow with receiver-plane bias, continuous cloud shade, softened ridge, linear lighting with sRGB write; both in `PXE_SHADERS` in sorted order.
- `engine/include/pxe/renderer.hpp`, `engine/src/renderer.cpp`: `progMeshLit_` (linked in perspective mode only, deleted in the destructor); new perspective `render()` contract with the extra not-yet-supported items.
- `engine/src/renderer_internal.hpp`, `engine/src/renderer_perspective.cpp`: `LightFit::depthSpan`; `bindLit` and a file-local `toLinear` replace `bindToon` and the actor tint block; the backdrop gets `uSunDir`/`uSunColour`/`uSunGlow`/`uOvercastDisc`; the edge pass gets `uHazeDensity` and the sun uniforms in place of `uHazeSpan`. `bindLit` also takes `params` (for the actor materials), which the spec's signature sketch did not list.
- `engine/shaders/backdrop.frag`, `engine/shaders/edge_perspective.frag`: continuous sky with glow and disc; exponential haze with sun glow.
- `engine/include/pxe/first_person_camera.hpp`: `setFarPlane` comment.
- `docs/tasks/perspective-smooth-lighting/`: captures, `scripts/capture-all.sh`.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `Scene` light fields and `albedo`; `Lighting` blended lights, `sunDir`, `sunGlowColour`, `phaseWeight` and copies; `Renderer::progMeshLit_`; `LightFit::depthSpan`; shaders `lit_perspective.glsl`, `mesh_perspective.frag`, and edited `backdrop.frag`, `edge_perspective.frag`.
- Game: none. The desert and `pxe_minimal --first-person` take the engine defaults.
- Misplaced code touched: none.

### How to playtest
- `pxe_desert` at several hours (`--hour 7`, `12`, `16.5`, `18.3`, `21`) and walk about: slopes shade continuously, lit slopes warm and turned-away ones cooler, the palm's shadow edge is soft, and distance fades smoothly. Look toward the sun at around 18.3 and at 7 to see the haze and sky glow and the sun disc. The captures happen to face away from the sun, so the disc and glow were not seen in them.
- Turn the view slowly while the shadow is on screen: the fixed 16-tap pattern should not crawl.
- Rain and lightning are not available in the desert, so the overcast and flash paths were only read, not seen.
- `--test-colours` has no perspective entry point in the desert or example, so the red/green/blue test view was not seen.

### Docs
- `docs/architecture.md`: the "Where things live" perspective row should say the sky is a continuous gradient with sun glow and disc, haze is exponential, and the lit model is `mesh_perspective.frag` + `lit_perspective.glsl`.
- `docs/engine-guide.md`: `Scene` perspective light fields, `albedo` (and that a game changing a day mid band sets the albedo itself), the `Lighting` additions, and the perspective `render()` contract (new not-yet-supported list: indoor window and fill light, dark regions, firelight). `pxe_minimal --first-person` now looks different, but the example's code is unchanged.
- `README.md`: none, unless it describes the perspective look.

### Known limitations
- `Lighting::sunDir` is `sun` as §2 says, so it is the sun's direction even at night (the moon gets no disc and no glow, as Decisions say), but the binding comment on the field says "whichever of sun and moon lights the scene". I followed the code. The comment is slightly wrong and may need a later spec fix.
- Rain, the lightning flash, the dark-region and indoor drop and `testColours` paths were not exercised by any capture; they compile and link (the program links and runs in every capture) and were checked by reading.
- A scene mesh with a surface index of 12 to 15 would index `uAlbedo` out of range, as the spec's `uAlbedo[vSurface]` does. No current mesh has one.
- The sun disc and glow are not visible in any of the required captures (the camera faces away from the sun in all of them).

## Cycle 2
Build: pass (fresh `check` preset, clean build; only the expected stb_image_write `sprintf` warning)

### Changes
- `engine/include/pxe/lighting.hpp`: reworded the comment above `Lighting::sunDir` to say it points toward the sun even below the horizon, that the moon gets no disc or glow (the glow fades as the sun sets), and that the colour is the sun's display colour blended by phase. Comment only.
- `docs/tasks/perspective-smooth-lighting/issues.md`: ISSUE-001 set to `fixed`, with fix notes.

### Issues addressed
- ISSUE-001: `sunDir` comment now matches `lightingAt` (`out.sunDir = sun`).

### Layering
- Engine: comment change only; no new or changed types or functions.
- Game: none.
- Misplaced code touched: none.

### How to playtest
- Nothing to play: the change is a comment. `tools/ortho-check.sh eb84614`: 6/6 cases identical. `tools/perspective-check.sh eb84614`: 3/3 cases identical (base eb84614 = "cycle 1: review").

### Docs
- None beyond the cycle 1 entry. The cycle 1 known limitation about the `sunDir` comment is resolved.

### Known limitations
- Unchanged from cycle 1, except the `sunDir` comment limitation, which is fixed.
