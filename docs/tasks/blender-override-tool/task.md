# Run a Blender build script with constants overridden
Type: tooling
Status: open
Cycle: 1

## Goal
Trying a value in an asset build script, or proving that one of its checks fails, keeps being done by hand: a scratch copy of `desert/tools/blender/build_arms.py` with a constant `sed`-edited (and `HERE` pinned so the copy still finds the repo), or a `runpy.run_path` wrapper that sets constants in the functions' own globals. Copies so far: `desert-arms-revision` (planning trial of `HAND_ROLL`; the designer's negative check runs) and `docs/tasks/desert-arms-pose/scripts/pose_trials.py` (on branch `desert-arms-pose`; overrides `GAUGE_S`, `GAUGE_TURN`, `WRIST`, `GLOVE_BULK`, `CUFF_BULK`). Friction log F-034. Turn them into one tool, `tools/blender-override.py`.

## Decisions
From the retro of the `capture-tool`/`perspective-dust`/`desert-arms-pose` batch, 2026-10-30, under the tooling rule in `.claude/skills/backlog-run/SKILL.md` (step 7).
- **A Blender-side Python runner**, run as `Blender -b --python-exit-code 1 -P tools/blender-override.py -- <build script> [NAME=expr ...]`. It works with every build script that ends in `if __name__ == "__main__" and bpy.app.background: main()` (all of `tools/blender/build_*.py` and `desert/tools/blender/build_*.py` except `build_ruins.py`, which has no `main()`: the tool exits 2 for it).
- **No change to the build scripts.** The script runs from its own path, so `HERE`, `__file__` and its env vars (`PXE_DESERT_OUT`, `PXE_SKIP_EXPORT`, `PXE_SKIP_PREVIEW`, ...) behave as in a plain run.
- **Overrides are Python expressions evaluated in the script's own namespace** after its top-level code has run and before `main()`, so `WRIST=v(0.200,-0.330,-0.140)` uses the script's helpers. Each `NAME` must already be a top-level name in the script; otherwise exit 2. Module-level values computed from an overridden name at import time don't follow it: the tool says so in its help.
- **Exit status:** whatever `main()` does (a raised exception exits 1 through `--python-exit-code 1`); 2 on bad usage (no script, a script without `main`, a malformed `NAME=expr`, an unknown name, an expression that raises).
- It prints one line per override before `main()` runs: `override: NAME = <repr of the new value>`.

## Layering
- Engine: none. Game: none.
- Tools: `tools/blender-override.py` (new), listed in `tools/README.md`.

## Approach
Read `docs/tasks/desert-arms-pose/scripts/pose_trials.py` (branch `desert-arms-pose`: `git show desert-arms-pose:docs/tasks/desert-arms-pose/scripts/pose_trials.py`) first; it shows the globals pitfall: `runpy.run_path` returns a copy of the module's globals, so set names in the dict the functions actually use.

Sketch:
```python
# Run a Blender asset build script with some of its top-level constants
# overridden. Usage (from the repo root):
#   /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 \
#     -P tools/blender-override.py -- <build script> [NAME=expr ...]
import os, sys
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
# parse; on bad usage print the usage to stderr and sys.exit(2)
path = os.path.abspath(script)
g = {"__name__": "__blender_override__", "__file__": path, "__builtins__": __builtins__}
sys.path.insert(0, os.path.dirname(path))
exec(compile(open(path).read(), path, "exec"), g)   # top-level code only: __name__ isn't "__main__"
# check "main" in g and callable; for each NAME=expr: NAME in g, else exit 2;
# value = eval(expr, g), exceptions -> exit 2; g[NAME] = value; print the override line
g["main"]()
```
`sys.exit(2)` inside Blender with `--python-exit-code 1` must still produce exit status 2: check it, and if Blender maps it to 1, use `os._exit(2)` after flushing stdout and stderr.

### `tools/README.md`
Add a row after the `tools/blender/build_<asset>.py` row (binding):
```
| `Blender -b --python-exit-code 1 -P tools/blender-override.py -- <build script> [NAME=expr ...]` | Runs an asset build script with top-level constants overridden (each `expr` is evaluated in the script's namespace, so its helpers work: `WRIST=v(0.2,-0.33,-0.14)`), for trying a value or proving a check fails without editing the script. Combine with the script's env vars (`PXE_SKIP_EXPORT=1`, `PXE_DESERT_OUT=<scratch>`). Values derived from a constant at import time don't follow its override. Exit as the build, 2 bad usage. |
```

## Verification
Run in the task's worktree. `B=/Applications/Blender.app/Contents/MacOS/Blender`, scratch `/tmp/override-check`.
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. A no-op override: `PXE_DESERT_OUT=/tmp/override-check/a PXE_SKIP_PREVIEW=1 $B -b --python-exit-code 1 -P tools/blender-override.py -- desert/tools/blender/build_arms.py HAND_ROLL=90.0` exits 0 and prints `override: HAND_ROLL = 90.0`. A plain run, `PXE_DESERT_OUT=/tmp/override-check/b PXE_SKIP_PREVIEW=1 $B -b --python-exit-code 1 -P desert/tools/blender/build_arms.py`, exits 0, and the two `arms.glb` are `cmp`-identical.
3. A failing override: `PXE_SKIP_EXPORT=1 $B -b --python-exit-code 1 -P tools/blender-override.py -- desert/tools/blender/build_arms.py HAND_YAW=-8.0 HAND_PITCH=4.0 'WRIST=v(0.200,-0.330,-0.140)'` prints three override lines and exits 1: at that pose the compass face leaves the view (66 of 216 vertices below the bottom edge, measured in `desert-arms-pose`'s replan-1.md).
4. Bad usage exits 2 each: no script; `NO_SUCH_NAME=1`; `HAND_ROLL` (no `=`); `HAND_ROLL=undefined_name`; `tools/blender/build_ruins.py` (no `main`).
5. `git status --porcelain` lists only `tools/blender-override.py`, `tools/README.md` and the task folder; no asset or preview changed.

## Acceptance criteria
- [ ] `tools/blender-override.py` exists and behaves as the Decisions say, with the override lines and exit statuses.
- [ ] `tools/README.md` has the binding row.
- [ ] Verification 2–5 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Changing any build script, asset or preview.
- Sweeps over several values in one run (call the tool once per value).

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30, base c2c6add
