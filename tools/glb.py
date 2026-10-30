#!/usr/bin/env python3
"""Read glTF 2.0 binary (.glb) files: the JSON and binary chunks, and
accessors as lists of tuples. Standard library only, so it runs under the
system python3 and under Blender's Python. Read only.

    import sys; sys.path.insert(0, "<repo>/tools"); import glb
    gj, binc = glb.read("desert/assets/characters/arms.glb")
    positions = glb.accessor(gj, binc, prim["attributes"]["POSITION"])

Command line: tools/glb.py info <file.glb> | compare <a.glb> <b.glb> [--tolerance T]
"""
import argparse
import json
import struct
import sys

_MAGIC = 0x46546C67  # "glTF"
_CHUNK_JSON = 0x4E4F534A  # "JSON"
_CHUNK_BIN = 0x004E4942  # "BIN\0"

# glTF componentType -> (struct code, size in bytes).
_COMPONENT = {5120: ("b", 1), 5121: ("B", 1), 5122: ("h", 2), 5123: ("H", 2), 5125: ("I", 4), 5126: ("f", 4)}
# glTF accessor type -> (rows, columns): a vector is one column.
_TYPE_SHAPE = {"SCALAR": (1, 1), "VEC2": (2, 1), "VEC3": (3, 1), "VEC4": (4, 1),
               "MAT2": (2, 2), "MAT3": (3, 3), "MAT4": (4, 4)}


class GlbError(Exception):
    """The data isn't a glb this module can read."""


def read(source):
    """(json dict, bin bytes) from a path or a glb's bytes. The bin chunk is
    b"" when the file has none. Raises GlbError on a bad magic, version or
    chunk type."""
    if isinstance(source, (bytes, bytearray, memoryview)):
        data = bytes(source)
    else:
        try:
            with open(source, "rb") as fh:
                data = fh.read()
        except OSError as e:
            raise GlbError(f"cannot read {source}: {e.strerror or e}") from e
    if len(data) < 20:
        raise GlbError("not a glb: shorter than a header and a chunk header")
    magic, version, length = struct.unpack_from("<III", data, 0)
    if magic != _MAGIC:
        raise GlbError("not a glb: bad magic")
    if version != 2:
        raise GlbError(f"unsupported glb version {version}")
    end = min(length, len(data))
    gj = None
    binc = b""
    off = 12
    while off + 8 <= end:
        clen, ctype = struct.unpack_from("<II", data, off)
        body = data[off + 8:off + 8 + clen]
        if len(body) < clen:
            raise GlbError("truncated glb: a chunk runs past the end of the file")
        if gj is None:
            if ctype != _CHUNK_JSON:
                raise GlbError(f"first chunk has type {ctype:#x}, not JSON")
            try:
                gj = json.loads(body.decode("utf-8"))
            except (UnicodeDecodeError, ValueError) as e:
                raise GlbError(f"bad JSON chunk: {e}") from e
        elif ctype == _CHUNK_BIN:
            binc = body
            break
        # Chunks of other types are skipped, as the glTF specification asks.
        off += 8 + ((clen + 3) & ~3)
    if gj is None:
        raise GlbError("glb has no JSON chunk")
    if not isinstance(gj, dict):
        raise GlbError("JSON chunk is not an object")
    return gj, binc


def accessor(gj, binc, index):
    """Accessor `index` as one tuple per element, its components as stored:
    floats for FLOAT (5126), ints for BYTE, UNSIGNED_BYTE, SHORT,
    UNSIGNED_SHORT and UNSIGNED_INT (5120-5125). Honours the buffer view's
    byteStride and both byte offsets. `normalized` is not applied. Types
    SCALAR, VEC2, VEC3, VEC4, MAT2, MAT3, MAT4 (MAT columns as stored, MAT4
    16 components). Raises GlbError for a sparse accessor or one without a
    bufferView."""
    try:
        acc = gj["accessors"][index]
    except (KeyError, IndexError, TypeError) as e:
        raise GlbError(f"no accessor {index}") from e
    if "sparse" in acc:
        raise GlbError(f"accessor {index} is sparse")
    if "bufferView" not in acc:
        raise GlbError(f"accessor {index} has no bufferView")
    if acc.get("componentType") not in _COMPONENT:
        raise GlbError(f"accessor {index}: unsupported componentType {acc.get('componentType')}")
    if acc.get("type") not in _TYPE_SHAPE:
        raise GlbError(f"accessor {index}: unsupported type {acc.get('type')}")
    try:
        bv = gj["bufferViews"][acc["bufferView"]]
    except (KeyError, IndexError, TypeError) as e:
        raise GlbError(f"accessor {index}: no bufferView {acc['bufferView']}") from e
    buffers = gj.get("buffers", [])
    if bv.get("buffer", 0) != 0 or (buffers and "uri" in buffers[0]):
        raise GlbError(f"accessor {index}: its buffer is not the glb's binary chunk")

    code, csize = _COMPONENT[acc["componentType"]]
    rows, cols = _TYPE_SHAPE[acc["type"]]
    col_bytes = rows * csize
    pad = 0
    if cols > 1 and col_bytes % 4:
        pad = 4 - col_bytes % 4  # glTF pads MAT2 and MAT3 columns of 1- and 2-byte components to 4 bytes
    col_fmt = f"{rows}{code}" + (f"{pad}x" if pad else "")
    fmt = struct.Struct("<" + col_fmt * cols)
    elem_size = (col_bytes + pad) * cols
    stride = bv.get("byteStride") or elem_size
    if stride < elem_size:
        raise GlbError(f"accessor {index}: byteStride {stride} is smaller than an element ({elem_size})")
    start = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
    count = acc["count"]
    if count and start + (count - 1) * stride + fmt.size > len(binc):
        raise GlbError(f"accessor {index}: runs past the end of the binary chunk")
    unpack = fmt.unpack_from
    return [unpack(binc, start + k * stride) for k in range(count)]


