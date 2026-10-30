## Cycle 1
Build: pass

### Changes
- .gitignore: added the line `__pycache__/` directly after `.backlog-runs/`, so every Python bytecode cache directory in the repository is ignored.

### Issues addressed
- None (cycle 1; no issues.md).

### Layering
- Engine: none
- Game: none
- Misplaced code touched: none

### Verification
1. `cmake --preset check --fresh && cmake --build --preset check --clean-first`: exit 0. One warning only, the stb_image_write `sprintf` deprecation (`engine/third_party/stb/stb_image_write.h:776`).
2. `python3 -c "import sys; sys.path.insert(0, 'tools'); import glb"`: `ls tools/__pycache__` lists `glb.cpython-314.pyc`. `git status --porcelain` showed only ` M .gitignore` (the change under test, uncommitted at that moment) and no `??` entry; `git status --porcelain --ignored` lists `!! tools/__pycache__/`. After the commit the porcelain output is empty.
3. `git check-ignore -v tools/__pycache__/x.pyc docs/tasks/glb-tool/scripts/__pycache__/x.pyc`:
   - `.gitignore:14:__pycache__/	tools/__pycache__/x.pyc`
   - `.gitignore:14:__pycache__/	docs/tasks/glb-tool/scripts/__pycache__/x.pyc`
4. `git diff 91c4860 --stat` after the commit: only `.gitignore` and `docs/tasks/gitignore-pycache/` (see commit).

### How to playtest
- Nothing to playtest; no game or engine code changed. To check by hand: import `tools/glb.py` from Python and run `git status --porcelain`; it prints nothing.

### Docs
- None.

### Known limitations
- None. Only `__pycache__/` directories are ignored, as the spec states; stray `*.pyc` files elsewhere are out of scope.
