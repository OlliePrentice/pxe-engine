# Ignore Python bytecode caches
Type: fix
Status: complete
Cycle: 1

## Goal
Importing `tools/glb.py` as a module (`sys.path.insert(0, "<repo>/tools"); import glb`), which build scripts, checks and Blender's Python now do, writes `tools/__pycache__/glb.cpython-*.pyc`. `.gitignore` doesn't cover it, so `git status --porcelain` shows `?? tools/__pycache__/` afterwards. A dirty main working tree stops `/backlog-run` at preflight, and an agent could commit the bytecode by mistake (`docs/tasks/glb-tool/issues.md`, ISSUE-001).

## Decisions
From the review of `glb-tool`, 2026-10-30, under the alignment rule in `CLAUDE.md` (a discrepancy found outside the task's files).
- Ignore every `__pycache__/` directory in the repository, wherever it appears, with one line in `.gitignore`. Task scripts and Blender builders can import each other too.
- No other change: the tool and the scripts stay as they are.

## Layering
- Engine: none.
- Game: none.
- Repository: `.gitignore`.

## Approach
Add the line `__pycache__/` to `.gitignore`, after `.backlog-runs/` (binding):
```
.backlog-runs/
__pycache__/
```

## Verification
Run in the task's worktree:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning (nothing in C++ changes).
2. `python3 -c "import sys; sys.path.insert(0, 'tools'); import glb"`, then `ls tools/__pycache__` lists a `glb.cpython-*.pyc` and `git status --porcelain` prints nothing.
3. `git check-ignore -v tools/__pycache__/x.pyc docs/tasks/glb-tool/scripts/__pycache__/x.pyc` names the `.gitignore` line for both.
4. `git diff <cycle base> --stat` lists only `.gitignore` and `docs/tasks/gitignore-pycache/`.

## Acceptance criteria
- [x] `.gitignore` has the line `__pycache__/` after `.backlog-runs/`. (`.gitignore:14`, directly after `.backlog-runs/` on line 13)
- [x] After importing `tools/glb.py`, `git status --porcelain` is empty (Verification 2 and 3). (`tools/__pycache__/glb.cpython-314.pyc` written, status empty; `git check-ignore -v` names `.gitignore:14:__pycache__/` for both paths)
- [x] Nothing outside `.gitignore` and the task folder changed (Verification 4). (`git diff 91c4860 --stat`: `.gitignore`, `docs/tasks/gitignore-pycache/handoff.md`, `task.md`)
- [x] Project builds cleanly from a fresh build directory with CMake (orchestrator's `check` build with `--fresh --clean-first`: exit 0, only the stb_image_write `sprintf` warning)

## Assets
None.

## Out of scope
- Changing `tools/glb.py`, any build script or any task script
- Ignoring other Python artefacts (`*.pyc` outside `__pycache__/`, virtual environments)

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30, base 91c4860.
- Cycle 1 review, 2026-10-30: developer commit b23c20a. Fresh build passes (stb `sprintf` warning only), Verification 1-4 pass, alignment review clean (no engine or game code). No issues. Complete.

## Playtest checklist
Nothing to playtest in the game: no engine or game code changed. To confirm the fix, run `python3 -c "import sys; sys.path.insert(0, 'tools'); import glb"` in the repository, then `git status --porcelain`: it should print nothing.
