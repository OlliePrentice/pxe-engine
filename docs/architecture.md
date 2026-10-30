# Architecture: engine and game

The project is **pxe**, a reusable engine for pixel-art 3D games in this style (`engine/`, the `pxe_engine` library, namespace `pxe`), and two games built on it:
- **the game** (`game/`, the `pxe` executable, namespace `game`): the ruins, the tavern, the hero and the foxes, seen through the orthographic camera. Its assets are in the top-level `assets/` and `tools/blender/`, and its art style in `game/docs/art-style.md`.
- **the desert game** (`desert/`, the `pxe_desert` executable, namespace `desert`): first person over procedural sand dunes with one date palm. Its assets are in `desert/assets/` and `desert/tools/blender/`, and its art style in `desert/docs/art-style.md`.

`examples/minimal/` is a small program that uses only the engine, as the engine guide's worked example. Every piece of code belongs to exactly one of the engine or a game, and where it goes is decided on purpose, never by where the nearest similar code happens to be. The rules below that say "the game" hold for each game. Neither game includes the other's code.

How to use the engine (CMake, start-up order, the frame loop, building a scene, its contracts) is in [engine-guide.md](engine-guide.md).

## The test
Would a different game in this style need this code, unchanged? Yes: it is engine. No: it is game.

A feature often needs both. Split it into the engine capability, named for what it does, and the game's use of it. A tavern with an opening door is a door model, placed and animated by the game (game), drawn with meshes, a skinned or rigid animated model and indoor lighting (engine). If the engine lacks a capability the game needs, add the capability to the engine in general terms. Don't add a game-shaped special case.

## Rules
- **Dependencies run one way.** The game includes engine headers. The engine never includes game headers, never calls game code, and never knows game data exists. The build already enforces the include rule: `engine/` has no include path into `game/` or `desert/`.
- **The game owns its objects.** The game creates, holds and destroys every object in its world: the player character, NPCs (the fox), buildings (the tavern), props, the fires it places, and any scene it builds. The engine supplies the types they're made from (`Mesh`, `SkinnedModel`, `Fire`, `Scene`, `HeightMap`, ...) and the systems that run them (`Renderer`, `Camera`, `Lighting`, ...). Each frame the game tells the engine what to draw through `FrameParams` and its draw structs. The engine never spawns a game object and never keeps a pointer to one between calls.
- **The engine owns only its own machinery.** GPU resources, shadow maps, particle state inside a `Fire`, and caches are the engine's, and are created and freed by the engine type that uses them.
- **No game content in the engine.** No names, asset paths, file names, world positions, layout constants or tuning that belong to this game: no fox, hero, tavern, ruins or Sleeping Fox in engine identifiers, APIs or comments. Generic real-world nouns used to explain a capability are fine ("1 is a campfire, about 0.2 a candle"). So are credits to references (t3ssel8r).
- **Game values come in as data.** When the game needs to set something, the engine takes it as a parameter, a field, or a loaded file. Don't add parameters speculatively. A value becomes a parameter when a game needs to set it. Until then it is an engine default.
- **No speculative promotion.** A small helper (math, geometry, an easing curve, a smoothing filter) stays with its only user, even if it is generic. It moves to the engine when a third user needs it unchanged, or when it is part of an engine capability being built. Until then a second user keeps its own copy (as with `SecondOrder` below), and engine code that needs a game helper writes its own: the engine never includes game code. Similar code that behaves differently (other edge cases, other fallbacks) is not the same helper. Duplicating a helper inside one layer is fine.
- **Asset choice is the game's.** The engine loads whatever file it is given. Which `.glb` to load, and where its objects go, is game code.

## Render modes
The engine renders in one of two separate modes, orthographic (`pxe::Camera`) and perspective (`pxe::FirstPersonCamera`), so that tuning one never means a trade-off against the other (user decision, 2026-10-29):
- **Work on one mode never changes the other's output.** `tools/ortho-check.sh` and `tools/perspective-check.sh` prove it: run both for any change to the renderer, the shaders, the cameras, `Scene` or `Lighting`.
- **Each mode has its own frame and its own screen-space shaders.** That means its own frame function (which passes run, in what order, with which uniforms) and its own edge pass. A feature for one mode goes in that mode's code. It is never a branch in the other's.
- **Each mode has its own lighting look** (user decision, 2026-10-30). Orthographic lights with the toon ramp (`mesh.frag`, `common.glsl`). Perspective lights with a smooth lit model (`mesh_perspective.frag`, `lit_perspective.glsl`, from task `perspective-smooth-lighting`): an albedo per surface or material, lit by the sun or moon with soft shadows, a sky fill and a ground bounce, with continuous haze and sky. Neither mode switches to the other's look.
- **The machinery underneath is shared.** That covers render targets, upload, mesh drawing, the mesh and toon shaders (`mesh.frag`, `common.glsl`), the shadow-map shader and present. Shared code changes only if both modes' checks pass, or with the user's explicit approval for the mode whose output changes. When one mode needs a shared shader to behave differently, fork that shader for that mode; don't add a mode branch to it.

