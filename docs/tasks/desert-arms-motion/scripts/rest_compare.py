"""Verification 3 (task desert-arms-motion): the rest mesh of this commit's
build_arms.py against the cycle base's, by vertex index.

Builds both through runpy, every step up to and including `rig` (no clips),
and compares the rest vertex positions: every vertex outside R.tube.1 within
1e-6 m, the same vertex and polygon counts, and the vertices that moved all
in R.tube.1's last rings (TUBE_SIDES vertices per ring along the path).

Run from the worktree root, with the base's script written out first:
    git show d5bccf4:desert/tools/blender/build_arms.py > $TMPDIR/build_arms_base.py
    Blender -b --python-exit-code 1 -P docs/tasks/desert-arms-motion/scripts/rest_compare.py -- $TMPDIR/build_arms_base.py
"""
import runpy
import sys

import bpy
import numpy as np

TOL = 1e-6


def build(path):
    am = runpy.run_path(path, run_name="arms")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = am["Builder"]()
    for st in am["Builder"].STEPS[:-1]:   # everything but animate
        getattr(b, st)()
    me = b.obj.data
    co = np.array([v.co[:] for v in me.vertices])
    return am, b, co, len(me.polygons), am["piece_indices"](b)


base_path = sys.argv[sys.argv.index("--") + 1]
am_n, b_n, co_n, np_n, idx_n = build("desert/tools/blender/build_arms.py")
am_b, b_b, co_b, np_b, idx_b = build(base_path)
problems = []
print("TUBE_R_END base", am_b["TUBE_R_END"], "new", am_n["TUBE_R_END"])
print("vertices base", len(co_b), "new", len(co_n), "; polygons base", np_b, "new", np_n)
if len(co_b) != len(co_n) or np_b != np_n:
    problems.append("vertex or polygon counts differ")
tube = idx_n["R.tube.1"][0]
if tube != idx_b["R.tube.1"][0]:
    problems.append("R.tube.1's vertex indices differ")
d = np.linalg.norm(co_n - co_b, axis=1)
outside = np.ones(len(d), bool)
outside[tube] = False
print(f"largest move outside R.tube.1: {d[outside].max():.3e} m")
if d[outside].max() > TOL:
    problems.append("a vertex outside R.tube.1 moved")
moved = [i for i in tube if d[i] > TOL]
n = am_n["TUBE_SIDES"]
local = [tube.index(i) for i in moved]
rings = len(tube) // n
moved_rings = sorted({j // n for j in local})
print(f"R.tube.1: {len(tube)} vertices, {rings} rings of {n} (+{len(tube) - rings * n} cap vertices); "
      f"{len(moved)} moved, in rings {moved_rings}, largest {d[moved].max() if moved else 0:.4f} m")
for r in moved_rings:
    ring = tube[r * n:(r + 1) * n]
    print(f"  ring {r}: moved {sum(d[i] > TOL for i in ring)}/{len(ring)}, largest {d[ring].max():.4f} m")
if moved and min(moved_rings) < rings - 3:
    problems.append(f"R.tube.1 moved outside its last rings: {moved_rings}")
print("PROBLEMS:" if problems else "rest mesh: OK", *problems)
if problems:
    raise SystemExit(1)
