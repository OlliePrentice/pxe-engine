## Cycle 1
Build: pass (`cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning; no C++ changed)

### Changes
- tools/glb.py (new, executable, standard library only): module (`GlbError`, `read`, `accessor`, `node_trs` as in the binding interface) and the `info` / `compare` command line. `accessor` handles byteStride, both byte offsets, all six component types and SCALAR to MAT4, pads MAT2/MAT3 columns of 1- and 2-byte components to 4 bytes, and raises `GlbError` for sparse accessors, a missing bufferView, a buffer that is not the glb's bin chunk, or a read past the end of the bin chunk. `compare` prints one line per check and ends with `glb compare: identical | within T | differs: <first difference>`.
- tools/README.md: the binding row after `perspective-check.sh`.
- docs/tasks/glb-tool/scripts/perturb_position.py: the Verification 7 snippet (below).
- docs/tasks/glb-tool/scripts/synthetic_accessors.py: checks `accessor` on hand-made data (stride, MAT3 of bytes, MAT2 of shorts, offsets, errors), which no current asset exercises.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none
- Game: none (tools only)
- Misplaced code touched: none

### Verification results
1. Fresh build: pass, only the `sprintf` warning.
2. `--help`, `info --help`, `compare --help` exit 0; no arguments, `info` with no file, `info README.md` exit 2.
3. `info desert/assets/characters/arms.glb`: exit 0; 14 materials (`Arms_Wrap` first, `Arms_Amber` last), one skin `Arms_Rig` with 39 joints, animations `Arms_Walk` and `Arms_Sprint` (file order) each `1.000 s, 117 channels, keys {31}, LINEAR`, `total triangles: 15844`.
4. `info` exit 0 on fox.glb (968 triangles; Fox_LieDown, Fox_LookAround, Fox_Rest, Fox_StandUp, Fox_Walk), hooded_hero.glb (5064; 8 clips), ruins.glb (100912; no animations), date_palm.glb (2080; no animations).
5. `compare` arms-before-walk (55838cc) vs arms.glb: exit 0, `glb compare: identical`; animations line `a: none; b: Arms_Walk, Arms_Sprint`.
6. `compare_joints.py` v1 (89c5126) vs v2 (b9c8d7a): `39 joints, same names; largest difference ... 0.00e+00`. `glb.py compare` agrees: `joints: skin 0 equal (39)`, joint transforms and inverse bind matrices largest difference 0; `materials: differ (5 vs 14)`, `mesh Arms: primitive counts differ (5 vs 14)`; last line `glb compare: differs: materials differ (5 vs 14)`; exit 1.
7. arms.glb vs date_palm.glb: exit 1, `glb compare: differs: materials differ (14 vs 2)` (also: skin counts differ, meshes only in one side). arms.glb vs the perturbed copy (first POSITION float 0.182433... to 0.183433...): exit 1, `glb compare: differs: mesh Arms primitive 0 positions: 1.000e-03 over tolerance 0`; with `--tolerance 1`: exit 0, `glb compare: within 1`. arms.glb vs itself: `glb compare: identical`.
   Snippet (also saved as `docs/tasks/glb-tool/scripts/perturb_position.py`, run as `python3 docs/tasks/glb-tool/scripts/perturb_position.py desert/assets/characters/arms.glb /tmp/glb-check/arms-perturbed.glb`):
   ```python
   import struct, sys
   sys.path.insert(0, "tools")
   import glb
   src, dst = sys.argv[1], sys.argv[2]
   data = open(src, "rb").read()
   gj, binc = glb.read(data)
   acc = gj["accessors"][gj["meshes"][0]["primitives"][0]["attributes"]["POSITION"]]
   bv = gj["bufferViews"][acc["bufferView"]]
   json_len = struct.unpack_from("<I", data, 12)[0]
   bin_start = 20 + ((json_len + 3) & ~3) + 8  # first byte of the bin chunk's data
   off = bin_start + bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
   old = struct.unpack_from("<f", data, off)[0]
   assert glb.accessor(gj, binc, gj["meshes"][0]["primitives"][0]["attributes"]["POSITION"])[0][0] == old
   out = bytearray(data)
   struct.pack_into("<f", out, off, old + 0.001)
   open(dst, "wb").write(out)
   ```
8. Blender -b with the given `--python-expr`: exit 0, `ANIMS 2`, `POS 1488` (matches `vertices 1488` of primitive 0 in `compare` output; `info` shows triangle counts, not vertex counts).
9. `git diff 5548df0 --stat` lists only tools/glb.py, tools/README.md and docs/tasks/glb-tool/ (checked at commit).

### How to playtest
- Not a game change. Try `tools/glb.py info <any .glb>` and `tools/glb.py compare <a> <b>`.

### Docs
- None needed beyond the tools/README.md row already added. (`docs/engine-guide.md` and `docs/architecture.md` are unaffected.)

### Known limitations
- `info` prints animations in file order (the spec's example shows alphabetical order, marked a sketch).
- `compare` matches meshes by name; a repeated or missing mesh name gets a `#<index>` suffix. Primitive material indices and other attributes (NORMAL, JOINTS, WEIGHTS) are not compared, as the spec lists only POSITION and indices.
- Joint transforms are compared by position in each skin's joint list (names must be equal first). A joint node with a `matrix` makes `compare` exit 2.
- `accessor` with an external buffer (`uri`) raises `GlbError`; only the glb's own bin chunk is read.
