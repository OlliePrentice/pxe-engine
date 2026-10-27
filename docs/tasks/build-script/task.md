# One-command build and run
Type: feature
Status: complete
Cycle: 1

## Goal
Manual validation needs one command that configures, builds the engine, game and example, and optionally runs one of them with flags. Today it takes three commands and remembered paths. Add CMake presets (the standard, IDE- and Windows-friendly way to name build setups) and a thin shell script over them for macOS and Linux.

## Decisions
- `CMakePresets.json` at the repo root, with configure and build presets:
  - `debug`: Debug, binary dir `build/debug`.
  - `release`: Release, binary dir `build/release`.
  - `check`: Debug, binary dir `build-check` (the review's fresh build; already git-ignored).
  Binary dirs are relative to `${sourceDir}`, so presets work inside any git worktree. Presets schema 3 (CMake 3.21+): schema 2, the only one 3.20 reads, requires a forced generator, which would break Visual Studio; decided in review, cycle 1. `cmake_minimum_required` stays 3.20 for the plain build. Build presets build in parallel. No generator is forced.
- `tools/build.sh` (bash, executable, `set -euo pipefail`):
  - `tools/build.sh [debug|release] [game|example] [-- args...]`. Default config `release`, default action build only.
  - Configures with the preset if the build dir isn't configured yet (otherwise just builds; CMake reconfigures itself when CMakeLists change), then builds the preset. Stops with the build's exit code on failure.
  - Works from any working directory (resolves the repo root from the script's own path) and doesn't change the caller's working directory for the run, so relative output paths like `--capture shot.png` land where the user is.
  - With `game` or `example`, runs that binary after a successful build with the args after `--`, and exits with its exit code.
  - Prints the paths of the built binaries. `-h`/`--help` prints usage. Unknown arguments print usage and exit non-zero.
- The CMake build is unchanged: CMake already builds the engine before the game and example through `target_link_libraries`.
- README's Build section uses the presets and the script (keeping the plain `cmake -S . -B ...` form as an alternative, and the Linux packages and xvfb note). Old `build/` paths in README are updated.

## Layering
- Engine: none.
- Game: none. Build tooling and README only.

## Acceptance criteria
- [x] `cmake --preset debug|release|check` configure and `cmake --build --preset debug|release|check` build, with the binary dirs above; checked with the installed CMake. (all three configure and build on CMake 3.30; developer evidence in handoff.md)
- [x] `cmake --preset check --fresh && cmake --build --preset check` gives a clean build (only the existing stb warning). (my run: cmake --preset check --fresh + --build --preset check --clean-first: 41 files compiled, only the stb warning)
- [x] `tools/build.sh` builds release from a clean tree; `tools/build.sh debug` builds debug; both print binary paths. (developer: clean build/release and build/debug; script prints game and example paths)
- [x] `tools/build.sh release game -- --capture <abs or relative path> --scene hills` builds then runs the game, writing the capture relative to the caller's directory, and exits with the game's exit code; same for `example -- --capture <path>`. (my run from the scratchpad: release example -- --capture ex.png wrote ex.png in my directory, exit 0; developer covered game, exit codes)
- [x] A build error makes the script exit non-zero without running anything (test with a scratch-only deliberate error, not committed). (developer: scratch static_assert, exit 2, nothing run, tree restored)
- [x] The script runs from a directory outside the repo, and from inside a git worktree of the repo. (developer: from outside the repo and inside a temporary worktree; worktree and branch removed)
- [x] `-h` prints usage; an unknown argument prints usage and exits non-zero. (tools/build.sh usage(); my run: unknown argument exits 2)
- [x] README Build section updated as decided. (README Build section: presets, script, plain form, Linux packages, xvfb)
- [x] The game's capture from the preset release build is byte-identical to one from a plain `cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release` build (presets must not change compile settings). (developer: 4 game captures and the example capture cmp-identical between preset and plain Release builds; CXX_FLAGS identical)
- [x] Project builds cleanly from a fresh build directory with CMake (my clean check build, exit 0)

## Assets
None

## Out of scope
- A Windows script (presets cover Windows and IDEs).
- Install, packaging, CI.
- CLAUDE.md and the /feature skill (orchestrator updates them after the review).

## Cycle log
- Cycle 1: 2026-10-27 started, base ce30489
- Cycle 1: review passed, no issues; complete. Accepted deviation: presets schema 3. CLAUDE.md and the /feature skill now use the presets, with --clean-first for the check build.
