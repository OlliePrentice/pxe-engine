# Perspective first-person camera
Type: feature
Status: complete
Cycle: 3

## Goal
The engine can only draw through its orthographic camera (`pxe::Camera`: 30° pitch, 45° yaw steps, texel-snapped eye). Add a perspective first-person camera as a general engine capability: an eye position, free yaw and pitch, and a vertical field of view. The renderer draws a scene through it with a sky behind the geometry and distance haze, so a game can be played from eye height. This is the engine half of a first-person desert game (task `desert-poc`, which waits on this one). The orthographic path, and so the existing game's output, must not change at all. `examples/minimal` gains a first-person mode, so the capability can be checked before any game uses it, and so the user can judge whether the art style holds up in first person before the desert game is built.

## Decisions
- **Separate camera class.** A new `pxe::FirstPersonCamera` with eye position, yaw, pitch and vertical field of view. It produces the same `pxe::CameraFrame`, which gains a projection kind (orthographic or perspective) and `fovY`. The renderer branches on the projection kind. `pxe::Camera` is not changed, so the orthographic path stays byte-identical.
- **Same internal resolution.** The 960×540 low-res target, padding and integer upscale are unchanged under perspective.
- **No snapping under perspective.** A perspective camera cannot be snapped onto the texel grid, so its eye is not snapped and its `texelError` is zero. The user accepts some edge crawl when turning and walking for now, and judges it at the look gate below.
- **Sky backdrop.** Wherever no geometry is drawn, the perspective view shows a sky: a vertical gradient from a horizon colour to a zenith colour, quantised into flat toon bands to match the surfaces. It has default palettes for day, golden, twilight and night, blended with the time of day the same way the surface palettes are. It has no sun or moon disc, stars or clouds. It is drawn only under the perspective camera. Window panes in indoor rooms keep their current sky colour.
- **View distance and haze.** The perspective camera's far plane is a parameter, default 300 m. Surfaces fade into the sky's horizon colour over the last 40% of the far distance, so the end of the geometry never shows as a hard edge. Perspective only.
- **Passes under perspective.** Only what the first user (the desert game) needs is ported. The rest is switched off when the camera is perspective and ported by its own task when a game first needs it:
  - Ported: the edge/ink pass. Lines stay 1 px. Depth thresholds scale with distance, so far terrain doesn't turn into solid ink. World positions are rebuilt from the perspective depth buffer.
  - Switched off: god rays, grass and leaf sprites, rain, flames and fire light circles, fireflies.
  - `docs/engine-guide.md` documents the switched-off list as "not yet supported under the perspective camera".
