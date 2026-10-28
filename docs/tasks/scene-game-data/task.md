# Game-only data out of pxe::Scene
Type: feature
Status: complete
Cycle: 1

## Goal
`pxe::Scene` carries three things only the game uses: `fires` (with the `FireSite` type), `focus` and `yawDegrees` (engine/include/pxe/scene.hpp). No engine code reads them. docs/architecture.md marks them game data that is misplaced in an engine type. Move them into a game-side result of building a scene, so `pxe::Scene` holds only what the engine draws and queries. This must land before the scene-geometry task, because an engine scene builder can't produce campfires or a camera start. Rendered output must not change.

## Decisions
- `FireSite` moves to the game unchanged (base, sheltered, size), in game/src/scene.hpp, namespace `game`.
- `game::buildScene` returns a game struct holding the `pxe::Scene` plus `fires`, `focus` and `yawDegrees` (name it for what it is, e.g. `game::BuiltScene`). `game::laidOut` keeps its signature and gets `focus` from the same place internally.
- Inside scene.cpp the ruins and hills builders set fires, focus and yaw on the game struct instead of the `pxe::Scene`. Fire placement keeps its exact order and RNG use.
- `WorldClock::placeFires` takes `std::vector<game::FireSite>`.

## Layering
- Engine: `pxe::Scene` loses `fires`, `focus`, `yawDegrees` and `FireSite` (engine/include/pxe/scene.hpp); comments updated. Nothing else in engine/ changes.
- Game: game/src/scene.hpp/.cpp own `FireSite` and the build result; main.cpp reads the camera start and fires from it; settings.hpp/.cpp take `game::FireSite`.

## Acceptance criteria
- [x] `grep -rn "FireSite\|focus\|yawDegrees\|fires" engine/include/pxe/scene.hpp` finds nothing. (dac6987: grep finds nothing)
- [x] No engine source refers to the removed fields. (git diff touches only engine/include/pxe/scene.hpp in engine/)
- [x] `game::buildScene` returns the game build result; every former use of `scene.fires`, `scene.focus` and `scene.yawDegrees` in game/ reads it from there. (game::BuiltScene; main.cpp, selfTest and placeFires read built.*)
- [x] Captures are byte-identical to the cycle base: `--capture` with `--hour 15`, `--hour 21`, `--scene hills`, and `--hour 21 --candles --shelter-test --extra-fires 6`; plus the build log lines for fires ("pxe: fire at ...", "candle at ...") identical for the last one. Record commands and results in the handoff. (developer: four cmp equal plus fire log lines; orchestrator: --hour 21 --candles --shelter-test --extra-fires 6 --foxes 3, cmp equal against the pre-change build)
- [x] Project builds cleanly from a fresh build directory with CMake. (fresh build-check exit 0)

## Assets
None

## Out of scope
- Moving any scene-building code into the engine (scene-geometry, scene-scatter tasks).

## Cycle log
- Cycle 1: 2026-10-26 started, base b2a7e0b
- Cycle 1: review passed, no issues; complete
