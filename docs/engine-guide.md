# Building a game on pxe

This guide is for anyone, person or model, starting a new game on the pxe engine. It covers how to add the engine to a project, the order things are created in, what a frame looks like, how to build a scene, and the contracts the engine relies on. It ends with a short reference to every public header.

A complete, small program follows it: [`examples/minimal/main.cpp`](../examples/minimal/main.cpp). It builds a procedural scene (rolling grass, a standing stone, a tree, a campfire on a slab), lights it at a fixed hour, and draws it in a window. With `--capture out.png` it renders a few frames offscreen, writes the view to a PNG and exits. `--first-person` shows the same scene from standing height through the perspective camera, turned by the mouse and walked with WASD, and `--hour H` lights it at another hour. The snippets below come from it. It is built with the rest of this repository, so it always compiles against the current headers.

## What the engine is and isn't
pxe is a C++20 engine for pixel-art 3D games in the style of t3ssel8r's videos. Everything is drawn into a low-resolution target (960x540 at 24 pixels per metre, `pxe::RenderConfig`) through an orthographic camera pitched 30° down, or through a perspective first-person camera (see [First-person camera](#first-person-camera)), then scaled up by a whole number to the window. Under the orthographic camera surfaces are lit with a hard three-step toon ramp per material; under the perspective camera they take a smooth lit model instead (see [First-person camera](#first-person-camera)). Grass and leaves are instanced sprites drawn in the fragment shader. On top of that it provides shadows, water reflections, outlines, god rays, rain, lightning, a day/night palette blend, campfires, a grass fire that spreads on the GPU, fireflies, skinned characters, input, a Dear ImGui overlay and audio output.

The engine supplies types and systems. It does not own your world. Your game creates, holds and destroys every object in it: the scene, its characters, its fires, its props. Each frame the game hands the renderer a description of what to draw (`pxe::FrameParams`), and the engine keeps no pointer to game objects between calls. The engine has no game loop, no entity system, no asset manager, no scene file format of its own beyond glTF geometry, no physics and no sound content. Your game decides what to load, where things go, what the keys do and what the player hears.

The rules for what belongs in the engine and what belongs in a game are in [docs/architecture.md](architecture.md). Read them before changing the engine.

## Adding pxe to a CMake project
The engine is the `engine/` directory of this repository. It is self-contained: every path in `engine/CMakeLists.txt` is relative to that directory or its build directory, and its shaders are compiled into the library. Link the `pxe_engine` target and include headers as `pxe/<name>.hpp`.

With the repository checked out next to your game (or as a submodule):

```cmake
cmake_minimum_required(VERSION 3.20)
project(mygame LANGUAGES CXX)

add_subdirectory(path/to/pxe-engine/engine pxe_engine)

add_executable(mygame main.cpp)
target_link_libraries(mygame PRIVATE pxe_engine)
```

The second argument to `add_subdirectory` is the build directory, which is required when the source is outside your project's tree.

Or fetched at configure time, using only the `engine/` subdirectory:

```cmake
include(FetchContent)
FetchContent_Declare(
  pxe
  GIT_REPOSITORY https://github.com/OlliePrentice/pxe-engine.git
  GIT_TAG <commit or tag>
  SOURCE_SUBDIR engine
)
FetchContent_MakeAvailable(pxe)

add_executable(mygame main.cpp)
target_link_libraries(mygame PRIVATE pxe_engine)
```

In this repository, the root `CMakeLists.txt` adds `engine`, the two games `game` and `desert`, and `examples/minimal`, and `examples/minimal/CMakeLists.txt` is the whole of what a game needs:

```cmake
add_executable(pxe_minimal main.cpp)
target_link_libraries(pxe_minimal PRIVATE pxe_engine)
```

**C++20.** `pxe_engine` sets `cxx_std_20` as a public compile feature, so anything linking it compiles as C++20 or later. The headers use `std::span`.

**Dependencies.** The engine fetches these with FetchContent the first time it is configured, so the first configure needs a network connection:

| Dependency | Version | How a game sees it |
|---|---|---|
| GLFW | 3.4 | Private. The engine owns the window; games use `pxe::Window` and `pxe::Input`. |
| glm | 1.0.1 | Public (`glm::glm`). Engine headers use glm vectors and matrices. |
| Dear ImGui | v1.91.8 | Public, as the `imgui` target. Include `<imgui.h>` to build UI between `Overlay::begin()` and `Overlay::end()`. |
| miniaudio | 0.11.21 | Private. Games use `pxe::AudioDevice`. |

cgltf, stb_image_write and the Khronos GL headers are vendored in `engine/third_party` and private. The engine links OpenGL (the framework on macOS, `opengl32` on Windows, `OpenGL::GL` elsewhere). It needs an OpenGL 4.1 core context. On Linux, GLFW's X11 backend needs the X11 development packages (`libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev` and `libgl1-mesa-dev` on Debian and Ubuntu).

**Known limits of the build.**
- `engine/CMakeLists.txt` sets GLFW's cache options with `FORCE` (no docs, tests, examples or install; X11 on, Wayland off). A consuming project can't override them.
- The engine defines a target named `imgui`. A project that defines its own `imgui` target will clash with it.
- Shaders live in `engine/shaders` and are embedded at build time. A new shader file must be added to `PXE_SHADERS` in `engine/CMakeLists.txt`, or `Renderer::init` throws "no embedded shader <name>". This only matters when changing the engine.

## Start-up order and ownership
Create things in this order and destroy them in reverse. Declaring them in this order in one scope does both.