def node_trs(node):
    """(translation, rotation xyzw, scale) of a node, with the glTF defaults
    for missing fields. Raises GlbError for a node with a `matrix`."""
    if "matrix" in node:
        raise GlbError(f"node {node.get('name', '?')} has a matrix")
    return (tuple(float(x) for x in node.get("translation", (0.0, 0.0, 0.0))),
            tuple(float(x) for x in node.get("rotation", (0.0, 0.0, 0.0, 1.0))),
            tuple(float(x) for x in node.get("scale", (1.0, 1.0, 1.0))))


# ---------------------------------------------------------------------------
# Command line

def _node_name(gj, i):
    return gj["nodes"][i].get("name", f"node{i}")


def _primitive_triangles(gj, prim):
    """Triangle count of a primitive: index count (or vertex count without
    indices) by the draw mode."""
    if "indices" in prim:
        n = gj["accessors"][prim["indices"]]["count"]
    else:
        n = gj["accessors"][prim["attributes"]["POSITION"]]["count"]
    mode = prim.get("mode", 4)
    if mode == 4:
        return n // 3
    if mode in (5, 6):
        return max(n - 2, 0)
    return 0


def _animation_info(gj, binc, anim):
    """(duration s, channel count, sorted key counts, sorted interpolations)."""
    duration = 0.0
    keys = set()
    interps = set()
    for s in anim.get("samplers", []):
        a = gj["accessors"][s["input"]]
        keys.add(a["count"])
        interps.add(s.get("interpolation", "LINEAR"))
        if a.get("max"):
            duration = max(duration, float(a["max"][0]))
        elif a["count"]:
            duration = max(duration, max(t[0] for t in accessor(gj, binc, s["input"])))
    return duration, len(anim.get("channels", [])), sorted(keys), sorted(interps)


def _fmt_anim(gj, binc, anim, i):
    duration, channels, keys, interps = _animation_info(gj, binc, anim)
    return (f"{anim.get('name', f'animation{i}')}: {duration:.3f} s, {channels} channels, "
            f"keys {{{', '.join(str(k) for k in keys)}}}, {', '.join(interps)}")


def cmd_info(path):
    gj, binc = read(path)
    print("generator:", gj.get("asset", {}).get("generator", "unknown"))
    mats = gj.get("materials", [])
    print("materials:", ", ".join(f"{i} {m.get('name', '')}".rstrip() for i, m in enumerate(mats)) or "none")
    total = 0
    print("meshes:" + ("" if gj.get("meshes") else " none"))
    for i, mesh in enumerate(gj.get("meshes", [])):
        prims = mesh.get("primitives", [])
        print(f"  {mesh.get('name', f'mesh{i}')}: {len(prims)} primitives")
        for k, prim in enumerate(prims):
            tris = _primitive_triangles(gj, prim)
            total += tris
            attrs = " ".join(sorted(prim.get("attributes", {})))
            print(f"    {k}: {tris} triangles, material {prim.get('material', 'none')}, attributes {attrs}")
    print("skins:" + ("" if gj.get("skins") else " none"))
    for i, skin in enumerate(gj.get("skins", [])):
        names = ", ".join(_node_name(gj, j) for j in skin.get("joints", []))
        print(f"  {skin.get('name', f'skin{i}')}: {len(skin.get('joints', []))} joints: {names}")
    holders = [_node_name(gj, i) for i, n in enumerate(gj.get("nodes", [])) if "mesh" in n or "skin" in n]
    print("nodes with a mesh or skin:", ", ".join(holders) or "none")
    anims = gj.get("animations", [])
    print("animations:" + ("" if anims else " none"))
    for i, anim in enumerate(anims):
        print("  " + _fmt_anim(gj, binc, anim, i))
    print(f"total triangles: {total}")
    return 0


