"""Checks glb.accessor on hand-made data: byteStride, offsets, padded MAT3 of
bytes, MAT2 of shorts, unsigned ints. Run: python3 synthetic_accessors.py"""
import struct, sys
sys.path.insert(0, "tools")
import glb

vec3 = struct.pack("<3f4x3f4x", 1, 2, 3, 4, 5, 6)  # VEC3 floats, stride 16
mat3 = struct.pack("<3Bx3Bx3Bx", 1, 2, 3, 4, 5, 6, 7, 8, 9) + b"\0" * 4  # MAT3 ubytes, columns padded to 4
mat2 = struct.pack("<2H2H", 1, 2, 3, 4)  # MAT2 ushorts, columns of 4 bytes
ints = struct.pack("<3I", 7, 8, 9)
off_mat3, off_mat2, off_int = len(vec3), len(vec3) + len(mat3), len(vec3) + len(mat3) + len(mat2)
binc = vec3 + mat3 + mat2 + ints
gj = {"buffers": [{"byteLength": len(binc)}],
      "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteStride": 16}, {"buffer": 0, "byteOffset": off_mat3},
                      {"buffer": 0, "byteOffset": off_mat2}, {"buffer": 0, "byteOffset": off_int}],
      "accessors": [{"bufferView": 0, "componentType": 5126, "count": 2, "type": "VEC3"},
                    {"bufferView": 1, "componentType": 5121, "count": 1, "type": "MAT3"},
                    {"bufferView": 2, "componentType": 5123, "count": 1, "type": "MAT2"},
                    {"bufferView": 3, "byteOffset": 4, "componentType": 5125, "count": 2, "type": "SCALAR"},
                    {"componentType": 5126, "count": 1, "type": "SCALAR"}]}
assert glb.accessor(gj, binc, 0) == [(1.0, 2.0, 3.0), (4.0, 5.0, 6.0)], glb.accessor(gj, binc, 0)
assert glb.accessor(gj, binc, 1) == [(1, 2, 3, 4, 5, 6, 7, 8, 9)], glb.accessor(gj, binc, 1)
assert glb.accessor(gj, binc, 2) == [(1, 2, 3, 4)], glb.accessor(gj, binc, 2)
assert glb.accessor(gj, binc, 3) == [(8,), (9,)], glb.accessor(gj, binc, 3)
try:
    glb.accessor(gj, binc, 4)
    raise SystemExit("no GlbError for an accessor without a bufferView")
except glb.GlbError:
    pass
try:
    glb.node_trs({"name": "m", "matrix": [1] * 16})
    raise SystemExit("no GlbError for a matrix node")
except glb.GlbError:
    pass
assert glb.node_trs({}) == ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0), (1.0, 1.0, 1.0))
for bad in (b"", b"x" * 40, struct.pack("<III", 0x46546C67, 1, 20) + b"\0" * 8):
    try:
        glb.read(bad)
        raise SystemExit("no GlbError for bad data")
    except glb.GlbError:
        pass
print("synthetic accessors: ok")
