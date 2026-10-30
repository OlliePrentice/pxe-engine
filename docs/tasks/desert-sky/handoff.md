## Cycle 1
Build: pass (`cmake --preset check --fresh && cmake --build --preset check --clean-first`; only the stb_image_write `sprintf` warning)

### Changes
- desert/src/world.cpp: `buildWorld` sets the sky: both covers (0.40, 0.25), layer 0's opacity (0.55) and dust density (0.4). Everything else (heights, lengths, widths, layer 1 opacity 0.70, brightness 1, dust size 1) is already the engine default, as `Scene::Scene` sets it.
- desert/src/settings.hpp: `Clock::skyCloudOffset`; `advance` comment updated.
- desert/src/settings.cpp: `kSkyCloudDrift = 15`; `Clock::advance` adds `windDir() * kSkyCloudDrift * dt * windStrength` inside the not-paused branch; `weather()` passes `skyCloudOffset`; new "Sky and dust" menu section after "Lighting" and before the fps line (per layer Cover, Opacity, Altitude, Streak length with `##i` IDs; Cloud brightness, Dust density, Dust size; the disabled note); `printLighting` appends the binding sky and dust lines (loop over the two layers, same format and order as the spec), and the button's note now says "and the sky".
- docs/tasks/desert-sky/sky-*.png and scripts/captures.cases: the verification captures and the cases file for `tools/captures.sh`.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none.
- Game: `desert::buildWorld` sets the scene's sky and dust; `desert::Clock` holds the sky clouds' offset; `SettingsMenu` edits `scene` directly.
- Misplaced code touched: none.

### Verification
- `git diff c2c6add -- engine game examples`: empty. `tools/ortho-check.sh c2c6add`: 6/6 identical.
- `tools/perspective-check.sh c2c6add`: `example-first-person` identical; `desert-default` and `desert-night` differ (expected).
- Captures (`tools/captures.sh --twice`, all six deterministic, in this folder):
  - `sky-1630.png` (default hour): blue sky with two layers of streaky white clouds along +x, stretched thin and fine toward the horizon (the low layer's fine streaks) and broader, softer ones higher up; the sand and palm unchanged; a few faint dust specks over the sand. The sun is not in view (it is behind and to the left of the spawn's view), so no disc or glow.
  - `sky-golden.png` (`--hour 18`): lilac/purple-pink sky, clouds with pink-peach edges that fade to the pale horizon; the sand and palm in dark warm shade; the horizon haze softens the far dune. No sun disc in view; dust motes barely visible (single-pixel specks, no obvious warm glints in this view because the sun is out of frame).
  - `sky-morning.png` (`--hour 7`): pale blue sky shading to warm cream near the horizon, clouds warm-tinted, long palm shadow, haze on the far dunes.
  - `sky-noon.png` (`--hour 12`): clear blue sky, white clouds in two layers, short palm shadow, light haze at the horizon.
  - `sky-night.png` (`--hour 21`): deep navy sky, the clouds dark blue and only just lighter than the sky, so they read as faint streaks; moon-cast palm shadow; no stars (out of scope).
  - `sky-midnight.png` (`--hour 0`): as night; dark streaks, the palm's shadow falls under it.
  - Captures use `Weather{}`'s zero offset, so the clouds do not drift in them.
- By reading: `advance` moves `skyCloudOffset` only past the `if (paused) return;`; `weather()` passes it; the menu controls and ranges are as in task.md section 4; the print lines are the binding ones.

### How to playtest
- Open the menu with Tab, untick "Paused" and set "Wind strength" above 1: the two cloud layers drift along the wind at different speeds (the high layer's engine drift is 1.5x the low layer's). Pause: they stop.
- Drag each control under "Sky and dust" (cover, opacity, altitude, streak length for each layer, cloud brightness, dust density, dust size): each takes effect at once. Set cover to 0 for a clear sky; set dust density to 1 and size to 2 px and walk to see the motes.
- Use the "Golden" preset or `--hour 18` and turn toward the sun to see the sun glow, clouds' warm edges and dust glints.
- "Print values" in the terminal now ends with the eleven `scene.skyClouds[...]`, `skyCloudBrightness`, `dustDensity` and `dustSize` lines (at start: 8000.0, 0.40, 0.55, 6000.0 / 2500.0, 0.25, 0.70, 2500.0 / 1.00 / 0.40 / 1.00).
- The Print button sits in the Lighting section, above the Sky and dust section (the spec places the section after Lighting and the button was not moved).

### Docs
- docs/architecture.md / docs/engine-guide.md: none. README.md: the desert's capture description (sky clouds and dust now on) and the menu's sections list, if README lists them.

### Known limitations
- The captures' view looks +z and the sun is out of frame in every one, so the sun disc and the sun-facing dust glints were not seen in these captures; check them in play by turning toward the sun.
