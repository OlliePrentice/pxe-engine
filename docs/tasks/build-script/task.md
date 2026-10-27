# One-command build and run
Type: feature
Status: open
Cycle: 1

## Goal
Manual validation needs one command that configures, builds the engine, game and example, and optionally runs one of them with flags. Today it takes three commands and remembered paths. Add CMake presets (the standard, IDE- and Windows-friendly way to name build setups) and a thin shell script over them for macOS and Linux.

## Decisions
- `CMakePresets.json` at the repo root, with configure and build presets:
  - `debug`: Debug, binary dir `build/debug`.
  - `release`: Release, binary dir `build/release`.
  - `check`: Debug, binary dir `build-check` (the review's fresh build; already git-ignored).
  Binary dirs are relative to `${sourceDir}`, so presets work inside any git worktree. Use the lowest presets schema version that the project's `cmake_minimum_required` (3.20) supports; don't raise the minimum. Build presets build in parallel. No generator is forced.
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
- [ ] `cmake --preset debug|release|check` configure and `cmake --build --preset debug|release|check` build, with the binary dirs above; checked with the installed CMake.
- [ ] `cmake --preset check --fresh && cmake --build --preset check` gives a clean build (only the existing stb warning).
- [ ] `tools/build.sh` builds release from a clean tree; `tools/build.sh debug` builds debug; both print binary paths.
- [ ] `tools/build.sh release game -- --capture <abs or relative path> --scene hills` builds then runs the game, writing the capture relative to the caller's directory, and exits with the game's exit code; same for `example -- --capture <path>`.
- [ ] A build error makes the script exit non-zero without running anything (test with a scratch-only deliberate error, not committed).
- [ ] The script runs from a directory outside the repo, and from inside a git worktree of the repo.
- [ ] `-h` prints usage; an unknown argument prints usage and exits non-zero.
- [ ] README Build section updated as decided.
- [ ] The game's capture from the preset release build is byte-identical to one from a plain `cmake -S . -B <dir> -DCMAKE_BUILD_TYPE=Release` build (presets must not change compile settings).
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None

## Out of scope
- A Windows script (presets cover Windows and IDEs).
- Install, packaging, CI.
- CLAUDE.md and the /feature skill (orchestrator updates them after the review).

## Cycle log
- Cycle 1: 2026-10-27 started, base ce30489
