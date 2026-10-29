"""Compare the joints of two arms glbs: names, node transforms and inverse
bind matrices, and report the largest difference.

    python3 docs/tasks/desert-arms-detail/scripts/compare_joints.py <old.glb> <new.glb>
"""
import json
import struct
import sys


def load(path):
    data = open(path, "rb").read()
    clen = struct.unpack_from("<I", data, 12)[0]
    gj = json.loads(data[20:20 + clen])
    bin_off = 20 + clen + 8
    skin = gj["skins"][0]
    acc = gj["accessors"][skin["inverseBindMatrices"]]
    bv = gj["bufferViews"][acc["bufferView"]]
    start = bin_off + bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    mats = struct.unpack_from("<%df" % (16 * acc["count"]), data, start)
    joints = {}
    for k, j in enumerate(skin["joints"]):
        n = gj["nodes"][j]
        joints[n["name"]] = {"t": n.get("translation", [0, 0, 0]), "r": n.get("rotation", [0, 0, 0, 1]),
                             "s": n.get("scale", [1, 1, 1]), "ibm": mats[16 * k:16 * k + 16]}
    return joints


a, b = load(sys.argv[1]), load(sys.argv[2])
worst = 0.0
if sorted(a) != sorted(b):
    print("joint names differ", sorted(set(a) ^ set(b)))
    sys.exit(1)
for name in a:
    for key in ("t", "r", "s", "ibm"):
        worst = max(worst, max(abs(x - y) for x, y in zip(a[name][key], b[name][key])))
print(f"{len(a)} joints, same names; largest difference in node transforms and inverse bind matrices {worst:.2e}")
sys.exit(0 if worst <= 1e-6 else 1)
