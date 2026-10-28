## Cycle 1
Build: pass

Fresh build check (`cmake --preset check --fresh`, `cmake --build --preset check --clean-first`) passes. The only warning is stb_image_write's `sprintf` deprecation.

### Changes
- `engine/include/pxe/first_person_camera.hpp`, `engine/src/first_person_camera.cpp` (new): `pxe::FirstPersonCamera` has an eye (`setEye`), free yaw (`setYawRadians`), pitch (`setPitchRadians`, positive looks down as `Camera`'s 30° does, held within ±89°), vertical FOV (`setFovYRadians`, default `kDefaultFovY` = 60°) and far plane (`setFarPlane`, default `kDefaultFar` = 300 m). The near plane is fixed at `kNear` = 0.1 m. `frame(rtW, rtH)` returns a perspective `CameraFrame`: the eye is not snapped, `texelError` is zero, the basis and hand-built view are the same as `Camera`'s, and the projection is `glm::perspective`.
- `engine/include/pxe/camera.hpp`: `CameraFrame` gains `Projection projection` (`kOrthographic` or `kPerspective`, defaulting to orthographic) and `fovY` (radians). Its comment says which fields mean nothing under each kind.
- `engine/src/camera.cpp`: `Camera::frame` sets `projection = kOrthographic` explicitly. Every other field is set exactly as before.
- `engine/include/pxe/scene.hpp`, `engine/src/scene.cpp`: `Scene::skyHorizon[kPhaseCount]` and `skyZenith[kPhaseCount]`, with default palettes for day, golden, twilight and night set in `Scene()`.
- `engine/include/pxe/lighting.hpp`, `engine/src/lighting.cpp`: `Lighting::skyHorizon` and `skyZenith`, blended in `lightingAt` with the same phase weights as the surface palettes, rim, ink and the window sky colour.
- `engine/shaders/backdrop.frag` (new, added to `PXE_SHADERS`): the sky backdrop. It draws a full-target triangle (it reuses `edge.vert`) at depth 1, tested less-or-equal, so it fills only empty pixels. The colour is a horizon-to-zenith gradient by the view ray's climb, in 6 flat bands, with the horizon colour below the horizon. It writes no normal, so the edge pass treats it as background.
- `engine/shaders/edge.frag`: a `PERSPECTIVE` variant behind `#ifdef`, built as a third edge program.
  - World positions are rebuilt from the perspective depth buffer (`rayAt`, `viewDepth`, `worldAt`).
  - The silhouette and crease depth step is `kSilhouette` scaled by distance once a pixel is wider than the orthographic view's texel. On top of that, each neighbour gets an allowance for this pixel's own plane carried onto the neighbour's view ray, so surfaces seen at a grazing angle don't ink.
  - Crease ink compares normals against the per-pixel view direction instead of `uFwd`.
  - Haze: surfaces fade into the horizon colour from 0.6 × far to far, by distance from the eye, in 8 flat steps. The last step is the full horizon colour.
  - Without `PERSPECTIVE` the code is the original. The only source change on that path is that the literal `kSilhouette` in the neighbour loop became a local `const float tol = kSilhouette`. Output is byte-identical (see below).
- `engine/include/pxe/renderer.hpp`, `engine/src/renderer.cpp`:
  - `render()` now copies `FrameParams` for a perspective frame and switches off what perspective doesn't support: `showGrass`, `fire`, `rays`, `fireflies`, `lights`, `grassFire`, `igniteGrass` and `gridStrokes`. It then calls the private `draw()`, which holds the old body unchanged. Orthographic frames go straight to `draw()` with the caller's params.
  - In `draw()` under perspective:
    - `fitLightAhead` fits the sun and lightning maps to a 60 m radius (`kShadowReach`) round the eye, pushed 30 m ahead along the view's flat forward. The centre is snapped to the map's texels in light space so shadow edges don't crawl as the eye moves. The depth range reaches back past the whole scene, so out-of-area casters still cast into it.
    - Water and its reflection are skipped.
    - The backdrop is drawn after the meshes and before the window panes.
    - The perspective edge program is used with its uniforms, and `uRain` is 0.
  - Shadow and flash maps now also redraw when their fit matrix changes (`shadowFit_`, `flashFit_`). For the orthographic camera the fit changes only with the light direction or an upload, both of which already redrew the maps.
  - The overcast greys the sky colours as it already greys window panes (`overcastSky`).
  - `rayThrough` and `windowPoint` are documented as orthographic only.
- `engine/include/pxe/window.hpp`, `engine/src/window.cpp`: `Window::setCursorCaptured(bool)` uses GLFW's disabled cursor (hidden, held, unbounded motion), plus raw motion where it is supported. `Window::cursorCaptured()` reports the state.
- `engine/include/pxe/input.hpp`, `engine/src/input.cpp`: `Input::mouseDelta()` returns the motion since the previous update in framebuffer pixels, with +y down. It is zero on the first update and on the update after a capture or release.
- `engine/CMakeLists.txt`: adds `src/first_person_camera.cpp` and `backdrop.frag`.
- `examples/minimal/main.cpp`:
  - `--first-person` draws through a `FirstPersonCamera` at `(-4, ground + 1.7, -7)`, turned toward a point between the stone and the tree. Its far plane is 50 m, just past the ground's far edges, so the ground edge fades into the sky.
  - In the window it captures the cursor and turns the view by `mouseDelta()`, with pitch clamped to ±85°.
  - `--hour H` (0-24) replaces the fixed 16.5. I added it to make the look-gate captures; `frameParams` takes the hour.
  - Arguments are now parsed in a loop, and a bad argument prints the usage line and exits 1.
  - `World` gains `eye` and `lookAt`.
- `docs/engine-guide.md`:
  - A new "First-person camera" section covers `FirstPersonCamera`, the sky, haze, outlines, shadows, cursor capture, mouse delta, and the "not yet supported under the perspective camera" list.
  - The intro, start-up order, frame-loop text, Scene field list and header reference are updated.
  - The snippets are updated. A script checked every example-derived snippet against `main.cpp`: all match, and the same non-example snippets fail to match as on the base commit.
- `docs/architecture.md`: the "Where things live" table gets a row for the perspective camera, sky backdrop, haze, eye-fitted shadow map and perspective edge pass. The input-devices row now names mouse motion and cursor capture. I edited this file because the orchestrator asked for it in this cycle's instructions.
- `README.md`: the perspective camera in the rendering paragraph, and the example's `--first-person` and `--hour`.
- `CLAUDE.md`: the standard byte-identical cases now include the example's `--capture` and `--first-person --capture`.
- `docs/tasks/perspective-camera/first-person-hour-{10,16.5,17.5,18.5,22}.png`: first-person captures for the look gate, covering day, the default afternoon, golden hour, twilight and night. I used 17.5 and 18.5 instead of 19.5, which is already full night in this scene.

### Verification
- **Byte-identical to base 4b0c221.** I built the base from `git archive` in a scratch dir (Release), then ran both binaries.
  - Game captures were identical (`cmp`) and the logs matched, apart from the output-file line, for the default, `--scene hills`, `--hour 21 --foxes 3`, and `--hour 21 --candles --shelter-test --extra-fires 6`.
  - `--self-test` passed on both, with identical logs.
  - `pxe_minimal --capture` was identical, and its logs matched.
- **First-person determinism.** Two runs of `pxe_minimal --first-person --capture` produced identical PNGs. A rebuilt binary also matches the committed 16.5 capture.
- **Visual check (I read the PNGs).**
  - The sky shows six bands, curved as a perspective view makes them, and the colours change through day, golden, twilight and night.
  - The ground's far edge fades into the horizon colour in steps.
  - Outlines are 1 px on the stone and slab silhouettes, and bark has its faint outline.
  - Shadows from the stone and tree are cast correctly at 10, 16.5 and 22 (moonlight).
  - The grass and crown are bare faceted surfaces, and there are no flames, as expected.
- **Ink thresholds.** I made a throwaway build with the example's ground switched to stone (reverted, not committed). The distant edged terrain did not fill with ink; only the real hill-crest silhouette took a rim.

### Issues addressed
- None (cycle 1).

### Layering
- Engine:
  - `pxe::FirstPersonCamera` (new header and source).
  - `CameraFrame::projection` and `fovY`.
  - `Scene::skyHorizon` and `skyZenith`, with their defaults.
  - `Lighting::skyHorizon` and `skyZenith`, and their blend in `lightingAt`.
  - Renderer: `fitLightAhead`, `overcastSky`, the private `draw()`, the backdrop pass, the perspective edge program, and shadow fit tracking.
  - `backdrop.frag`, and the `PERSPECTIVE` variant of `edge.frag`.
  - `Window::setCursorCaptured` and `cursorCaptured`.
  - `Input::mouseDelta`.
- Game: none. `game/` is unchanged.
- Example: `examples/minimal/main.cpp` creates and owns its `FirstPersonCamera` and does its own mouse look and pitch clamp.
- Misplaced code touched: none.

### How to playtest
Run `./build/release/examples/minimal/pxe_minimal --first-person` (window mode):
- The window opens and the cursor disappears. The view looks at the stone (left), the slab (centre) and the tree (right) from standing height.
- Move the mouse right: the view turns right. Move it left: the view turns left. Mouse down looks down and mouse up looks up.
- Pitch stops at about 85° up and down. Check it never flips at the stops.
- Turn a full circle and look for edge crawl on the stone, slab and trunk outlines while turning. This is accepted for now; judge whether it's tolerable.
- Look along the ground in every direction. The ground's edge should fade into the sky's horizon colour, not end in a hard line.
- Look straight up: the sky's top band is the zenith colour, and there is no seam.
- Shadows should stay put on the ground while you turn the view, because the shadow map follows the eye and is texel-snapped. Any shimmer of shadow edges while turning is worth noting.
- On the first frame after the window opens, the view should not jump.
- Escape closes the window.
- Try `--hour 10`, `--hour 17.5`, `--hour 18.5` and `--hour 22` to see each sky palette.
- `pxe_minimal` without `--first-person` should behave exactly as before: Q and E turn the orthographic view, and the cursor is visible.

### Known limitations
- Not yet supported under perspective, and skipped whatever `FrameParams` says: grass and leaf sprites, god rays, rain, fires (flames, smoke, firelight) and the grass fire, fireflies, and point lights.
  - **Two additions beyond the spec's list.** I also switched off water (sheets and reflections) and all `FrameParams::lights` point lights. The reason is that `water.frag`'s depth-to-bed maths and the point lights' tile culling both assume the orthographic projection, so leaving them on would draw them wrongly rather than not at all.
  - `Renderer::rayThrough` and `windowPoint` stay orthographic only.
  - All of these are documented in `docs/engine-guide.md` and the `Renderer::render` comment.
- **Haze steps.** Haze is quantised into 8 flat steps to match the banded sky and toon ramps, so on long rolling terrain the steps show as contour-like bands toward the far plane. That is a look-gate call; it is the single constant `kSteps` in `edge.frag`.
- **Mouse delta units.** `Input::mouseDelta` is in framebuffer pixels, the same units as `mousePosition`. On a high-DPI display one point of mouse motion is two pixels, so look sensitivity per inch differs between displays unless the game scales it.
- **Indoors.** Under perspective, an indoor scene also gets the sky backdrop and haze wherever nothing is drawn. Window panes still draw over it in their own sky colour, as the spec requires. No indoor scene has been tested under perspective.
- **Mouse look is untested here.** Keyboard and mouse play isn't exercised by any check. It needs the playtest above.
