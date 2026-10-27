# pxe

A C++20 engine for t3ssel8r-style 3D pixel art (`engine/`, library `pxe_engine`, namespace `pxe`) and one game built on it (`game/`, executable `pxe`, namespace `game`). `examples/minimal/` is a small procedural program that uses only the engine.

## Read first
- `docs/architecture.md`: what belongs in the engine and what in the game, and where every area lives now. Follow it for every change. Settle where code goes by its test ("Would a different game in this style need this code, unchanged?"), not by where similar code happens to be.
- `docs/engine-guide.md`: how to use the engine: CMake, start-up order, the frame loop, building a scene, contracts. `examples/minimal/main.cpp` is its worked example.
- `docs/art-style.md`: the standing art style, for any asset work.

## Hard rules
- The engine never includes game headers, calls game code or names game content (fox, hero, tavern, ruins...) in identifiers, APIs or comments.
- The game creates and owns every object in its world. The engine keeps no pointer to game data between calls.
- `engine/` stays self-contained: no path outside it in `engine/CMakeLists.txt`, no files read from the source tree at run time. A new shader goes in `engine/shaders/` and in `PXE_SHADERS` in `engine/CMakeLists.txt`.
- Never commit to `main`. Work on a branch.

## Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game/pxe                      # the game
./build/examples/minimal/pxe_minimal  # the example
```
For a clean check, build from a fresh directory (git-ignored):
```bash
cmake -S . -B build-check -DCMAKE_BUILD_TYPE=Debug --fresh
cmake --build build-check --parallel
```
The only expected warning is stb_image_write's `sprintf` deprecation.

## Verifying a change
Nothing tests rendering automatically. `./build/game/pxe --help` lists every flag. The useful ones:
- `--capture <png>`: render a few frames at fixed times and write the 960x540 view. Captures are deterministic.
- `--window-png <png>`, `--audio-wav <wav>`, `--self-test`, `--bench`.
- Scene and state: `--scene hills`, `--hour <0-24>`, `--foxes <n>`, `--candles`, `--shelter-test`, `--extra-fires <n>`, `--rain <0-1>`.

A refactor that should not change output must be **byte-identical**. Build the base commit from `git archive <base> | tar -x -C <dir>` in a scratch directory, run both binaries with the same flags, and compare with `cmp`. Standard cases: default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`, and `--self-test`. Logs match apart from the line naming the output file. In zsh, pass multi-word arguments through `"$@"` or an array.

Keyboard, mouse and gamepad play is never exercised by these checks. Give the user a playtest checklist for anything that changes it.

## Workflow
- Features and fixes run through the `/feature` skill (`.claude/skills/feature/SKILL.md`): spec in `docs/tasks/<task-id>/task.md`, code by the `game-developer` agent, assets by the `designer` agent (Blender through MCP), then a fresh build check and review. Task folders record every past change and its verification.
- Independent tasks run in parallel, each in its own git worktree (see the skill).
