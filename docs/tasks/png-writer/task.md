# PNG writing in the engine
Type: feature
Status: open
Cycle: 1

## Goal
`writePng` (game/src/main.cpp:388-399) and stb_image_write (included and implemented at main.cpp:17-26, include path in game/CMakeLists.txt) live in the game. docs/architecture.md puts writing images to PNG in the engine; the capture, bench and self-test modes stay in the game. Move PNG writing into the engine.

## Decisions
- `bool pxe::writePng(const std::string& path, int width, int height, const std::vector<std::uint8_t>& bottomUpRgba)`, same behaviour as today: rejects bad sizes, creates parent directories, flips rows, writes RGBA.
- stb_image_write's implementation and its include path become PRIVATE to `pxe_engine`, with the same warning suppression.

## Layering
- Engine: engine/include/pxe/image.hpp, engine/src/image.cpp; engine/CMakeLists.txt.
- Game: main.cpp calls `pxe::writePng` at every former `writePng` call site and no longer includes stb; game/CMakeLists.txt drops the stb include path.

## Acceptance criteria
- [ ] `grep -rn "stb\|writePng(" game/src` finds only calls to `pxe::writePng`.
- [ ] `pxe::writePng` behaves as the old function (size check, parent directories, row flip).
- [ ] A `--capture` PNG is byte-identical before and after, checked with `cmp` against the cycle base build. Record in the handoff.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- `differingPixels`, `shiftedDiff` and the self-test (game tooling).
- WAV writing for `--audio-wav`.

## Cycle log
