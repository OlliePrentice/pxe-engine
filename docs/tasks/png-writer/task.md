# PNG writing in the engine
Type: feature
Status: complete
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
- [x] `grep -rn "stb\|writePng(" game/src` finds only calls to `pxe::writePng`. (5a9ac17: only the four pxe::writePng calls)
- [x] `pxe::writePng` behaves as the old function (size check, parent directories, row flip). (engine/src/image.cpp: same body as the old main.cpp writePng)
- [x] A `--capture` PNG is byte-identical before and after, checked with `cmp` against the cycle base build. Record in the handoff. (developer: cmp equal vs 43e8f52; orchestrator: --hour 22 into a new subdirectory, cmp equal vs an older base build)
- [x] Project builds cleanly from a fresh build directory with CMake. (fresh build-check exit 0; stb sprintf warning moved from main.cpp to image.cpp, not new)

## Assets
None

## Out of scope
- `differingPixels`, `shiftedDiff` and the self-test (game tooling).
- WAV writing for `--audio-wav`.

## Cycle log
- Cycle 1: 2026-10-26 started, base 43e8f52
- Cycle 1: review passed, no issues; complete
