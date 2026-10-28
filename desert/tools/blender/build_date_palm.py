"""Build the desert's date palm and export desert/assets/scenes/date_palm.glb.

Run headless (builds, checks, exports, then renders the previews beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_date_palm.py
    PXE_DESERT_OUT=/some/dir ... writes the glb and previews there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.

Or step by step in an open Blender, to watch it go together:
    import runpy; dp = runpy.run_path(".../build_date_palm.py", run_name="date_palm")
    b = dp["Builder"](); b.start(); b.trunk(); b.crown(); b.fronds(); b.dates()
    dp["check"](b)
Each step adds its pieces to the Date_Palm collection of the current scene,
leaving the rest of the file alone.

Real scale, 1 unit = 1 metre (desert/docs/art-style.md). The trunk is about
11 m tall and bows gently toward Blender -X (engine -X); its base stands
vertical so the collision cylinder fits it. The trunk is a stack of short
7-sided cups, each wider at its top than its foot and turned half a side
from the one below, so the cut frond stubs read as stepped, scaly rings.
A faceted knob caps it, and 18 fronds (3-4 m) leave the knob in three
tiers: upright inner ones, spreading middle ones, and drooping outer ones.
Each frond is a thin closed rib (a three-sided prism) with a row of flat
triangular blades either side. Meshes are drawn back-face culled, so every
blade is two-sided: a triangle and a reversed copy on its own vertices.
Three blocky date clusters (stacked boxes on a stalk) hang below the crown.

Objects (node names reaching the game), all with identity transforms so
their shared origin is the trunk base centre at ground level:
    Palm_Trunk   Bark   trunk, ring stubs and crown knob (one closed volume)
    Palm_Fronds  Leaf   18 fronds
    Palm_Dates   Bark   3 stalks with their date clusters
The trunk runs 0.5 m below the origin so it stays planted on a sloping dune.

Axes: Blender +Z up, exported +Y up; Blender -Y is engine +Z. The build
prints the measurements the game needs in engine coordinates.
"""

import json
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_DESERT_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "scenes"))
OUT = os.path.join(OUT_DIR, "date_palm.glb")
PREVIEW = os.path.join(OUT_DIR, "date_palm.png")
PREVIEW_EYE = os.path.join(OUT_DIR, "date_palm_eye.png")
PREVIEW_BELOW = os.path.join(OUT_DIR, "date_palm_below.png")
COLLECTION = "Date_Palm"


def srgb_hex(h):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin((h >> 16) & 255), lin((h >> 8) & 255), lin(h & 255), 1.0)


# Preview colours only: the engine colours static meshes from its palettes by
# material name. These are its day mid tones for Bark and Leaf.
MATERIALS = {"Bark": srgb_hex(0x8a8684), "Leaf": srgb_hex(0x68905a)}
OBJECTS = {"Palm_Trunk": "Bark", "Palm_Fronds": "Leaf", "Palm_Dates": "Bark"}

TRI_BUDGET = (2600, 3300)  # the task: about 3,000

# Trunk
SIDES = 7
TRUNK_H = 11.0       # height of the trunk's top (the knob's foot) above the origin
LEAN = 0.9           # the top sits this far toward -X; offset grows as (z/H)^LEAN_POW
LEAN_POW = 1.8
SINK = 0.5           # the trunk runs this far below the origin
BASE_R = 0.40        # circumradius at the ground: the collision radius
FLARE_TOP = 0.6      # the base flare narrows to the trunk by this height
R_FOOT, R_TOP = 0.32, 0.25   # a ring's foot radius at the bottom and the top of the trunk
LIP_FOOT, LIP_TOP = 0.05, 0.10  # how far a ring's top stands out past its foot
RINGS = 26
# Crown knob above the trunk's top, along its last tangent: (rise, radius).
KNOB = [(0.35, 0.44), (0.80, 0.36), (1.10, 0.16)]

