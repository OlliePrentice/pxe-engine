"""Verification 7: copy arms.glb with the first POSITION float of the first
primitive increased by 0.001. Usage: perturb_position.py <in.glb> <out.glb>"""
import struct
import sys

sys.path.insert(0, "tools")
import glb

src, dst = sys.argv[1], sys.argv[2]
data = open(src, "rb").read()
gj, binc = glb.read(data)
acc = gj["accessors"][gj["meshes"][0]["primitives"][0]["attributes"]["POSITION"]]
bv = gj["bufferViews"][acc["bufferView"]]
json_len = struct.unpack_from("<I", data, 12)[0]
bin_start = 20 + ((json_len + 3) & ~3) + 8  # first byte of the bin chunk's data
off = bin_start + bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
old = struct.unpack_from("<f", data, off)[0]
assert glb.accessor(gj, binc, gj["meshes"][0]["primitives"][0]["attributes"]["POSITION"])[0][0] == old
out = bytearray(data)
struct.pack_into("<f", out, off, old + 0.001)
open(dst, "wb").write(out)
print("changed float at byte", off, old, "->", struct.unpack_from("<f", out, off)[0])
