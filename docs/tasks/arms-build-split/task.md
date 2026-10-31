# Split the arms build script into modules
Type: tooling
Status: queued
Cycle: 0

## Goal
Every arms task makes the designer read a large part of `desert/tools/blender/build_arms.py` (3,893 lines), most of which the task doesn't touch. Split it into a package, `desert/tools/blender/arms_parts/`, of one module per component of the arms plus shared modules. Give it a module index the build checks, and teach the designer to read through the index and grep, never whole files. A compass task should then read only the index, `compass.py`, and the lines of `constants.py` it greps for. This is a pure refactor: the built `arms.glb` stays byte-identical, and every check runs and reports exactly as before.

## Decisions
From the planning interview, 2026-10-31.
- **Mixins.** Each component module defines one plain class holding that component's `Builder` methods, moved unchanged (`class Compass:` with `compass()`). `build_arms.py` composes them: `class Builder(Core, Sleeves, ...)`. Method bodies, `self.` calls across modules, the step API (`b.compass()`) and `Builder.STEPS` stay as they are. [Checked at planning: no method name is defined twice in today's class, so the method resolution order can't change which method runs.]
- **One `constants.py`** holds every module-level constant, in today's order and sections with their comments. The index names its sections.
- **Package name `arms_parts`.** The live flow runs `runpy.run_path(..., run_name="arms")`, which puts `sys.modules["arms"]` in place as the script's own temporary module while it runs, so a package named `arms` can't be imported then. Future splits are named `<asset>_parts/`.
- **Imports inside the package:** relative. Each module star-imports what it uses of `helpers`, `constants` and `view` (`from .constants import *`), imports functions from any other module by name (`from .clips import action_fcurves`), and imports the standard and Blender modules it uses.
- **Reload:** `build_arms.py` deletes `arms_parts` and every `arms_parts.*` from `sys.modules` each time it loads. After an edit, the live flow runs `run_path` again and makes a new `Builder()`.
- **The long asset description moves**, verbatim apart from indentation, into the docstrings of the modules that build what it describes (Approach step 5). `build_arms.py`'s docstring keeps the summary line, the run instructions and the module index.
- **The module index** follows the grammar of `docs/tasks/module-index-tools/task.md` (Approach step 1) and covers every name each file defines, private ones included. `build_arms.py` runs `tools/module_index.py` when it loads, so a stale index, or an undefined name in any module, fails the build.
- **The reading rule** goes in `.claude/agents/designer.md` for every build script: index first where there is one, otherwise grep, and read only the lines needed. The `/backlog-run` review checks the index. Only `build_arms.py` is split now. The game's large scripts (`build_tavern_exterior.py` 2,446 lines, `build_tavern_interior.py` 2,073, `build_fox.py` 1,853) keep one file each.
- **Determinism (measured at planning):** two headless builds of 6bb469a with `PXE_SKIP_PREVIEW=1` into separate `PXE_DESERT_OUT` directories wrote the same `arms.glb`, SHA-1 `6306223fe0c9c29acfe93c93fdaea6d733efac3b`, equal to the committed `desert/assets/characters/arms.glb`. Their logs were the same apart from the glTF exporter's `HH:MM:SS | INFO:` lines. So the split's glb must be byte-identical to the committed one, with no looser fallback: a difference means the refactor changed something.

## Layering
- Engine: none. Game: none (`desert::Arms` loads the same glb).
- Assets: `desert/tools/blender/build_arms.py` and the new `desert/tools/blender/arms_parts/` build the same `arms.glb`. Instructions: `.claude/agents/designer.md`, `.claude/skills/backlog-run/SKILL.md`, `desert/docs/art-style.md`, `README.md`.

## Approach
Designer only. No game code changes, so the developer has nothing to build. The source is `build_arms.py` at 6bb469a (`git show 6bb469a:desert/tools/blender/build_arms.py`; `module-index-tools` doesn't touch it, so the cycle base's copy is the same). The line numbers below are that file's.

