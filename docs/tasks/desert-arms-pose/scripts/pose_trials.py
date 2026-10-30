"""desert-arms-pose, cycle 1: why the spec's pose can't pass the build's checks.

Runs against build_arms.py with build_arms-pose.patch applied (the spec's
constants, GLOVE_BULK / CUFF_BULK, choose_gauge and build_checked):
    git apply docs/tasks/desert-arms-pose/scripts/build_arms-pose.patch
    ARMS_SCRIPT=desert/tools/blender/build_arms.py \
    /Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 \
        -P docs/tasks/desert-arms-pose/scripts/pose_trials.py
Prints one JSON report (prefixed "trials:"). Nothing is exported. With
TRIAL_PNG=<abs path> it also renders the eye view at the spec's values (gauge
at its first candidate, -0.0535) there: pose-trial-eye.png was made so.
"""

import json
import os
import re
import runpy

import bpy
import numpy as np
from mathutils import Vector

bpy.ops.wm.read_factory_settings(use_empty=True)
am = runpy.run_path(os.environ["ARMS_SCRIPT"], run_name="arms")
G = am["choose_gauge"].__globals__   # the functions' own globals (run_path returns a copy)
SPEC = dict(GLOVE_BULK=1.15, CUFF_BULK=1.10, GAUGE_TURN=0.55)


def setup(**kw):
    vals = dict(SPEC, WRIST_Z=-0.140, GAUGE_S=-0.0535)
    vals.update(kw)
    G["WRIST"] = Vector((0.200, -0.330, vals.pop("WRIST_Z")))
    G.update(vals)


def kinds(problems):
    out = {}
    for p in problems:
        k = re.sub(r"near \(.*\)", "", p).strip()
        k = re.sub(r"^(rest|wrist \w+ [+-]\d+|fingers curled 60|elbows \w+ \d+): ", "", k)
        out[k] = out.get(k, 0) + 1
    return out


def rest(**kw):
    setup(**kw)
    b = G["Builder"]()
    b.build_all()
    pr, m = G["check"](b)
    f = m["framing"]
    return b, {"problems": kinds(pr), "triangles": m["triangles"],
               "highest_ndc_y": f["highest_ndc_y_in_view"], "nearest_m": f["nearest_in_view_m"],
               "index_knuckle_top_ndc_y": f["index_knuckle_top_ndc_y.L"], "hand_centre_ndc": f["hand_centre_ndc.L"],
               "forearm_exit_ndc_x": f.get("forearm_exit_ndc_x.L"), "lcd": f["lcd"],
               "compass": {k: f["compass"][k] for k in ("centre_ndc", "centre_m_ahead", "face_vertices", "face_vertices_blocked",
                                                        "dial_px_wide", "lamp_px_wide")}}


report = {}
# 1. The gauge search as the spec gives it (choose_gauge raises when none passes).
setup()
try:
    G["choose_gauge"]()
    report["gauge_search"] = "a candidate passed"
except RuntimeError as e:
    report["gauge_search"] = [json.loads(line) for line in str(e).splitlines()[1:]]
# The same search with the cuff at its shipped size.
setup(CUFF_BULK=1.0)
try:
    tried = G["choose_gauge"]()
    report["gauge_search_cuff_1.0"] = {"passes_at": [G["GAUGE_S"], G["GAUGE_TURN"]], "tried": len(tried)}
except RuntimeError:
    report["gauge_search_cuff_1.0"] = "none passes"
# 2. Rest checks: the planning trial (no bulk, the shipped gauge), and the spec with the bulk.
_, report["rest_trial_no_bulk_gauge_-0.0585"] = rest(GLOVE_BULK=1.0, CUFF_BULK=1.0, GAUGE_S=-0.0585)
_, report["rest_spec_bulk_gauge_-0.0535"] = rest()
for cb in (1.15, 1.20):
    _, r = rest(CUFF_BULK=cb)
    report[f"rest_spec_cuff_{cb}_palm_cover"] = {k: n for k, n in r["problems"].items() if "palm" in k}
# 3. Clips: the highest point in view over every sampled frame, at each wrist z.
clips = {}
for z in (-0.140, -0.145, -0.150, -0.155):
    b, _ = rest(WRIST_Z=z)
    cp, cm = G["clip_checks"](b)
    clips[z] = {"walk_top": cm["Arms_Walk"]["highest_ndc_y_in_view"], "sprint_top": cm["Arms_Sprint"]["highest_ndc_y_in_view"],
                "framing_problems": [p for p in cp if "NDC y" in p]}
report["clips"] = clips
print("trials:", json.dumps(report, indent=1))

# An eye view at the spec's values, gauge at its first candidate (-0.0535).
b, _ = rest()
if os.environ.get("TRIAL_PNG"):
    G["PREVIEW_EYE"] = os.environ["TRIAL_PNG"]
    tmpdir = os.path.dirname(os.environ["TRIAL_PNG"])
    for k in ("PREVIEW", "PREVIEW_LEFT", "PREVIEW_RIGHT"):
        G[k] = os.path.join(tmpdir, f".trial_{k.lower()}.png")
    G["OUT_DIR"] = tmpdir
    G["PREVIEW_CLIPS"] = {n: os.path.join(tmpdir, f".trial_{n}.png") for n in G["CLIP_ORDER"]}
    G["render_previews"](b)
    for p in [G[k] for k in ("PREVIEW", "PREVIEW_LEFT", "PREVIEW_RIGHT")] + list(G["PREVIEW_CLIPS"].values()):
        os.remove(p)