# Fronds: tiers of (count, base rise on the knob, elevation at the base
# (degrees), droop over the length (degrees), length (m), azimuth offset).
TIERS = [
    (5, 0.95, 68.0, 38.0, 3.1, 0.0),
    (7, 0.70, 32.0, 62.0, 3.6, 0.45),
    (6, 0.45, 8.0, 88.0, 3.9, 0.2),
]
RIB_SEGS = 6
RIB_W0, RIB_W1 = 0.06, 0.015   # rib half-width at the base and the tip
RIB_H0, RIB_H1 = 0.05, 0.012   # rib half-height
BLADES = 15                    # per side
BLADE_FROM, BLADE_TO = 0.17, 0.98   # the blades run over this fraction of the frond
BLADE_FWD = math.radians(38.0)  # blade angle from the rib, toward the tip
BLADE_V = math.radians(22.0)    # blades rise out of the frond plane: a V
BLADE_FOOT = 0.75               # a blade's foot spans this fraction of its slot on the rib

# Dates: azimuths (degrees), how far the stalk reaches out and down.
DATES = [(40.0, 0.75, 1.0), (165.0, 0.65, 0.9), (285.0, 0.8, 1.1)]


def frame_from(t):
    """Two unit vectors perpendicular to t."""
    t = t.normalized()
    a = Vector((0, 1, 0)) if abs(t.y) < 0.9 else Vector((1, 0, 0))
    u = a.cross(t).normalized()
    v = t.cross(u).normalized()
    return u, v


def spine(z):
    """The trunk's centre line at height z (z may be below 0 in the sunk part)."""
    f = max(z, 0.0) / TRUNK_H
    return Vector((-LEAN * f ** LEAN_POW, 0.0, z))


def spine_tangent(z):
    f = max(z, 0.0) / TRUNK_H
    dx = -LEAN * LEAN_POW * f ** (LEAN_POW - 1.0) / TRUNK_H if z > 0 else 0.0
    return Vector((dx, 0.0, 1.0)).normalized()


def lerp(a, b, t):
    return a + (b - a) * t