1. **`pxe::Window`.** One per process. It starts GLFW, opens the window, makes its OpenGL 4.1 core context current on the calling thread and loads every GL entry point. Everything below that touches the GPU (`Renderer`, `Mesh`, `SkinnedMesh`, `Scene`, `SkinnedModel`, `Overlay`) needs it to exist and must be destroyed before it. Set `WindowOptions::visible` to false to draw offscreen: a hidden window still has its context and framebuffer. The constructor throws `std::runtime_error` on failure.
2. **`pxe::Renderer`.** Default-construct it, then call `init(mode)`, which compiles the shaders and makes the render targets. It throws if a shader fails to compile. `mode` is `pxe::RenderMode::kOrthographic` (drawn through `Camera`) or `kPerspective` (drawn through `FirstPersonCamera`), and it is fixed for the renderer's life: each mode has its own frame and edge pass and links only its own programs, so work on one never changes the other's output (`docs/architecture.md`, "Render modes"). A program that wants both views needs two renderers.
3. **Your scene.** Build a `pxe::Scene` (see [Building a Scene](#building-a-scene)), then pass it to `Renderer::upload`.
4. **`pxe::Overlay`**, if the game has UI. Construct it with the window, after the window and before the first frame. Destroy it before the window. Only one may exist at a time. It throws `std::runtime_error` if an ImGui backend can't start.
5. **`pxe::AudioDevice`**, if the game makes sound. See [Audio](#audio) for its ownership rule.
6. **`pxe::Camera`** or **`pxe::FirstPersonCamera`.** Plain values with no GPU state. Set where the orthographic `Camera` looks with `setYawRadians` and `setTarget`; place a `FirstPersonCamera` with `setEye`, `setYawRadians` and `setPitchRadians`.

From the example:

```cpp
// The window first: it makes the GL context everything after it needs.
pxe::WindowOptions options;
options.width = 1920;
options.height = 1080;
options.title = "pxe minimal";
options.visible = capture.empty();
options.vsync = capture.empty();
pxe::Window window(options);

pxe::Renderer renderer;
// The renderer is made for one camera for its whole life: the first-person
// view needs perspective mode, the orbiting one orthographic.
renderer.init(firstPerson ? pxe::RenderMode::kPerspective : pxe::RenderMode::kOrthographic);

World world = buildWorld();
renderer.upload(world.scene);

pxe::Camera camera;
camera.setYawRadians(kYaw);
camera.setTarget(world.focus);
```

`World` is the example's own struct. It holds the `pxe::Scene` and the game's data about it (where its fire burns, where the camera starts). A real game keeps such data in its own types, beside the scene, not in it.

The camera's yaw is in radians. At yaw 0 the view looks along +z; in general its flat forward direction is `(sin yaw, 0, cos yaw)` and its right is `(-cos yaw, 0, sin yaw)`. The pitch (30°), the pixel density and the eye distance are fixed. `setTarget` jumps to a point. `follow` eases toward one. `setPanVelocity` pans on the ground, and `rotateSteps` turns the view by 45° steps with a short ease. Call `update(dt)` once a frame to run the easing.

## The frame loop
A frame is: poll input, update the game, work out the lighting, render, draw any UI, swap. While the window is minimised to a 0x0 framebuffer, a frame stops after the update.

```cpp
pxe::Input input;
// Mouse look holds the cursor in the window.
if (firstPerson) window.setCursorCaptured(true);
double previous = window.time();
while (!window.shouldClose()) {
  const double now = window.time();
  const float dt = static_cast<float>(now - previous);
  previous = now;

  window.pollEvents();
  input.update(window);
  if (firstPerson) {
    // Escape lets the cursor go, and quits once it is free; a click
    // takes it back. The view looks and walks only while it is held.
    if (input.keyPressed(pxe::Key::kEscape)) {
      if (window.cursorCaptured()) {
        window.setCursorCaptured(false);
      } else {
        window.requestClose();
      }
    } else if (!window.cursorCaptured() && input.mousePressed(pxe::MouseButton::kLeft)) {
      window.setCursorCaptured(true);
    }
    if (window.cursorCaptured()) {
      // Moving the mouse right turns right (a smaller yaw), down looks down.
      const glm::vec2 turn = input.mouseDelta() * kLookPerPoint;
      eyeCamera.setYawRadians(eyeCamera.yawRadians() - turn.x);
      eyeCamera.setPitchRadians(std::clamp(eyeCamera.pitchRadians() + turn.y, -kMaxPitch, kMaxPitch));
      walk(eyeCamera, input, dt);
    }
  } else {
    if (input.keyPressed(pxe::Key::kEscape)) window.requestClose();
    if (input.keyPressed(pxe::Key::kQ)) camera.rotateSteps(-1);
    if (input.keyPressed(pxe::Key::kE)) camera.rotateSteps(1);
    camera.update(dt);
  }

  // A minimised window can have a 0x0 framebuffer, and render throws when
  // asked to present at that size: skip drawing, and wait rather than spin.
  const glm::ivec2 framebuffer = window.framebufferSize();
  if (framebuffer.x <= 0 || framebuffer.y <= 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    continue;
  }
  const pxe::FrameParams params = frameParams(world, framebuffer, static_cast<float>(now), hour);
  renderFrame(params);
  window.swap();
}
```

where `renderFrame` draws through whichever camera the run looks through (see [First-person camera](#first-person-camera)). Each camera has its own frame type, and `Renderer::render` has an overload for each:

```cpp
// Draw a frame through whichever camera this run looks through: each
// has its own frame type, for the renderer's mode.
auto renderFrame = [&](const pxe::FrameParams& params) {
  if (firstPerson) {
    renderer.render(world.scene, eyeCamera.frame(renderer.rtW(), renderer.rtH()), params);
  } else {
    renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
  }
};
```

with the frame's draw list built fresh each time:

```cpp
pxe::FrameParams frameParams(const World& world, glm::ivec2 framebuffer, float time, float hour) {
  pxe::FrameParams params;
  params.framebufferW = framebuffer.x;
  params.framebufferH = framebuffer.y;
  params.time = time;
  params.lighting = pxe::lightingAt(world.scene, hour, pxe::Weather{});
  params.fires.push_back({1, world.fireBase, 1.f, 0.f, 1.f});
  return params;
}
```

Step by step:

- **Input.** `Window::pollEvents()` first, then `Input::update(window)`. `keyDown` is the state now. `keyPressed` is true only on the frame a key goes down. The mouse (`mousePosition` in framebuffer pixels from the top left, `mouseDelta` for how far it moved since the last update, in window points, `mouseDown`, `mousePressed`) and the first connected gamepad (`gamepadDown`, `gamepadPressed`, `gamepadStick` with a dead zone, `gamepadAxis` raw) work the same way. What a key does is the game's decision. If an `Overlay` is up, skip game keys while `Overlay::wantsKeyboard()` is true and clicks while `wantsMouse()` is true.
- **Game update.** Move your characters, advance your clock and weather, and update the camera (`Camera::update(dt)`, or move and turn a `FirstPersonCamera`).
- **Lighting.** `pxe::lightingAt(scene, hour, weather)` turns the hour (0-24) and a `pxe::Weather` into a `pxe::Lighting`: the sun or moon direction, the palettes blended for the time of day, clouds, wind, rain, god rays, firefly and firelight colours and any lightning flash. Use this three-argument form for the game's frames. The two-argument `lightingAt(scene, hour)` leaves clouds, wind, rain, firelight and firefly light at zero, so fires draw flames but light nothing: it suits tests that want the sun and palette alone. `Weather` holds what your game decides: `cloudCover`, `cloudOffset`, `cloudStrength`, `skyCloudOffset` (metres the perspective sky clouds have drifted; the game advances it), `windDir`, `windStrength`, `rain`, `flash` and `flashDir`. A default `Weather{}` is clear and dry, with the wind at its default strength (1) blowing toward +x.
- **Render.** Fill a `pxe::FrameParams` and call `Renderer::render(scene, camera.frame(renderer.rtW(), renderer.rtH()), params)`. In orthographic mode the frame is `Camera::frame`'s `CameraFrame`; in perspective mode it is `FirstPersonCamera::frame`'s `PerspectiveFrame`. Giving `render` the other mode's frame throws `std::invalid_argument` before anything is drawn. Pass the renderer's padded target size (`rtW()`, `rtH()`) to the camera's `frame`, not the window's. Set `framebufferW` and `framebufferH` to `Window::framebufferSize()` every frame, as the window may be resized or on a high-DPI display. They default to 0, and `render` throws `std::invalid_argument` if `present` is true and either is not positive. With `present` false they are not used and may stay 0. Set `time` to seconds of game time. It drives grass wind, fire, fireflies and water. Hold it fixed for repeatable captures. With `present` true (the default), `render` ends by scaling the view into the window's framebuffer, centred with black borders.
- **Overlay.** Between `Overlay::begin()` and `Overlay::end()` the game calls ImGui itself. `end()` draws the UI over the presented frame, so call it after `render` and before `swap`:

  ```cpp
  overlay.begin();
  ImGui::Begin("Settings");
  ImGui::SliderFloat("Hour", &hour, 0.f, 24.f);
  ImGui::End();
  overlay.end();
  ```

  Every frame that calls `begin()` must call `end()`. Style the UI with `ImGui::GetStyle()` and `ImGui::GetIO()` once the overlay exists.
- **Swap.** `Window::swap()` shows the frame.
- **Minimised window.** On some platforms (GLFW on Windows) a minimised window's framebuffer is 0x0, and `render` throws if asked to present at that size. While `Window::framebufferSize()` has a zero dimension, skip the frame's drawing: no `render`, no overlay frame (so `begin()` and `end()` stay paired) and no `swap`. Keep polling events and updating the game, and wait a little each time round (the example sleeps 16 ms) so the loop does not spin flat out. A hidden window keeps its size, so offscreen captures are unaffected.

`FrameParams` also carries switches: `showGrass`, `edges` (outlines and rims), `fire`, `rays`, `fireflies`, `grassFire`. It carries the frame's lists too: `fires`, `lights` (extra `PointLight`s), `igniteGrass`, `gridStrokes`, `actors`, `actorMaterials` and `actorEffects`. Debug views are `magentaGrass`, `showNdl`, `testColours` and `grassDepthTest`. Build it fresh each frame; nothing in it is kept.

To find what the player clicks, `Renderer::rayThrough(camera, fbW, fbH, px)` gives the start of the view ray through a window pixel. The ray runs along `camera.forward`, and `scene.ground.raycast(origin, camera.forward, maxDist, hit)` finds where it meets the ground. `Renderer::windowPoint` goes the other way, from a world point to window pixels. Pass both the `CameraFrame` that was last rendered. Both are for the orthographic mode only, and throw `std::invalid_argument` in perspective mode.

## First-person camera
`pxe::FirstPersonCamera` (`pxe/first_person_camera.hpp`) is a perspective camera at an eye position, for a game played from eye height. Its `frame(rtW, rtH)` gives a `PerspectiveFrame` (declared beside it): the view and projection matrices, the eye, its basis (`forward`, `right`, `up`), yaw, pitch, `fovY`, and the near and far planes. Draw it with a renderer initialised with `RenderMode::kPerspective`. It draws into the same 960x540 target, padding and whole-number upscale.

- `setEye(point)`: where the eye is, in metres. The game moves it; the camera does no smoothing or collision.
- `setYawRadians` and `setPitchRadians`: free yaw, and pitch with positive looking down, as `Camera`'s 30° does. At yaw 0 it looks along +z, its flat forward is `(sin yaw, 0, cos yaw)` and its right `(-cos yaw, 0, sin yaw)`, so a larger yaw turns left. Pitch is held within ±89° so the view never flips; clamp it tighter yourself for mouse look (the example uses ±85°).
- `setFovYRadians`: the vertical field of view, 60° by default (`kDefaultFovY`).
- `setFarPlane`: how far the view reaches, 300 m by default (`kDefaultFar`). The near plane is 0.1 m (`kNear`).

The eye is never snapped to the texel grid (a perspective view can't be), so the frame's `texelError` is zero, and edges crawl a little as the view turns and moves. The frame's `fovY` is the field of view in radians.

From the example, turned toward a point to start with:

```cpp
// The first-person camera, turned toward world.lookAt. A positive pitch
// looks down.
pxe::FirstPersonCamera eyeCamera;
eyeCamera.setEye(world.eye);
eyeCamera.setFarPlane(kFirstPersonFar);
const glm::vec3 look = world.lookAt - world.eye;
eyeCamera.setYawRadians(std::atan2(look.x, look.z));
eyeCamera.setPitchRadians(-std::atan2(look.y, std::hypot(look.x, look.z)));
```

**Light.** The perspective mode doesn't use the toon ramp. Each surface has one base colour, `Scene::albedo[kSurfCount]` (display sRGB, defaulting to each surface's day mid band `palette[kPhaseDay][s].band[1]`; a game that changes that band sets the albedo itself), and each character material uses its `ActorMaterial::colour` as albedo (glowing materials stay unlit at full colour). In linear light a pixel is `albedo × (sun × N·L × shadow × (1 − cloud) + skyFill × up + groundBounce × down)`, plus lightning as a second light with its own shadows, times `Scene::exposure`, clamped and written as sRGB. The sun (the moon at night), sky fill and ground bounce are set per phase on `Scene` (`sunColour`/`sunStrength`, `skyFill`/`skyFillStrength`, `groundBounce`/`groundBounceStrength`, display colours with linear strengths) and blended by `lightingAt` into `Lighting::sunLight`, `skyFillLight` and `bounceLight`. Shadows have a soft edge `Scene::shadowPenumbra` metres wide (default 0.3), and cloud shadows are continuous. `FrameParams::testColours` shows the sun, fill and bounce terms as red, green and blue on white. `sunDirection` puts the sun exactly on the horizon at hours 6 and 18, so at those hours there is no direct sunlight at all: capture a low-sun look at 17.5 or 6.5.

**Surface detail.** Two controls shape the light on scene surfaces, both off by default. `Scene::terminator[kSurfCount]` (0..1, default 0) sharpens a surface's change from lit to shadow: above 0 the sun's direct term is `pow(N·L, 1 − 0.85 × terminator)`, so a crest's lit face against its shadowed face reads as a line while lit faces stay graded. `Scene::ripples` (`pxe::SurfaceRipples`) ripples one surface (`surface`, −1 none by default): a shading-only tilt of the normal in a wave of `wavelength` metres (default 0.25) across `headingDegrees` (the direction the wind blows toward, 0 toward +z, 90 toward +x), with a gentle side facing into the wind and a steep side facing downwind (`depth`, default 0.35, how far the steep side tilts the normal), the crests wandering a little. They are full to `fadeStart` metres from the eye (default 6), gone by `fadeEnd` (20), and absent on ground steeper than `maxSlopeDegrees` (22), fading over the last 4°. Geometry, shadows and outlines don't change, and characters and the view model never ripple. Both act through the direct sunlight, so neither shows with the sun at or below the horizon (hour 18 is exact sunset).

**Sky and haze.** Under the perspective camera the renderer fills every pixel with no geometry with a sky: a continuous gradient from `Lighting::skyHorizon` at the horizon to `Lighting::skyZenith` overhead, and the horizon colour below the horizon. `lightingAt` blends them from `Scene::skyHorizon[kPhaseCount]` and `skyZenith[kPhaseCount]`, which a new `Scene` fills with the engine's defaults for day, golden hour, twilight and night, like the surface palettes. Rain's overcast greys them as it greys a room's window panes. Round the sun the sky takes the sun's colour in a glow (`Scene::sunGlow`, 0..1, default 0.6), and the sun is a small soft-edged disc, hidden by rain cloud. Over the glow and the disc go up to two layers of sky clouds, `Scene::skyClouds[2]` (`pxe::SkyCloudLayer`: `height` of its flat plane above the eye, `cover` 0..1, `opacity`, the `length` and `width` of a streak along and across `Lighting::windDir`, and `drift`, the multiple of `Weather::skyCloudOffset` it moves by). Layer 0 is high and thin (the struct's defaults: `height` 8,000 m, `opacity` 0.6, `length` 6,000 m, `width` 900 m, `drift` 1.5), layer 1 lower and fuller (2,500 m, 0.7, 2,500 m, 900 m, `drift` 1.0); both have `cover` 0, so a scene that sets nothing has no clouds. They have no colours of their own: they take the horizon colour, warmed toward the sun's glow on the sunward side and darker where thick, times `Scene::skyCloudBrightness` (default 1), and fade out toward the horizon. They cast no shadows (ground cloud shadows are `Weather::cloudCover`). There is no moon disc or stars. Over the scene and sky, behind the view model, can drift airborne dust: `Scene::dustDensity` (0..1, default 0) times 6000 tiny half-transparent motes in an 80 m box that wraps round the eye, carried at 2 m/s × `Lighting::windStrength` along `Lighting::windDir` with a small wobble, fading from full at 10 m to nothing at 40 m. Motes nearer than 4 m are `Scene::dustSize` pixels (1..2, default 1), farther ones 1. They take the haze colour, glinting toward the sun's glow colour when they lie toward it. They are depth-tested against the scene but write no depth or normals, so they take no outlines; at density 0 nothing is drawn. The motes are generated from their index each frame, so there is no particle state to keep; the wind's carry restarts at `upload()`. Surfaces fade into the horizon colour with distance, `1 − exp(−Scene::hazeDensity × distance)` (default 0.006 per metre), and fully by the far plane, so the end of the geometry never shows as a hard edge against the sky; looking toward the sun the haze takes the same glow as the sky. Set the far plane to about where your geometry ends (the example's 50 m is just past its ground's far edges). Neither the sky nor the haze is drawn under the orthographic `Camera`. A room's window panes keep their own `skyColour`.

**Outlines.** The edge pass works under perspective: lines stay 1 px, and the depth step that counts as an outline grows with distance and with how steeply a surface runs away from the eye, so far terrain doesn't fill with ink.

**Shadows.** The sun's (and lightning's) shadow map covers a 60 m radius round the eye, pushed 30 m ahead along the view's flat forward, and is redrawn as the eye moves. Anything in the scene between the light and that area still casts into it. Nothing beyond it is shadowed.

**Not yet supported under the perspective camera.** These are skipped under perspective, whatever `FrameParams` says, and will be ported when a game first needs them:
- grass and leaf sprites (`showGrass`): the ground and crowns show as bare faceted surfaces;
- water sheets and their reflections (`Scene::water` isn't drawn);
- god rays (`rays`) and rain (the edge pass's streaks, splashes and murk; `Lighting::rain`'s overcast still dims the colours);
- fires (`fire`, `fires`): their flames, smoke and firelight, and the grass fire (`grassFire`, `igniteGrass`, `gridStrokes`);
- fireflies (`fireflies`) and point lights (`lights`);
- indoor light (`Scene::windows`' sky light and the room's fill light), dark regions (`Scene::darkRegions`) and firelight on surfaces;
- `Renderer::rayThrough` and `windowPoint`.

**View model.** `FrameParams::viewModel` takes characters attached to the view, such as a first-person view's arms or a held tool: `ActorDraw`s like `actors`, posed in world space in front of the eye (typically skinned with the view's own transform, so a model built round the eye sits where the eye is). The perspective frame draws them after the scene, the sky and any window panes, squeezed into the nearest 1% of the depth range, so nothing in the scene ever cuts or hides them, even with the eye against a wall. They take the scene's light and shadows through the actor shader like any character, and their materials come from `actorMaterials`, but they cast no shadows. Their joints must last until `render` returns. The edge pass reads their pixels at their true depth: every silhouette of the view model, against the scene or the sky and wherever one of its parts stands more than 1 cm in front of another, is inked dark (the heavy mix a slate crease takes), and it takes a light rim only at an upward crease, never on a silhouette. The scene never outlines against it, and a frame with no view model is drawn exactly as before. One limit: while a view model is drawn, a scene surface within about a millimetre past the near plane is read as part of it, so keep the eye at least a few centimetres from the scene (the desert keeps it 0.3 m from the palm and 1.7 m above the sand). The orthographic mode ignores the field. The desert game's arms (`desert/src/arms.cpp`) are a worked example.

**Mouse look.** `Window::setCursorCaptured(true)` hides the cursor and holds it in the window with unbounded motion (raw, unaccelerated motion where the system supports it); `setCursorCaptured(false)` gives it back, and `cursorCaptured()` says which. `Input::mouseDelta()` is how far the mouse moved since the previous update, in window points (the cursor's own units, not framebuffer pixels, so the same motion gives the same delta on any display density), +x right and +y down. It is zero on the first update and on the update after the cursor is captured or released, when the position jumps. The example captures the cursor before its loop:

```cpp
pxe::Input input;
// Mouse look holds the cursor in the window.
if (firstPerson) window.setCursorCaptured(true);
```

and turns the view by `mouseDelta()` each frame while the cursor is held (see [The frame loop](#the-frame-loop)). Escape releases the cursor, a left click captures it again, and Escape with the cursor free quits. While the cursor is free the view neither turns nor walks.

**Walking.** The camera only holds the eye; moving it is the game's. The example's walker moves it with WASD along the view's flat forward and right (3.5 m/s, 6 m/s with Shift), keeps it 1.7 m above the ground with a short ease so the slopes don't jolt the view, and holds it 2 m inside the ground's edges. It has no collision: it walks through the stone, the slab and the tree. A game writes its own, as the player is game code (the desert game's, in `desert/src/player.cpp`, also slides along a tree trunk and the play area's edge):

```cpp
void walk(pxe::FirstPersonCamera& eyeCamera, const pxe::Input& input, float dt) {
  const float yaw = eyeCamera.yawRadians();
  const glm::vec2 forward{std::sin(yaw), std::cos(yaw)};  // in x and z
  const glm::vec2 right{-std::cos(yaw), std::sin(yaw)};
  glm::vec2 move{0.f};
  if (input.keyDown(pxe::Key::kW)) move += forward;
  if (input.keyDown(pxe::Key::kS)) move -= forward;
  if (input.keyDown(pxe::Key::kD)) move += right;
  if (input.keyDown(pxe::Key::kA)) move -= right;
  glm::vec3 eye = eyeCamera.eye();
  if (glm::dot(move, move) > 0.f) {
    const bool sprint = input.keyDown(pxe::Key::kLeftShift) || input.keyDown(pxe::Key::kRightShift);
    const glm::vec2 step = glm::normalize(move) * (sprint ? kSprintSpeed : kWalkSpeed) * dt;
    eye.x = std::clamp(eye.x + step.x, -kWalkHalf, kWalkHalf);
    eye.z = std::clamp(eye.z + step.y, -kWalkHalf, kWalkHalf);
  }
  const float standing = groundHeight(eye.x, eye.z) + kEyeHeight;
  eye.y += (standing - eye.y) * (1.f - std::exp(-kEyeEase * dt));
  eyeCamera.setEye(eye);
}
```

## Building a Scene
A `pxe::Scene` is plain data plus GPU meshes:

- `meshes`: opaque meshes, which all cast shadows. `shadowOnly`: meshes drawn only into the sun, moon and lightning shadow maps, never seen (a cutaway room's roof and near walls, so it is lit as if closed). `water`: water sheets, with `waterLevel`, `waterMin` and `waterMax`.
- `grass`: every grass and leaf sprite (`GrassInstance`).
- `ground`, `waterTop` (`HeightMap`) and `shelter` (`ShelterMap`): the maps the game queries.
- `fireflyHomes`: points the fireflies wander round.
- `palette[kPhaseCount][kSurfCount]`, `rim[kPhaseCount]`, `ink[kPhaseCount]`: colours per phase of the day (`kPhaseDay`, `kPhaseGolden`, `kPhaseTwilight`, `kPhaseNight`) and per surface. A new `Scene` starts with the engine's default palettes.
- `tLit`, `tMid`: the toon ramp's N·L thresholds.
- `sunEast`, `sunSouth`, `sunPeakDegrees`: the sun's path.
- `horizonCentre`, `horizonRadius`, `horizonHeight`, `horizonRelief`: an optional ridge round the scene that the rising sun must clear.
- `boundsMin`, `boundsMax`: the scene's extent. Shadow maps are fitted to these.
- `skyHorizon[kPhaseCount]`, `skyZenith[kPhaseCount]`: the sky behind a perspective view, and the colour far surfaces fade into (see [First-person camera](#first-person-camera)). Default to the engine's.
- `sunColour`, `sunStrength`, `skyFill`, `skyFillStrength`, `groundBounce`, `groundBounceStrength` (each `[kPhaseCount]`), `albedo[kSurfCount]`, `hazeDensity`, `sunGlow`, `shadowPenumbra`, `exposure`: the perspective mode's light, sky glow and haze (see [First-person camera](#first-person-camera)). Default to the engine's. The orthographic mode ignores them.
- `skyClouds[kSkyCloudLayers]` (`SkyCloudLayer`), `skyCloudBrightness`: the perspective mode's sky clouds (see [First-person camera](#first-person-camera)). Cover 0 (none) by default. The orthographic mode ignores them.
- `dustDensity`, `dustSize`: the perspective mode's airborne dust (see [First-person camera](#first-person-camera)). Density 0 (none) by default. The orthographic mode ignores them.
- `terminator[kSurfCount]`, `ripples` (`pxe::SurfaceRipples`): the perspective mode's terminator sharpness per surface and ripples on one surface (see [First-person camera](#first-person-camera)). Both off by default. The orthographic mode ignores them.
- `indoors`, `fillDir`, `windows`, `skyTint[kPhaseCount]`, `skyColour[kPhaseCount]`: a room under a roof. Indoors no rain is drawn, no cloud shadows drift and no god rays shine; sun, moon and lightning still light it through the shadow map. A fill light from `fillDir` lifts faces by how far they turn toward it (`Lighting::fill`, full by day, gone at night). Each `SkyWindow{centre, inward, width, height, depth}` lets the sky's light onto the floor and walls in front of it (`Lighting::skyLight`, tinted by `skyTint`) and shows `skyColour` through the opening.
- `darkRegions`: boxes the scene's light does not reach, indoors or out, such as a deep room seen through an open door. Each `DarkRegion` has a `centre`, `halfExtents` and a `yaw` about +y (its own +z goes to `(sin yaw, 0, cos yaw)`), an `openFace` (`BoxFace::kPosX` .. `kNegZ`, in its own frame), and `startStrength`, `endStrength` and `fadeDistance`. Every surface and character inside the box is drawn unlit, as a share of its surface's mid colour for the time of day: `startStrength` at the open face, moving linearly to `endStrength` at `fadeDistance` metres in, then held. Sun, moon, fill, sky light, lightning, cloud shade, firelight and point lights add nothing there, and the edge pass's light rims take the same share; dark outlines and a character's glowing materials are unchanged. Where regions overlap the darker share wins. The renderer reads the list each frame and draws up to 8. A surface lying exactly in a face's plane may fall either side of it pixel by pixel, so make the box a little larger than the floor and walls it darkens on every face but the open one. Empty by default, which changes no pixel.

Every vertex names its surface: `kSurfGrass`, `kSurfDirt`, `kSurfStone`, `kSurfBark`, `kSurfLeaf`, `kSurfWater`, `kSurfFoam`, `kSurfWood`, `kSurfPlaster`, `kSurfRoof` (slate), `kSurfOak` (furniture) or `kSurfSand` (desert ground: banded like grass and dirt, with no outline or rim). `loadGltf` picks it from the material name. Each surface has its own ramp: `band`, `detail` and `tip`, each shadow, mid and lit. A default-constructed `Scene` already has every ramp of every surface in all four phases, and `rim` and `ink` per phase: the engine's reference style, sampled from t3ssel8r's videos (day from his scene video; golden hour, twilight and night from his day/night video). `lightingAt` cross-fades the phases by sun elevation. The example keeps them all.

To change the look, overwrite only the ramps you want different, after constructing the scene. For a brighter lit band on day grass, set `scene.palette[pxe::kPhaseDay][pxe::kSurfGrass].band[2]` (a linear 0..1 `glm::vec3`); for a different outline at night, `scene.ink[pxe::kPhaseNight]`. The phases cross-fade, so a ramp changed in one phase only gives way to the default colours as the time of day moves on. Change it in every phase to keep the new look all day.

The sun rises from `sunEast`, peaks toward `sunSouth` at `sunPeakDegrees` above the horizon and sets opposite `sunEast`. The moon crosses the same arc at night. Choose them relative to your camera: the example puts sunrise on the view's right and noon ahead of it.

```cpp
scene.sunEast = {-std::cos(kYaw), 0.f, std::sin(kYaw)};
scene.sunSouth = {std::sin(kYaw), 0.f, std::cos(kYaw)};
```

### Geometry, by hand
Geometry goes through `pxe::SurfaceGeometry`, which keeps one vertex and index list per surface so that each surface draws as one mesh. Vertices are in world space (`pxe::Vertex`: position, normal, surface as a float). Lighting uses the vertex normal, so give smooth normals where you want smooth shading. Wind triangles counter-clockwise seen from outside, because meshes are drawn with back faces culled. `append(surface, vertices, indices)` adds a batch, offsets its indices and grows `boundsMin` and `boundsMax`. Writing into `vertices[s]` and `indices[s]` directly also works, but does not grow the bounds.

When the geometry is complete:

```cpp
pxe::buildSceneMaps(scene, geometry);
pxe::uploadScene(scene, geometry);
```

`buildSceneMaps` builds `scene.shelter` from the stone and roofs, `scene.ground` from grass, dirt, stone, bark, foam, wood, plaster, oak and sand (not roofs, which are overhead), and `scene.waterTop` from the water. Its optional third argument, `overhead(xz, y)`, leaves out of the ground what things on it pass under, such as a doorway's head (as `HeightMap::build`'s does). `uploadScene` makes one `Mesh` per non-empty surface (water into `scene.water`), sets the water level and extent, and grows the scene's bounds. It needs the GL context.

The example's ground is a height grid appended as grass, followed by boxes and a sphere:

```cpp
pxe::SurfaceGeometry geometry;
geometry.append(pxe::kSurfGrass, groundVerts, groundIdx);
// A standing stone, a tree, and a stone slab for the campfire.
addBox(geometry, pxe::kSurfStone, 3.f, -1.f, {0.6f, 1.6f, 0.5f});
addBox(geometry, pxe::kSurfBark, -3.f, 2.f, {0.25f, 2.2f, 0.25f});
const std::vector<pxe::Triangle> crown =
    addCrown(geometry, {-3.f, groundHeight(-3.f, 2.f) + 4.2f, 2.f}, 1.8f);
addBox(geometry, pxe::kSurfStone, 1.f, 3.f, {0.8f, 0.28f, 0.8f});
```

For one-off meshes outside a scene, `pxe::Mesh` has `upload(vertices, indices)`, `update` (rewrite the vertices of a mesh rebuilt every frame) and `draw()`. It is move-only and frees its buffers when destroyed.

### Geometry, from a .glb
`pxe::loadGltf(path)` reads a glTF 2.0 file (.glb or .gltf) into `GltfPrimitive`s: node name, material name, surface, world-space vertices and indices. Node transforms are already applied. The surface comes from the material name, as the [asset contract](#asset-contract) lists. Textures, skinning and animation are ignored. It throws `std::runtime_error` if the file is missing or unreadable.

```cpp
pxe::SurfaceGeometry geometry;
for (const pxe::GltfPrimitive& prim : pxe::loadGltf(path)) geometry.append(prim);
// add any procedural geometry to `geometry` here
pxe::buildSceneMaps(scene, geometry);
pxe::uploadScene(scene, geometry);
```

Which file to load, and where its contents go, is the game's choice. Keep the primitives' triangles if you will scatter on them (see below): `pxe::trianglesOf(prim.vertices, prim.indices)`.

### Scattering grass and leaves
Grass and leaves are `GrassInstance` sprites in `scene.grass`, scattered after the geometry is uploaded:

```cpp
std::mt19937 rng(7);
const pxe::TriangleGrid ground(pxe::trianglesOf(groundVerts, groundIdx), 1.f);
pxe::scatterGrass(scene, ground, pxe::GrassScatter{}, rng);
pxe::dropGrassUnder(scene, geometry, {pxe::kSurfStone, pxe::kSurfBark});
pxe::scatterLeaves(scene, crown, pxe::kDefaultLeafDensity, rng);
```

- `scatterGrass(scene, ground, params, rng)` carpets the triangles of a `TriangleGrid` with blade clumps (Poisson-disk spaced by `GrassScatter::clumpSpacing`) and, when `tufts` is set, a sparser layer of tufts (`tuftSpacing`). `GrassScatter::height` is a 0..1 field over xz that sets clump height. The default is `pxe::grassPatchiness`. Build the `TriangleGrid` from the triangles grass should grow on, not from walls or roofs. The second argument is the bucket size in metres.
- `GrassScatter`'s default spacings are already at the engine's pixel density (`RenderConfig::kPixelsPerUnit`): they were tuned at 16 pixels per metre and are scaled to keep the same density on screen. Use them as they are, or set your own in metres.
- `dropGrassUnder(scene, geometry, {surfaces})` removes grass rooted beneath the given surfaces. Ground that runs on under a stone or trunk would otherwise show grass through its faces. It works over the scene's bounds, so call it after `uploadScene`.
- `scatterLeaves(scene, crowns, density, rng)` covers crown triangles with leaf clusters, about `density` per square metre. `pxe::kDefaultLeafDensity` is the engine's default, at its pixel density.
- `poissonDisk(lo, hi, minDist, rng, accept)` is the sampler underneath, for scattering your own things.

Leaves and grass are drawn with their surface's `detail` and `tip` colours as well as `band`.

### HeightMap and ShelterMap
`scene.ground` (`pxe::HeightMap`) is the top of everything walkable. `heightAt(xz)` gives a smooth height for something moving over the ground, `cellTop(xz)` the raw cell, `raycast(origin, dir, maxDist, hit)` a ray hit, and `min()` and `max()` the extent. Off the map, heights are `HeightMap::kNone`. `scene.waterTop` is the same for the water. Build your own with `HeightMap::build(triangleCorners, cell)` for other queries (three corners per triangle). Walls seen edge-on from above add nothing.

`scene.shelter` (`pxe::ShelterMap`) is the roofing from stone. `sheltered(p)` says whether rain can reach a point, and `coveredLines(p)` says how many of nine upward sight lines are roofed. The grass fire uses it to keep rain off roofed cells. Your game can use it to decide whether its fires go out in the rain.

`pxe::TriangleGrid::at(x, z)` finds the triangle under a point and returns its position and interpolated normal. Use it to stand things on the ground while building a scene, before `scene.ground` exists.

## Fires, FireGrid and fireflies
**Campfires, torches, candles.** The game owns its fires and lists the ones burning each frame in `FrameParams::fires`, as `pxe::FireDraw`:

```cpp
params.fires.push_back({1, world.fireBase, 1.f, 0.f, 1.f});  // id, base, level, smoke, size
```

The renderer keeps each fire's particles from frame to frame by `id`, so keep a fire's id stable while it lives. A fire that is no longer listed is dropped. Move it by changing `base`. `level` is how well it burns (0 out .. 1). `smoke` is how much it still smokes once out (1 just out .. 0). `size` scales the flames: 1 is a campfire and about 0.2 a candle. Campfires (size 1 or more) light the scene with shadows for the brightest few. Smaller fires light only through point lights. The lighting strength comes from `Lighting::fireLight`, so use the three-argument `lightingAt`. `FrameParams::fire = false` hides every fire and its light. Burning down, going out in the rain and relighting are game rules: change `level` and `smoke` as your game decides.

`pxe::Fire` is the particle system behind each `FireDraw`. The renderer runs them, so a game doesn't normally need it.

**Grass fire.** `pxe::FireGrid` burns the scene's grass on the GPU. The renderer owns its own grid, and the game drives it through `FrameParams`:
- `grassFire = true` turns it on. While on, it runs every frame; the first frame builds it from the scene's grass (its fuel), leaf sprites (tree crowns) and shelter. Keep it on once a fire has started.
- `igniteGrass`: world xz points to set alight this frame.
- `gridStrokes`: brush strokes this frame, as `FireGrid::Stroke{from, to, radius, kind}` with `kind` one of `FireGrid::Stroke::kFire`, `kWater` or `kRegrow`.
- `grassFireLife`: seconds a fire set alight keeps spreading (`FireGrid::setLifetime`). 0 means until there is nothing left to burn.

Rain (`Lighting::rain`) wets cells open to the sky, and wind (`Lighting::windDir`, `windStrength`) pushes the spread. `Renderer::upload` resets the grid, unless called as `upload(scene, true)`: then the grid and its burnt grass are kept, for coming back to the scene it was built for from one drawn with `grassFire` off throughout. `Renderer::quenchGrassFire()` puts every flame and ember out at once and leaves burnt grass burnt.

**Fireflies.** Fill `scene.fireflyHomes` with points (knee to head height over open ground) when building the scene, and set `FrameParams::fireflies = true` on the frames they are out. `pxe::firefliesAt(homes, time)` places them as a pure function of time, so captures repeat. Their light (`Lighting::fireflyLight`) comes from the three-argument `lightingAt` and shows mainly at night.

**Point lights.** Any extra light without shadows goes in `FrameParams::lights` as a `pxe::PointLight{pos, radius, strength, kind}`. `kind` is `PointLight::kFire` or `PointLight::kFirefly` and picks the colour (`Lighting::fireBand` or `fireflyBand`).

## Skinned models
`pxe::SkinnedModel::load(path, firstMaterial)` reads one skinned mesh, its skeleton and its animation clips from a glTF 2.0 file and uploads the mesh at rest. It needs the GL context, and throws `std::runtime_error` if the file is missing, has no skinned mesh, or has more than `kMaxJoints` (64) joints. One model serves any number of characters. The model keeps nothing per character, so each character keeps its own pose and joint matrices.

Per frame, per character:

```cpp
pxe::SkinnedModel::Pose pose;              // per character, reused
std::vector<glm::mat4> joints;             // per character, must outlive render()

model.sample(walk, time, pose);            // walk = model.clip("Walk")
model.sample(idle, time, idlePose);
pxe::SkinnedModel::blend(idlePose, pose, moving, pose);
model.skin(pose, worldTransform, joints);
params.actors.push_back({&model.mesh(), joints});
```

- `clip(name)` and `joint(name)` look up by name (-1 if absent). `clipDuration(clip)` gives a clip's length. `sample(clip, time, pose, loop)` fills a pose from a clip, wrapping or holding at the ends. `restPose(pose)` gives the rest pose.
- `blend(a, b, w, out)` mixes two poses. The overload taking a `std::vector<float>` of per-joint weights layers one action over part of the body. `turn(pose, joint, axis, angle)` adds a turn after animation (a head looking round, a cape lagging).
- `skin(pose, model, joints)` gives the joint matrices for drawing, with `model` the character's world transform. `jointPoint(joints, joint, restPoint)` finds where a point carried by a joint is now (a sword tip, a paw). `restJoint(joint)` is the joint at rest. `vertexPoints` is for measuring clearances, not drawing.
- `FrameParams::actors` holds `ActorDraw{mesh, joints}`. The `std::span` points into your vector, so it must stay alive and unchanged until `render` returns.

**Materials.** Characters draw as surface `kActorSurface` plus a slot in `FrameParams::actorMaterials` (`kActorMaterials` slots of `ActorMaterial{colour, glow, crease}`, colour in sRGB; `crease` inks the material along its sharp creases, as for a door's planks and grain). A model's material *i* takes slot `firstMaterial() + i`, so give each kind of model its own range when loading. Each frame, copy every drawn model's `materials()` into those slots:

```cpp
const auto& mats = model.materials();
for (int i = 0; i < static_cast<int>(mats.size()) && model.firstMaterial() + i < pxe::kActorMaterials; ++i)
  params.actorMaterials[model.firstMaterial() + i] = mats[static_cast<std::size_t>(i)];
```

`FrameParams::actorEffects` takes world-space `Mesh` pointers drawn with the characters but without shadows, such as a sword's smear. `FrameParams::viewModel` takes characters attached to a first-person view (see [First-person camera](#first-person-camera)).

## Audio
`pxe::AudioDevice` plays whatever the game renders into it. The engine plays sound; the game owns every sound: samples, synthesis recipes, mixing and when each one plays.

```cpp
struct Sound {
  std::atomic<float> volume{0.5f};
  // ... the game's mixer state ...
  pxe::AudioDevice device;  // last member: destroyed first
};

sound.device.open(48000, [&sound](float* out, int frames) {
  // fill 2 * frames interleaved stereo floats
});
```

`open(sampleRate, render)` opens the default output and starts calling `render` on the audio thread. It returns false, and nothing plays, if there is no output. Run the game silently in that case. The callback must not block, and anything it shares with the game thread needs atomics or a lock. The device stops calling it when destroyed, so destroy the device before anything the callback reads. Making it the last member of the struct that owns the sound state does this.

## Capture, writePng and writeWav
To write the view to an image, render with a fixed `time`, then read back:

```cpp
// A few frames at fixed times, so a capture is the same on every run.
for (int f = 0; f < kCaptureFrames; ++f) {
  const pxe::FrameParams params =
      frameParams(world, window.framebufferSize(), static_cast<float>(f) / 60.f, hour);
  renderFrame(params);
}
const bool ok = pxe::writePng(capture, renderer.viewW(), renderer.viewH(), renderer.readView());
```

- `Renderer::readView()` returns the low-res view (`viewW()` x `viewH()`, 960x540) without its padding, bottom row first, RGBA8. This is the art at one pixel per texel, before the window's upscale and sub-texel shift.
- `Renderer::readPresented(width, height)` reads the window's framebuffer, including any overlay drawn into it. Call it before `Window::swap`.
- `pxe::writePng(path, width, height, bottomUpRgba)` writes either to a PNG, flipped to top row first, creating parent directories. It returns false on failure.
- `pxe::writeWav(path, sampleRate, interleavedStereo)` writes interleaved stereo float samples (the layout `AudioDevice` renders, -1..1) to a 16-bit PCM stereo WAV with a 44-byte header. Samples are clamped to -1..1 and scaled by 32767. It does not create directories. It returns false if the sample rate is not positive, the sample count is odd, or the file can't be written. Use it to render sound offline, calling the same render function the device would, and compare runs.

Use a hidden window (`WindowOptions::visible = false`, `vsync = false`) for captures and tests. A capture is repeatable when the scene is built from fixed seeds and every frame uses a fixed `time`, hour and weather.

## Asset contract
What the engine requires of every model file, whichever game makes it. Each game's own art style doc (`game/docs/art-style.md`, `desert/docs/art-style.md`) sets how its assets look; this section sets what loads.

- **Format and axes.** glTF 2.0, binary (.glb) by convention, exported from Blender +Y up. 1 unit = 1 metre. Blender's -Y is the engine's +Z, so a model facing -Y in Blender faces +Z in the engine. The engine applies no scale of its own: a game scales a model when it places it.
- **Static surfaces by material name.** `loadGltf` picks each primitive's surface from its material name's prefix, case-insensitive, so Blender's `.001` duplicate suffix still matches: `Grass`, `Dirt`, `Stone`, `Bark`, `Leaf`, `Water`, `Foam`, `Wood`, `Plaster`, `Roof` (slate), `Oak` (furniture) and `Sand` (desert ground). Any other name loads as Stone. Static meshes are coloured from the scene's palettes by surface, so colours in the file are for previews only.
- **Node names reach the game.** Each `GltfPrimitive` carries its node name (the Blender object name), which a game can use to pick out pieces. The engine gives no node name a meaning of its own.
- **Skinned models.** One skinned mesh with its skeleton, at most `kMaxJoints` (64) joints. The model's glTF material *i* draws with actor-material slot `firstMaterial + i` (`SkinnedModel::load(path, firstMaterial)`), out of `kActorMaterials` (64) slots shared by every model in a frame, so a game gives each kind of model its own range. Joints and clips are looked up by name. Clips are sampled linearly between keys, rotations slerped the short way.

## Contracts
- **GL context first, last.** `Window` before anything that touches the GPU, and destroyed after all of it. `Mesh`, `SkinnedMesh`, `Scene` (it holds meshes), `SkinnedModel`, `Renderer` and `Overlay` all free GPU objects in their destructors.
- **Upload, then render.** Call `Renderer::upload(scene)` after building a scene and before rendering it, and again whenever you build or swap in a different scene. `render(scene, ...)` must be given the scene last uploaded. The renderer keeps shadow maps, fire state and the grass fire drawn from that scene and redraws them only when the light changes or `upload` is called. It never checks which scene it is given. If you change `scene.grass` afterwards (clearing blades under something placed later), call `Renderer::uploadGrass(scene)`. Don't change a scene's meshes without uploading again. To swap back to a scene whose grass fire should survive the visit, pass `keepGrassFire` (see Grass fire): the grid must only ever advance in the scene it was built for.
- **Scene build order.** Geometry into `SurfaceGeometry`, then `buildSceneMaps` and `uploadScene`, then scattering (`dropGrassUnder` reads the bounds `uploadScene` sets), then `Renderer::upload` (which sends the grass to the GPU).
- **The game owns every object.** The engine never creates a game object and keeps no pointer to one between calls. Everything per-frame (fires, actors and their joints, lights, ignitions, strokes) is passed in `FrameParams` and read during `render`. Joint matrices and effect meshes must stay alive until `render` returns. Fire ids must stay stable while a fire lives.
- **RNG order when scattering.** Every scatter function draws from the `std::mt19937` it is given, in a fixed order. Seed one generator and pass it to each call in a fixed sequence to get the same scene every run. Any change to an earlier call (another scatter, different spacing, a different ground) changes everything after it. To add something without disturbing an existing scene, scatter it from its own generator, or remove sprites after scattering (as `dropGrassUnder` does) rather than before.
- **Time.** `FrameParams::time` is seconds. Fires and the wind phase restart if it jumps backward or far ahead, and fires are always already burning when they start. Hold it fixed for repeatable images.
- **Threads.** The GL context is current only on the thread that made the window, so call the engine from that thread. Only the `AudioDevice` callback runs elsewhere.

## Header reference
Every public header, in `engine/include/pxe/`, namespace `pxe`.

- **`audio_device.hpp`.** `AudioDevice`: the default audio output, interleaved stereo float, filled by a `Render` callback on the audio thread. `open(sampleRate, render)`.
- **`audio_file.hpp`.** `writeWav(path, sampleRate, interleavedStereo)`: 16-bit stereo WAV.
- **`camera.hpp`.** `Camera`: the orthographic camera, pitched 30°, yaw in 45° steps, smoothed and snapped to the texel grid (`setTarget`, `follow`, `setYawRadians`, `rotateSteps`, `setPanVelocity`, `nudgeImmediate`, `update`, `frame`). `CameraFrame`: one orthographic frame's matrices, eye, basis and snap error.
- **`config.hpp`.** `RenderConfig`: the fixed low-res target (`kViewW` 960, `kViewH` 540), `kPixelsPerUnit` (24), `kPad`, and the shadow map size `kShadow`.
- **`fire.hpp`.** `Fire`: one campfire's particle system (core, flames, embers, smoke). `FireParticle`: a particle as drawn. The renderer runs these for each `FireDraw`.
- **`fireflies.hpp`.** `Firefly` and `firefliesAt(homes, time)`: the swarm as a pure function of time.
- **`first_person_camera.hpp`.** `FirstPersonCamera`: the perspective camera at an eye position (`setEye`, `setYawRadians`, `setPitchRadians`, `setFovYRadians`, `setFarPlane`, their getters, `frame`), with `kDefaultFovY`, `kDefaultFar` and `kNear`. `PerspectiveFrame`: one perspective frame, which it returns.
- **`firegrid.hpp`.** `FireGrid`: the GPU grass and crown fire. `FireGrid::Stroke`: a fire, water or regrowth brush stroke. The renderer owns the grid. Games drive it through `FrameParams`.
- **`gl_loader.hpp`.** `loadGl(getProc)`: loads the GL entry points. `Window` calls it. A game only needs it if it makes its own context.
- **`gltf.hpp`.** `loadGltf(path)` and `GltfPrimitive`: static glTF geometry in world space, with a surface per material name.
- **`heightmap.hpp`.** `HeightMap`: highest surface per grid cell (`build`, with an optional `overhead` filter, `heightAt`, `cellTop`, `raycast`, `min`, `max`, `kNone`).
- **`image.hpp`.** `writePng(path, width, height, bottomUpRgba)`.
- **`input.hpp`.** `Input`: keyboard, mouse and first gamepad, read once a frame (`update`, `keyDown`, `keyPressed`, `mousePosition`, `mouseDelta`, `mouseDown`, `mousePressed`, `gamepadConnected`, `gamepadName`, `gamepadDown`, `gamepadPressed`, `gamepadStick`, `gamepadAxis`). The enums are `Key`, `MouseButton`, `GamepadButton`, `GamepadStick` and `GamepadAxis`.
- **`lighting.hpp`.** `Lighting`: everything that lights one frame, including a room's `fill`, `skyLight`, `skyTint` and `skyColour`, and the perspective mode's `skyHorizon`, `skyZenith`, `sunLight`, `skyFillLight`, `bounceLight`, `sunDir`, `sunGlowColour`, `phaseWeight`, `skyCloudOffset` and the copied `hazeDensity`, `sunGlow`, `shadowPenumbra` and `exposure`. `Weather`: the game's weather. `lightingAt(scene, hour)` and `lightingAt(scene, hour, weather)`, `sunDirection`, `moonDirection`.
- **`mesh.hpp`.** `Surface` (`kSurfGrass` .. `kSurfSand`, `kSurfCount`), `Vertex`, `Mesh`. Also `SkinVertex` and `SkinnedMesh`, a skinned mesh uploaded once at rest.
- **`overlay.hpp`.** `Overlay`: Dear ImGui over the window (`begin`, `end`, `wantsKeyboard`, `wantsMouse`).
- **`renderer.hpp`.** `RenderMode` (`kOrthographic`, `kPerspective`). `Renderer` (`init`, `mode`, `upload`, `uploadGrass`, `quenchGrassFire`, `render`, `readView`, `readPresented`, `rayThrough`, `windowPoint`, `viewW`, `viewH`, `rtW`, `rtH`, `setProfiling`, `passTimes`). Also `FrameParams`, `FireDraw`, `PointLight`, `ActorDraw` and `PassTimes`.
- **`scatter.hpp`.** `scatterGrass` with `GrassScatter`, `dropGrassUnder`, `scatterLeaves` with `kDefaultLeafDensity`, `poissonDisk` and `grassPatchiness`.
- **`scene.hpp`.** `Scene` (constructed with the default palettes and sky colours), `SkyWindow`, `DarkRegion` and `BoxFace`, `GrassInstance`, `Palette` and `Phase` (`kPhaseDay` .. `kPhaseNight`, `kPhaseCount`).
- **`scene_geometry.hpp`.** `SurfaceGeometry` (`append`, `corners`), `buildSceneMaps`, `uploadScene`.
- **`shelter.hpp`.** `ShelterMap`: roofing over the ground (`build`, `sheltered`, `coveredLines`).
- **`skinned.hpp`.** `SkinnedModel`: skinned glTF characters with clips, poses, blending and skinning. Also `ActorMaterial`, `kActorSurface`, `kActorMaterials` and `kMaxJoints`.
- **`triangle_grid.hpp`.** `Triangle`, `trianglesOf(vertices, indices)`, and `TriangleGrid`, which finds the triangle under an xz point (`at`).
- **`window.hpp`.** `Window`: the window and its GL context (`shouldClose`, `requestClose`, `pollEvents`, `swap`, `framebufferSize`, `windowSize`, `time`, `setTitle`, `setCursorCaptured`, `cursorCaptured`). `WindowOptions` sets its size, title, visibility and vsync.