def _fmt_diff(d):
    return "0" if d == 0 else f"{d:.3e}"


class _Compare:
    """Collects a comparison's findings: the first difference (a structural
    mismatch, or a number over the tolerance) and the largest number."""

    def __init__(self, tolerance):
        self.tolerance = tolerance
        self.largest = 0.0
        self.first = None

    def fail(self, text):
        if self.first is None:
            self.first = text

    def number(self, value, text):
        self.largest = max(self.largest, value)
        if value > self.tolerance:
            self.fail(f"{text}: {_fmt_diff(value)} over tolerance {self.tolerance:g}")


def _max_abs_diff(xs, ys):
    return max((abs(a - b) for x, y in zip(xs, ys) for a, b in zip(x, y)), default=0.0)


def _mesh_keys(gj):
    """Mesh index by name; a repeated or missing name gets a '#<index>' suffix."""
    out = {}
    for i, mesh in enumerate(gj.get("meshes", [])):
        name = mesh.get("name", f"#{i}")
        if name in out:
            name = f"{name}#{i}"
        out[name] = i
    return out


def cmd_compare(path_a, path_b, tolerance):
    ga, ba = read(path_a)
    gb, bb = read(path_b)
    cmp = _Compare(tolerance)

    names_a = [m.get("name", "") for m in ga.get("materials", [])]
    names_b = [m.get("name", "") for m in gb.get("materials", [])]
    if names_a == names_b:
        print(f"materials: equal ({len(names_a)})")
    else:
        print(f"materials: differ ({len(names_a)} vs {len(names_b)})")
        print(f"  a: {', '.join(names_a) or 'none'}")
        print(f"  b: {', '.join(names_b) or 'none'}")
        cmp.fail(f"materials differ ({len(names_a)} vs {len(names_b)})")

    skins_a, skins_b = ga.get("skins", []), gb.get("skins", [])
    if len(skins_a) != len(skins_b):
        print(f"joints: skin counts differ ({len(skins_a)} vs {len(skins_b)})")
        cmp.fail(f"skin counts differ ({len(skins_a)} vs {len(skins_b)})")
    for si, (sa, sb) in enumerate(zip(skins_a, skins_b)):
        ja = [_node_name(ga, j) for j in sa.get("joints", [])]
        jb = [_node_name(gb, j) for j in sb.get("joints", [])]
        if ja != jb:
            print(f"joints: skin {si} differ ({len(ja)} vs {len(jb)})")
            cmp.fail(f"skin {si} joints differ ({len(ja)} vs {len(jb)})")
            print(f"joint transforms: skin {si} skipped (joints differ)")
            print(f"inverse bind matrices: skin {si} skipped (joints differ)")
            continue
        print(f"joints: skin {si} equal ({len(ja)})")
        worst = 0.0
        for name, na, nb in zip(ja, sa["joints"], sb["joints"]):
            ta, tb = node_trs(ga["nodes"][na]), node_trs(gb["nodes"][nb])
            worst = max(worst, max(_max_abs_diff([x], [y]) for x, y in zip(ta, tb)))
        print(f"joint transforms: skin {si} largest difference {_fmt_diff(worst)}")
        cmp.number(worst, f"skin {si} joint transforms")
        if "inverseBindMatrices" in sa and "inverseBindMatrices" in sb:
            ma = accessor(ga, ba, sa["inverseBindMatrices"])
            mb = accessor(gb, bb, sb["inverseBindMatrices"])
            if len(ma) != len(mb):
                print(f"inverse bind matrices: skin {si} counts differ ({len(ma)} vs {len(mb)})")
                cmp.fail(f"skin {si} inverse bind matrix counts differ ({len(ma)} vs {len(mb)})")
            else:
                d = _max_abs_diff(ma, mb)
                print(f"inverse bind matrices: skin {si} largest difference {_fmt_diff(d)}")
                cmp.number(d, f"skin {si} inverse bind matrices")
        elif ("inverseBindMatrices" in sa) != ("inverseBindMatrices" in sb):
            print(f"inverse bind matrices: skin {si} present on one side only")
            cmp.fail(f"skin {si} inverse bind matrices on one side only")
        else:
            print(f"inverse bind matrices: skin {si} none on either side")

    keys_a, keys_b = _mesh_keys(ga), _mesh_keys(gb)
    for name in keys_a:
        if name not in keys_b:
            print(f"mesh {name}: only in a")
            cmp.fail(f"mesh {name} only in a")
    for name in keys_b:
        if name not in keys_a:
            print(f"mesh {name}: only in b")
            cmp.fail(f"mesh {name} only in b")
    for name, ia in keys_a.items():
        if name not in keys_b:
            continue
        pa, pb = ga["meshes"][ia].get("primitives", []), gb["meshes"][keys_b[name]].get("primitives", [])
        if len(pa) != len(pb):
            print(f"mesh {name}: primitive counts differ ({len(pa)} vs {len(pb)})")
            cmp.fail(f"mesh {name} primitive counts differ ({len(pa)} vs {len(pb)})")
            continue
        for k, (xa, xb) in enumerate(zip(pa, pb)):
            label = f"mesh {name} primitive {k}"
            if "POSITION" not in xa["attributes"] or "POSITION" not in xb["attributes"]:
                print(f"{label}: no POSITION on one side")
                cmp.fail(f"{label} has no POSITION on one side")
                continue
            va = ga["accessors"][xa["attributes"]["POSITION"]]["count"]
            vb = gb["accessors"][xb["attributes"]["POSITION"]]["count"]
            ia_ = ga["accessors"][xa["indices"]]["count"] if "indices" in xa else None
            ib_ = gb["accessors"][xb["indices"]]["count"] if "indices" in xb else None
            counts = f"vertices {va}/{vb}, indices {'none' if ia_ is None else ia_}/{'none' if ib_ is None else ib_}"
            if va != vb or ia_ != ib_:
                print(f"{label}: {counts}, counts differ")
                cmp.fail(f"{label} counts differ ({counts})")
                continue
            d = _max_abs_diff(accessor(ga, ba, xa["attributes"]["POSITION"]),
                              accessor(gb, bb, xb["attributes"]["POSITION"]))
            if ia_ is None:
                same_idx = True
                idx_text = "no indices"
            else:
                same_idx = accessor(ga, ba, xa["indices"]) == accessor(gb, bb, xb["indices"])
                idx_text = "indices equal" if same_idx else "indices differ"
            print(f"{label}: {counts}, largest position difference {_fmt_diff(d)}, {idx_text}")
            cmp.number(d, f"{label} positions")
            if not same_idx:
                cmp.fail(f"{label} indices differ")

    def anim_names(gj):
        return ", ".join(a.get("name", f"animation{i}") for i, a in enumerate(gj.get("animations", []))) or "none"

    print(f"animations (not compared): a: {anim_names(ga)}; b: {anim_names(gb)}")

    if cmp.first is not None:
        print(f"glb compare: differs: {cmp.first}")
        return 1
    if cmp.largest == 0:
        print("glb compare: identical")
    else:
        print(f"glb compare: within {tolerance:g}")
    return 0


