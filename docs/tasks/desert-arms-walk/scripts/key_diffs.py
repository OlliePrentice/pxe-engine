"""How far a glb's non-turning keys sit from the joint nodes' rest TRS.
Used on a glb exported by Blender's glTF exporter with the clips' actions
(export_animations=True): 1-2 float steps off, so build_arms.py writes the
clips itself. Needs numpy, so run it in Blender's Python:
    Blender -b --python-expr "import sys; sys.argv=['x', '<glb>']; exec(open('<this file>').read())"
"""
import json, struct, sys
import numpy as np

data = open(sys.argv[1], "rb").read()
clen = struct.unpack_from("<I", data, 12)[0]
gj = json.loads(data[20:20 + clen])
binc = data[20 + clen + 8:]

def acc(i):
    a = gj["accessors"][i]; bv = gj["bufferViews"][a["bufferView"]]
    n = {"SCALAR": 1, "VEC3": 3, "VEC4": 4}[a["type"]]
    return np.frombuffer(binc, "<f4", n * a["count"], bv.get("byteOffset", 0) + a.get("byteOffset", 0)).reshape(a["count"], n).astype(np.float64)

nodes = gj["nodes"]
for an in gj["animations"]:
    worst = {"translation": 0, "rotation": 0, "scale": 0}
    for ch in an["channels"]:
        nd = nodes[ch["target"]["node"]]; path = ch["target"]["path"]
        rest = np.array(nd.get(path, {"translation": [0, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1]}[path]))
        v = acc(an["samplers"][ch["sampler"]]["output"])
        if path == "rotation" and nd["name"].split(".")[0] in ("shoulder", "upper_arm", "forearm", "hand"):
            continue
        if path == "rotation" and an["name"] == "Arms_Sprint" and nd["name"].split(".")[0] in ("thumb", "index", "middle", "ring", "pinky"):
            continue
        worst[path] = max(worst[path], float(np.abs(v - rest).max()))
    print(an["name"], worst)
