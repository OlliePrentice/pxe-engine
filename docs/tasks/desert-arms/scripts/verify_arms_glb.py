"""Re-import desert/assets/characters/arms.glb into an empty scene and compare
it with a fresh build of desert/tools/blender/build_arms.py: one skinned mesh
on one armature, the 39 joints, the material order, the triangle count, the
rest-pose vertex bounds, and the framing numbers read from the glb's own
positions in engine coordinates (+Y up, +Z forward, +X the player's left).

    /Applications/Blender.app/Contents/MacOS/Blender -b -P docs/tasks/desert-arms/scripts/verify_arms_glb.py
"""
import json
import math
import os
import runpy
import struct

import bpy
from mathutils import Vector

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
GLB = os.path.join(ROOT, "desert", "assets", "characters", "arms.glb")
am = runpy.run_path(os.path.join(ROOT, "desert", "tools", "blender", "build_arms.py"), run_name="arms")

bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
b.build_all()
src = [b.obj.matrix_world @ v.co for v in b.obj.data.vertices]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=GLB)
# The importer adds a bone-shape Icosphere in its glTF_not_exported collection: skip it.
meshes = [o for o in bpy.data.objects if o.type == "MESH" and any(m.type == "ARMATURE" for m in o.modifiers)]
rigs = [o for o in bpy.data.objects if o.type == "ARMATURE"]
problems = []
if len(meshes) != 1 or len(rigs) != 1:
    problems.append(f"{len(meshes)} meshes, {len(rigs)} armatures")
mesh, rig = meshes[0], rigs[0]
bones = sorted(bn.name for bn in rig.data.bones)
if bones != sorted(am["JOINTS"]):
    problems.append("joint names differ")
mats = [m.name for m in mesh.data.materials]
if mats != [n for n, _, _ in am["MATERIALS"]]:
    problems.append(f"materials {mats}")
tris = sum(len(p.vertices) - 2 for p in mesh.data.polygons)
dg = bpy.context.evaluated_depsgraph_get()
ev = mesh.evaluated_get(dg)
me = ev.to_mesh()
imp = [mesh.matrix_world @ v.co for v in me.vertices]
ev.to_mesh_clear()


def box(pts):
    return [min(p[i] for p in pts) for i in range(3)] + [max(p[i] for p in pts) for i in range(3)]


bs, bi = box(src), box(imp)
worst = max(abs(a - c) for a, c in zip(bs, bi))
if worst > 1e-4:
    problems.append(f"re-imported bounds differ by {worst:.6f} m")

# The glb's own POSITION accessors, in engine coordinates.
with open(GLB, "rb") as fh:
    data = fh.read()
clen = struct.unpack_from("<I", data, 12)[0]
gj = json.loads(data[20:20 + clen])
bin_off = 20 + clen + 8
pos = []
for prim in gj["meshes"][0]["primitives"]:
    acc = gj["accessors"][prim["attributes"]["POSITION"]]
    view = gj["bufferViews"][acc["bufferView"]]
    off = bin_off + view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = view.get("byteStride", 12)
    for i in range(acc["count"]):
        pos.append(struct.unpack_from("<fff", data, off + i * stride))
tan_y = math.tan(math.radians(30))
top, near = -9.0, 9.0
for x, y, z in pos:
    if z <= 0:
        continue
    nx, ny = x / (z * tan_y * 16 / 9), y / (z * tan_y)  # engine +X is the player's left: screen x is -nx
    if abs(nx) <= 1 and abs(ny) <= 1:
        top, near = max(top, ny), min(near, z)
out = {"meshes": len(meshes), "armatures": len(rigs), "joints": len(bones), "materials": mats,
       "triangles": tris, "bounds_diff_m": round(worst, 7),
       "engine_bounds": [round(c, 3) for c in box(pos)],
       "glb_highest_ndc_y_in_view": round(top, 3), "glb_nearest_in_view_m": round(near, 3),
       "mesh_node_names": [n.get("name") for n in gj["nodes"] if "mesh" in n],
       "animations": len(gj.get("animations", []))}
print("verify:", json.dumps(out))
if problems:
    raise RuntimeError("arms.glb: " + "; ".join(problems))
print("verify: ok")