class Builder:
    def __init__(self):
        self.rng = random.Random(11)
        self.objects = {}
        self.top = None       # trunk's top point and tangent, set by trunk()
        self.top_t = None

    # --- setup -------------------------------------------------------------
    def start(self):
        scene = bpy.context.scene
        coll = bpy.data.collections.get(COLLECTION)
        if coll is None:
            coll = bpy.data.collections.new(COLLECTION)
        if coll.name not in scene.collection.children:
            scene.collection.children.link(coll)
        self.coll = coll
        self.mats = {}
        for name, col in MATERIALS.items():
            m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
            m.diffuse_color = col
            bsdf = m.node_tree.nodes.get("Principled BSDF") if m.node_tree else None
            if bsdf:
                bsdf.inputs["Base Color"].default_value = col
                bsdf.inputs["Roughness"].default_value = 1.0
            self.mats[name] = m

    def _object(self, name, bm):
        mesh = bpy.data.meshes.new(name)
        bm.to_mesh(mesh)
        bm.free()
        for p in mesh.polygons:
            p.use_smooth = False
        mesh.materials.append(self.mats[OBJECTS[name]])
        obj = bpy.data.objects.new(name, mesh)
        self.coll.objects.link(obj)
        self.objects[name] = obj
        return obj

    # --- trunk -------------------------------------------------------------
    def _ring(self, bm, centre, t, radius, offset):
        u, v = frame_from(t)
        return [bm.verts.new(centre + (u * math.cos(offset + 2 * math.pi * i / SIDES)
                                       + v * math.sin(offset + 2 * math.pi * i / SIDES)) * radius)
                for i in range(SIDES)]

    @staticmethod
    def _band(bm, lo, hi):
        """Quads between two rings at the same angular offset."""
        n = len(lo)
        for i in range(n):
            bm.faces.new((lo[i], lo[(i + 1) % n], hi[(i + 1) % n], hi[i]))

    @staticmethod
    def _twisted_band(bm, a, b):
        """Triangles between ring a and ring b, b turned half a side from a."""
        n = len(a)
        for i in range(n):
            bm.faces.new((a[i], a[(i + 1) % n], b[i]))
            bm.faces.new((b[i], a[(i + 1) % n], b[(i + 1) % n]))

    def trunk(self):
        bm = bmesh.new()
        up = Vector((0, 0, 1))
        step = math.pi / SIDES
        # Sunk part and flare, vertical: rings at -SINK, 0 and FLARE_TOP.
        bottom = self._ring(bm, spine(-SINK), up, BASE_R, 0.0)
        ground = self._ring(bm, spine(0.0), up, BASE_R, 0.0)
        self.ground_ring = [v.co.copy() for v in ground]
        self._band(bm, bottom, ground)
        bm.faces.new(list(reversed(bottom)))
        z0 = FLARE_TOP
        prev = self._ring(bm, spine(z0), spine_tangent(z0), R_FOOT, 0.0)
        self._band(bm, ground, prev)
        # Stepped rings: each a cup from its foot radius up to foot + lip, then a
        # step in to the next ring's foot, turned half a side.
        dz = (TRUNK_H - z0) / RINGS
        offset = 0.0
        for k in range(RINGS):
            za, zb = z0 + dz * k, z0 + dz * (k + 1)
            f = zb / TRUNK_H
            r_top = lerp(R_FOOT, R_TOP, f) + lerp(LIP_FOOT, LIP_TOP, f)
            lip = self._ring(bm, spine(zb), spine_tangent(zb), r_top, offset)
            self._band(bm, prev, lip)
            offset += step
            r_next = lerp(R_FOOT, R_TOP, f)
            nxt = self._ring(bm, spine(zb), spine_tangent(zb), r_next, offset)
            self._twisted_band(bm, lip, nxt)
            prev = nxt
        self.top = spine(TRUNK_H)
        self.top_t = spine_tangent(TRUNK_H)
        self.trunk_bm = bm
        self.trunk_last = prev
        self.trunk_offset = offset
        return bm

    def crown(self):
        """The knob on the trunk's top; closes the trunk into one volume."""
        bm = self.trunk_bm
        prev = self.trunk_last
        for rise, radius in KNOB:
            ring = self._ring(bm, self.top + self.top_t * rise, self.top_t, radius, self.trunk_offset)
            self._band(bm, prev, ring)
            prev = ring
        bm.faces.new(prev)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        self._object("Palm_Trunk", bm)
        self.trunk_bm = None

    # --- fronds ------------------------------------------------------------
    def _frond(self, bm, base, az, e0, droop, length, roll):
        h = Vector((math.cos(az), math.sin(az), 0.0))
        side = Vector((-math.sin(az), math.cos(az), 0.0))

        def elev(s):
            return math.radians(e0 - droop * (s / length) ** 1.5)

        def tangent(s):
            e = elev(s)
            return (h * math.cos(e) + Vector((0, 0, math.sin(e)))).normalized()

        # Rib stations, integrated finely along the drooping curve.
        fine = 60
        pts = [base.copy()]
        p = base.copy()
        for i in range(fine):
            s = length * (i + 0.5) / fine
            p = p + tangent(s) * (length / fine)
            pts.append(p.copy())

        def at(s):
            x = max(0.0, min(1.0, s / length)) * fine
            i = min(int(x), fine - 1)
            return lerp(pts[i], pts[i + 1], x - i)

        stations = [length * i / RIB_SEGS for i in range(RIB_SEGS + 1)]
        coarse = [at(s) for s in stations]

        def on_rib(s):
            x = max(0.0, min(1.0, s / length)) * RIB_SEGS
            i = min(int(x), RIB_SEGS - 1)
            return lerp(coarse[i], coarse[i + 1], x - i)

        def normal(s):
            n = tangent(s).cross(side).normalized()
            # A small roll about the rib so the crown isn't too regular.
            return (n * math.cos(roll) + side * math.sin(roll)).normalized()

        def sidev(s):
            return normal(s).cross(tangent(s)).normalized() * -1.0

        # Rib: three-sided closed prism, closing to a point at the tip.
        rib_faces = []
        rings = []
        for i, s in enumerate(stations[:-1]):
            f = s / length
            w, hh = lerp(RIB_W0, RIB_W1, f), lerp(RIB_H0, RIB_H1, f)
            n, sd, c = normal(s), sidev(s), coarse[i]
            rings.append([bm.verts.new(c + n * hh), bm.verts.new(c - sd * w - n * hh * 0.4),
                          bm.verts.new(c + sd * w - n * hh * 0.4)])
        tip = bm.verts.new(coarse[-1])
        rib_faces.append(bm.faces.new(rings[0]))
        for a, b in zip(rings, rings[1:]):
            for j in range(3):
                rib_faces.append(bm.faces.new((a[j], a[(j + 1) % 3], b[(j + 1) % 3], b[j])))
        for j in range(3):
            rib_faces.append(bm.faces.new((rings[-1][j], rings[-1][(j + 1) % 3], tip)))
        bmesh.ops.recalc_face_normals(bm, faces=rib_faces)

        # Blades: flat triangles either side, each with a reversed copy.
        s0, s1 = BLADE_FROM * length, BLADE_TO * length
        slot = (s1 - s0) / BLADES
        for sgn in (-1.0, 1.0):
            for j in range(BLADES):
                sa = s0 + slot * j
                sb = sa + slot * BLADE_FOOT
                u = (j + 0.5) / BLADES
                blen = (0.28 + 0.62 * math.sin(math.pi * (0.12 + 0.82 * u))) * (length / 3.6)
                blen *= 1.0 + 0.12 * (self.rng.random() - 0.5)
                sm = 0.5 * (sa + sb)
                t, n, sd = tangent(sm), normal(sm), sidev(sm)
                w = lerp(RIB_W0, RIB_W1, sm / length) * 0.7
                dirv = (t * math.cos(BLADE_FWD)
                        + (sd * sgn * math.cos(BLADE_V) + n * math.sin(BLADE_V)) * math.sin(BLADE_FWD)).normalized()
                a = on_rib(sa) + sidev(sa) * sgn * w
                b = on_rib(sb) + sidev(sb) * sgn * w
                c = on_rib(sm) + sd * sgn * w + dirv * blen
                tri = [a, b, c]
                if (b - a).cross(c - a).dot(n) < 0:
                    tri = [a, c, b]
                bm.faces.new([bm.verts.new(q) for q in tri])            # faces the frond's top
                bm.faces.new([bm.verts.new(q) for q in reversed(tri)])  # faces its underside

    def fronds(self):
        bm = bmesh.new()
        self.frond_count = 0
        for count, rise, e0, droop, length, az_off in TIERS:
            for k in range(count):
                az = 2 * math.pi * (k + az_off) / count + self.rng.uniform(-0.12, 0.12)
                h = Vector((math.cos(az), math.sin(az), 0.0))
                base = self.top + self.top_t * rise + h * 0.18
                self._frond(bm, base, az,
                            e0 + self.rng.uniform(-5.0, 5.0),
                            droop + self.rng.uniform(-6.0, 6.0),
                            length + self.rng.uniform(-0.12, 0.12),
                            self.rng.uniform(-0.2, 0.2))
                self.frond_count += 1
        self._object("Palm_Fronds", bm)

    # --- dates -------------------------------------------------------------
    def dates(self):
        bm = bmesh.new()
        for az_deg, reach, drop in DATES:
            az = math.radians(az_deg)
            h = Vector((math.cos(az), math.sin(az), 0.0))
            start = self.top + self.top_t * 0.3 + h * 0.3
            pts = [start + h * reach * s + Vector((0, 0, 0.12 * s - drop * s * s)) for s in (0.0, 0.35, 0.7, 1.0)]
            # Stalk: a four-sided prism, 0.05 m square.
            rings = []
            for i, p in enumerate(pts):
                t = (pts[min(i + 1, 3)] - pts[max(i - 1, 0)]).normalized()
                u, v = frame_from(t)
                rings.append([bm.verts.new(p + (u * math.cos(a) + v * math.sin(a)) * 0.035)
                              for a in (math.pi * (0.25 + 0.5 * q) for q in range(4))])
            faces = [bm.faces.new(list(reversed(rings[0]))), bm.faces.new(rings[-1])]
            for a, b in zip(rings, rings[1:]):
                for j in range(4):
                    faces.append(bm.faces.new((a[j], a[(j + 1) % 4], b[(j + 1) % 4], b[j])))
            bmesh.ops.recalc_face_normals(bm, faces=faces)
            # Cluster: four stacked boxes, narrowing downward, each turned a little.
            end = pts[-1]
            z = end.z + 0.05
            for k, (sx, sy, sz) in enumerate([(0.42, 0.36, 0.26), (0.38, 0.34, 0.24),
                                              (0.30, 0.28, 0.22), (0.20, 0.18, 0.16)]):
                z -= sz * 0.85
                rot = self.rng.uniform(-0.5, 0.5) + az
                c = Vector((end.x + self.rng.uniform(-0.04, 0.04), end.y + self.rng.uniform(-0.04, 0.04), z))
                m = Matrix.Translation(c) @ Matrix.Rotation(rot, 4, "Z") @ Matrix.Diagonal((sx, sy, sz, 1.0))
                bmesh.ops.create_cube(bm, size=1.0, matrix=m)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        self._object("Palm_Dates", bm)

    def build_all(self):
        self.start()
        self.trunk()
        self.crown()
        self.fronds()
        self.dates()


