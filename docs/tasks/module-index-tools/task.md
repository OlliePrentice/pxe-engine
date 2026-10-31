# Module index check, and overrides that reach a build script's modules
Type: tooling
Status: queued
Cycle: 0

## Goal
`arms-build-split` splits `desert/tools/blender/build_arms.py` (3,893 lines) into a package of modules, so the designer reads only the module a task needs. Two tools have to exist first:
- **`tools/module_index.py`**, new. It checks a split build script's module index (the table of contents in its docstring) against the code, and checks that every global name a module's code uses is defined or imported, so a missing import in a branch that only runs when a check fails can't hide.
- **`tools/blender-override.py`**, extended. Today an override reaches only names the build script itself defines. Once constants live in a module that every other module star-imports, each module holds its own copy of each name, so an override must find the module that defines the name and rebind every copy.

## Decisions
From the planning interview, 2026-10-31.
- **The module index is part of the entry script's docstring** and has a fixed grammar (below), so a tool can check it. Its `names:` lines are checked. Its description lines are free text, where the parts a spec describes in words (stitching, straps, fray) are named.
- **The index covers every name a module defines**, private ones included. Only dunders are left out. A name defined in two modules fails: it would shadow silently across mixin classes, and it would make an override ambiguous.
- **The undefined-name check is static**: standard library only (`ast`, `symtable`, `fnmatch`), so it runs under `python3` (3.14 here) and inside Blender (Python 3.13), without importing the modules.
- **The override tool keeps today's behaviour for names the script defines.** Only names the script doesn't define go to its modules. The game's build scripts don't import sibling modules [checked: every import in `tools/blender/*.py` and `desert/tools/blender/*.py` is a standard, Blender or numpy module], so nothing changes for them.
- **Rebinding follows star-import copies**: the override rebinds NAME in every module loaded from under the script's directory, and in the script's own globals, wherever NAME is bound to the same object as in the defining module. Derived values computed at import time still don't follow, and neither do default argument values.

## Layering
- Engine: none. Game: none.
- Tools: `tools/module_index.py` (new), `tools/blender-override.py` (changed), `tools/README.md` (both rows). Test fixture: `docs/tasks/module-index-tools/scripts/fixture/`.

## Approach

### 1. The index grammar (binding)
The entry script is a build script, such as `build_arms.py`. Its module docstring (`ast.get_docstring` of the parsed file) holds the index:

```
Modules (arms_parts/):
  build_arms.py
      The entry point: composes Builder from the mixins and runs the build.
      names: Builder, main
  constants.py
      Every constant, in sections: ...
      names: HERE, OUT_DIR, OUT, PREVIEW*, COLLECTION, MESH, RIG
      names: MATERIALS, M, TRI_BUDGET, ...
  compass.py
      The sun compass on the right cuff: strap, post, housing, hinged sun-shade lid, ...
      names: Compass, compass
```

- **Start.** The index starts at the first docstring line whose stripped text matches `^Modules \(([^)]+)/\):$`. Group 1 is the package directory, relative to the entry script's directory. Call that line's indentation `I`.
- **Entry headers.** A line indented exactly `I + 2` whose stripped text matches `^[A-Za-z_][A-Za-z0-9_]*\.py$` starts an entry. The entry named after the entry script's own file name is the entry script. Every other entry names a file in the package directory.
- **Entry body.** Lines indented more than `I + 2` belong to the entry above them. A line whose stripped text starts with `names:` holds comma-separated tokens: exact names or `fnmatch` globs (`PREVIEW*`). Tokens are stripped and empty ones ignored; an entry may have several `names:` lines. Every other body line is description and is not checked.
- **End.** The index ends at the first non-blank line indented less than `I + 2` after the start line, or at the end of the docstring. Blank lines inside it are allowed.
- **Malformed.** A malformed index is a usage error (exit 2): no start line, a body line before the first entry, a duplicate entry, or a line indented exactly `I + 2` that isn't an entry header.

