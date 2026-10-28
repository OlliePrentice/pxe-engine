# Engine defaults that work for a new game
Type: fix
Status: complete
Cycle: 2

## Goal
Three engine defaults make a new game wrong until it overrides them, as the engine guide had to warn (docs/engine-guide.md):
- `pxe::Scene`'s palettes, `rim` and `ink` start black, so a scene renders black until the game sets 4 phases x 7 surfaces x 3 ramps.
- `pxe::GrassScatter`'s spacings are tuned at 16 pixels per metre while the engine draws at `RenderConfig::kPixelsPerUnit` (24), so every caller must rescale them; `scatterLeaves` has no default density at all.
- `pxe::FrameParams::framebufferW/H` default to 1920x1080, so a caller that forgets them presents at the wrong size on any other window or a high-DPI display, silently.
Give each a default that is right, update the game to rely on the new defaults where it only restated or rescaled them, and update the guide and example. The game's output must not change.

## Decisions
- **Palette:** the game's current palettes (`setPhasePalettes` in game/src/scene.cpp: all four phases, all seven surfaces, and `rim`/`ink`) become the engine's default for a default-constructed `pxe::Scene`. They are the reference style sampled from t3ssel8r's videos (crediting him is allowed), not this game's content. The game deletes `setPhasePalettes` and keeps only what differs from the default: the hills scene's day-grass override, and each scene's `tLit`/`tMid`. Colours are computed exactly as now (`channel / 255.f`), so they are bit-identical.
- **Scatter density:** `GrassScatter::clumpSpacing` and `tuftSpacing` default to their current values scaled to the engine's pixel density, i.e. `0.34f / (RenderConfig::kPixelsPerUnit / 16.f)` and `2.f / (RenderConfig::kPixelsPerUnit / 16.f)`, written so the float operations match what the game does today. `scatterLeaves` gets a default density likewise (6 per m² at 16 px/m, times the density factor squared). How to expose the leaf default (a named constant, or reordering parameters to allow a default argument) is the developer's choice; prefer the smaller API change.
- **Game scatter options:** the game's rescale in main.cpp goes. `game::BuildOptions` defaults its spacings and leaf density from the engine defaults rather than restating numbers. `--min-dist` now takes the clump spacing in metres as actually used (README already says "metres"); its default is the engine default.
- **Framebuffer size:** `FrameParams::framebufferW/H` default to 0. `Renderer::render` throws `std::invalid_argument` when `present` is true and either is not positive. When `present` is false they are not used and may stay 0. Recommended over keeping a guessed default (silent) or having the renderer hold the window (a pointer kept between calls, which architecture.md rules out).
- **Game framebuffer:** every `render` call with `present` true already sets the size; keep their values as they are. If any presenting path sets something other than `Window::framebufferSize()`, record it in handoff.md, don't change it in this task.
- **Minimised window (cycle 2, ISSUE-001):** GLFW reports 0x0 for a minimised window on Windows. The engine keeps the throw; the game's interactive loop and the example's window loop skip drawing (render, overlay, swap) while the framebuffer has a zero dimension, keep polling events and updating, and don't busy-spin. The guide says so.
- **Help text:** fix the game's `--help` text that says captures are 640x360 (they are `RenderConfig::kViewW` x `kViewH`, 960x540), and any other stale size in it.
- **Example and guide:** the example drops its own tinted palettes and its density rescale and relies on the defaults; it still sets the framebuffer size. The guide is updated to match: palettes have a default (and how to override one surface/phase), scatter defaults are at the engine's density, framebuffer size is required when presenting and `render` throws otherwise. The guide's "Palettes start black" and rescale advice go.

## Layering
- Engine: `pxe::Scene` default palette (scene.hpp, plus a .cpp if needed), `pxe::GrassScatter` defaults and the leaf default (scatter.hpp/.cpp), `FrameParams` defaults and the check in `Renderer::render` (renderer.hpp/.cpp).
- Game: scene.cpp (drop `setPhasePalettes`, keep overrides), scene.hpp (`BuildOptions` defaults), main.cpp (drop rescale, `--min-dist` default, help text).
- Example and docs: examples/minimal/main.cpp, docs/engine-guide.md.

## Acceptance criteria
- [x] A default-constructed `pxe::Scene` has the palettes, rim and ink that `game::setPhasePalettes` sets today, bit for bit; the engine comment credits the source without naming game content. (engine/src/scene.cpp Scene::Scene: every set/rim/ink line identical to the removed game function, same hex()/255.f; captures identical)
- [x] `game::setPhasePalettes` is gone; the game sets only the hills day-grass ramps and each scene's `tLit`/`tMid`. (game/src/scene.cpp: buildHills sets the day-grass ramps and tLit/tMid; setUpRuins sets tLit/tMid)
- [x] `GrassScatter` defaults equal the values the game passes today (0.34f and 2.f divided by `kPixelsPerUnit / 16.f`), and `scatterLeaves` has a default density equal to what the game passes today (13.5). (scatter.hpp GrassScatter defaults and kDefaultLeafDensity = 6*(1.5*1.5) = 13.5; clump counts unchanged)
- [x] main.cpp no longer rescales scatter options; `BuildOptions` takes its defaults from the engine; `--min-dist` is the spacing actually used. (main.cpp rescale removed; BuildOptions uses GrassScatter{} and kDefaultLeafDensity; Options::minDist = GrassScatter{}.clumpSpacing)
- [x] `FrameParams::framebufferW/H` default to 0, and `render` throws `std::invalid_argument` with a clear message when presenting without a positive size. Every presenting call in the game and the example sets it. (renderer.hpp defaults 0; Renderer::render throws invalid_argument with the size and the fix; all presenting calls pass framebufferSize(); minimised-window skip in the game and example, ISSUE-001)
- [x] `--help` no longer says 640x360. (printUsage prints RenderConfig::kViewW x kViewH)
- [x] The example relies on the default palette and scatter defaults; its capture still shows the scene (record what it looks like in handoff.md). (example drops its palettes and rescale; my capture shows lawn, stone, tree, campfire, god rays)
- [x] docs/engine-guide.md matches the new defaults; every snippet still matches the example word for word, and every name exists. (guide updated; developer script-checked snippets against the example)
- [x] The game's captures and logs are byte-identical to the cycle base for the four standard cases (default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`), plus `--window-png` on the default scene; `--self-test` passes. (my cmp against the b709c58 build after cycle 2: 4 captures + window PNG identical, logs identical apart from 'wrote'; --self-test PASS)
- [x] Project builds cleanly from a fresh build directory with CMake (fresh build-check exit 0; only the stb sprintf warning)

## Assets
None

## Out of scope
- Any other default (the two-argument `lightingAt`, logging to stderr, camera zoom).
- Changing which framebuffer size any game path passes.
- A palette API beyond the defaults (loading palettes from a file, named presets).

## Cycle log
- Cycle 1: 2026-10-27 started, base ea2fd4a
- Cycle 1: review: game output byte-identical (4 captures, window PNG, self-test); ISSUE-001 opened (minimised window throws)
- Cycle 2: 2026-10-27 started, base 37a9732; ISSUE-001
- Cycle 2: ISSUE-001 resolved; review passed; complete. The skip path was exercised on macOS with a scratch hack, not on Windows.