# --- checks and measurements ------------------------------------------------
def to_engine(v):
    return (v[0], v[2], -v[1])


def triangles(obj):
    obj.data.calc_loop_triangles()
    return len(obj.data.loop_triangles)


def measure(b):
    objs = list(b.objects.values())
    allv = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    fr = b.objects["Palm_Fronds"]
    frv = [v.co for v in fr.data.vertices]

    def box(vs):
        e = [to_engine(v) for v in vs]
        return ([round(min(p[i] for p in e), 3) for i in range(3)],
                [round(max(p[i] for p in e), 3) for i in range(3)])

    ax = b.top  # crown axis: the trunk's top
    crown_r = max(math.hypot(v.x - ax.x, v.y - ax.y) for v in frv)
    ground = b.ground_ring
    return {
        "trunk_base_radius": round(max(math.hypot(v.x, v.y) for v in ground), 3),
        "trunk_base_inradius": round(BASE_R * math.cos(math.pi / SIDES), 3),
        "height": round(max(v.z for v in allv), 3),
        "bounds_engine": box(allv),
        "crown_bounds_engine": box(frv),
        "crown_centre_engine": [round(c, 3) for c in to_engine(ax)],
        "crown_radius_from_axis": round(crown_r, 3),
        "crown_lowest_y": round(min(v.z for v in frv), 3),
        "trunk_top_engine": [round(c, 3) for c in to_engine(b.top)],
    }


