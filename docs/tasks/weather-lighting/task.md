# Weather to lighting in the engine
Type: feature
Status: open
Cycle: 1

## Goal
`lightingFor` in game/src/main.cpp:527-611 turns the time of day and the weather into `pxe::Lighting`: overcast and cloud-shadow fade from rain, god-ray strength and colour, firefly and firelight bands, and the lightning wash. It reads only the `pxe::Scene` and the weather values, so it is engine logic (docs/architecture.md marks it misplaced). Move it into the engine as `pxe::Weather` and an overload `pxe::lightingAt(scene, hour, weather)`. The game keeps deciding the weather (`WorldClock`) and hands it over as data. Rendered output must not change by a single bit.

## Decisions
- `pxe::Weather` holds what lightingFor reads from the clock: cloudCover, cloudOffset (vec2), cloudStrength, windDir (vec2, world xz the wind blows toward), windStrength, rain, flash, flashDir. Defaults: as `pxe::Lighting`'s own defaults where it has them, otherwise no weather (0, or `{0,1,0}` for flashDir).
- The colour targets and curves (ray colours, firefly tint, firelight tint, the cold wash, the ramps and thresholds) become engine defaults inside `lightingAt`, unchanged. No new parameters: a value becomes a parameter when a game needs to set it.
- The existing `pxe::lightingAt(scene, hour)` stays as is; the new overload builds on it.
- Bit-identical output: keep the float expression order of today's code. The midday lighting (`lightingAt(scene, 12.f)`, computed three times today) may be computed once and reused, since it is the same value.

## Layering
- Engine: `pxe::Weather` (engine/include/pxe/lighting.hpp) and `Lighting lightingAt(const Scene&, float hour, const Weather&)` (engine/src/lighting.cpp), with the comments from lightingFor carried over in generic terms.
- Game: `WorldClock` gains `pxe::Weather weather() const` (game/src/settings.hpp/.cpp) filling it from its fields (windDir() for windDir, cloudSpeed for windStrength). main.cpp's four `lightingFor(scene, clock)` calls become `pxe::lightingAt(scene, clock.hour, clock.weather())`, and lightingFor is deleted.

## Acceptance criteria
- [ ] `lightingFor` no longer exists in game/; its logic is in `pxe::lightingAt(scene, hour, weather)` in engine/src/lighting.cpp.
- [ ] `pxe::Weather` exists with the fields under Decisions and no game content in names or comments.
- [ ] `WorldClock::weather()` fills every field from the clock exactly as lightingFor read it.
- [ ] Every former lightingFor call site (main.cpp:1071, 1124, 1205, 1488) uses the new overload.
- [ ] Captures are byte-identical: build the cycle base and the new code, run `--capture` on each with `--hour 15`, `--hour 7 --rain 0.2`, `--hour 19 --rain 0.6`, `--hour 22`, and `--hour 15 --rain 0.6 --flash 120,40`, and `cmp` each pair. Record the commands and results in the handoff.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- `WorldClock` itself (lightning timing, cloud drift, fire burn-down): game, per docs/architecture.md.
- Any change to how anything looks.

## Cycle log
- Cycle 1: 2026-10-26 started, base 75f49c9 (worktree /Users/sampr/Projects/Games/pxe/pxe-engine-weather-lighting, branch weather-lighting)
