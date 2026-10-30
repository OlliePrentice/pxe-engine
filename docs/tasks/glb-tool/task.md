# A shared glb reader and checker
Type: tooling
Status: open
Cycle: 1

## Goal
Asset tasks keep reading glb files by hand to check what a build exported: the JSON chunk, the binary chunk and accessors, decoded with `struct`. There are four hand-written copies so far, across three tasks: `docs/tasks/desert-arms/scripts/verify_arms_glb.py`, `docs/tasks/desert-arms-detail/scripts/compare_joints.py`, `read_glb` and `read_accessor` in `desert/tools/blender/build_arms.py`, and `docs/tasks/desert-arms-walk/scripts/key_diffs.py` (friction log F-026, with F-019). Turn them into one tool, `tools/glb.py`. It is an importable module for build scripts and checks, and a command line with two jobs: `info` lists what a glb holds, and `compare` proves a rebuild kept a model's rig, materials and rest mesh.

## Decisions
From the retro of `desert-arms-walk`, 2026-10-30, under the tooling rule in `.claude/skills/backlog-run/SKILL.md` (step 7).
- **One file, `tools/glb.py`**, both a module and a command-line tool. It lives in `tools/` and not in `tools/blender/`, because `tools/blender/` holds the ruins game's builders and both games use this tool.
- **Python 3 standard library only** (`json`, `struct`, `sys`, `argparse`, `math`), with no numpy. It then runs under the system `python3` and under Blender's bundled Python alike. Accessors come back as lists of tuples. A caller that wants arrays wraps them itself.
- **Read only.** It never writes or changes a glb.
- **`compare` covers the rig, the materials and the rest mesh**, which is what the copies checked. Animations are listed for each side, for information. They are not compared: a task that adds clips must still pass `compare` against the glb before the clips.
- **Exit status** for the command line: 0 for success (or, for `compare`, everything within the tolerance), 1 when `compare` finds a difference, 2 for bad usage or a file that isn't a readable glb.
- **The existing build scripts and task scripts stay as they are.** Switching a builder to the module changes an asset script that the designer owns, so a later asset task does it if it wants.

## Layering
- Engine: none.
- Game: none.
- Tools: `tools/glb.py` (new), listed in `tools/README.md`.

## Approach
Read the four copies named in the Goal first. The module generalises their reading code. Follow `tools/README.md`'s style for the new row.

### Module interface (binding)
```python
"""Read glTF 2.0 binary (.glb) files: the JSON and binary chunks, and
accessors as lists of tuples. Standard library only, so it runs under the
system python3 and under Blender's Python. Read only.

    import sys; sys.path.insert(0, "<repo>/tools"); import glb
    gj, binc = glb.read("desert/assets/characters/arms.glb")
    positions = glb.accessor(gj, binc, prim["attributes"]["POSITION"])

Command line: tools/glb.py info <file.glb> | compare <a.glb> <b.glb> [--tolerance T]
"""

class GlbError(Exception):
    """The data isn't a glb this module can read."""

def read(source):
    """(json dict, bin bytes) from a path or a glb's bytes. The bin chunk is
    b"" when the file has none. Raises GlbError on a bad magic, version or
    chunk type."""

def accessor(gj, binc, index):
    """Accessor `index` as one tuple per element, its components as stored:
    floats for FLOAT (5126), ints for BYTE, UNSIGNED_BYTE, SHORT,
    UNSIGNED_SHORT and UNSIGNED_INT (5120-5125). Honours the buffer view's
    byteStride and both byte offsets. `normalized` is not applied. Types
    SCALAR, VEC2, VEC3, VEC4, MAT2, MAT3, MAT4 (MAT columns as stored, MAT4
    16 components). Raises GlbError for a sparse accessor or one without a
    bufferView."""

def node_trs(node):
    """(translation, rotation xyzw, scale) of a node, with the glTF defaults
    for missing fields. Raises GlbError for a node with a `matrix`."""
```
Element alignment: glTF pads each MAT2 and MAT3 column of 1- and 2-byte components to 4 bytes. Handle that, or raise `GlbError` for those cases (no current asset uses them).

