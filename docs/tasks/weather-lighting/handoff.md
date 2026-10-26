## Cycle 1
Build: pass

### Changes
- engine/include/pxe/lighting.hpp: new `pxe::Weather` (cloudCover, cloudOffset, cloudStrength, windDir, windStrength, rain, flash, flashDir; defaults match `pxe::Lighting`'s, flashDir `{0,1,0}`) and the overload `Lighting lightingAt(const Scene&, float hour, const Weather&)`.
- engine/src/lighting.cpp: the overload, lightingFor's body moved over with its comments in generic terms ("round the campfire" became "round each fire"). Float expressions and their order are unchanged. The midday lighting (`lightingAt(scene, 12.f)`) is computed once as `noon` and used by the firefly band, the fire band and the flash wash (it was computed three times before). Added `<glm/common.hpp>` for `glm::mix`.
- game/src/settings.hpp/.cpp: `WorldClock::weather()` fills `pxe::Weather` from cloudCover, cloudOffset, cloudStrength, windDir(), cloudSpeed (windStrength), rain, flash and flashDir, exactly as lightingFor read them. settings.hpp now includes `pxe/lighting.hpp`.
- game/src/main.cpp: lightingFor deleted; its four call sites (formerly lines 1071, 1124, 1205, 1488) now call `pxe::lightingAt(scene, clock.hour, clock.weather())`. No other edits in main.cpp.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::Weather`, `pxe::lightingAt(const Scene&, float, const Weather&)`. The existing `lightingAt(scene, hour)` is untouched.
- Game: `WorldClock` still decides the weather and hands it over through `WorldClock::weather()`; main.cpp passes it to the engine each frame.
- Misplaced code touched: `lightingFor` in game/src/main.cpp, now moved to the engine as the task asks. docs/architecture.md line 24 still marks it misplaced; that row needs updating to "in place" by the orchestrator.

### Bit-identical check
Base 75f49c9 extracted with `git archive 75f49c9 | tar -x -C /private/tmp/claude-501/wl-base` and built Debug in its own build dir; the worktree built Debug from a fresh `build/`. Each binary run from its own source root:
```
pxe --capture <out>.png --hour 15
pxe --capture <out>.png --hour 7 --rain 0.2
pxe --capture <out>.png --hour 19 --rain 0.6
pxe --capture <out>.png --hour 22
pxe --capture <out>.png --hour 15 --rain 0.6 --flash 120,40
cmp base<i>.png new<i>.png
```
All five pairs: `cmp` silent, exit 0 (identical). SHA-1s: ef1b55f1..., b266118b..., 6bf5b5d2..., 53ee187e..., 283fda9d... (base and new match for each; the five differ from each other).

### How to playtest
- Nothing should look different. Cycle through dawn, dusk and night, drizzle (god rays stronger), rain (rays and cloud shadows gone, fires out), and a downpour with lightning (cold flash wash, hard flash shadows). Change wind direction and strength in the menu; grass sway and cloud drift should follow as before.

### Known limitations
- None. Build warnings (stb `sprintf` deprecation, duplicate glfw library at link) are pre-existing and also appear in the base build.