def check(b):
    problems = []
    names = sorted(o.name for o in b.objects.values())
    if names != sorted(OBJECTS):
        problems.append(f"object names {names} are not {sorted(OBJECTS)}")
    all_names = [o.name for o in b.coll.objects]
    if len(all_names) != len(set(all_names)) or any("." in n for n in all_names):
        problems.append(f"object names not unique / clean: {all_names}")
    mats = set()
    for o in b.objects.values():
        ms = [m.name for m in o.data.materials]
        mats.update(ms)
        if ms != [OBJECTS[o.name]]:
            problems.append(f"{o.name} has materials {ms}, wants [{OBJECTS[o.name]}]")
        if any(abs(o.matrix_world[i][j] - (1.0 if i == j else 0.0)) > 1e-6 for i in range(4) for j in range(4)):
            problems.append(f"{o.name} has a transform; the origin must be the shared base")
    if mats != {"Bark", "Leaf"}:
        problems.append(f"materials {sorted(mats)} are not Bark and Leaf")
    tris = sum(triangles(o) for o in b.objects.values())
    if not TRI_BUDGET[0] <= tris <= TRI_BUDGET[1]:
        problems.append(f"{tris} triangles, outside {TRI_BUDGET}")
    # Origin: the trunk's ground ring is centred on the origin at z = 0.
    g = b.ground_ring
    c = sum(g, Vector()) / len(g)
    if c.length > 1e-4 or any(abs(v.z) > 1e-6 for v in g):
        problems.append(f"trunk base centre {tuple(c)} is not the origin")
    tr = b.objects["Palm_Trunk"].data
    lowest_ring = min(v.co.z for v in tr.vertices)
    if abs(lowest_ring + SINK) > 1e-6:
        problems.append(f"trunk bottom at {lowest_ring}, wants {-SINK}")
    # Closed volumes: trunk and dates are manifold.
    for name in ("Palm_Trunk", "Palm_Dates"):
        bm = bmesh.new()
        bm.from_mesh(b.objects[name].data)
        if not all(e.is_manifold for e in bm.edges):
            problems.append(f"{name} is not closed")
        bm.free()
    # Fronds: every blade has a reversed twin; every rib edge is manifold.
    bm = bmesh.new()
    bm.from_mesh(b.objects["Palm_Fronds"].data)
    singles = [f for f in bm.faces if len(f.verts) == 3 and all(len(v.link_faces) == 1 for v in f.verts)]
    by_centre = {}
    for f in singles:
        key = tuple(sorted(tuple(round(x, 5) for x in v.co) for v in f.verts))
        by_centre.setdefault(key, []).append(f.normal.copy())
    unpaired = sum(1 for ns in by_centre.values() if len(ns) != 2 or ns[0].dot(ns[1]) > -0.999)
    blades = len(singles)
    if unpaired or blades != b.frond_count * BLADES * 2 * 2:
        problems.append(f"{unpaired} blades lack a reversed twin ({blades} blade faces)")
    ribs = [e for e in bm.edges if len(e.verts[0].link_faces) > 1]
    if not all(e.is_manifold for e in ribs):
        problems.append("a frond rib is not closed")
    bm.free()
    if b.frond_count != 18:
        problems.append(f"{b.frond_count} fronds, wants 18")
    m = measure(b)
    m["triangles"] = tris
    m["triangles_by_object"] = {o.name: triangles(o) for o in b.objects.values()}
    m["fronds"] = b.frond_count
    return problems, m