### `info` (sketch of the output, binding in content)
`tools/glb.py info <file.glb>` prints, in file order:
- the asset's generator;
- `materials:` each material's index and name;
- `meshes:` each mesh's name, then each primitive's triangle count (index count / 3, or vertex count / 3 without indices), material index and attribute names;
- `skins:` each skin's name, joint count and joint names in order;
- `nodes with a mesh or skin:` their names;
- `animations:` each animation's name, its duration (the largest sampler input max, in seconds), its channel count, the sorted set of key counts and the set of interpolations;
- a last line `total triangles: <n>`.

For example (sketch):
```
generator: Khronos glTF Blender I/O v5.2.x
materials: 0 Arms_Wrap, 1 Arms_Glove, ...
meshes:
  Arms: 14 primitives
    0: 1234 triangles, material 0, attributes JOINTS_0 NORMAL POSITION WEIGHTS_0
skins:
  Arms_Rig: 39 joints: root, shoulder.L, ...
nodes with a mesh or skin: Arms
animations:
  Arms_Sprint: 1.000 s, 117 channels, keys {31}, LINEAR
  Arms_Walk: 1.000 s, 117 channels, keys {31}, LINEAR
total triangles: 15844
```

### `compare`
`tools/glb.py compare <a.glb> <b.glb> [--tolerance T]` (T default 0) compares, and prints one line for each:
- **materials:** the names in order; equal or not;
- **joints:** each skin's joint names in order; equal or not;
- **joint transforms:** each joint node's translation, rotation and scale, the largest absolute difference;
- **inverse bind matrices:** the largest absolute difference;
- **meshes:** for each mesh, matched by name, and each primitive by index: the vertex and index counts, the largest absolute difference in POSITION, and whether the indices are equal. Different counts are a difference, with no position comparison.
- **animations:** each side's animation names, for information only.

Its last line is `glb compare: identical` when every name list and count matches and every difference is 0, `glb compare: within <T>` when they match and the largest difference is ≤ T, and otherwise `glb compare: differs` followed by the first difference found. Exit 0 for identical or within, 1 for differs.

### `--help`
`tools/glb.py --help` and each subcommand's `--help` print their usage and exit 0. The script is executable, with `#!/usr/bin/env python3`.

### `tools/README.md`
Add a row after the `perspective-check.sh` row (binding):
```
| `tools/glb.py info <file.glb>` / `tools/glb.py compare <a.glb> <b.glb> [--tolerance T]` | Reads glb files, standard library only, so it runs under `python3` and inside Blender. `info` lists the materials, meshes and triangle counts, skins and joints, and each animation's duration, channels, key counts and interpolation. `compare` checks two glbs' materials, joints, joint transforms, inverse bind matrices and rest mesh (positions and indices), and lists their animations without comparing them: exit 0 identical or within the tolerance, 1 differs, 2 bad usage. As a module (`sys.path.insert(0, "<repo>/tools"); import glb`): `glb.read`, `glb.accessor`, `glb.node_trs`. |
```

