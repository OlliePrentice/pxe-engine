## Cycle 1
Build: pass

### Changes
- CMakePresets.json (new): configure and build presets `debug` (Debug, `${sourceDir}/build/debug`), `release` (Release, `${sourceDir}/build/release`), `check` (Debug, `${sourceDir}/build-check`). Only `CMAKE_BUILD_TYPE` is set; no generator. Build presets set `"jobs": 0`, which CMake treats as "parallel at the native default" (verified with a logging make/ninja wrapper in a scratch project: Makefiles get `-j`, Ninja gets no `-j`, i.e. its own default; `--parallel N` on the command line still overrides). Build presets also set `configuration` so multi-config generators (Visual Studio, Xcode, Ninja Multi-Config) build the matching config; single-config generators ignore it.
- tools/build.sh (new, mode 100755): `tools/build.sh [debug|release] [game|example] [-- args...]`, bash, `set -euo pipefail`. Resolves the repo root from `BASH_SOURCE`, runs `cmake --preset` only if `build/<config>/CMakeCache.txt` is missing, then `cmake --build --preset`, both in subshells `cd`'d to the repo root so the caller's cwd is untouched. Prints game and example paths (also finds them in a `Debug/`/`Release/` subdir for multi-config generators). With `game`/`example`, `exec`s the binary with the args after `--`, so its exit code is the script's. `-h`/`--help` prints usage (exit 0). Unknown argument, a repeated config or target, or `--` without `game`/`example` prints the error and usage to stderr and exits 2. Empty-array expansion is guarded for macOS's bash 3.2.
- README.md: Build section rewritten around the presets and the script; the plain `cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release` form kept as the alternative; Linux packages and the xvfb note kept; `./build/game/pxe` and `./build/examples/minimal/pxe_minimal` paths updated to `build/release/...`.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: none
- Game: none (build tooling and README only)
- Misplaced code touched: none

### Verification (CMake 3.30.2, macOS arm64, default generator Unix Makefiles, /bin/bash 3.2.57)
- `cmake --preset debug|release|check`: all three configure, binary dirs `build/debug`, `build/release`, `build-check`. `cmake --build --preset debug|release|check`: all three build, exit 0.
- Fresh check: `rm -rf build-check && cmake --preset check --fresh && cmake --build --preset check`: 41 compiles, exit 0, only warning is stb_image_write's `sprintf` deprecation.
- `rm -rf build/release && tools/build.sh`: configures and builds release from clean (41 compiles, only the stb warning), prints `game: .../build/release/game/pxe` and `example: .../build/release/examples/minimal/pxe_minimal`. `tools/build.sh debug` builds debug and prints the `build/debug` paths.
- Run modes, from the scratchpad (outside the repo): `tools/build.sh release game -- --capture rel_game.png --scene hills` wrote `rel_game.png` in the caller's directory, exit 0; same with an absolute path, the two PNGs `cmp` identical; `tools/build.sh example -- --capture rel_ex.png` wrote it in the caller's directory, exit 0. No PNG appeared in the repo root.
- Exit code: `tools/build.sh game -- --bogus-flag` exits 1, as the game does directly (its usage text is printed, so it ran); `tools/build.sh example -- --capture /nonexistent/dir/x.png` exits 1, as the example does directly.
- Build failure: appended `static_assert(false, ...)` to game/src/fox.cpp (scratch only), `tools/build.sh game -- --capture fail.png` exited 2 (make's code), no `game:` line, no `fail.png`. File restored from a copy; `git status` showed only the new files afterwards.
- Worktree: `git worktree add <scratchpad>/bs-wt -b bs-wt-test`, copied the two new files in, ran `<wt>/tools/build.sh release game -- --capture wt_game.png --scene hills` from a separate scratch directory: configured and built in `<wt>/build/release`, capture written in the caller's directory and byte-identical to the main tree's. Also ran `../tools/build.sh example -- --capture in_wt.png` from `<wt>/game`: capture landed in `<wt>/game`. Worktree removed (`--force`, it had untracked build output) and `bs-wt-test` deleted.
- `-h`/`--help`: usage, exit 0. `relase`, `--capture x.png` (without `--`), `debug release`, `-- --capture x`: error + usage on stderr, exit 2.
- Byte-identical: plain `cmake -S . -B <scratchpad>/plain-release -DCMAKE_BUILD_TYPE=Release` build vs the preset release build. Game captures for default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`, and the example's capture: all `cmp` identical. `CXX_FLAGS` in `game/CMakeFiles/pxe.dir/flags.make` identical (`-O3 -DNDEBUG -std=c++20 ... -Wall -Wextra -Wpedantic`); only the build-dir include paths differ.
- shellcheck is not installed, so it was not run. `bash -n` passes.

### How to playtest
- `tools/build.sh release game` from the repo root, then from another directory with the script's full path; the game should open as before.
- `tools/build.sh debug game -- --hour 21` runs the debug build at night.
- In an IDE that reads presets (CLion, VS Code CMake Tools, Visual Studio), the debug/release/check presets should be listed.

### Known limitations
- **Deviation from the spec: presets schema version 3, not 2.** Schema 2 (the only version CMake 3.20 reads) requires a `generator` in every configure preset; CMake rejected the file with `Preset "r" missing field "generator"`. The spec's "no generator is forced" and "lowest version 3.20 supports" can't both hold, so I kept "no generator" and used version 3 (CMake 3.21), with `cmakeMinimumRequired` 3.21 in the presets file. `cmake_minimum_required(VERSION 3.20)` in CMakeLists.txt is unchanged, and the README says presets need 3.21+ while the plain form works on 3.20. If schema 2 is preferred, every configure preset needs `"generator": "Unix Makefiles"` (or similar), which forces a generator and breaks Visual Studio use.
- `cmake --preset check --fresh` only recreates the cache; it does not delete object files, so on an existing `build-check` it recompiles nothing unless flags changed. A truly fresh check build needs `rm -rf build-check` first (that is how the check above was run). This was already true of the old `cmake -S . -B build-check --fresh` command in CLAUDE.md.
- The build presets' `"jobs": 0` overrides a `CMAKE_BUILD_PARALLEL_LEVEL` environment variable; `cmake --build --preset X --parallel N` still sets an explicit count. With Makefiles, `-j` with no count is unbounded, the same as the old README's `cmake --build build -j`.
- The existing `build/` directory (old layout, configured at `build/` itself) is left as is; the presets use `build/debug` and `build/release` inside it, which works alongside it.