### 1. The modules (binding assignment)
Move each range into its module. A range means the whole definitions in it, with the comment lines directly above each definition. The blank lines between definitions don't matter. A class name in the "Class" column means the range's methods go in that one mixin class in that module, at their existing indentation.

| Module | Class | Lines of today's file | What |
| :- | :- | :- | :- |
| `helpers.py` | | 182-217, 607-660, 3198-3199 | `X`, `Y`, `Z`, `v`, `lin`, `to_srgb`, `clamp01`, `smoothstep`, `rot`, `mirror`; `ring_frame`, `circle`, `section_pts`, `clip_half`; `to_engine` (moved here because `checks`, `clip_checks` and `export` all use it, and keeping it in `clip_checks` would make an import cycle with `checks`) |
| `constants.py` | | 169-180, 219-296, 350-604, 754-763 | paths and names; materials, budgets, `GEAR_TARGETS`, the eye camera's constants; the held pose, sleeve, glove, detail and clip constants (`palm_front` included); `bone_names`, `JOINTS` |
| `view.py` | | 299-347 | `project`, `in_view`, `to_px`, `screen_width`, `first_hit`, `seen` |
| `core.py` | `Core` | 662-752 (`Side`, a top-level class), 766-775 (`Piece`, a top-level class); in `Core`: 781-1090, 1094-1104, 2300-2302 | `Side` and `Piece`; `Core` holds `STEPS`, `__init__`, scene plumbing, geometry helpers, weights, `_ring` (used by sleeves, gloves and glove detail) and `build_all` |
| `sleeves.py` | `Sleeves` | 1092, 1106-1142, 1472-1569 | `arms`, `sleeve_r`, `strip_at`; `sleeve_weights`, `wrap_detail` |
| `gloves.py` | `Gloves` | 1144-1221 | `gloves`, `_segments`, `fingers` |
| `glove_detail.py` | `GloveDetail` | 1223-1470 | `cuff_r` to `glove_detail` |
| `vambraces.py` | `Vambraces` | 1571-1777 | `_bracer_point` to `vambrace` |
| `gauge.py` | `Gauge` | 1779-1871 | `gauge` |
| `bracer_detail.py` | `BracerDetail` | 1873-2041 | `bracer_detail` |
| `compass.py` | `Compass` | 2043-2154 | `compass` |
| `tubes.py` | `Tubes` | 2156-2241 | `tube_lift`, `plate_run`, `tubes`, `lay_tube` |
| `rig.py` | `Rig` | 2243-2298, 2304-2360 | `bone_table`, `rig`; `clear_pose`, `turn`, `test_poses` |
| `clips.py` | `Clips`, then top-level | `Clips`: 2362-2549; top-level: 2552-2574 | `rest_np` to `sample`; `rodrigues`, `action_fcurves` |
| `checks.py` | | 2577-2672, 3072-3195 | `piece_indices`, `closed_problems`, `evaluated_points`, `attachment_problems`; `check` |
| `framing.py` | | 2674-2892 | `framing` |
| `gear_checks.py` | | 2894-3069 | `gear_problems` |
| `clip_checks.py` | | 3202-3451 | `posed_array` to `fit_clips` |
| `export.py` | | 3454-3766 | `export`, `add_clips`, `check_glb`, `read_glb`, `read_accessor`, `f32`, `node_trs`, `trs_matrix`, `check_glb_clips` |
| `previews.py` | | 3769-3858 | `render_previews` |
| `__init__.py` | | | a docstring only: `"""The arms build's modules. The index is in ../build_arms.py's docstring."""` |
| `build_arms.py` | `Builder` | 1-18 (rewritten, step 4), 3861-3889, 3892-3893 | the entry point |

[Checked at planning: with this assignment the imports have no cycle. `constants` uses `helpers`; `view` uses `constants`; `checks` uses `framing` and `gear_checks`; `clip_checks` uses `checks`, `clips` and `view`; nothing imports `build_arms`.]

