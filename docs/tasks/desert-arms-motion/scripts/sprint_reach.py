"""Reach check for the spec's Arms_Sprint (task desert-arms-motion).

Only rotations are allowed, so the upper arm's head S can only move on the
sphere of radius |S - C| about the shoulder joint's head C (the clavicle), and
the wrist then lies |S - W(e)| from S, where W(e) is the wrist with the forearm
closed by e degrees. A wrist target T is reachable only if
    |T - C| <= |S - C| + |S - W(e)|
This prints, for each scale k in CLIP_SCALES, the frames of the left arm whose
target is out of reach and the worst shortfall, then the largest elbow close
that reaches the forward-peak target, and Builder.carry's own result.

Run: Blender -b --python-exit-code 1 -P docs/tasks/desert-arms-motion/scripts/sprint_reach.py
(from the worktree root)."""
import math
import runpy

import bpy
import numpy as np

am = runpy.run_path("desert/tools/blender/build_arms.py", run_name="arms")
bpy.ops.wm.read_factory_settings(use_empty=True)
b = am["Builder"]()
for st in am["Builder"].STEPS[:-1]:   # everything but animate
    getattr(b, st)()
b._rest = b.rest_np()
Vector = am["Vector"]
S = "L"
sd = b.sides[S]
spec = am["CLIPS"]["Arms_Sprint"]
hinge = np.array(sd.au.cross(sd.af).normalized())
C, Sh = np.array(sd.C), np.array(sd.S)
r_cs = float(np.linalg.norm(Sh - C))


def sw(e):
    """|S - W| with the forearm closed by e degrees."""
    w = b.chain(S, {f"forearm.{S}": hinge * math.radians(e)})
    return float(np.linalg.norm(w - Sh))


def target_and_e(phase, k):
    tau = 2 * math.pi
    c = math.cos(tau * phase)
    s = abs(math.sin(tau * phase))
    dz = -spec["back"] + spec["swing"] * c
    dy = k * (spec["lift"] + spec["arc"] * c * c + spec["drive"] * max(c, 0.0)) - spec["bob"] * (1 - s) ** spec["bob_pow"]
    dx = spec["out"] * max(-c, 0.0) - spec["drift"] * max(c, 0.0)
    T = np.array(sd.W + Vector((dx, -dz, dy)))
    cl = math.cos(tau * (phase - spec["elbow_lag"]))
    pump = spec["pump"][0] * cl if cl > 0 else spec["pump"][1] * cl
    return T, k * (spec["hold"] + spec["give"] * max(cl, 0.0) + pump), (dx, dy, dz)


print(f"|S - C| = {r_cs:.4f} m; |S - W| at rest = {sw(0.0):.4f} m; |W - C| at rest = {np.linalg.norm(np.array(sd.W) - C):.4f} m")
for k in am["CLIP_SCALES"]:
    out, worst = [], 0.0
    for fr in range(am["CLIP_FRAMES"]):
        T, e, _ = target_and_e(fr / 30, k)
        short = float(np.linalg.norm(T - C)) - (r_cs + sw(e))
        if short > 0:
            out.append(fr)
            worst = max(worst, short)
    g, fails = np.zeros(6), []
    b.solve_misses = []
    for fr in range(am["CLIP_FRAMES"]):
        n = len(b.solve_misses)
        _, g = b.carry(spec, fr / 30, S, g, k)
        if len(b.solve_misses) > n:
            fails.append(fr)
    print(f"k {k}: out of reach at frames {out}, worst short by {worst * 1000:.1f} mm; carry()'s solve misses at frames {fails}")
T, e, d = target_and_e(0.0, 1.0)
need = float(np.linalg.norm(T - C)) - r_cs
lo, hi = 0.0, 30.0
for _ in range(40):
    mid = (lo + hi) / 2
    lo, hi = (mid, hi) if sw(mid) >= need else (lo, mid)
print(f"forward peak (phase 0, k 1): offset (x, y, z) {tuple(round(x, 4) for x in d)} m, |T - C| {need + r_cs:.4f} m, "
      f"spec closes the elbow {e:.1f} deg; the most it can close and still reach: {lo:.1f} deg")
for e2 in (0.0, 5.0, 10.0, 15.0, 20.0, 26.8):
    print(f"  elbow closed {e2:4.1f} deg: |S - W| {sw(e2):.4f} m, reach from C {r_cs + sw(e2):.4f} m")
