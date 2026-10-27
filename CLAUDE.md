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
CMake presets (`CMakePresets.json`, needs CMake 3.21+) and a wrapper script that builds the engine, game and example, and can run one of them:
```bash
tools/build.sh                                        # release build into build/release
tools/build.sh debug                                  # debug build into build/debug
tools/build.sh release game -- --hour 21 --foxes 3    # build, then run the game with flags
tools/build.sh release example -- --capture out.png   # build, then run the example
```
The binaries are `build/<config>/game/pxe` and `build/<config>/examples/minimal/pxe_minimal`. Run from the caller's directory, so relative output paths land there.

For a clean check, reconfigure and recompile everything (`--fresh` alone keeps the object files, so it is not a clean build):
```bash
cmake --preset check --fresh
cmake --build --preset check --clean-first
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

## Alignment review
Every change the game-developer returns is checked against `docs/architecture.md` and `docs/engine-guide.md`, not only against its task spec. So is every merge into a working branch: a task branch merged back, another branch merged in, or `main` merged in. Review the merged diff (`git diff <branch before merge>..HEAD`), not just the task's.

Check that the change:
- puts each piece in the layer the architecture test and rules give it, and fits its row in "Where things live";
- keeps the engine free of game content and game includes, and the engine holding no pointer to game objects between calls;
- uses the engine as the guide describes: start-up order, the frame loop, the scene build order, upload-then-render, ownership, RNG order when scattering;
- keeps `engine/` self-contained (no path outside it, nothing read from the source tree at run time).

Act on every discrepancy without asking the user first:
- **In the change itself:** record it as an issue in the task's `issues.md` and send it back to game-developer as the next cycle.
- **In code the change didn't touch, or found when reviewing a merge:** create a new task folder with its spec, and delegate it to game-developer. These alignment tasks don't need the user's confirmation of the spec. They still stop and ask when a fix would move, remove or restyle existing game content, change the game's output, or add an engine capability whose scope is unclear.

Once the developer has fixed every discrepancy and the review passes, bring the docs up to date with the code: `docs/architecture.md` (the "Where things live" table, and any new standing rule), `docs/engine-guide.md` (APIs, contracts, snippets, which must still match `examples/minimal/main.cpp` word for word) and `README.md` (features, build, flags). Commit the doc changes with the task's review.
