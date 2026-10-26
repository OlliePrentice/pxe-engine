## Cycle 1
Build: pass

### Changes
- engine/include/pxe/image.hpp (new): declares `bool pxe::writePng(const std::string& path, int width, int height, const std::vector<std::uint8_t>& bottomUpRgba)`.
- engine/src/image.cpp (new): the former game `writePng` body, unchanged in behaviour (rejects non-positive sizes and short buffers, creates parent directories, flips rows bottom-up to top-down, writes RGBA via `stbi_write_png`). Holds `STB_IMAGE_WRITE_IMPLEMENTATION` with the same `-Wmissing-field-initializers` push/pop suppression as before.
- engine/CMakeLists.txt: adds `src/image.cpp`; adds `third_party/stb` as a PRIVATE include directory of `pxe_engine`.
- game/src/main.cpp: removes the stb include/implementation block, the local `writePng` and the now-unused `<filesystem>` include; includes `"pxe/image.hpp"`; the four call sites (`--capture`, `--window-png`, the in-game shot key, `--ui-shot`) call `pxe::writePng`.
- game/CMakeLists.txt: drops the `third_party/stb` include directory (the `target_include_directories` block held only that).

### Issues addressed
- None (cycle 1).

### Layering
- Engine: new `pxe::writePng` in engine/include/pxe/image.hpp, engine/src/image.cpp; stb_image_write is private to `pxe_engine`.
- Game: main.cpp's capture, window-PNG, shot and UI-shot paths call `pxe::writePng`; the modes themselves stay in the game.
- Misplaced code touched: `writePng` in game/src/main.cpp (moved out by this task; architecture.md's "Writing images to PNG" row can now be marked in place, which is the orchestrator's edit).

### Verification
- `grep -rn "stb\|writePng(" game/src` finds only the four `pxe::writePng(` calls.
- Fresh build directory (`cmake -S . -B <fresh> -DCMAKE_BUILD_TYPE=Debug && cmake --build <fresh> --parallel`): passes. The only warning is stb_image_write.h:776 `sprintf` deprecated (`-Wdeprecated-declarations`), now reported from engine/src/image.cpp. It is pre-existing: the base 43e8f52 build gives the same warning from game/src/main.cpp. Not introduced by this change; the spec asks for the same suppression, so I did not widen it.
- Byte-identical capture: base 43e8f52 (git archive, own build dir) and this build each ran `pxe --capture <dir>/cap.png` into a directory that didn't exist yet (so parent-directory creation was exercised). `cmp` reports no difference; both SHA-1 ef1b55f19887d5b4ed55f068e9a31ed49ef27c63, 960x540 RGBA PNG.

### How to playtest
- `pxe --capture out/sub/cap.png`: the file is written, the directories are created, and the image is right way up.
- In play, press the screenshot key: `shot.png` is written to the working directory, right way up.
- `--ui-shot` and `--window-png` write full-window PNGs as before.

### Known limitations
- The pre-existing stb `sprintf` deprecation warning remains (see Verification).
