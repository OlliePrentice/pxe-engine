"""desert-arms-pose unblock trial: the user's answers to replan-1, run through
the build's whole check() and clip_checks().

Answers: palm wrist ring unbulked, CUFF_BULK 1.0, compass strap slides
toward the hand in 5 mm steps, the sprint's lift then bob lowered in 5 mm
steps, the walk unchanged.

Run against a scratch build_arms.py with build_arms-pose.patch applied and
PALM's first ring spared from GLOVE_BULK in gloves():
    ARMS_SCRIPT=<scratch>/build_arms.py \
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 \
        -P docs/tasks/desert-arms-pose/scripts/unblock_trials.py
It showed the toggles off the plate at every step. A second pass of the same
setup() at wrist z -0.150, compass +0.020, sprint lift 0.0 bob 0.020, with
BRACER_S's wrist end at -0.040, -0.035, -0.030, -0.025 and -0.020, passed
every check first at -0.020. build_arms-unblock.patch is the result.
"""

import json
import os
import re
import runpy

import bpy
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
am = runpy.run_path(os.environ["ARMS_SCRIPT"], run_name="arms")
G = am["choose_gauge"].__globals__
STRAP0 = tuple(G["COMPASS_STRAP"])
CLIPS0 = {k: dict(v) for k, v in G["CLIPS"].items()}
TARGETS0 = {k: dict(v) for k, v in G["CLIP_TARGETS"].items()}


def kinds(problems):
    out = {}
    for p in problems:
        k = re.sub(r"near \(.*\)", "", p).strip()
        k = re.sub(r"^(rest|wrist \w+ [+-]\d+|fingers curled 60|elbows \w+ \d+): ", "", k)
        k = re.sub(r"[-\d.]+", "#", k)
        out[k] = out.get(k, 0) + 1
    return out


def setup(z, compass=0.0, lift=0.015, bob=0.025):
    G["GLOVE_BULK"], G["CUFF_BULK"] = 1.15, 1.0
    G["WRIST"] = Vector((0.200, -0.330, z))
    G["COMPASS_STRAP"] = tuple(s + compass for s in STRAP0)
    G["CLIPS"] = {k: dict(v) for k, v in CLIPS0.items()}
    G["CLIP_TARGETS"] = {k: dict(v) for k, v in TARGETS0.items()}
    G["CLIPS"]["Arms_Sprint"].update(lift=lift, bob=bob)
    G["CLIP_TARGETS"]["Arms_Sprint"].update(mean=lift, bob=bob)


def gauge():
    try:
        G["choose_gauge"]()
        return [G["GAUGE_S"], G["GAUGE_TURN"]]
    except RuntimeError:
        return None


def build(clips=False):
    b = G["Builder"]()
    b.build_all()
    pr, m = G["check"](b)
    out = {"problems": kinds(pr), "triangles": m["triangles"], "compass": m["framing"]["compass"],
           "lcd": m["framing"]["lcd"]}
    if clips:
        cp, cm = G["clip_checks"](b)
        out["clip_problems"] = kinds(cp)
        out["walk_top"] = cm["Arms_Walk"]["highest_ndc_y_in_view"]
        out["sprint_top"] = cm["Arms_Sprint"]["highest_ndc_y_in_view"]
    return out


report = {"wrist": {}, "compass": {}, "sprint": {}}
# 1. Each wrist z, compass where it is, sprint as shipped: rest problems and clip tops.
for z in G["WRIST_ZS"]:
    setup(z)
    g = gauge()
    r = build(clips=True)
    report["wrist"][z] = {"gauge": g, **{k: r[k] for k in ("problems", "walk_top", "sprint_top", "clip_problems", "triangles")},
                          "compass_centre": r["compass"]["centre_ndc"]}
    print("wrist", z, json.dumps(report["wrist"][z]), flush=True)
# The first z where the walk passes (its top at or below -0.30).
z = next(z for z in G["WRIST_ZS"] if report["wrist"][z]["walk_top"] <= -0.30)
report["chosen_z"] = z
# 2. Compass steps at that z.
setup(z)
g = gauge()
chosen_compass = None
for d in (0.0, 0.005, 0.010, 0.015, 0.020, 0.025, 0.030, 0.035):
    setup(z, compass=d)
    G["GAUGE_S"], G["GAUGE_TURN"] = g
    r = build()
    report["compass"][d] = {"problems": r["problems"], "compass": r["compass"]}
    print("compass", d, json.dumps(report["compass"][d]), flush=True)
    if not r["problems"]:
        chosen_compass = d
        break
report["chosen_compass"] = chosen_compass
# 3. Sprint damping at that z and compass.
for lift, bob in ((0.015, 0.025), (0.010, 0.025), (0.005, 0.025), (0.0, 0.025), (0.0, 0.020), (0.0, 0.015)):
    setup(z, compass=chosen_compass or 0.0, lift=lift, bob=bob)
    G["GAUGE_S"], G["GAUGE_TURN"] = g
    r = build(clips=True)
    key = f"lift {lift} bob {bob}"
    report["sprint"][key] = {k: r[k] for k in ("problems", "clip_problems", "walk_top", "sprint_top", "triangles")}
    print("sprint", key, json.dumps(report["sprint"][key]), flush=True)
    if not r["problems"] and not r["clip_problems"]:
        report["chosen_sprint"] = [lift, bob]
        report["final"] = {"gauge": g, "lcd": r.get("lcd"), "triangles": r["triangles"]}
        break
print("trials:", json.dumps(report, indent=1))
