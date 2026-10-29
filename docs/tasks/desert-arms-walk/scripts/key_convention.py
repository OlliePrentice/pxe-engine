"""Which composition gives the exporter's rotation keys: node rotation x pose
quaternion (bone-local axes kept), or with the pose quaternion turned to Y-up.
Run: Blender -b --python-exit-code 1 -P key_convention.py -- <glb exported with the clips>"""
import os, runpy, sys
import bpy
import numpy as np
from mathutils import Quaternion, Matrix

HERE = os.path.dirname(os.path.abspath(__file__))
am = runpy.run_path(os.path.join(HERE, "..", "..", "..", "..", "desert", "tools", "blender", "build_arms.py"), run_name="arms")
glb = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
b.build_all()
gj, binc = am["read_glb"](open(glb, "rb").read())
nodes = gj["nodes"]
C = Matrix(((1, 0, 0), (0, 0, 1), (0, -1, 0))).to_quaternion()   # Blender -> Y-up
worst = {"plain": 0.0, "converted": 0.0}
for an in gj["animations"]:
    for ch in an["channels"]:
        if ch["target"]["path"] != "rotation":
            continue
        nd = nodes[ch["target"]["node"]]
        x, y, z, w = nd.get("rotation", (0, 0, 0, 1))
        qn = Quaternion((w, x, y, z))
        vals = am["read_accessor"](gj, binc, an["samplers"][ch["sampler"]]["output"])
        for k, (vx, vy, vz, vw) in enumerate(vals):
            qb = b.keys[an["name"]][k][nd["name"]]
            got = Quaternion((vw, vx, vy, vz))
            for kind, q in (("plain", qn @ qb), ("converted", qn @ (C @ qb @ C.inverted()))):
                if q.dot(got) < 0:
                    q = -q
                worst[kind] = max(worst[kind], max(abs(a - c) for a, c in zip(q, got)))
print("CONVENTION", worst)
