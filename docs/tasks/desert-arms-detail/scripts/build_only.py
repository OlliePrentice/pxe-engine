"""Build the arms without checks or export and print triangle counts by part.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P docs/tasks/desert-arms-detail/scripts/build_only.py
"""
import os
import runpy

import bpy

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
am = runpy.run_path(os.path.join(ROOT, "desert", "tools", "blender", "build_arms.py"), run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
b.build_all()
by = {}
for name, p in b.pieces.items():
    key = name.split(".")[1].split("_")[0]
    by[key] = by.get(key, 0) + sum(len(f.verts) - 2 for f in p.faces)
print("PIECES", len(b.pieces), "TRIS", sum(by.values()))
print("BY", sorted(by.items(), key=lambda kv: -kv[1]))