Only these lines of moved code may change:
- **`HERE` in `constants.py`** becomes `HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))   # desert/tools/blender, where build_arms.py is` (binding). The `OUT_DIR` line after it stays as it is, so the glb and previews land where they do today.
- **`class Builder:` (line 780)** becomes `class Core:` with the docstring `"""The builder's state, scene plumbing and geometry primitives; build_arms.Builder mixes the steps in."""`.
- **Each mixin class header** has the shape (binding): `class Compass:` then `"""<The component> steps; mixed into build_arms.Builder, so self is the whole builder."""`. This also applies to `Sleeves`, `Gloves`, `GloveDetail`, `Vambraces`, `Gauge`, `BracerDetail`, `Tubes`, `Rig` and `Clips`.
- **Each module's imports**, at its top after its docstring: the standard and Blender modules it uses (from today's lines 157-167), then its package imports. Run `tools/module_index.py` (step 4) to find a missing one.

Everything else is moved as it is. Comments and docstrings inside definitions included, apart from those step 6 corrects.

### 2. build_arms.py (binding)
```python
"""<step 4>"""

import json
import os
import sys

import bpy

# Each load imports arms_parts afresh (a headless run, or runpy.run_path in a
# live Blender), so an edited module takes effect. A Builder made before the
# edit still runs the old code: make a new one.
for _mod in [n for n in sys.modules if n == "arms_parts" or n.startswith("arms_parts.")]:
    del sys.modules[_mod]
_here = os.path.dirname(os.path.abspath(__file__))
for _p in (_here, os.path.normpath(os.path.join(_here, "..", "..", "..", "tools"))):
    if _p not in sys.path:
        sys.path.insert(0, _p)

import module_index  # noqa: E402  (tools/module_index.py)

if module_index.problems(os.path.abspath(__file__)):
    raise RuntimeError("build_arms.py's module index doesn't match the code (tools/module_index.py):\n  "
                       + "\n  ".join(module_index.problems(os.path.abspath(__file__))))

from arms_parts.bracer_detail import BracerDetail  # noqa: E402
from arms_parts.checks import check  # noqa: E402
# ... one line per name main() and the docstring's live flow use, alphabetical by module:
#     Clips, fit_clips, Compass, OUT, Core, add_clips, check_glb, check_glb_clips, export,
#     GloveDetail, Gloves, Gauge, render_previews, Rig, Sleeves, Tubes, Vambraces


class Builder(Core, Sleeves, Gloves, GloveDetail, Vambraces, Gauge, BracerDetail, Compass, Tubes, Rig, Clips):
    """The whole builder: Core's state and primitives, and each mixin's steps (the index above)."""


def main():
    ...  # lines 3862-3889 unchanged


if __name__ == "__main__" and bpy.app.background:
    main()
```
The comments may be reworded. The code may not. `_here` is a top-level assignment, so the index lists it under `build_arms.py` (`names: Builder, main, _here`). `_mod` and `_p` are loop targets and are not definitions.

### 3. The index content (sketch: complete the `names:` lines until the tool passes)
Each description names, in words, every part the module builds that a spec would describe without an identifier: take them from the material table rows step 5 moves into that module, and from its methods' docstrings. The names in the sketch are real; the lists are not complete.

