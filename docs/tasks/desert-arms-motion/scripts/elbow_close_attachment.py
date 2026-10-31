"""Attachment with both elbows closed further than the closed test pose
(ELBOW_CLOSE_TEST, 10 degrees), the rest of the arm at rest (task
desert-arms-motion): the sprint's held 15 degrees and its pump to about 27.

Run: Blender -b --python-exit-code 1 -P docs/tasks/desert-arms-motion/scripts/elbow_close_attachment.py
(from the worktree root)."""
import runpy

import bpy

am = runpy.run_path("desert/tools/blender/build_arms.py", run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
for st in am["Builder"].STEPS[:-1]:   # everything but animate
    getattr(b, st)()
idx = am["piece_indices"](b)
polys = [tuple(p.vertices) for p in b.obj.data.polygons]
for deg in (10.0, 15.0, 18.0, 20.0, 22.0, 24.0, 27.0):
    b.clear_pose()
    for S in "LR":
        sd = b.sides[S]
        b.turn(f"forearm.{S}", sd.au.cross(sd.af).normalized(), deg)
    pts = am["evaluated_points"](b)
    probs = am["attachment_problems"](b, f"elbows closed {deg:.0f}", pts, idx, polys)
    print(f"elbows closed {deg:4.1f}: {len(probs)} attachment problems", *(["\n  " + p for p in probs]))
b.clear_pose()
