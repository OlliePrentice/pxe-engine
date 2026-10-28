# Architecture: engine and game

The project is two things: **pxe**, a reusable engine for pixel-art 3D games in this style (`engine/`, the `pxe_engine` library, namespace `pxe`), and **the game**, one implementation built on it (`game/`, the `pxe` executable, namespace `game`). Every piece of code belongs to exactly one of them, and where it goes is decided on purpose, never by where the nearest similar code happens to be.

How to use the engine (CMake, start-up order, the frame loop, building a scene, its contracts) is in [engine-guide.md](engine-guide.md).

## The test
Would a different game in this style need this code, unchanged? Yes: it is engine. No: it is game.

A feature often needs both. Split it into the engine capability, named for what it does, and the game's use of it. A tavern with an opening door is a door model, placed and animated by the game (game), drawn with meshes, a skinned or rigid animated model and indoor lighting (engine). If the engine lacks a capability the game needs, add the capability to the engine in general terms. Don't add a game-shaped special case.

## Rules
- **Dependencies run one way.** The game includes engine headers. The engine never includes game headers, never calls game code, and never knows game data exists. The build already enforces the include rule: `engine/` has no include path into `game/`.
- **The game owns its objects.** The game creates, holds and destroys every object in its world: the player character, NPCs (the fox), buildings (the tavern), props, the fires it places, and any scene it builds. The engine supplies the types they're made from (`Mesh`, `SkinnedModel`, `Fire`, `Scene`, `HeightMap`, ...) and the systems that run them (`Renderer`, `Camera`, `Lighting`, ...). Each frame the game tells the engine what to draw through `FrameParams` and its draw structs. The engine never spawns a game object and never keeps a pointer to one between calls.
- **The engine owns only its own machinery.** GPU resources, shadow maps, particle state inside a `Fire`, and caches are the engine's, and are created and freed by the engine type that uses them.
- **No game content in the engine.** No names, asset paths, file names, world positions, layout constants or tuning that belong to this game: no fox, hero, tavern, ruins or Sleeping Fox in engine identifiers, APIs or comments. Generic real-world nouns used to explain a capability are fine ("1 is a campfire, about 0.2 a candle"). So are credits to references (t3ssel8r).
- **Game values come in as data.** When the game needs to set something, the engine takes it as a parameter, a field, or a loaded file. Don't add parameters speculatively. A value becomes a parameter when a game needs to set it. Until then it is an engine default.
- **No speculative promotion.** A small helper (math, geometry, an easing curve, a smoothing filter) stays with its only user, even if it is generic. It moves to the engine when a third user needs it unchanged, or when it is part of an engine capability being built. Until then a second user keeps its own copy (as with `SecondOrder` below), and engine code that needs a game helper writes its own: the engine never includes game code. Similar code that behaves differently (other edge cases, other fallbacks) is not the same helper. Duplicating a helper inside one layer is fine.
- **Asset choice is the game's.** The engine loads whatever file it is given. Which `.glb` to load, and where its objects go, is game code.

## Where things live
| Area | Home | Status |
|---|---|---|
| Rendering, shaders, toon ramps, edge pass, water, rain, god rays | engine | in place |
| Meshes, glTF loading, skinning and animation clips | engine | in place |
| Camera, lighting, day/night palette blending | engine | in place |
| Turning weather (rain, cloud, wind, lightning) and time of day into `pxe::Lighting`: overcast, god-ray strength, firefly and firelight bands, the lightning wash | engine | in place: `pxe::Weather`, `pxe::lightingAt(scene, hour, weather)` |
| Fire, grass fire, fireflies, shelter map, height maps | engine | in place |
| Indoor rooms: shadow-only geometry, sky windows, fill light, no sky effects indoors; wood, plaster, roof and oak surfaces with default palettes | engine | in place: `pxe::Scene::indoors`, `shadowOnly`, `windows`, `fillDir` |
| A room's layout, which glTF nodes are shadow-only or kept out of the ground, the doorway passages between scenes, night-only fires | game | in place: `game/src/scene.cpp`, `game/src/tavern.cpp`, `WorldClock::followSun` |
| Window and main loop (GLFW) | engine | in place: `pxe::Window` |
| UI overlay: Dear ImGui set up against the window and OpenGL (context, platform and renderer backends, per-frame begin and end, whether the UI has the keyboard or mouse) | engine | in place: `pxe::Overlay` |
| Input devices: keyboard, mouse and gamepad to raw input (key and button edges, stick dead zones, controller mappings) | engine | in place: `pxe::Input` |
| Audio playback (miniaudio) | engine | in place: `pxe::AudioDevice` |
| Turning a `.glb` into a `pxe::Scene`: per-surface geometry, meshes, water, height maps, shelter, the triangle grid | engine | in place: `pxe::SurfaceGeometry`, `pxe::TriangleGrid`, `pxe::buildSceneMaps`, `pxe::uploadScene` |
| Grass and leaf scattering, Poisson sampling, dropping grass under stone and bark | engine | in place: `pxe/scatter.hpp` |
| Writing images to PNG | engine | in place: `pxe::writePng` |
| Writing audio to WAV | engine | in place: `pxe::writeWav` (`pxe/audio_file.hpp`) |
| Second-order smoothing (`SecondOrder`) | engine | duplicated: a private copy in `engine/src/camera.cpp`, a public one in `game/src/hero.hpp`. Promote to the engine when a third user needs it |
| Game-only data on `pxe::Scene` (`fires`, `focus`, `yawDegrees`): the engine never reads it | game | in place: `game::BuiltScene`, `game::FireSite` |
| Which scene to build, and where objects are placed in it (`kTavernSpot`, `kRuinsFoxSpot`, ...) | game | in place |
| Player character, fox, tavern, fireball | game | in place |
| World clock and weather rules (`WorldClock`): time of day, when it rains, when lightning strikes, cloud drift, fires burning down and rain putting them out. The engine takes the results as data | game | in place |
| Input actions (what a key or button does), the settings menu's contents and style (its `ImGui::` widget calls), which sounds play when | game | in place |
| The sounds themselves: sample files, or the recipe of a synthesised sound (the wind, rain and thunder synth in `game/src/audio.cpp`). A sound is content, like a model. Generic synthesis building blocks (noise, filters, envelopes) move to the engine when a second sound or game needs them | game | in place |
| Command-line modes: capture, bench, self-test | game | in place |

## Misplaced code
Code marked **misplaced** above is moved only by a dedicated refactor task, never as a side effect of a feature. Until then:
- Extend misplaced code where it currently lives. Don't start a second copy of it in the other layer.
- A new, self-contained capability that passes the test goes in the engine, even if related misplaced code is still in the game.
- Report any misplaced code a task touched in its handoff, so the refactor task can be scoped.
