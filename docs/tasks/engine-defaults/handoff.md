## Cycle 1
Build: pass

### Changes
- engine/include/pxe/scene.hpp: `Scene` gets a default constructor that sets the reference palettes; comments on `palette`, `rim` and `ink` say they default to the reference style sampled from t3ssel8r's videos.
- engine/src/scene.cpp (new, added to `engine/CMakeLists.txt`): `Scene::Scene()` sets all four phases x seven surfaces x band/detail/tip and `rim`/`ink`. The values and the `hex` conversion (`vec3{r, g, b} / 255.f`) are copied unchanged from `game::setPhasePalettes`, so the floats are bit-identical. The comment credits t3ssel8r and names no game content.
- engine/include/pxe/scatter.hpp: `GrassScatter::clumpSpacing = 0.34f / (RenderConfig::kPixelsPerUnit / 16.f)` and `tuftSpacing = 2.f / (RenderConfig::kPixelsPerUnit / 16.f)`, the same float operations the game used. New `inline constexpr float kDefaultLeafDensity = 6.f * ((kPixelsPerUnit / 16.f) * (kPixelsPerUnit / 16.f))`, which is 13.5. I used a named constant rather than reordering `scatterLeaves`'s parameters, because it is the smaller change: no existing call changes. The header now includes `pxe/config.hpp`.
- engine/include/pxe/renderer.hpp, engine/src/renderer.cpp: `FrameParams::framebufferW/H` default to 0, with a comment saying they are required when presenting. `Renderer::render` throws `std::invalid_argument` before drawing anything when `present` is true and either size is not positive. The message gives the size it was handed, e.g. "Renderer::render: present is set but FrameParams::framebufferW/H is 0x0; set it to Window::framebufferSize(), or clear present". The `render` comment documents the throw.
- game/src/scene.cpp: deleted `setPhasePalettes` and its two calls. The hills still set their day-grass band/detail/tip and `tLit`/`tMid`, and the ruins set `tLit`/`tMid`. `hex` and `setRamp` stay because the hills override uses them.
- game/src/scene.hpp: `BuildOptions::minDist`, `tuftDist` and `leafDensity` default to `pxe::GrassScatter{}.clumpSpacing`, `pxe::GrassScatter{}.tuftSpacing` and `pxe::kDefaultLeafDensity`. It includes `pxe/scatter.hpp`.
- game/src/main.cpp: removed the density rescale. `Options::minDist` defaults to `pxe::GrassScatter{}.clumpSpacing` and goes to `BuildOptions::minDist` as given, so `--min-dist` is now the spacing actually used, in metres. In `--help`, `--capture` now reads "the 960x540 low-res view" (from `RenderConfig::kViewW/kViewH`, was "640x360 low-res framebuffer"), and `--min-dist` gives its default from the engine ("default 0.2267, the engine's", was "0.34"). I found no other stale size in `--help`.
- examples/minimal/main.cpp: dropped `hex`, `setPalettes` (the tinted palettes) and the density rescale, plus the now-unused `pxe/config.hpp` include. It scatters with `pxe::GrassScatter{}` and `pxe::kDefaultLeafDensity`. It still sets the framebuffer size from `window.framebufferSize()` in `frameParams`.
- docs/engine-guide.md:
  - Render step: the framebuffer size defaults to 0, is required when presenting, and `render` throws `std::invalid_argument` otherwise. It may stay 0 when not presenting.
  - Building a Scene: replaced "Palettes start black" and the tint snippet with the default palettes and how to override one ramp or phase (`scene.palette[pxe::kPhaseDay][pxe::kSurfGrass].band[2]`, `scene.ink[pxe::kPhaseNight]`).
  - Scattering: the snippet matches the example word for word, and the rescale advice is gone. Scatter defaults are at the engine's density, with `kDefaultLeafDensity` for leaves.
  - Header reference: lists `kDefaultLeafDensity` and notes that `Scene` is constructed with the default palettes.
  - Every cpp snippet that came from the example still matches it after whitespace stripping (checked by script). The others were already illustrative and are unchanged.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::Scene::Scene()` (default palettes), `GrassScatter` default spacings, `pxe::kDefaultLeafDensity`, `FrameParams::framebufferW/H = 0`, and the `std::invalid_argument` check in `Renderer::render`.
- Game: `game::buildHills` and `setUpRuins` set only their overrides (hills day grass, `tLit`/`tMid`). `game::BuildOptions` takes its scatter defaults from the engine. main.cpp passes `--min-dist` through unscaled.
- Misplaced code touched: none.

### Framebuffer sizes on presenting paths
Every `render` call with `present` true sets `framebufferW/H` to `window->framebufferSize()`:
- `--bench`
- headless capture's `--settle` loop
- headless capture's final frame
- the interactive loop

`makeParams` first sets `opt.width/height` (default 1920x1080), but every caller overwrites them with the window's framebuffer size before rendering. `selfTest` sets 320x180 with `present = false`, so the size is unused there. No presenting path passes anything other than `Window::framebufferSize()`. The example passes `window.framebufferSize()` on both its paths.

### Verification
- Base built from `git archive b709c58` and this tree, both Release, in the scratchpad. The capture PNG and the log (apart from the "wrote" line) are byte-identical (`cmp`/`diff`) for:
  - default
  - `--scene hills`
  - `--hour 21 --foxes 3`
  - `--hour 21 --candles --shelter-test --extra-fires 6`
  - `--window-png` on the default scene (1920x1080)
- `--self-test`: PASS, with a log identical to the base.
- `--scene hills --min-dist 0.22666667` gives the same clump count as the default (99170), so `--min-dist` is the spacing actually used.
- Scratch throw test, not committed: `render` with `present` and sizes 0x0, 1920x0 and -5x1080 throws `std::invalid_argument`. 0x0 without `present`, and the window size with `present`, do not throw. A default `Scene`'s day grass lit band equals `0xacbc6c / 255.f`.
- Fresh Debug build (`build-check --fresh`, clean first): passes. The only warning is stb_image_write's `sprintf`.
- Example capture: the rolling green lawn with tufts, the standing stone, the tree with its leafy crown and long shadow to the right, the lit campfire on its slab, and god rays. It looks almost the same as before. The scatter is identical (78277 clumps, 2297 tufts, as at base). The colours differ slightly because the 16:30 blend now uses the hand-chosen golden-hour palette instead of a tinted day palette: the grass is a touch greener and more saturated.

### How to playtest
- Play the game normally (ruins, then `--scene hills`) and scrub the hour through day, golden hour, twilight and night in the settings menu. Colours should look exactly as before.
- Resize the window and move it between displays, including a Retina one. The view should stay centred and integer-scaled as before.
- Minimise and restore the window (see the first limitation).
- Run `pxe_minimal` and turn with Q/E. The scene should be lit and coloured, never black.

### Known limitations
- **Minimised windows.** The old `drawPresent` clamped a 0 size to 1. Now a 0x0 framebuffer while presenting throws. On platforms where GLFW reports a 0x0 framebuffer for an iconified window (Windows, at least), the game's interactive loop and the example would throw when minimised and exit with an error. I did not verify this on Windows. macOS keeps the size when minimised. I did not change the game or the example to skip rendering at 0x0, because the task says to leave the sizes and paths as they are. If wanted, a follow-up could skip `render` (or set `present = false`) while the framebuffer is 0x0, in the game and the example, and say so in the guide.
- `--min-dist` changed meaning: it used to be scaled by 1/1.5 and is now the spacing used. A script that passed, say, `--min-dist 0.34` now gets sparser grass than before. The default output is unchanged.
- The `std::max(..., 1)` in `drawPresent` is now redundant after the check in `render`. I left it as is.
