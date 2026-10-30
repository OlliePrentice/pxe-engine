"""Measure how much of each forearm's sleeve the eye can see at rest, behind
the cuff, with the shipped bracer (same place and span as the spec's left
vambrace: BRACER_S (-0.160, -0.020), +-70 degrees about `back`).

Run headless from the worktree root:
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P \
        docs/tasks/desert-arms-gear/scripts/undersuit_visibility.py

Prints, per forearm:
- the in-view, eye-facing part of the forearm surface (radius fore_r(s)) behind
  the cuff: its s range and its angles round the forearm from `back`;
- for rings of 12 vertices at fore_r(s) every 2.5 mm from the cuff's rear face,
  how many would be seen with every piece but the arm tubes and tails in place.
"""

import json
import math
import os
import runpy

import bpy
from mathutils.bvhtree import BVHTree

ROOT = os.getcwd()
am = runpy.run_path(os.path.join(ROOT, "desert/tools/blender/build_arms.py"), run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
b.build_all()
b.clear_pose()
idx = am["piece_indices"](b)
pts = am["evaluated_points"](b)
skip = set()
for n, (vi, pi) in idx.items():
    if n in ("R.arm", "L.arm") or ".tail" in n:
        skip.update(pi)
tree = BVHTree.FromPolygons(pts, [p.vertices for p in b.obj.data.polygons if p.index not in skip])
out = {}
for S in "LR":
    sd = b.sides[S]
    vis = {}
    for si in range(121):
        s = -0.030 - 0.0005 * si
        for a in range(-180, 180, 2):
            d = b.radial(sd, a)
            p = sd.on_forearm(s) + d * sd.fore_r(s)
            if am["in_view"](p) and d.dot((-p).normalized()) > 0:
                vis.setdefault(round(s, 4), []).append(a)
    rings = []
    for k in range(13):
        s = -0.036 - 0.0025 * k
        n = 0
        for p in b._ring(sd.on_forearm(s), sd.af, sd.fore_r(s), am["X"]):
            if not am["in_view"](p):
                continue
            hit, _, _, _ = tree.ray_cast(p * 0, p / p.length, p.length - 0.0006)
            rad = (p - sd.on_forearm(s)).normalized()
            if hit is None and rad.dot((-p).normalized()) > 0:
                n += 1
        rings.append((round(s, 4), n))
    out[S] = {"surface_s_range": [min(vis), max(vis)],
              "surface_angles_from_back": [min(min(x) for x in vis.values()), max(max(x) for x in vis.values())],
              "ring_vertices_seen": rings, "total_seen": sum(n for _, n in rings)}
print("undersuit:", json.dumps(out))
