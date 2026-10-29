"""Re-import a glb with Blender's glTF importer into an empty scene and list
its skinned meshes, armatures and actions (with frame ranges).
Run: Blender -b --python-exit-code 1 -P reimport_glb.py -- <glb>"""
import sys
import bpy

glb = sys.argv[sys.argv.index("--") + 1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=glb)
skinned = [o.name for o in bpy.data.objects if o.type == "MESH" and any(m.type == "ARMATURE" for m in o.modifiers)]
arms = [(o.name, len(o.data.bones)) for o in bpy.data.objects if o.type == "ARMATURE"]
acts = sorted((a.name, tuple(round(x, 3) for x in a.frame_range)) for a in bpy.data.actions)
tris = sum(len(p.vertices) - 2 for o in bpy.data.objects if o.name in skinned for p in o.data.polygons)
print("REIMPORT", {"skinned": skinned, "armatures": arms, "actions": acts, "triangles": tris})
if len(skinned) != 1 or len(acts) < 2:
    raise SystemExit(1)