### 2. What a file defines (binding)
For each indexed file (the entry script and each entry's module), parse it with `ast`. Its **defined names** are taken only from statements directly in the module body:
- the names of `def`, `async def` and `class` statements;
- the targets of `Assign`, `AnnAssign` and `AugAssign` statements that are `Name`s, or `Tuple`s or `List`s of `Name`s (recursively): `VIEW_W, VIEW_H = 960, 540` defines both;
- for each top-level `class`, the names of the `def`s and of the assignment targets (as above) directly in its body.

Names that both start and end with `__` are left out. Imports, `for` targets and anything inside `if`, `try` or `with` blocks are not definitions. (The `if __name__ == "__main__"` guard holds only a call.)

### 3. The index checks (binding)
`problems(entry_path)` returns a list of strings, one per problem, empty when the index is right. It raises `ValueError` for a malformed index or a missing package directory (the CLI exits 2 on those). The problems:
1. An entry names a file that doesn't exist: `index lists <file>, which does not exist`.
2. A `.py` file in the package directory (not recursive) has no entry: `<file> has no entry in the index`. The exception is `__init__.py`, which has no entry and must define no names (`__init__.py defines <name>: keep it empty`). Its docstring is allowed.
3. A token matches none of its own file's defined names (`fnmatch.fnmatchcase`): `<file>: index name <token> matches nothing`.
4. A defined name matches none of its own entry's tokens: `<file>: <name> is not in the index`.
5. A defined name is defined in more than one indexed file: `<name> is defined in <file a> and <file b>`.
6. Every problem `undefined_names` (step 4) reports for each indexed file.

Report problems in a stable order: file order as the index lists them, then line order.

### 4. The undefined-name check (binding behaviour, sketch code)
`undefined_names(path, package_dir=None)` returns `<file>:<line>: <name> is not defined` strings for one file. `package_dir` is the directory that `from .x import *` and `from <package>.x import *` resolve in. It defaults to the file's own directory for a file inside a package (one with `__init__.py`), and to none otherwise. For an entry script, `problems` passes the index's package directory.

1. **Available names.** These are the names bound at module scope, taken from `symtable`: each module-scope symbol that `is_assigned()` or `is_imported()`. Add the names each `from M import *` exports, where M must resolve to a file in `package_dir` (`from .constants import *` in a package file; `from arms_parts.constants import *` in the entry script, with `arms_parts` the package directory's name). A file's star exports are its module-scope bound names that don't start with `_`, its imports included, plus the star exports of its own star imports, followed recursively with a visited set. This is Python's rule for a module without `__all__`. Add `dir(builtins)` and the module attributes `__file__`, `__name__`, `__doc__`, `__spec__`, `__loader__`, `__package__`, `__builtins__`.
2. **Unresolvable star imports.** A star import that doesn't resolve to a file in `package_dir` is a problem in itself: `<file>:<line>: can't check star import from <module>`.
3. **Scopes.** Walk every scope under the module scope (functions, methods, classes, lambdas, comprehensions, recursively). Each symbol that `is_global()` and `is_referenced()` and isn't available is a problem. In the module scope itself, each symbol that `is_referenced()` and is neither assigned nor imported, and isn't available, is a problem. This rule matters: since Python 3.12, comprehensions are inlined, so their loop variables (`M = {name: i for i, ...}`) appear as module-scope symbols that are assigned. Take the line from the first `ast.Name` node with that id in that scope's code (fall back to the scope's `get_lineno()`).
4. **Declared globals.** Names a function declares `global` and assigns count as available.

Before writing the index checks, prove the rule on the current monolith: `undefined_names("desert/tools/blender/build_arms.py")` must return `[]` under `python3` and under Blender's Python (Verification 2). It has no undefined names today.

### 5. CLI and module use (binding)
```
python3 tools/module_index.py <entry script>
```
- Exit 0 with one line `module index: <n> files, <m> names, ok`.
- Exit 1 printing each problem on its own line to stdout, then `module index: <k> problems`.
- Exit 2 printing `module_index: <message>` and the usage line to stderr: no argument, a missing file, a malformed index, a missing package directory.
- `--help` prints the grammar of step 1 in brief, and exits 0.

As a module (`sys.path.insert(0, "<repo>/tools"); import module_index`), it provides `problems(entry_path)` and `undefined_names(path, package_dir=None)`. The file name has an underscore so it can be imported; its top-level code runs the CLI only under `if __name__ == "__main__":`. Start the file with a header comment in the style of `tools/blender-override.py`: usage, exit status, and the grammar in brief.

### 6. blender-override.py (binding behaviour)
Keep everything the tool does today. Change only how each `NAME=expr` is applied, after the script's top-level code has run in `g`:
1. **Script-defined names.** If the script's own AST body defines NAME (the definition rule of step 2, top-level statements only, without class bodies), apply it exactly as today: `value = eval(expr, g)`, `g[NAME] = value`, and print `override: NAME = <repr>`.
2. **Module-defined names.** Otherwise, the candidate modules are every `sys.modules` value whose `__file__` (realpath) lies under the script's directory (realpath, subdirectories included). The defining modules are the candidates whose source (parsed from `__file__`) defines NAME at top level, by the same rule.
   - **Exactly one**, D: evaluate `value = eval(expr, vars(D))`, so the module's helpers and `{**CLIPS, ...}` work. With `old = vars(D)[NAME]`, set `vars(M)[NAME] = value` for every candidate M, and set `g[NAME] = value`, wherever the current binding `is old`. Print `override: NAME = <repr> (<D.__name__>, <k> namespaces)`, where k counts the namespaces rebound, `g` included.
   - **None**: usage error, exit 2, `<script> has no top-level name NAME, and none of its modules defines it`.
   - **More than one**: usage error, exit 2, `NAME is defined in several modules: <names>`.
3. **Header comment.** Add: a script that imports a package from its own directory can have that package's constants overridden too; the copies made by `from x import *` follow; values derived at import time and default argument values (`def f(names=CLIP_ORDER)`) don't.

### 7. Fixture
Write `docs/tasks/module-index-tools/scripts/fixture/`. It is a small build script with a package, shaped as `arms-build-split` will shape `build_arms.py`. It doesn't touch the scene, so it runs headless in a second.
- `build_demo.py`:
  - a docstring with a correct index for itself and `demo_parts/`;
  - the `sys.modules` purge and `sys.path` insert from `arms-build-split` (sketch: delete `demo_parts` and every `demo_parts.*` key, then insert the script's directory if it's missing);
  - `from demo_parts.constants import SIDE`, `from demo_parts.shapes import Square, area`;
  - `main()` printing `demo: side=<SIDE> area=<area()> square=<Square().side()> derived=<constants.AREA>`;
  - the guard `if __name__ == "__main__" and bpy.app.background: main()`.
- `demo_parts/__init__.py`: docstring only.
- `demo_parts/constants.py`: `SIDE = 2.0`, `AREA = SIDE * SIDE` (derived), `def scaled(k): return SIDE * k`.
- `demo_parts/shapes.py`: `from .constants import *`; `def area(): return SIDE * SIDE`; `class Square:` with `def side(self): return SIDE`.

### 8. tools/README.md
- Add a row for `tools/module_index.py`: what it checks, the CLI, the exit codes, and use as a module.
- Extend the `blender-override.py` row: names defined in the script's package modules are overridden in their defining module and every star-import copy; default argument values don't follow.

## Verification
Run from the worktree root. Use `B=/Applications/Blender.app/Contents/MacOS/Blender`, `F=docs/tasks/module-index-tools/scripts/fixture`, and a scratch `T=$(mktemp -d)`. Never edit the fixture in place for a negative case: copy it with `cp -R $F $T/<case>`, then edit the copy.

1. `python3 tools/module_index.py $F/build_demo.py` exits 0. Running the same through Blender's Python also exits 0: `$B -b --python-exit-code 1 --python-expr 'import sys; sys.path.insert(0, "tools"); import module_index; p = module_index.problems("'$F'/build_demo.py"); assert not p, p'`.
2. `python3 -c 'import sys; sys.path.insert(0, "tools"); import module_index; print(module_index.undefined_names("desert/tools/blender/build_arms.py"))'` prints `[]`. So does the same expression run through Blender's Python.
3. Each of these negative cases, made in its own copy of the fixture, makes `python3 tools/module_index.py <copy>/build_demo.py` exit 1 with the problem named:
   - (a) `Square` removed from `shapes.py`'s `names:` line: not in the index;
   - (b) `def extra(): pass` appended to `shapes.py`: not in the index;
   - (c) an entry `ghost.py` added: does not exist;
   - (d) a new file `demo_parts/stray.py` defining `X = 1`: no entry;
   - (e) the star import deleted from `shapes.py`: `SIDE` not defined, with its line;
   - (f) `SIDE = 5.0` appended to `shapes.py` and `SIDE` added to its `names:` line: defined in two files;
   - (g) a token `NOPE_*` added to `constants.py`'s `names:` line: matches nothing.
4. A copy with the docstring's `Modules (` line removed exits 2. So does `python3 tools/module_index.py` with no argument.
5. Overrides on the fixture: `$B -b --python-exit-code 1 -P tools/blender-override.py -- $F/build_demo.py SIDE=3.0` exits 0. It prints `override: SIDE = 3.0 (demo_parts.constants, 3 namespaces)` and `demo: side=3.0 area=9.0 square=3.0 derived=4.0`. The 3 namespaces are `demo_parts.constants`, `demo_parts.shapes` and the script's globals, and `derived` stays 4.0 because AREA was computed at import time.
6. `... build_demo.py 'SIDE=scaled(2)'` prints `side=4.0`, which proves `expr` is evaluated in the defining module.
7. `... build_demo.py NOPE=1` exits 2. So does case (f)'s copy with `SIDE=3.0` (several modules).
8. **Single-file scripts are unchanged.** `PXE_SKIP_EXPORT=1 $B -b --python-exit-code 1 -P tools/blender-override.py -- desert/tools/blender/build_arms.py HAND_ROLL=90.0` exits 0 and prints `override: HAND_ROLL = 90.0` with no suffix. `... build_arms.py NOPE=1` exits 2.
9. `git status --short` lists only the two tools, `tools/README.md`, and the task folder (fixture, `handoff.md`). Delete any `__pycache__` the runs left in the fixture: it is git-ignored, but don't leave it in the worktree.

## Acceptance criteria
- [ ] `tools/module_index.py` exists with the grammar, definition rule, checks, undefined-name check, CLI and module API of Approach steps 1-5, standard library only.
- [ ] Verification 1-4 pass, under both `python3` and Blender's Python where stated.
- [ ] `tools/blender-override.py` applies script-defined names exactly as before and module-defined names as in Approach step 6. Verification 5-8 pass.
- [ ] The fixture is in `docs/tasks/module-index-tools/scripts/fixture/` as in Approach step 7.
- [ ] `tools/README.md` has the new row and the extended override row.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None.

## Out of scope
- Splitting any build script (`arms-build-split` does the arms).
- Any change to `desert/tools/blender/build_arms.py` or any other build script.
- Overriding attributes inside a module (`module.NAME` syntax), and following derived values.

## Spec revisions
None.

## Cycle log
