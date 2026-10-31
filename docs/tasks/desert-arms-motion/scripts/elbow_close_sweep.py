"""Attachment with both elbows closed 10-33 degrees (the rest of the arm at rest),
for the old and the new R.tube.1 end (TUBE_R_END -0.205, -0.195). Task
desert-arms-motion, cycle 2.

Run from the worktree root:
    Blender -b --python-exit-code 1 -P docs/tasks/desert-arms-motion/scripts/elbow_close_sweep.py
"""
import sys, bpy
p = "desert/tools/blender/build_arms.py"
for end in (-0.205, -0.195):
    g = {"__name__": "arms", "__file__": __import__("os").path.abspath(p)}
    exec(compile(open(p).read(), p, "exec"), g)
    g["TUBE_R_END"] = end
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = g["Builder"]()
    for st in g["Builder"].STEPS[:-1]:
        getattr(b, st)()
    idx = g["piece_indices"](b)
    polys = [tuple(pp.vertices) for pp in b.obj.data.polygons]
    for deg in (10, 18, 22, 27, 30, 33):
        b.clear_pose()
        for S in "LR":
            sd = b.sides[S]
            b.turn(f"forearm.{S}", sd.au.cross(sd.af).normalized(), float(deg))
        pts = g["evaluated_points"](b)
        probs = g["attachment_problems"](b, f"closed {deg}", pts, idx, polys)
        print(f"END {end} closed {deg}: {len(probs)}", probs)