```
Modules (arms_parts/):
  build_arms.py
      The entry point: purges and imports arms_parts, checks this index,
      composes Builder from the mixins below, and runs the build (main).
      names: Builder, main, _here
  helpers.py
      Small maths and geometry helpers: vectors, sRGB conversion, rotations,
      ring frames, circles, finger and strap cross-sections, polygon clipping,
      Blender-to-engine axes.
      names: X, Y, Z, v, lin, to_srgb, clamp01, smoothstep, rot, mirror
      names: ring_frame, circle, section_pts, clip_half, to_engine
  constants.py
      Every constant, in sections: output paths and names; materials (the
      engine's order); triangle budget; gear targets (GEAR_TARGETS); the eye
      camera and framing limits; the held pose; sleeve and wrap strips; cuff,
      palm, fingers and thumb; vambraces, straps and buckles, scratches, rust;
      the gauge, its LCD readout; stitches, knuckle guards, worn patches, hand
      plates, tails; screws, knurled dial, toggles, coil, canister, cable;
      tubes; the compass; clips (CLIPS) and their check targets (CLIP_TARGETS);
      the joint names.
      names: HERE, OUT_DIR, OUT, PREVIEW*, COLLECTION, MESH, RIG
      names: MATERIALS, M, TRI_BUDGET, CREASED, ROUND_MIN, SPEC_MATERIALS, GEAR_TARGETS
      names: COMPASS_*, FACE_R, HOUSING_R, LID_OPEN, TICK, NEEDLE, LAMP
      names: CLIP*, FRAME_TOP*, LIMBS, DIGITS, SOLVE_*, ...
  view.py
      Projection through the eye camera: NDC, on-screen pixels and widths,
      first hits and whether a vertex is seen.
      names: project, in_view, to_px, screen_width, first_hit, seen
  core.py
      Each arm's joints and frames (Side), a named piece (Piece), and the
      builder's state, scene plumbing, lofts, boxes, cylinders, tubes, slabs,
      stitch dashes and the weight functions (Core).
      names: Side, Piece, Core, STEPS, start, refresh, piece, loft, ...
  sleeves.py
      The dark undersuit arm tubes, the three frayed khaki wrap strips on each
      forearm, the grime strip and the knotted tails.
      names: Sleeves, arms, sleeve_r, strip_at, sleeve_weights, wrap_detail
  ...
  compass.py
      The sun compass on the right cuff: its strap, post, gunmetal housing,
      hinged sun-shade lid propped open, side buttons, dark face, 12 tick
      marks, fixed needle and status lamp.
      names: Compass, compass
  ...
```
Put the index right after the run instructions, so `sed -n '/^Modules (/,/^[^ ]/p' build_arms.py` prints it. The index ends at the closing `"""` or at the next unindented line.

### 4. build_arms.py's docstring (binding content, sketch wording)
In this order:
1. Line 1 as today.
2. The run instructions of lines 3-18 as today, except where the live flow's description needs an addition. After "fit_clips(b) re-keys each at the first scale that passes its clip_checks.", add: "After editing a module, run runpy.run_path again and make a new Builder: each load imports arms_parts afresh, and a builder made before the edit still runs the old code."
3. One line: `The code is in arms_parts/; the index below says which module builds what. tools/module_index.py checks it each time this script loads.`
4. The index (step 3).

### 5. Where the rest of today's docstring goes (binding)
Move each passage verbatim. Only the indentation may change. Each module's docstring starts with a one-line summary, then the passages it receives.
- **"A view model: the wanderer's two arms ..."** through "... and fails if any is off." goes to `constants.py`: the frame, the held pose and what the framing checks.
- **"Every piece is a closed volume ..."** through "... (desert/docs/art-style.md, "View models are the exception"):" goes to `constants.py`. So does the final paragraph, "Material order is the contract with the engine ...".
- **Each row of the material table** goes to the module whose step builds the pieces it describes. Find the step by grepping the piece names the step creates.
  - `arm tube`, and the `wrap` rows (the strips, `Arms_Grime` and the tails) go to `sleeves.py`.
  - The `glove` `Arms_Glove` row goes to `gloves.py`. The `Arms_Gunmetal` plate, `Arms_Thread`, `Arms_Worn` and `Arms_Strap` glove rows go to `glove_detail.py`.
  - The `vambraces` rows go to `vambraces.py`.
  - The `gauge` bezel, `Arms_Screen` and `Arms_Glow` readout rows go to `gauge.py`. Its `Arms_Steel`, `Arms_Copper` and `Arms_Rubber` rows go to `bracer_detail.py`.
  - The `compass` rows go to `compass.py`, and the `tubes` row to `tubes.py`.
  - If a row's pieces are built by a step other than the one named here, it goes with that step. Record any such move in `assets.md`.
