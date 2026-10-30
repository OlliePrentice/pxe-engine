# The desert's sky clouds and dust, and their menu controls
Type: feature
Status: complete
Cycle: 1

## Goal
Turn on the perspective mode's sky clouds (`perspective-sky-clouds`) and dust (`perspective-dust`) in the desert with its starting values. Make the desert's clock drift the sky clouds on the wind, and add a "Sky and dust" section to the settings menu, included in "Print values", so the user can tune the look live, golden hour above all.

## Decisions
- **Starting values** (user-agreed, to be tuned in the menu afterwards):
  - High layer (`skyClouds[0]`): height 8,000 m, cover 0.40, opacity 0.55, length 6,000 m, width 900 m.
  - Low layer (`skyClouds[1]`): height 2,500 m, cover 0.25, opacity 0.70, length 2,500 m, width 900 m.
  - Cloud brightness 1.0.
  - Dust density 0.4 (about 2,400 motes), size 1.

  Only values that differ from the engine defaults need setting: both covers, layer 0's opacity, and the dust density.
- **Drift:** `desert::Clock` advances `Weather::skyCloudOffset` along the wind at 15 m/s × wind strength while the clock runs, and holds it while paused, as it does the ground clouds' offset. Captures use `Weather{}`, so their clouds sit still.
- **The default hour stays 16.5.** Golden hour, the hero look, is checked with `--hour 18` captures.
- **Menu "Sky and dust" section**:
  - per layer ("High clouds", "Low clouds"): cover, opacity, altitude, streak length;
  - then cloud brightness, dust density and dust size.

  "Print values" adds them all.
- **Captures change:** the desert's default and night captures gain clouds and dust. That is expected.

## Layering
- Engine: none. It uses `Scene::skyClouds`, `skyCloudBrightness`, `dustDensity`, `dustSize` and `Weather::skyCloudOffset`.
- Game (`desert`):
  - `world.cpp` sets the values.
  - `settings.hpp`/`settings.cpp` gain the clock's drift, the menu section and the print lines.

## Approach
1. **`desert/src/world.cpp`, `buildWorld`.** After the ground bounce block (from `desert-lighting`), set the values in Decisions on the scene. Comment: "the desert's sky: two streaky cloud layers, high and low, and a little airborne dust".
2. **`desert/src/settings.hpp`.** `Clock` gains `glm::vec2 skyCloudOffset{0.f};  // metres the sky clouds have drifted`. Update the comment on `advance`: the hour, the ground clouds' drift and the sky clouds' drift, all held while paused.
3. **`desert/src/settings.cpp`.**
   - `kSkyCloudDrift = 15.f;  // metres a second the sky clouds drift at wind strength 1`.
   - In `Clock::advance`, inside the not-paused branch: `skyCloudOffset += windDir() * kSkyCloudDrift * dt * windStrength;`. That is along the wind: the shader adds the offset to the sample point, which moves the pattern against it, so the sign doesn't matter visually. Keep the sign as written.
   - `Clock::weather()` sets `w.skyCloudOffset = skyCloudOffset`.
4. **The menu.** `SettingsMenu::draw` already takes `pxe::Scene& scene` (from `desert-lighting`). Add the section **"Sky and dust"** after "Lighting" and before the fps line:
   - For `i` in 0, 1 with the labels "High clouds" and "Low clouds" (`ImGui::Text`), with `##i` suffixes so the IDs are unique:
     - "Cover" (0–1, `%.2f`);
     - "Opacity" (0–1, `%.2f`);
     - "Altitude" (500–15000, `%.0f m`, logarithmic);
     - "Streak length" (500–12000, `%.0f m`, logarithmic).
   - "Cloud brightness" (0–2, `%.2f`).
   - "Dust density" (0–1, `%.2f`).
   - "Dust size" (1–2, `%.1f px`).
   - The disabled text "Clouds cast no shadows; ground cloud shadows are Clouds and wind's Cover".