## Verification
Run in the task's worktree, with `/tmp/glb-check` as scratch (`mkdir -p /tmp/glb-check`):
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning (nothing in C++ changes).
2. `tools/glb.py --help`, `tools/glb.py info --help` and `tools/glb.py compare --help` each exit 0. `tools/glb.py` with no arguments, `tools/glb.py info` with no file, and `tools/glb.py info README.md` each exit 2.
3. `tools/glb.py info desert/assets/characters/arms.glb` exits 0 and shows 14 materials (`Arms_Wrap` first, `Arms_Amber` last, in the order of the slot comment in `desert/src/arms.hpp`), one 39-joint skin, the animations `Arms_Sprint` and `Arms_Walk`, each `1.000 s` with keys `{31}` and `LINEAR`, and `total triangles: 15844` (`docs/tasks/desert-arms-walk/assets.md`).
4. `tools/glb.py info` on `assets/characters/fox.glb`, `assets/characters/hooded_hero.glb`, `assets/scenes/ruins.glb` and `desert/assets/scenes/date_palm.glb` each exits 0. The fox lists the five clips `game/src/fox.cpp` loads (`kClips`: `Fox_Rest`, `Fox_StandUp`, `Fox_LookAround`, `Fox_LieDown`, `Fox_Walk`) among its animations.
5. `compare` against the glb before the clips:
   ```
   git show 55838cc:desert/assets/characters/arms.glb > /tmp/glb-check/arms-before-walk.glb
   tools/glb.py compare /tmp/glb-check/arms-before-walk.glb desert/assets/characters/arms.glb
   ```
   Exit 0, `glb compare: identical`, with the animations line showing none on the first side and the two clips on the second. `desert-arms-walk` read back the same result (rest vertices max difference 0.0 m, `docs/tasks/desert-arms-walk/assets.md`).
6. `compare` against the same results from an existing copy:
   ```
   git show 89c5126:desert/assets/characters/arms.glb > /tmp/glb-check/arms-v1.glb
   git show b9c8d7a:desert/assets/characters/arms.glb > /tmp/glb-check/arms-v2.glb
   python3 docs/tasks/desert-arms-detail/scripts/compare_joints.py /tmp/glb-check/arms-v1.glb /tmp/glb-check/arms-v2.glb
   tools/glb.py compare /tmp/glb-check/arms-v1.glb /tmp/glb-check/arms-v2.glb
   ```
   `compare_joints.py` prints `39 joints, same names; largest difference in node transforms and inverse bind matrices 0.00e+00`. `glb.py` agrees: the joints equal, and joint transforms and inverse bind matrices with a largest difference of 0. Its materials line reports a difference (5 materials before `desert-arms-detail`, 14 after), its mesh line reports different counts, and it exits 1 with `glb compare: differs`.
7. Negative: `tools/glb.py compare desert/assets/characters/arms.glb desert/assets/scenes/date_palm.glb` exits 1 with `glb compare: differs`. A copy of `arms.glb` with one POSITION float changed exits 1, and the same copy with `--tolerance 1` exits 0 with `glb compare: within 1`. Make the copy in `/tmp/glb-check` with a short Python snippet that uses `glb.read` and `glb.accessor` to find the first POSITION float's byte offset and writes the file with that float increased by 0.001. Record the snippet in the handoff.
8. Inside Blender's Python:
   ```
   /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 --python-expr "import sys; sys.path.insert(0, 'tools'); import glb; gj, b = glb.read('desert/assets/characters/arms.glb'); print('ANIMS', len(gj['animations'])); print('POS', len(glb.accessor(gj, b, gj['meshes'][0]['primitives'][0]['attributes']['POSITION'])))"
   ```
   Exit 0, printing `ANIMS 2` and a vertex count that matches what `info` shows for that primitive.
9. `git diff <cycle base> --stat` lists only `tools/glb.py`, `tools/README.md` and `docs/tasks/glb-tool/`.

## Acceptance criteria
- [ ] `tools/glb.py` exists, is executable, uses only the standard library, and has the binding `read`, `accessor`, `node_trs` and `GlbError`.
- [ ] `info` prints what the `info` section lists, and Verification 3 and 4 show it.
- [ ] `compare` compares what the `compare` section lists, prints its last line and exits as specified, and Verification 5, 6 and 7 show it.
- [ ] The module imports and reads under Blender's Python (Verification 8).
- [ ] `tools/README.md` has the binding row.
- [ ] Nothing outside `tools/glb.py`, `tools/README.md` and the task folder changed (Verification 9).
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Changing `desert/tools/blender/build_arms.py` or any other build script to use the module
- Removing or editing the copies in the task folders' `scripts/`
- Re-importing a glb through Blender's importer (F-015)
- Comparing animations' keys, or checking key values against rest
- Writing or editing glb files
- Engine or game code

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 5548df0.