# --- export and previews ----------------------------------------------------
def export():
    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_yup=True,
        export_normals=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_animations=False,
        export_skins=False,
        export_apply=True,
    )
    print("wrote", OUT)


def render_previews(b):
    """Three previews, Workbench with back faces culled as the engine draws:
    date_palm.png        three-quarter view from above eye height
    date_palm_eye.png    from 1.7 m, 14 m south (Blender +Y, engine -Z)
    date_palm_below.png  standing under the crown, looking up at it
    A sand-coloured ground plane is added for the previews only, after export."""
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    sh = scene.display.shading
    sh.light = "STUDIO"
    sh.studio_light = "outdoor.sl"
    sh.color_type = "MATERIAL"
    sh.show_backface_culling = True
    sh.show_object_outline = True
    sh.show_shadows = False
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("preview")
    world.color = srgb_hex(0x9cc0dc)[:3]
    scene.world = world
    sand = bpy.data.materials.new("Preview_Sand")
    sand.diffuse_color = srgb_hex(0xd6b076)
    gm = bpy.data.meshes.new("Preview_Ground")
    gm.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    gm.materials.append(sand)
    ground = bpy.data.objects.new("Preview_Ground", gm)
    scene.collection.objects.link(ground)
    cam_data = bpy.data.cameras.new("Preview_Camera")
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle_y = math.radians(60.0)
    cam_data.clip_end = 400.0
    cam = bpy.data.objects.new("Preview_Camera", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam

    def shoot(path, eye, target, res):
        scene.render.resolution_x, scene.render.resolution_y = res
        cam.location = Vector(eye)
        cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("wrote", path)

    top = b.top
    shoot(PREVIEW, (11.0, 15.0, 7.5), (top.x * 0.6, 0.0, 7.0), (720, 900))
    shoot(PREVIEW_EYE, (0.0, 14.0, 1.7), (0.0, 0.0, 7.0), (960, 540))
    shoot(PREVIEW_BELOW, (1.6, 1.8, 1.7), (top.x - 0.4, -0.3, top.z + 0.6), (960, 540))
    for o in (ground, cam):
        bpy.data.objects.remove(o)
    bpy.data.meshes.remove(gm)
    bpy.data.cameras.remove(cam_data)
    bpy.data.materials.remove(sand)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = Builder()
    b.build_all()
    problems, m = check(b)
    print("date_palm:", json.dumps(m))
    if problems:
        raise RuntimeError("the date palm fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_previews(b)


if __name__ == "__main__" and bpy.app.background:
    main()