5. **"Print values".** After the lines `desert-lighting` prints, append (binding format, `%.1f` for heights and lengths, `%.2f` for the rest):
   ```
   scene.skyClouds[0].height = 8000.0f;
   scene.skyClouds[0].cover = 0.40f;
   scene.skyClouds[0].opacity = 0.55f;
   scene.skyClouds[0].length = 6000.0f;
   scene.skyClouds[1].height = 2500.0f;
   scene.skyClouds[1].cover = 0.25f;
   scene.skyClouds[1].opacity = 0.70f;
   scene.skyClouds[1].length = 2500.0f;
   scene.skyCloudBrightness = 1.00f;
   scene.dustDensity = 0.40f;
   scene.dustSize = 1.00f;
   ```

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `git diff <cycle base> -- engine game examples`: no output. `tools/ortho-check.sh <cycle base>`: 6/6 identical.
3. `tools/perspective-check.sh <cycle base>`:
   - `example-first-person` is identical;
   - `desert-default` and `desert-night` differ, because the clouds and dust are on.
4. Captures into `docs/tasks/desert-sky/`, each rendered twice and `cmp`-identical:
   - `pxe_desert --capture sky-1630.png`
   - `--hour 18 --capture sky-golden.png` (the hero look)
   - `--hour 7 --capture sky-morning.png`
   - `--hour 12 --capture sky-noon.png`
   - `--hour 21 --capture sky-night.png`
   - `--hour 0 --capture sky-midnight.png`

   Describe each in the handoff: streaky clouds in two layers that thin toward the horizon, warm edges toward the sun at golden hour, dark clouds at night, dust motes in the air (warm glints toward the sun at golden hour), the sun disc and glow where the sun is in view, and haze softening the far dunes.
5. By reading the code:
   - `Clock::advance` moves `skyCloudOffset` only while not paused.
   - `weather()` passes it.
   - The menu section has the controls and ranges in §4.
   - "Print values" appends the binding lines.
6. Add to the handoff, for the user's playtest:
   - unpause the clock with some wind strength and watch the two layers drift at different speeds;
   - drag each Sky and dust control and see it take effect at once;
   - "Print values" includes the new lines.

## Acceptance criteria
- [x] `buildWorld` sets the Decisions' values that differ from the engine defaults, and nothing else of the sky or dust. (`desert/src/world.cpp` buildWorld: covers, layer 0 opacity, dust density; other values are the defaults in `engine/src/scene.cpp`)
- [x] `Clock` advances `skyCloudOffset` as specified and passes it through `weather()`. (`desert/src/settings.cpp` Clock::advance, inside the not-paused branch; Clock::weather)
- [x] The "Sky and dust" section and the print lines are as specified. (`settings.cpp` SettingsMenu::draw and printLighting)
- [x] Verification 2–4 pass, with the captures kept and described. (orchestrator: ortho-check 6/6; perspective-check example-first-person identical, desert-default/night differ; six captures via captures.sh --twice, described in handoff.md)
- [x] Project builds cleanly from a fresh build directory with CMake (orchestrator check build: only the stb sprintf warning)

## Assets
None.

## Out of scope
- Engine changes; changing the desert's default hour; stars or a moon disc; per-phase cloud colours.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30, base c2c6add
- Cycle 1 review: build clean, verification passes, no issues. Complete.

## Playtest checklist
- Tab, untick Paused, raise Wind strength above 1: the high and low cloud layers drift along the wind at different speeds; pause and they stop.
- Drag each Sky and dust control (cover, opacity, altitude, streak length per layer; cloud brightness; dust density; dust size): each takes effect at once. Cover 0 clears a layer; dust density 1 and size 2 px make the motes easy to see while walking.
- `--hour 18` (or the Golden preset), turn toward the sun: sun disc and glow, warm cloud edges, warm dust glints (no capture shows these, as the spawn view never faces the sun).
- Print values: the terminal ends with the eleven sky and dust lines, matching the starting values.
