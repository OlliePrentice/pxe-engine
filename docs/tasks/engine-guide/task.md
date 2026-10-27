# Engine integration guide and minimal example
Type: feature
Status: complete
Cycle: 1

## Goal
Nothing tells a new project, or a model working on one, how to use pxe. docs/architecture.md says what goes where; README.md describes this game. Write docs/engine-guide.md, a guide to building a game on the engine, and examples/minimal/, a small program built in this repo that follows the guide, so the guide is compile-checked on every build.

## Decisions
- The guide is written for both people and LLMs: plain, complete, every API name exact, no reliance on reading the game's code.
- The example is procedural: it uses no files from assets/ or game/, so it shows the engine alone.
- The example is built by the root CMakeLists.txt by default, so every build check compiles it.
- The example takes `--capture <file.png>` (render a few frames, write the view, exit) so it can be checked without a person. No other flags.

## Layering
- Engine: none. No engine API changes; if the guide exposes a gap, record it in handoff.md rather than changing the engine.
- Game: none. examples/minimal is its own executable, using only `pxe/*.hpp`.

## Acceptance criteria
- [x] docs/engine-guide.md covers, in this order: what the engine is and isn't (pointing to docs/architecture.md for the rules); adding it to a CMake project (add_subdirectory and FetchContent SOURCE_SUBDIR forms, dependencies it brings, the C++20 requirement); start-up order and ownership (Window, Renderer, Overlay, AudioDevice, Camera); the frame loop (input, game update, lighting via pxe::lightingAt, Renderer::render with FrameParams, overlay, swap); building a Scene (by hand, from .glb via loadGltf + buildSceneMaps + uploadScene, scattering grass and leaves, HeightMap/ShelterMap); fires, FireGrid and fireflies; skinned models; audio (engine plays, game owns sounds); capture/writePng; contracts (upload-then-render, the game owns every object, RNG order when scattering); and a header-by-header reference of engine/include/pxe with one or two lines each. (docs/engine-guide.md: sections in the required order, ending with a header reference covering all 22 headers)
- [x] Every type, function, field and header the guide names exists with that exact name and signature in engine/include/pxe. (developer script-checked every backticked name; I spot-checked 42 names plus the FireDraw, Overlay, rayThrough, framebufferSize and Weather signatures in engine/include/pxe: all present)
- [x] examples/minimal/main.cpp builds a small procedural scene (ground plus at least one solid, with grass), lights it at a fixed hour, renders in a window until closed or Escape, and supports `--capture`. It includes only pxe/ headers, glm and the standard library. (examples/minimal/main.cpp: buildWorld, frameParams, main; includes only pxe/, glm, std)
- [x] The example's code matches what the guide shows: snippets in the guide are taken from it or consistent with it. (the guide's snippets match main.cpp word for word: start-up, frame loop, frameParams, palettes, sun, geometry, scatter, capture)
- [x] The example's `--capture` output shows the scene (record the PNG path and a description in handoff.md). (I re-ran it from the scratchpad: the PNG shows lawn, stone, tree, campfire and god rays, and is byte-identical to the developer's)
- [x] The game's captures are byte-identical to the cycle base (same four cases as engine-standalone-build). (diff 34d0256..aa36acb touches no engine/ or game/ source; developer's 4-case cmp identical)
- [x] README.md mentions the example and the guide in one or two lines. (README.md Build section: a paragraph pointing to the guide and example)
- [x] Project builds cleanly from a fresh build directory with CMake (fresh build-check exit 0; only warning is the pre-existing stb sprintf one)

## Assets
None

## Out of scope
- Engine or game code changes.
- More than one example.
- The root CLAUDE.md (orchestrator writes it after this task).

## Cycle log
- Cycle 1: 2026-10-27 started, base 61c7d08
- Cycle 1: review passed, no issues; complete. Engine gaps found by the developer are listed in handoff.md (palettes default black, GrassScatter tuned at 16 px/m, framebuffer size defaults, unconditional stderr logging, etc.); not acted on.
