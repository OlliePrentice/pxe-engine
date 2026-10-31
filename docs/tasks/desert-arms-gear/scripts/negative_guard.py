"""Negative run for the knuckle-guard count (no constant sets it): build up to the rig, drop one guard, run
gear_problems. Expect "L hand has 3 knuckle guards, wants 4". Run from the worktree root with Blender -b -P."""
import runpy, bpy
am = runpy.run_path("desert/tools/blender/build_arms.py", run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
for st in b.STEPS[:-2]:
    getattr(b, st)()
del b.pieces["L.guard_pinky"]
idx = am["piece_indices"](b)
p, m = am["gear_problems"](b, idx)
print("NEG", p)
