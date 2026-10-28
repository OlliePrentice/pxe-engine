## Cycle 1
Build: pass

### Changes
- engine/include/pxe/scene.hpp: removed `FireSite`, `Scene::fires`, `Scene::focus` and `Scene::yawDegrees`. Reworded the `shelter` comment ("keeps the rain off what stands under it") so it no longer names fires and the acceptance grep finds nothing. No other engine file changed.
- game/src/scene.hpp: added `game::FireSite` (base, sheltered, size, the same fields and defaults as before) and `game::BuiltScene` { `pxe::Scene scene`; `std::vector<FireSite> fires`; `glm::vec3 focus{0}`; `float yawDegrees = 45` }. `buildScene` now returns `BuiltScene`. `laidOut` keeps its signature.
- game/src/scene.cpp: `buildHills`, `buildRuins`, `setUpRuins` and `layOutRuins` now work on a `BuiltScene&`, and each binds `pxe::Scene& scene = built.scene` for the engine fields. Every `scene.fires`, `scene.focus` and `scene.yawDegrees` is now `built.*`. Statements, their order and RNG use are unchanged. `laidOut` builds a local `BuiltScene` and takes `focus` from it for `bedsLo` and `bedsHi`.
- game/src/settings.hpp/.cpp: `WorldClock::placeFires` takes `const std::vector<FireSite>&` (namespace `game`). settings.hpp now includes "scene.hpp".
- game/src/main.cpp: the scene is held as `game::BuiltScene built`, with `pxe::Scene& scene = built.scene` for everything the engine draws and queries. The camera yaw and target come from `built.yawDegrees` and `built.focus`. The three `clock.placeFires` calls use `built.fires`. `selfTest` takes `const game::BuiltScene&` and reads its fires from it.

### Issues addressed
- None (cycle 1).

### Verification
Base: b2a7e0b, exported with `git archive` to /private/tmp/claude-501/sgd/base and built in its own build directory. The new code was built from a fresh directory (/private/tmp/claude-501/sgd/fresh) and in ./build. Each run used `pxe --capture out/<label>-<n>.png <args>`:
1. `--hour 15`: PNG byte-identical (cmp)
2. `--hour 21`: PNG byte-identical
3. `--scene hills`: PNG byte-identical
4. `--hour 21 --candles --shelter-test --extra-fires 6`: PNG byte-identical. The 12 "pxe: fire at ..." and "pxe: candle at ..." lines are identical (cmp). The extra fires don't get their own lines in the base either.

All four stderr logs are identical apart from the "wrote <path>" line. `--self-test` passes on both builds with identical output. The only build warning is the existing third-party one (stb_image_write `sprintf`), which the base build also gives.

### Layering
- Engine: `pxe::Scene` lost `fires`, `focus`, `yawDegrees` and `FireSite`. Nothing else changed.
- Game: game/src/scene.hpp/.cpp own `game::FireSite` and `game::BuiltScene`. main.cpp holds the `BuiltScene` for the run and reads the camera start and the starting fires from it.
- Misplaced code touched: game/src/scene.cpp, the generic scene-building parts that architecture.md marks as misplaced (`buildHills`, `buildRuins`, `layOutRuins`, `setUpRuins`). Only their signatures and the fields they write changed. The architecture.md row "Game-only data on `pxe::Scene`" can now be marked in place.

### How to playtest
- Run the game as usual (ruins). The camera should start where it did (yaw 180, centred near the campfire) and the campfire should burn.
- `--scene hills`: the camera starts at yaw 45, centred at the origin.
- `--candles --shelter-test --extra-fires 6`: all fires and candles appear. With rain, the sheltered ones stay lit as before.
- Settings menu: "Relight fires" and painting fire or water still work on the starting fires.

### Known limitations
- None.
