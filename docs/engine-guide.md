# Building a game on pxe

This guide is for anyone, person or model, starting a new game on the pxe engine. It covers how to add the engine to a project, the order things are created in, what a frame looks like, how to build a scene, and the contracts the engine relies on. It ends with a short reference to every public header.

A complete, small program follows it: [`examples/minimal/main.cpp`](../examples/minimal/main.cpp). It builds a procedural scene (rolling grass, a standing stone, a tree, a campfire on a slab), lights it at a fixed hour, and draws it in a window. With `--capture out.png` it renders a few frames offscreen, writes the view to a PNG and exits. The snippets below come from it. It is built with the rest of this repository, so it always compiles against the current headers.

## What the engine is and isn't
pxe is a C++20 engine for pixel-art 3D games in the style of t3ssel8r's videos. Everything is drawn into a low-resolution target (960x540 at 24 pixels per metre, `pxe::RenderConfig`) through an orthographic camera pitched 30° down, then scaled up by a whole number to the window. Surfaces are lit with a hard three-step toon ramp per material. Grass and leaves are instanced sprites drawn in the fragment shader. On top of that it provides shadows, water reflections, outlines, god rays, rain, lightning, a day/night palette blend, campfires, a grass fire that spreads on the GPU, fireflies, skinned characters, input, a Dear ImGui overlay and audio output.

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