- **"One skinned mesh (`Arms`) ..."** through "... each elbow opened 30 degrees and closed 30." goes to `rig.py`.
- **"Two looping clips ..."** through "... sample every key and half-key as the engine does." goes to `clips.py`. Its "The formulas are above CLIPS" stays true, because the comment above `CLIPS` moves with it into `constants.py`. Add "(in constants.py)" after "CLIPS".

### 6. Comments and docstrings the split makes wrong
Fix any comment or docstring that names `build_arms.py` as the place where code that has moved now lives. List each edit in `assets.md` (designer.md: fix docstrings the change makes wrong).

### 7. Docs (binding text)
**`.claude/agents/designer.md`**:
- Add this section before `## Building assets`:
  ```
  ## Reading build scripts
  Read only the code the task needs. Build scripts run to thousands of lines, and reading code you won't change costs tokens and nothing else.
  - A build script split into modules (`desert/tools/blender/build_arms.py` and its `arms_parts/`) has a module index in its docstring: each module, what it builds, the parts in it, and every name it defines. Read the index first (`sed -n '/^Modules (/,/^[^ ]/p' <script>`), then grep for the exact name (`grep -n 'def compass(' desert/tools/blender/arms_parts/*.py`), and read only that module, or only the lines around the match (Read with `offset` and `limit`).
  - In a script without an index, grep it for the name and read only the line ranges you need.
  - Never read a whole script, or a module the task doesn't change, to find out what's in it.
  - When you add, rename, move or remove a function, method, class or constant in a split script, update its module index in the same change, and name in the module's description any part you add that a spec would describe in words (a strap, a stitch row) but no identifier spells. The script runs `tools/module_index.py` each time it loads, and fails on an index that doesn't match the code or on a name no module defines.
  ```
- Append to the bullet that starts "To build step by step in the live session": ` A split script deletes its `<asset>_parts` modules from `sys.modules` each time it loads: after editing a module, run `runpy.run_path` again, make a new `Builder()` and build from `start()`, because a builder kept in the namespace still runs the old code.`
- In the bullet that starts "Blender 5.2's glTF exporter", replace `` `add_clips` in `desert/tools/blender/build_arms.py` `` with `` `add_clips` in `desert/tools/blender/arms_parts/export.py` ``.
- In the bullet that starts "Take each check's targets", replace `` (`CLIP_TARGETS` in `desert/tools/blender/build_arms.py`) `` with `` (`CLIP_TARGETS` in `desert/tools/blender/arms_parts/constants.py`) ``.

**`.claude/skills/backlog-run/SKILL.md`**, step 4 (**Review**): append to the end of its first paragraph, after "Check that every asset the developer loads matches its `assets.md` entry.": ` When the change touches a build script with a module index (a `Modules (` line in its docstring), run `python3 tools/module_index.py <script>` (it must exit 0), and check that each part the change adds that the spec describes in words is named in its module's description.` The skill gives the designer no other instructions about build scripts, so nothing else in it changes.

**`desert/docs/art-style.md`**: in the Characters passage, replace `` Built by `desert/tools/blender/build_<name>.py` and exported `` with `` Built by `desert/tools/blender/build_<name>.py` (a large one split into modules in `<name>_parts/` beside it, with a module index in its docstring) and exported ``. Everything else in that passage stays.

**`README.md`**: in the paragraph that starts "To try a value in an asset build script, or prove one of its checks fails, without editing it,", replace `with `NAME=expr` overrides of its top-level constants (each evaluated in the script's own namespace):` with `with `NAME=expr` overrides of its top-level constants or of those in its package's modules (each evaluated in the module that defines it, and followed into every module that star-imports it):`. The command block after it stays as it is.

Leave `docs/friction-log.md` and every older `docs/tasks/*/` file alone: they record the past.

## Verification
Run from the worktree root. Use `B=/Applications/Blender.app/Contents/MacOS/Blender`, `T=$(mktemp -d)`, and `strip() { sed -E '/^[0-9]{2}:[0-9]{2}:[0-9]{2} \| INFO: /d; /^added Arms_Walk/d' "$1"; }`. The base script for log comparisons is `git show 6bb469a:desert/tools/blender/build_arms.py > $T/base_build_arms.py`. It writes only where `PXE_DESERT_OUT` points, so it runs from `$T`.

1. **Byte-identical glb.**
   - `PXE_SKIP_PREVIEW=1 PXE_DESERT_OUT=$T/new $B -b --python-exit-code 1 -P desert/tools/blender/build_arms.py > $T/new.log 2>&1` exits 0.
   - `cmp $T/new/arms.glb desert/assets/characters/arms.glb` prints nothing.
   - `shasum $T/new/arms.glb` gives `6306223fe0c9c29acfe93c93fdaea6d733efac3b`.
2. **Same checks, same numbers.**
   - `PXE_SKIP_PREVIEW=1 PXE_DESERT_OUT=$T/base $B -b --python-exit-code 1 -P $T/base_build_arms.py > $T/base.log 2>&1` exits 0.
   - `diff <(strip $T/base.log) <(strip $T/new.log)` prints nothing. This covers the `arms:`, `clips:`, `clip_scale:` and `arms.glb:` measurements and every message.
3. **The in-repo build with previews.**
   - `$B -b --python-exit-code 1 -P desert/tools/blender/build_arms.py` exits 0. This is the only run that exercises `render_previews`.
   - `git diff --quiet desert/assets/characters/arms.glb` exits 0.
   - Restore the previews: `git checkout -- desert/assets/characters/`.
4. **Overrides reach the modules.** Each runs `$B -b --python-exit-code 1 -P tools/blender-override.py -- desert/tools/blender/build_arms.py <override>` with `PXE_SKIP_PREVIEW=1 PXE_DESERT_OUT=$T/<case>`. Each override line names `arms_parts.constants`. Each case was run at planning on the unsplit script, with the results stated:
   - (a) `FRAME_TOP=-0.9` exits 1. The log has "something in view reaches NDC y -0.410, above -0.9" (from `framing.py`).
   - (b) `'GEAR_TARGETS={**GEAR_TARGETS, "tube_r": (0.0030, 0.0035)}'` exits 1. The log lists `L.tube.1` with `'ok': False` (from `gear_checks.py`).
   - (c) `'CLIP_TARGETS={**CLIP_TARGETS, "Arms_Walk": dict(CLIP_TARGETS["Arms_Walk"], hand_lag=0.30)}'` exits 1 with "Arms_Walk passes its checks at no scale" (from `clip_checks.py`).
   - (d) `LID_OPEN=108.0` exits 0, and `shasum $T/d/arms.glb` gives `20cbe413af19dd0bfc788d0756e923890e2c5d46` (from `compass.py`).
   - (e) The README example, `PXE_SKIP_EXPORT=1 ... HAND_ROLL=90.0`, exits 0.
5. **Index check.**
   - `python3 tools/module_index.py desert/tools/blender/build_arms.py` exits 0.
   - Negative (a): remove `compass` from `compass.py`'s `names:` line in the `build_arms.py` docstring. Then `python3 tools/module_index.py desert/tools/blender/build_arms.py` exits 1 naming `compass`, and `PXE_SKIP_EXPORT=1 $B -b --python-exit-code 1 -P desert/tools/blender/build_arms.py` exits 1 with "module index doesn't match" in its output.
   - Negative (b): append `def stray(): pass` to `tubes.py`. The tool exits 1 naming `stray`.
   - Negative (c): delete `from .constants import *` from `compass.py`. The tool exits 1 naming an undefined constant in `compass.py`.
   - Make these negative runs after committing the split. Restore each file after its case (`git checkout -- <file>`). Afterwards, `git status --short desert/tools/blender/` must print nothing.
6. **Moved verbatim.** Write `docs/tasks/arms-build-split/scripts/moved_verbatim.py` (run with `python3`). It parses today's file (`git show 6bb469a:desert/tools/blender/build_arms.py`) with `ast`. For each top-level statement other than the docstring, the imports, the `HERE` assignment, the `Builder` class and the `__main__` guard, it takes the source segment (`ast.get_source_segment`). It does the same for each statement in the `Builder` class body other than its header. It checks that each segment occurs exactly once across the new files (`build_arms.py` and `arms_parts/*.py`), and in the module that step 1 assigns it to. It prints the counts and exits 0, or lists each segment that's missing, duplicated or in the wrong module and exits 1. It must exit 0.
7. **Live session** (through the Blender MCP, following "Protect the user's Blender session" in designer.md). Only if `bpy.data.is_dirty` is false: `bpy.ops.wm.read_homefile(use_empty=True)`, then:
   - `am = runpy.run_path("<worktree>/desert/tools/blender/build_arms.py", run_name="arms")`.
   - Run the docstring's step-by-step sequence, `b.start()` through `b.animate()`, then `am["check"](b)`: no problems.
   - Temporarily edit `arms_parts/compass.py`: insert `        self.compass_reloaded = True` as the first line of `compass()`'s body. Run `run_path` again with a new `Builder()`, through `b.compass()`. `getattr(b, "compass_reloaded", False)` is `True`.
   - Restore the file, run once more with a new `Builder()`, and the attribute is absent.
   - Don't save the file. Record the three results in `assets.md`.
8. **Leftovers.**
   - `git grep -n "build_arms.py\`" .claude desert/docs README.md` shows no line that names `build_arms.py` as the home of `add_clips`, `CLIP_TARGETS` or any other moved name.
   - `wc -l desert/tools/blender/build_arms.py desert/tools/blender/arms_parts/*.py`, with the counts recorded in `assets.md`.

## Acceptance criteria
- [ ] `desert/tools/blender/arms_parts/` holds the modules of Approach step 1, each with the code that step assigns it, and `moved_verbatim.py` exits 0.
- [ ] `build_arms.py` is the entry point of Approach step 2. Its docstring holds the summary, the run instructions (with the reload sentence), the pointer line and the module index. `python3 tools/module_index.py desert/tools/blender/build_arms.py` exits 0.
- [ ] Every index description names the parts its module builds, in the words of the material table rows step 5 moved there.
- [ ] Today's docstring passages are in the module docstrings step 5 names, verbatim apart from indentation and the "(in constants.py)" addition.
- [ ] Verification 1-4: the glb is byte-identical, the logs match the base, the previews render, and overrides (a)-(e) give the stated results.
- [ ] Verification 5 and 7: the stale-index negatives fail the tool and the build, and the live session picks up an edit after `run_path`.
- [ ] `.claude/agents/designer.md`, `.claude/skills/backlog-run/SKILL.md`, `desert/docs/art-style.md` and `README.md` have the step 7 edits.
- [ ] `desert/assets/characters/` is unchanged (`git diff --quiet 6bb469a -- desert/assets/characters/arms.glb`; previews restored).
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
- **arms** (unchanged): `desert/assets/characters/arms.glb` rebuilt from the split script, byte-identical to the committed file (SHA-1 `6306223fe0c9c29acfe93c93fdaea6d733efac3b`). Format, units, axes, origin, rig, materials, clips and triangle count are all as shipped. Previews are not compared: they are rewritten by Verification 3 and restored.
  - Format: glb, as shipped.
  - Units and axes: as shipped.
  - Origin: the eye, as shipped.
  - Budget: 24,208 triangles, as shipped.
  - Animations: `Arms_Walk`, `Arms_Sprint`, as shipped.

## Out of scope
- Any change to the arms' geometry, rig, clips, materials, checks or their numbers, and any C++ change.
- Splitting `desert/tools/blender/build_date_palm.py` or any script in `tools/blender/`.
- Renaming or restructuring code beyond the moves and the listed line changes. That includes fixing existing code smells, adding `__all__` lists and removing unused imports from moved code.
- `docs/friction-log.md` and older task folders.

## Spec revisions
None.

## Cycle log
