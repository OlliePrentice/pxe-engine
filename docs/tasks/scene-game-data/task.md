# Game-only data out of pxe::Scene
Type: feature
Status: open
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
- [ ] `grep -rn "FireSite\|focus\|yawDegrees\|fires" engine/include/pxe/scene.hpp` finds nothing.
- [ ] No engine source refers to the removed fields.
- [ ] `game::buildScene` returns the game build result; every former use of `scene.fires`, `scene.focus` and `scene.yawDegrees` in game/ reads it from there.
- [ ] Captures are byte-identical to the cycle base: `--capture` with `--hour 15`, `--hour 21`, `--scene hills`, and `--hour 21 --candles --shelter-test --extra-fires 6`; plus the build log lines for fires ("pxe: fire at ...", "candle at ...") identical for the last one. Record commands and results in the handoff.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- Moving any scene-building code into the engine (scene-geometry, scene-scatter tasks).

## Cycle log