In this repository, the root `CMakeLists.txt` adds `engine`, `game` and `examples/minimal`, and `examples/minimal/CMakeLists.txt` is the whole of what a game needs:

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
2. **`pxe::Renderer`.** Default-construct it, then call `init()`, which compiles the shaders and makes the render targets. It throws if a shader fails to compile.
3. **Your scene.** Build a `pxe::Scene` (see [Building a Scene](#building-a-scene)), then pass it to `Renderer::upload`.
4. **`pxe::Overlay`**, if the game has UI. Construct it with the window, after the window and before the first frame. Destroy it before the window. Only one may exist at a time. It throws `std::runtime_error` if an ImGui backend can't start.
5. **`pxe::AudioDevice`**, if the game makes sound. See [Audio](#audio) for its ownership rule.
6. **`pxe::Camera`.** A plain value with no GPU state. Set where it looks with `setYawRadians` and `setTarget`.

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
renderer.init();

World world = buildWorld();
renderer.upload(world.scene);

pxe::Camera camera;
camera.setYawRadians(kYaw);
camera.setTarget(world.focus);
```

`World` is the example's own struct. It holds the `pxe::Scene` and the game's data about it (where its fire burns, where the camera starts). A real game keeps such data in its own types, beside the scene, not in it.

The camera's yaw is in radians. At yaw 0 the view looks along +z; in general its flat forward direction is `(sin yaw, 0, cos yaw)` and its right is `(-cos yaw, 0, sin yaw)`. The pitch (30°), the pixel density and the eye distance are fixed. `setTarget` jumps to a point. `follow` eases toward one. `setPanVelocity` pans on the ground, and `rotateSteps` turns the view by 45° steps with a short ease. Call `update(dt)` once a frame to run the easing.

## The frame loop
A frame is: poll input, update the game, work out the lighting, render, draw any UI, swap.

```cpp
pxe::Input input;
double previous = window.time();
while (!window.shouldClose()) {
  const double now = window.time();
  const float dt = static_cast<float>(now - previous);
  previous = now;

  window.pollEvents();
  input.update(window);
  if (input.keyPressed(pxe::Key::kEscape)) window.requestClose();
  if (input.keyPressed(pxe::Key::kQ)) camera.rotateSteps(-1);
  if (input.keyPressed(pxe::Key::kE)) camera.rotateSteps(1);
  camera.update(dt);

  const pxe::FrameParams params = frameParams(world, window.framebufferSize(), static_cast<float>(now));
  renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
  window.swap();
}
```

with the frame's draw list built fresh each time:

```cpp
pxe::FrameParams frameParams(const World& world, glm::ivec2 framebuffer, float time) {
  pxe::FrameParams params;
  params.framebufferW = framebuffer.x;
  params.framebufferH = framebuffer.y;
  params.time = time;
  params.lighting = pxe::lightingAt(world.scene, kHour, pxe::Weather{});
  params.fires.push_back({1, world.fireBase, 1.f, 0.f, 1.f});
  return params;
}
```

Step by step:

- **Input.** `Window::pollEvents()` first, then `Input::update(window)`. `keyDown` is the state now. `keyPressed` is true only on the frame a key goes down. The mouse (`mousePosition` in framebuffer pixels from the top left, `mouseDown`, `mousePressed`) and the first connected gamepad (`gamepadDown`, `gamepadPressed`, `gamepadStick` with a dead zone, `gamepadAxis` raw) work the same way. What a key does is the game's decision. If an `Overlay` is up, skip game keys while `Overlay::wantsKeyboard()` is true and clicks while `wantsMouse()` is true.
- **Game update.** Move your characters, advance your clock and weather, and update the camera (`Camera::update(dt)`).
- **Lighting.** `pxe::lightingAt(scene, hour, weather)` turns the hour (0-24) and a `pxe::Weather` into a `pxe::Lighting`: the sun or moon direction, the palettes blended for the time of day, clouds, wind, rain, god rays, firefly and firelight colours and any lightning flash. Use this three-argument form. The two-argument `lightingAt(scene, hour)` leaves clouds, wind, rain, firelight and firefly light at zero, so fires draw flames but light nothing. `Weather` holds what your game decides: `cloudCover`, `cloudOffset`, `cloudStrength`, `windDir`, `windStrength`, `rain`, `flash` and `flashDir`. A default `Weather{}` is clear and dry, with the wind at its default strength (1) blowing toward +x.
- **Render.** Fill a `pxe::FrameParams` and call `Renderer::render(scene, camera.frame(renderer.rtW(), renderer.rtH()), params)`. Pass the renderer's padded target size (`rtW()`, `rtH()`) to `Camera::frame`, not the window's. Set `framebufferW` and `framebufferH` to `Window::framebufferSize()` every frame, as the window may be resized or on a high-DPI display. They default to 0, and `render` throws `std::invalid_argument` if `present` is true and either is not positive. With `present` false they are not used and may stay 0. Set `time` to seconds of game time. It drives grass wind, fire, fireflies and water. Hold it fixed for repeatable captures. With `present` true (the default), `render` ends by scaling the view into the window's framebuffer, centred with black borders.
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

`FrameParams` also carries switches: `showGrass`, `edges` (outlines and rims), `fire`, `rays`, `fireflies`, `grassFire`. It carries the frame's lists too: `fires`, `lights` (extra `PointLight`s), `igniteGrass`, `gridStrokes`, `actors`, `actorMaterials` and `actorEffects`. Debug views are `magentaGrass`, `showNdl`, `testColours` and `grassDepthTest`. Build it fresh each frame; nothing in it is kept.

To find what the player clicks, `Renderer::rayThrough(camera, fbW, fbH, px)` gives the start of the view ray through a window pixel. The ray runs along `camera.forward`, and `scene.ground.raycast(origin, camera.forward, maxDist, hit)` finds where it meets the ground. `Renderer::windowPoint` goes the other way, from a world point to window pixels. Pass both the `CameraFrame` that was last rendered.

## Building a Scene
A `pxe::Scene` is plain data plus GPU meshes:

- `meshes`: opaque meshes, which all cast shadows. `water`: water sheets, with `waterLevel`, `waterMin` and `waterMax`.
- `grass`: every grass and leaf sprite (`GrassInstance`).
- `ground`, `waterTop` (`HeightMap`) and `shelter` (`ShelterMap`): the maps the game queries.
- `fireflyHomes`: points the fireflies wander round.
- `palette[kPhaseCount][kSurfCount]`, `rim[kPhaseCount]`, `ink[kPhaseCount]`: colours per phase of the day (`kPhaseDay`, `kPhaseGolden`, `kPhaseTwilight`, `kPhaseNight`) and per surface. A new `Scene` starts with the engine's default palettes.
- `tLit`, `tMid`: the toon ramp's N·L thresholds.
- `sunEast`, `sunSouth`, `sunPeakDegrees`: the sun's path.
- `horizonCentre`, `horizonRadius`, `horizonHeight`, `horizonRelief`: an optional ridge round the scene that the rising sun must clear.
- `boundsMin`, `boundsMax`: the scene's extent. Shadow maps are fitted to these.

Every vertex names its surface: `kSurfGrass`, `kSurfDirt`, `kSurfStone`, `kSurfBark`, `kSurfLeaf`, `kSurfWater` or `kSurfFoam`. Each surface has its own ramp: `band`, `detail` and `tip`, each shadow, mid and lit. A default-constructed `Scene` already has every ramp of every surface in all four phases, and `rim` and `ink` per phase: the engine's reference style, sampled from t3ssel8r's videos (day from his scene video; golden hour, twilight and night from his day/night video). `lightingAt` cross-fades the phases by sun elevation. The example keeps them all.

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

`buildSceneMaps` builds `scene.shelter` from the stone, `scene.ground` from grass, dirt, stone, bark and foam, and `scene.waterTop` from the water. `uploadScene` makes one `Mesh` per non-empty surface (water into `scene.water`), sets the water level and extent, and grows the scene's bounds. It needs the GL context.

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
`pxe::loadGltf(path)` reads a glTF 2.0 file (.glb or .gltf) into `GltfPrimitive`s: node name, material name, surface, world-space vertices and indices. Node transforms are already applied. The surface comes from the material name's prefix, case-insensitive: `Grass`, `Dirt`, `Bark`, `Leaf`, `Water` and `Foam` map to their surfaces, and anything else is stone. Textures, skinning and animation are ignored. It throws `std::runtime_error` if the file is missing or unreadable.

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

Rain (`Lighting::rain`) wets cells open to the sky, and wind (`Lighting::windDir`, `windStrength`) pushes the spread. `Renderer::upload` resets the grid.

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

**Materials.** Characters draw as surface `kActorSurface` plus a slot in `FrameParams::actorMaterials` (`kActorMaterials` slots of `ActorMaterial{colour, glow}`, colour in sRGB). A model's material *i* takes slot `firstMaterial() + i`, so give each kind of model its own range when loading. Each frame, copy every drawn model's `materials()` into those slots:

```cpp
const auto& mats = model.materials();
for (int i = 0; i < static_cast<int>(mats.size()) && model.firstMaterial() + i < pxe::kActorMaterials; ++i)
  params.actorMaterials[model.firstMaterial() + i] = mats[static_cast<std::size_t>(i)];
```

`FrameParams::actorEffects` takes world-space `Mesh` pointers drawn with the characters but without shadows, such as a sword's smear.

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

## Capture and writePng
To write the view to an image, render with a fixed `time`, then read back:

```cpp
// A few frames at fixed times, so a capture is the same on every run.
for (int f = 0; f < kCaptureFrames; ++f) {
  const pxe::FrameParams params = frameParams(world, window.framebufferSize(), static_cast<float>(f) / 60.f);
  renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
}
const bool ok = pxe::writePng(capture, renderer.viewW(), renderer.viewH(), renderer.readView());
```

- `Renderer::readView()` returns the low-res view (`viewW()` x `viewH()`, 960x540) without its padding, bottom row first, RGBA8. This is the art at one pixel per texel, before the window's upscale and sub-texel shift.
- `Renderer::readPresented(width, height)` reads the window's framebuffer, including any overlay drawn into it. Call it before `Window::swap`.
- `pxe::writePng(path, width, height, bottomUpRgba)` writes either to a PNG, flipped to top row first, creating parent directories. It returns false on failure.

Use a hidden window (`WindowOptions::visible = false`, `vsync = false`) for captures and tests. A capture is repeatable when the scene is built from fixed seeds and every frame uses a fixed `time`, hour and weather.

## Contracts
- **GL context first, last.** `Window` before anything that touches the GPU, and destroyed after all of it. `Mesh`, `SkinnedMesh`, `Scene` (it holds meshes), `SkinnedModel`, `Renderer` and `Overlay` all free GPU objects in their destructors.
- **Upload, then render.** Call `Renderer::upload(scene)` after building a scene and before rendering it, and again whenever you build or swap in a different scene. `render(scene, ...)` must be given the scene last uploaded. The renderer keeps shadow maps, fire state and the grass fire drawn from that scene and redraws them only when the light changes or `upload` is called. It never checks which scene it is given. If you change `scene.grass` afterwards (clearing blades under something placed later), call `Renderer::uploadGrass(scene)`. Don't change a scene's meshes without uploading again.
- **Scene build order.** Geometry into `SurfaceGeometry`, then `buildSceneMaps` and `uploadScene`, then scattering (`dropGrassUnder` reads the bounds `uploadScene` sets), then `Renderer::upload` (which sends the grass to the GPU).
- **The game owns every object.** The engine never creates a game object and keeps no pointer to one between calls. Everything per-frame (fires, actors and their joints, lights, ignitions, strokes) is passed in `FrameParams` and read during `render`. Joint matrices and effect meshes must stay alive until `render` returns. Fire ids must stay stable while a fire lives.
- **RNG order when scattering.** Every scatter function draws from the `std::mt19937` it is given, in a fixed order. Seed one generator and pass it to each call in a fixed sequence to get the same scene every run. Any change to an earlier call (another scatter, different spacing, a different ground) changes everything after it. To add something without disturbing an existing scene, scatter it from its own generator, or remove sprites after scattering (as `dropGrassUnder` does) rather than before.
- **Time.** `FrameParams::time` is seconds. Fires and the wind phase restart if it jumps backward or far ahead, and fires are always already burning when they start. Hold it fixed for repeatable images.
- **Threads.** The GL context is current only on the thread that made the window, so call the engine from that thread. Only the `AudioDevice` callback runs elsewhere.

## Header reference
Every public header, in `engine/include/pxe/`, namespace `pxe`.

- **`audio_device.hpp`.** `AudioDevice`: the default audio output, interleaved stereo float, filled by a `Render` callback on the audio thread. `open(sampleRate, render)`.
- **`camera.hpp`.** `Camera`: the orthographic camera, pitched 30°, yaw in 45° steps, smoothed and snapped to the texel grid (`setTarget`, `follow`, `setYawRadians`, `rotateSteps`, `setPanVelocity`, `nudgeImmediate`, `update`, `frame`). `CameraFrame`: one frame's matrices, eye, basis and snap error.
- **`config.hpp`.** `RenderConfig`: the fixed low-res target (`kViewW` 960, `kViewH` 540), `kPixelsPerUnit` (24), `kPad`, and the shadow map size `kShadow`.
- **`fire.hpp`.** `Fire`: one campfire's particle system (core, flames, embers, smoke). `FireParticle`: a particle as drawn. The renderer runs these for each `FireDraw`.
- **`fireflies.hpp`.** `Firefly` and `firefliesAt(homes, time)`: the swarm as a pure function of time.
- **`firegrid.hpp`.** `FireGrid`: the GPU grass and crown fire. `FireGrid::Stroke`: a fire, water or regrowth brush stroke. The renderer owns the grid. Games drive it through `FrameParams`.
- **`gl_loader.hpp`.** `loadGl(getProc)`: loads the GL entry points. `Window` calls it. A game only needs it if it makes its own context.
- **`gltf.hpp`.** `loadGltf(path)` and `GltfPrimitive`: static glTF geometry in world space, with a surface per material name.
- **`heightmap.hpp`.** `HeightMap`: highest surface per grid cell (`build`, `heightAt`, `cellTop`, `raycast`, `min`, `max`, `kNone`).
- **`image.hpp`.** `writePng(path, width, height, bottomUpRgba)`.
- **`input.hpp`.** `Input`: keyboard, mouse and first gamepad, read once a frame (`update`, `keyDown`, `keyPressed`, `mousePosition`, `mouseDown`, `mousePressed`, `gamepadConnected`, `gamepadName`, `gamepadDown`, `gamepadPressed`, `gamepadStick`, `gamepadAxis`). The enums are `Key`, `MouseButton`, `GamepadButton`, `GamepadStick` and `GamepadAxis`.
- **`lighting.hpp`.** `Lighting`: everything that lights one frame. `Weather`: the game's weather. `lightingAt(scene, hour)` and `lightingAt(scene, hour, weather)`, `sunDirection`, `moonDirection`.
- **`mesh.hpp`.** `Surface` (`kSurfGrass` .. `kSurfFoam`, `kSurfCount`), `Vertex`, `Mesh`. Also `SkinVertex` and `SkinnedMesh`, a skinned mesh uploaded once at rest.
- **`overlay.hpp`.** `Overlay`: Dear ImGui over the window (`begin`, `end`, `wantsKeyboard`, `wantsMouse`).
- **`renderer.hpp`.** `Renderer` (`init`, `upload`, `uploadGrass`, `render`, `readView`, `readPresented`, `rayThrough`, `windowPoint`, `viewW`, `viewH`, `rtW`, `rtH`, `setProfiling`, `passTimes`). Also `FrameParams`, `FireDraw`, `PointLight`, `ActorDraw` and `PassTimes`.
- **`scatter.hpp`.** `scatterGrass` with `GrassScatter`, `dropGrassUnder`, `scatterLeaves` with `kDefaultLeafDensity`, `poissonDisk` and `grassPatchiness`.
- **`scene.hpp`.** `Scene` (constructed with the default palettes), `GrassInstance`, `Palette` and `Phase` (`kPhaseDay` .. `kPhaseNight`, `kPhaseCount`).
- **`scene_geometry.hpp`.** `SurfaceGeometry` (`append`, `corners`), `buildSceneMaps`, `uploadScene`.
- **`shelter.hpp`.** `ShelterMap`: roofing over the ground (`build`, `sheltered`, `coveredLines`).
- **`skinned.hpp`.** `SkinnedModel`: skinned glTF characters with clips, poses, blending and skinning. Also `ActorMaterial`, `kActorSurface`, `kActorMaterials` and `kMaxJoints`.
- **`triangle_grid.hpp`.** `Triangle`, `trianglesOf(vertices, indices)`, and `TriangleGrid`, which finds the triangle under an xz point (`at`).
- **`window.hpp`.** `Window`: the window and its GL context (`shouldClose`, `requestClose`, `pollEvents`, `swap`, `framebufferSize`, `windowSize`, `time`, `setTitle`). `WindowOptions` sets its size, title, visibility and vsync.
