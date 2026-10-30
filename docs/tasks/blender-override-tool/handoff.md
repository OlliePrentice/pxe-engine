## Cycle 1
Build: pass

### Changes
- tools/blender-override.py: new. Runs a build script's top-level code (exec'd under `__name__ = "__blender_override__"`, so its `__main__` guard stays closed), evaluates each `NAME=expr` in the script's namespace, assigns it, prints `override: NAME = <repr>`, then calls `main()`. Usage errors exit 2 through `os._exit(2)` after flushing (checked: Blender's `--python-exit-code 1` does not change it, exit status is 2 in every case below).
- tools/README.md: the binding row, after the `tools/blender/build_<asset>.py` row.

### Issues addressed
- none (first cycle)

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### How to playtest
- Not a game change. Verification run (scratch `/tmp/override-check`):
  - V1 fresh `check` build: passes, only the stb_image_write `sprintf` warning.
  - V2 `HAND_ROLL=90.0` run: exit 0, printed `override: HAND_ROLL = 90.0`; plain run exit 0; the two `arms.glb` are `cmp`-identical.
  - V3 `HAND_YAW=-8.0 HAND_PITCH=4.0 'WRIST=v(0.200,-0.330,-0.140)'` with `PXE_SKIP_EXPORT=1`: three override lines, exit 1, "66 compass face vertices are out of view".
  - V4 exit 2 for: no script; `NO_SUCH_NAME=1`; `HAND_ROLL`; `HAND_ROLL=undefined_name`; `tools/blender/build_ruins.py`.
  - V5 `git status --porcelain`: only `tools/blender-override.py`, `tools/README.md` and the task folder.

### Docs
- tools/README.md row added (done in this cycle as the spec's binding text). Nothing else.

### Known limitations
- Spec deviation in detection of "no main": `build_ruins.py` does define `main()`, but calls it unguarded at the end of the file (line 722). Loading it with exec runs the whole build and rewrites `assets/scenes/ruins.glb` (my first test did so; I restored it with `git checkout`). The tool therefore checks the script's AST before executing anything: it needs a top-level `def main` and a top-level `if` that mentions `__name__`; otherwise it exits 2 with "has no main() behind an 'if __name__ == "__main__"' guard". The result matches the spec (exit 2 for build_ruins, accepted for all other build scripts), the message differs from "no main".
- Unknown-name checks run after the script's top-level code has run (the names don't exist before), so a typo costs the script's load time (a few seconds for build_arms.py). Overrides are applied in order; a failing later override has already printed the earlier lines.