- **Shadow fit.** Under perspective, the single shadow map is fitted to a 60 m radius around the eye, pushed ahead along the view direction. The radius is an engine default, not a parameter, until a game needs to set it. Beyond it nothing is shadowed.
- **Field of view.** Default 60° vertical, settable by the game.
- **Mouse look needs input support.** The engine gains cursor capture on `pxe::Window` (the cursor hidden and held, with unbounded motion) and a per-frame mouse motion delta on `pxe::Input`. These are input-device machinery, which `docs/architecture.md` places in the engine.
- **Example flag.** `pxe_minimal --first-person` renders the existing scene from a fixed eye at standing height (1.7 m above the ground) looking at the stone and tree. It works with `--capture`, which stays deterministic. In the window, the mouse looks around, with no walking. Under perspective the example's grass and tree crown show as bare faceted surfaces with no sprites, and the campfire has no flames or light pool. That is expected.
- **Walking in the example (added at the look gate, at the user's request).** Turning alone can't show how much edges crawl when the eye moves, so `--first-person` also walks: WASD at 3.5 m/s, Shift sprint at 6 m/s, relative to the view's flat forward; the eye stays 1.7 m above `groundHeight`, lightly smoothed; the walk is held inside the ground's extent (kept 2 m inside its edges). No collision with the stone, slab or tree. The cursor is captured while walking (`Window::setCursorCaptured`); Esc releases it, Esc again while released quits, a click captures it again. Without `--first-person` the example's controls are unchanged. `--capture` stays deterministic (no input read). The player code is the example's own; the desert game writes its own copy, since the player is game code (`docs/architecture.md`).
- **Look gate.** When this task passes review, the orchestrator stops and gives the user first-person captures at several hours plus a checklist for turning the view in the window. `desert-poc` starts only once the user approves the look.
- **Also switched off under perspective (cycle 1 review).** Water and its reflections (`water.frag` assumes the orthographic depth buffer) and all `FrameParams::lights` (their screen-tile culling assumes the orthographic projection). `Renderer::rayThrough` and `windowPoint` are orthographic only. The desert game needs none of them, so this follows the "port only what the first user needs" decision; each is ported when a game needs it.
- **Standard byte-identical cases.** The example's `--first-person --capture` is added to the refactor cases in `CLAUDE.md`. The desert game's capture is added by `desert-poc`.

## Layering
- Engine:
  - `pxe::FirstPersonCamera` (new header and source): eye, yaw, pitch, vertical FOV, far plane, producing a `pxe::CameraFrame`.
  - `pxe::CameraFrame` gains a projection kind and `fovY`. Orthographic frames keep every existing field value.
  - Renderer: perspective branches for the edge pass, shadow fit, sky backdrop and haze, and switching off the unsupported passes. The sky's default palettes and its blending go with the existing lighting and palette code.
  - New sky shader(s) in `engine/shaders/` and `PXE_SHADERS`, or the existing `sky.vert`/`sky.frag` extended, without changing how window panes render.
  - `pxe::Window`: cursor capture. `pxe::Input`: mouse motion delta.
- Game: none. `game/` is not changed.
- Example: `examples/minimal/main.cpp` gains `--first-person`, using `pxe::FirstPersonCamera` and the mouse delta.

## Acceptance criteria
- [x] `pxe::FirstPersonCamera` exists in `engine/include/pxe/` with setters or fields for eye position, yaw, pitch, vertical FOV (default 60°) and far plane (default 300 m), and a `frame(rtW, rtH)` returning a perspective `CameraFrame` — `engine/include/pxe/first_person_camera.hpp`, `FirstPersonCamera::frame`
- [x] `CameraFrame` has a projection kind and `fovY`. `pxe::Camera::frame` sets the orthographic kind and every field it set before, unchanged — `camera.hpp` `CameraFrame::Projection`, `Camera::frame`; byte-identical verified
- [x] Perspective frames are not snapped: `texelError` is zero and the eye is the camera's eye — `FirstPersonCamera::frame` sets `texelError` 0, `eye = eye_`
- [x] The edge pass rebuilds world positions correctly from the perspective depth buffer, and its depth-step thresholds scale with distance under perspective. The orthographic code path is unchanged — `edge.frag` `PERSPECTIVE`: `viewDepth`, `worldAt`, distance-scaled `tol`
- [x] A sky gradient (horizon to zenith, flat bands) fills every pixel with no geometry under perspective, with default day, golden, twilight and night palettes blended by time of day. It is not drawn under the orthographic camera — `backdrop.frag`, `Renderer::draw` perspective block, `Scene::skyHorizon/skyZenith`, `lightingAt`
- [x] Haze fades surfaces into the horizon colour over the last 40% of the far distance under perspective only — `edge.frag` `distanceHaze`, `uHazeSpan` 0.6–1.0 × far
- [x] Under perspective, the shadow map fits a 60 m radius around the eye, pushed ahead along the view — `renderer.cpp` `fitLightAhead`, `kShadowReach` 60
- [x] Under perspective, god rays, grass and leaf sprites, rain, flames, fire light circles and fireflies are skipped — `Renderer::render` cuts `FrameParams`; water and point lights off too (Decisions)
- [x] `pxe::Window` can capture and release the cursor. `pxe::Input` reports the mouse motion since the previous update — `Window::setCursorCaptured`, `Input::mouseDelta` (window points, ISSUE-001)
- [x] `pxe_minimal --first-person` walks: WASD 3.5 m/s, Shift 6 m/s relative to the flat view forward, eye 1.7 m above `groundHeight` with smoothing, held 2 m inside the ground's edges; cursor captured, Esc releases, Esc again quits, click recaptures; ortho controls unchanged — `examples/minimal/main.cpp` `walk()` and the first-person branch of the loop
- [x] `pxe_minimal --first-person` renders from a fixed eye 1.7 m above the ground looking at the stone and tree, `--capture` works with it and two runs produce identical PNGs, and in the window the mouse turns the view with pitch clamped to ±85° — `examples/minimal/main.cpp` `--first-person`, `kMaxPitch`; two captures identical
- [x] The engine names no game content and includes no game headers (`docs/architecture.md`) — cycle 1 and 2 diffs reviewed
- [x] Byte-identical to the base commit: the game's standard cases in `CLAUDE.md` (default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`, `--self-test`) and `pxe_minimal --capture` — rerun by the orchestrator against 4b0c221: all cases `cmp` identical
- [x] `docs/engine-guide.md` documents `FirstPersonCamera`, the sky, haze, cursor capture and mouse delta, and the passes not yet supported under perspective. Its snippets still match `examples/minimal/main.cpp` word for word — First-person camera section; snippets checked against main.cpp
- [x] `docs/architecture.md`'s "Where things live" table lists the perspective camera, the sky and haze — table row "Perspective first-person camera..."
- [x] `CLAUDE.md` lists the example's `--first-person --capture` as a standard byte-identical case. `README.md` documents the example's new flag — CLAUDE.md standard cases; README example flags
- [x] Project builds cleanly from a fresh build directory with CMake — fresh `check` build, cycle 2: only the stb warning

## Assets
None.

## Out of scope
- Any new game, and any change to `game/`
- Collision in the example
- New surfaces or materials
- Porting god rays, grass and leaf sprites, rain, flames, light circles or fireflies to perspective
- Sun or moon disc, stars or clouds in the sky
- Edge-crawl stabilisation under perspective

## Cycle log
- Cycle 1: 2026-10-28 started, base 4b0c221
- Cycle 1 review: build clean, byte-identical verified, first-person capture deterministic. ISSUE-001 open.
- Cycle 2: 2026-10-28 started, base 7326d56 (task.md log fix on top), fixing ISSUE-001
- Cycle 2 review: build clean, ISSUE-001 resolved. Task complete; waiting on the user's look gate.
- Cycle 3: 2026-10-28 started, base 5d7eb44, walking in the example (user request at the look gate)
- Cycle 3 review: fresh build clean, diff matches the walking decision, no issues. Task complete; waiting on the user's look gate. Walking and looking pause while the cursor is free; the example's movement step is not clamped after a long stall (accepted for the example; the desert spec clamps it).
