"""Build the arms, pose at rest, and print diagnostics for the compass face's
first-hit check and the bezel screws' screen extents.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P docs/tasks/desert-arms-detail/scripts/probe.py
"""
import json
import os
import runpy

import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
am = runpy.run_path(os.path.join(ROOT, "desert", "tools", "blender", "build_arms.py"), run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
b.build_all()
idx = am["piece_indices"](b)
b.clear_pose()
pts = am["evaluated_points"](b)
me = b.obj.data
tree = BVHTree.FromPolygons(pts, [p.vertices for p in me.polygons])
poly_piece = {i: n for n, (vi, pi) in idx.items() for i in pi}
project = am["project"]
out = {}
ci = b.compass_info
C, nF = ci["centre"], ci["normal"]
blocked = []
for name in ci["face"]:
    for i in idx[name][0]:
        p = pts[i]
        hit, pidx = am["first_hit"](tree, p)
        if hit is not None and (hit - p).length > am["SEEN_TOL"] and poly_piece.get(pidx) not in ci["face"]:
            rel = p - C
            h = rel.dot(nF)
            blocked.append([name, round(h * 1000, 2), round((rel - nF * h).length * 1000, 2), poly_piece.get(pidx),
                            round((hit - p).length * 1000, 2)])
out["blocked"] = blocked
out["compass_centre_ndc"] = [round(c, 3) for c in project(C)]
for n in b.screws:
    qs = [project(pts[i]) for i in idx[n][0]]
    out[n] = {"y": [round(min(q[1] for q in qs), 4), round(max(q[1] for q in qs), 4)],
              "x": [round(min(q[0] for q in qs), 4), round(max(q[0] for q in qs), 4)],
              "w": am["screen_width"]([pts[i] for i in idx[n][0]])}
print("PROBE", json.dumps(out, indent=1))
