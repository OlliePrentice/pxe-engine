"""Prove a rebuild of the desert arms kept every piece it should keep: each
vertex of each kept piece, at rest, lies within TOL of a rest vertex of the
shipped glb. The kept pieces are every piece except the hands (palm, finger
and thumb segments, their stitches, pads and patches) and the gauge's screen
and readout, which task desert-arms-revision rebuilds.

Run headless (exit 1 when a kept piece moved):
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 \
        -P docs/tasks/desert-arms-revision/scripts/kept_pieces.py -- <shipped.glb> [build_arms.py]
The build script defaults to desert/tools/blender/build_arms.py.
"""
import os
import re
import runpy
import sys

import bpy
from mathutils import Vector
from mathutils.kdtree import KDTree

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
import glb  # noqa: E402

TOL = 1e-6  # metres
REBUILT = re.compile(r"^[LR]\.(palm|(thumb|index|middle|ring|pinky)\.\d|stitch_(thumb|index|middle|ring|pinky)\."
                     r"|patch_tip_|pad_|patch_back|patch_palm)|^L\.(screen|lcd\.|bar\.)")

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if not args:
    raise SystemExit("usage: ... -- <shipped.glb> [build_arms.py]")
shipped = args[0]
script = args[1] if len(args) > 1 else os.path.join(REPO, "desert", "tools", "blender", "build_arms.py")

gj, binc = glb.read(shipped)
old = []
for mesh in gj["meshes"]:
    for prim in mesh["primitives"]:
        old += glb.accessor(gj, binc, prim["attributes"]["POSITION"])
tree = KDTree(len(old))
for i, p in enumerate(old):
    tree.insert(Vector(p), i)
tree.balance()

bpy.ops.wm.read_factory_settings(use_empty=True)
am = runpy.run_path(script, run_name="arms")
b = am["Builder"]()
b.build_all()
idx = am["piece_indices"](b)
b.bm.verts.ensure_lookup_table()
worst, worst_piece, kept, verts = 0.0, None, 0, 0
for name, (vi, _) in sorted(idx.items()):
    if REBUILT.match(name):
        continue
    kept += 1
    for i in vi:
        verts += 1
        _, _, d = tree.find(Vector(am["to_engine"](b.bm.verts[i].co)))
        if d > worst:
            worst, worst_piece = d, name
print(f"kept pieces: {kept}, vertices: {verts}, shipped vertices: {len(old)}")
print(f"furthest kept vertex from the shipped glb: {worst:.3g} m ({worst_piece})")
if worst > TOL:
    raise RuntimeError(f"kept piece {worst_piece} moved {worst:.3g} m (tolerance {TOL})")
print("kept pieces: OK")
