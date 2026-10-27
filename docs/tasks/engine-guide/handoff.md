## Cycle 1
Build: pass

### Changes
- docs/engine-guide.md (new): the guide, with sections in the order the first acceptance criterion gives. What the engine is and isn't (pointing to docs/architecture.md). Adding it to CMake: add_subdirectory and FetchContent `SOURCE_SUBDIR engine` forms, the fetched and vendored dependencies with versions, C++20, and known build limits (GLFW cache options forced, the `imgui` target name, `PXE_SHADERS`). Start-up order and ownership (Window, Renderer, scene, Overlay, AudioDevice, Camera). The frame loop. Building a Scene: palettes, sun path, geometry by hand, from .glb, scattering, HeightMap/ShelterMap/TriangleGrid. Fires, FireGrid, fireflies and point lights. Skinned models and actor materials. Audio. Capture and writePng. Contracts. A header-by-header reference of all 22 headers in engine/include/pxe.
- examples/minimal/main.cpp (new): a procedural scene of 80x80 m of rolling grass ground, a stone pillar, a bark trunk with a sphere leaf crown, and a stone slab with a campfire on it. It scatters grass and leaves from one seeded generator, drops the grass under stone and bark, and lights the scene at 16:30 with `lightingAt(scene, hour, Weather{})`. It runs a windowed loop until the window is closed or Escape is pressed, with Q/E to turn the view. `--capture <file.png>` renders 4 frames at fixed times into a hidden window, writes `readView()` with `writePng` and exits. Any other argument prints usage and exits 1. Includes only `pxe/` headers, glm and std.
- examples/minimal/CMakeLists.txt (new): the `pxe_minimal` executable, with the same warning flags as the game, linked to `pxe_engine`.
- CMakeLists.txt: `add_subdirectory(examples/minimal)`, so the example builds by default.
- README.md: one paragraph at the end of the Build section, pointing to the guide and the example.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: none.
- Game: none. The example is its own executable. Its `World` struct owns its `pxe::Scene` and the fire's base. It creates the scene after the window and destroys it before the renderer and window, through declaration order in `main`.
- Misplaced code touched: none.

### Verification
- Fresh builds: two separate fresh build directories in the scratchpad (`fresh-build`, `fresh2`), Debug, configure and build pass. The only warning is stb_image_write's `sprintf` deprecation, which the base also has. The example compiles with `-Wall -Wextra -Wpedantic` and no warnings.
- Example capture: `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/d14f2cb1-88f9-453a-96ce-d0bdb809fbd5/scratchpad/guide/minimal.png` (960x540). It shows the rolling lawn in toon bands with grass tufts, the stone pillar, the tree with a leafy two-tone crown and its long shadow to the right, and the campfire burning on its slab with a warm glow and sparks. Afternoon god rays wash across the view. No black ground edge shows. Rebuilding from a second fresh directory gives a byte-identical capture (`minimal2.png`).
- Windowed loop: ran for 6 s and was then killed by an alarm. No errors. It was not interacted with (Escape, Q/E untested by hand).
- Game captures: base 34d0256 was taken with `git archive`, built in the scratchpad, and both binaries were run from scratchpad working directories. The PNGs are byte-identical (cmp) for: default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`. The logs are identical apart from the "wrote <path>" line.
- Guide names: a script checked every backticked identifier in the guide against engine/include/pxe and the example. All match, except CMake, file and package names, which are not C++. Every line in the guide's snippets taken from the example appears verbatim in examples/minimal/main.cpp. The illustrative snippets (overlay, glTF loading, skinned model, actor materials, AudioDevice) were compiled with `-fsyntax-only` against the engine headers, glm and imgui, and pass.

### Engine gaps and awkwardness found (not fixed, per the task)
1. **Scene palettes default to black.** `Scene::palette`, `rim` and `ink` are zero-initialised, so a new game that sets nothing renders black. Every game must write all four phases for every surface it uses: up to 4 x 7 x 3 ramps of 3 colours. The example works around it with day colours and a per-phase tint. A default palette, or a helper that fills phases from a day palette, would remove the biggest start-up hurdle.
2. **GrassScatter defaults don't match the engine's own pixel density.** `GrassScatter::clumpSpacing` (0.34) and `tuftSpacing` (2) are tuned at 16 px/m, but `RenderConfig::kPixelsPerUnit` is 24. Every caller has to divide by `kPixelsPerUnit / 16`, as the game and the example both do. `scatterLeaves` has no default density at all: the game and the example both use 6 x (24/16)^2.
3. **The two-argument `lightingAt(scene, hour)` is a trap.** It leaves `fireLight`, `fireflyLight`, wind and rain at zero, so fires draw flames that light nothing. The guide steers readers to the three-argument form.
4. **`FrameParams::framebufferW/H` default to 1920x1080** rather than coming from the window. Forgetting to set them mis-scales the present on any other window size or on high DPI.
5. **Order dependencies that aren't enforced.** `dropGrassUnder` reads the scene bounds that `uploadScene` sets, and `Renderer::render` silently assumes the scene passed is the one last uploaded. Both are documented in the guide's contracts. Neither is checked.
6. **`Fire` and `FireGrid` are public but run by the renderer.** Their headers expose `init`, `advance`, `drawFlames`, `advanceTo` and so on, which a game has no use for: the renderer owns one `FireGrid` and one `Fire` per `FireDraw`. The guide says to drive them through `FrameParams`. The public surface invites misuse.
7. **The engine logs to stderr unconditionally.** `Renderer::init` and `scatterGrass` print, and a game can't silence them.
8. **Build limits (carried over from engine-standalone-build).** GLFW cache options are FORCE-set, the engine defines a target named `imgui` that can clash with a consumer's, and new shaders must be listed in `PXE_SHADERS`. A FetchContent consumer clones the whole repository (about 7 MB of assets plus history) even with `SOURCE_SUBDIR engine`. There is no install or export.
9. **No camera zoom or pitch control.** Pitch, pixel density and eye distance are fixed constants in camera.cpp and config.hpp. That suits this style, but a game can't frame a smaller scene more tightly. The example's objects are small in the 40 m wide view.

### How to playtest
- `./build/examples/minimal/pxe_minimal`: a window opens on the lawn, stone, tree and campfire in late-afternoon light. The grass sways and the fire flickers. Q and E turn the view by 45° steps with an ease. Escape or closing the window quits.
- `./build/examples/minimal/pxe_minimal --capture out.png`: no window shows. It writes a 960x540 PNG and exits 0, and the result is identical on every run.
- `./build/examples/minimal/pxe_minimal --foo` prints usage and exits 1.
- Read docs/engine-guide.md as a newcomer would: every name in it exists in engine/include/pxe.
- The game is unchanged. Its captures match the base byte for byte.

### Known limitations
- The windowed example was run headlessly only (6 s, no input). Its keys weren't pressed by hand.
- The FetchContent snippet in the guide uses this repository's origin URL (github.com/OlliePrentice/pxe-engine) with a `<commit or tag>` placeholder. It wasn't tried as a real fetch, because that needs network access to the repository. The add_subdirectory form was verified by engine-standalone-build's consumer test.
- The example's phase tints are a shortcut, so golden hour, twilight and night are only rough. Only 16:30 is shown.
- Not built on Windows or Linux.