def _build_parser():
    parser = argparse.ArgumentParser(
        prog="glb.py", description="Read glTF binary (.glb) files. Read only; standard library only.",
        epilog="Exit status: 0 success (compare: identical or within the tolerance), "
               "1 compare found a difference, 2 bad usage or a file that isn't a readable glb.")
    sub = parser.add_subparsers(dest="command", required=True, metavar="{info,compare}")
    p_info = sub.add_parser("info", help="list a glb's materials, meshes, skins and animations",
                            description="List a glb's generator, materials, meshes with triangle counts, skins "
                                        "with their joints, nodes with a mesh or skin, and animations "
                                        "(duration, channels, key counts, interpolation).")
    p_info.add_argument("file", metavar="file.glb")
    p_cmp = sub.add_parser("compare", help="check that a rebuild kept a model's rig, materials and rest mesh",
                           description="Compare two glbs' material names, skin joints, joint transforms, inverse "
                                       "bind matrices and rest meshes (counts, POSITION, indices). Animations are "
                                       "listed, not compared. The last line is 'glb compare: identical', "
                                       "'glb compare: within T' or 'glb compare: differs'.")
    p_cmp.add_argument("a", metavar="a.glb")
    p_cmp.add_argument("b", metavar="b.glb")
    p_cmp.add_argument("--tolerance", type=float, default=0.0, metavar="T",
                       help="largest numeric difference still counted as the same (default 0)")
    return parser


def main(argv=None):
    args = _build_parser().parse_args(argv)
    try:
        if args.command == "info":
            return cmd_info(args.file)
        if args.tolerance < 0:
            print("glb.py: --tolerance must not be negative", file=sys.stderr)
            return 2
        return cmd_compare(args.a, args.b, args.tolerance)
    except (GlbError, KeyError, IndexError, TypeError, struct.error) as e:
        what = e if isinstance(e, GlbError) else f"malformed glb ({type(e).__name__}: {e})"
        print(f"glb.py: {what}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