## Where things live
Home `game` is `game/`, `desert` is `desert/`.

| Area | Home | Status |
|---|---|---|
| Rendering, shaders, toon ramps, edge pass, water, rain, god rays | engine | in place: the orthographic frame in `engine/src/renderer_orthographic.cpp` and its edge pass `engine/shaders/edge.frag`; the machinery both modes share in `engine/src/renderer.cpp` and `renderer_internal.hpp` |
| Meshes, glTF loading, skinning and animation clips | engine | in place |
| Camera, lighting, day/night palette blending | engine | in place |
| Perspective first-person camera, and what the renderer draws under it: the smooth lit model (albedo per surface, sun or moon with soft shadows, sky fill, ground bounce), the sky backdrop (a continuous horizon-to-zenith gradient with a sun glow and disc, default palettes per phase blended by time of day, and two layers of wind-aligned sky clouds coloured from the light, off by default and casting no shadows), airborne dust (stateless motes drifting on the wind round the eye, coloured from the haze, off by default, drawn before the view model), exponential distance haze into the horizon colour, the eye-fitted shadow map, the perspective edge pass | engine | in place: `pxe::FirstPersonCamera`, `pxe::PerspectiveFrame`, `pxe::RenderMode::kPerspective`, the perspective frame in `engine/src/renderer_perspective.cpp` and its edge pass `engine/shaders/edge_perspective.frag`, its mesh shader `mesh_perspective.frag` with `lit_perspective.glsl`, the sky `backdrop.frag`, `Scene::skyHorizon`/`skyZenith` and the light fields after them, `pxe::SkyCloudLayer` and `Scene::skyClouds`, `Scene::dustDensity`/`dustSize` and `dust.vert`/`dust.frag`, `Weather::skyCloudOffset`, `Lighting::sunLight` and the rest |
| A view model under the perspective camera: characters attached to the view, drawn in front of the whole scene, lit and shadowed but casting no shadow, outlined at its true depth | engine | in place: `FrameParams::viewModel`, drawn in `engine/src/renderer_perspective.cpp`, its outlines in `engine/shaders/edge_perspective.frag` |
| Turning weather (rain, cloud, wind, lightning) and time of day into `pxe::Lighting`: overcast, god-ray strength, firefly and firelight bands, the lightning wash | engine | in place: `pxe::Weather`, `pxe::lightingAt(scene, hour, weather)` |
| Fire, grass fire, fireflies, shelter map, height maps | engine | in place |
| Desert ground: a sand surface with default palettes for every phase, walkable like grass and dirt | engine | in place: `pxe::kSurfSand`, glTF material `Sand` |
| Indoor rooms: shadow-only geometry, sky windows, fill light, no sky effects indoors; wood, plaster, roof and oak surfaces with default palettes; dark regions (unlit boxes open on one face, fading with depth) | engine | in place: `pxe::Scene::indoors`, `shadowOnly`, `windows`, `fillDir`, `darkRegions` |
| A room's layout, which glTF nodes are shadow-only or kept out of the ground, the doorway passages between scenes, night-only fires, where a dark region goes and how dark it is | game | in place: `game/src/scene.cpp`, `game/src/tavern.cpp`, `WorldClock::followSun` |
| Window and main loop (GLFW) | engine | in place: `pxe::Window` |
| UI overlay: Dear ImGui set up against the window and OpenGL (context, platform and renderer backends, per-frame begin and end, whether the UI has the keyboard or mouse) | engine | in place: `pxe::Overlay` |
| Input devices: keyboard, mouse and gamepad to raw input (key and button edges, mouse motion, stick dead zones, controller mappings), and capturing the cursor for mouse look | engine | in place: `pxe::Input` (`mouseDelta`), `pxe::Window::setCursorCaptured` |
| Audio playback (miniaudio) | engine | in place: `pxe::AudioDevice` |
| Turning a `.glb` into a `pxe::Scene`: per-surface geometry, meshes, water, height maps, shelter, the triangle grid | engine | in place: `pxe::SurfaceGeometry`, `pxe::TriangleGrid`, `pxe::buildSceneMaps`, `pxe::uploadScene` |
| Grass and leaf scattering, Poisson sampling, dropping grass under stone and bark | engine | in place: `pxe/scatter.hpp` |
| Writing images to PNG | engine | in place: `pxe::writePng` |
| Writing audio to WAV | engine | in place: `pxe::writeWav` (`pxe/audio_file.hpp`) |
| Second-order smoothing (`SecondOrder`) | engine | duplicated: a private copy in `engine/src/camera.cpp`, a public one in `game/src/hero.hpp`. Promote to the engine when a third user needs it |
| Game-only data on `pxe::Scene` (`fires`, `focus`, `yawDegrees`): the engine never reads it | game | in place: `game::BuiltScene`, `game::FireSite` |
| Which scene to build, and where objects are placed in it (`kTavernSpot`, `kRuinsFoxSpot`, ...) | game | in place |
| Player character, fox, tavern, fireball | game | in place |
| The desert's world: the dune height function and its smooth-shaded sand ground (1 m cells in the middle, 4 m cells out to the edge), the date palm loaded and placed at the origin, the sun's directions, the warm sand ground bounce, its two sky cloud layers and airborne dust (the rest of its light, sky and haze are the engine defaults) | desert | in place: `desert/src/world.cpp` |
| The desert's player: the eye, its yaw and pitch, walking and sprinting, ground following, sliding along the palm's trunk and the play area's edge, the ground distance the eye moved in each update, driving `pxe::FirstPersonCamera` | desert | in place: `desert/src/player.cpp` |
| The desert player's arms: the arms model loaded into actor slots 0-14, placed at the eye from the view each frame, crease ink on its wraps, gloves, brass, worn patches and grime; the walk: the `Arms_Walk` and `Arms_Sprint` clips played by the distance moved and blended in by speed, the rest pose when still; holding one clip at a stride phase for the menu's preview | desert | in place: `desert/src/arms.cpp` |
| World clock and weather rules (`WorldClock`): time of day, when it rains, when lightning strikes, cloud drift, fires burning down and rain putting them out. The engine takes the results as data | game | in place |
| Input actions (what a key or button does), the settings menu's contents and style (its `ImGui::` widget calls), which sounds play when | game | in place |
| The desert's input actions (mouse and right stick look, WASD and left stick walk, sprint, cursor capture and release, Tab and Start for the settings menu) | desert | in place: `desert/src/main.cpp` |
| The desert's settings menu: its contents and `ImGui::` calls on `pxe::Overlay`, the running clock and cloud weather it edits (`desert::Clock`, which also drifts the sky clouds on the wind), the player and view tunables (`PlayerSettings`, `ViewSettings`), the Lighting section that edits the scene's light, sky and haze for the phase in effect and prints them as code for `buildWorld`, the Sky and dust section (cloud layers, cloud brightness, dust) printed with them | desert | in place: `desert/src/settings.cpp`, created in `main.cpp`'s play branch only |
| The sounds themselves: sample files, or the recipe of a synthesised sound (the wind, rain and thunder synth in `game/src/audio.cpp`). A sound is content, like a model. Generic synthesis building blocks (noise, filters, envelopes) move to the engine when a second sound or game needs them | game | in place |
| Command-line modes: capture, bench, self-test | game | in place |
| Command-line modes: help, capture, window PNG, hour, walk, menu | desert | in place: `desert/src/main.cpp` |

## Standing rules per game
- **Desert tunables go in its settings menu.** Every desert feature that adds a tunable (sky, haze, clouds, dust, sand, lighting, ...) adds its controls to the desert's settings menu (`desert::SettingsMenu`, `desert/src/settings.cpp`, from task `desert-settings-menu`), defaulting to the value the desert uses without the menu, so play opens unchanged and captures stay byte-identical (user decision, 2026-10-30). The desert's lighting tunables (light, sky, haze, exposure) are in its Lighting section (task `desert-lighting`).

## Misplaced code
Code marked **misplaced** above is moved only by a dedicated refactor task, never as a side effect of a feature. Until then:
- Extend misplaced code where it currently lives. Don't start a second copy of it in the other layer.
- A new, self-contained capability that passes the test goes in the engine, even if related misplaced code is still in the game.
- Report any misplaced code a task touched in its handoff, so the refactor task can be scoped.
