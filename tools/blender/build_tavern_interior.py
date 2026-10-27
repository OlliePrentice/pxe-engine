"""Build the inside of the Sleeping Fox - one room, seen in a cutaway - and
export assets/scenes/tavern_interior.glb.

Run headless (builds, checks, exports, then renders the previews beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_interior.py
    PXE_TAVERN_OUT=/some/dir ... writes the glb and previews there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.

Or step by step in an open Blender, to watch it go together:
    import runpy; ti = runpy.run_path(".../build_tavern_interior.py", run_name="tavern")
    b = ti["Builder"](); b.start(); b.floor(); b.walls(); ...  (see Builder.STEPS)
Each step adds its pieces to the Tavern_Interior collection, leaving the rest
of the scene alone.

The room: 22.6 x 17 m inside at hero scale (1.6x real), 4.6 m to the wall
plate. It is designed for one camera yaw, the engine's default 45 degrees:
the camera looks toward engine +x and +z (Blender +x and -y), 30 degrees
down. The two walls nearest that camera - engine -x and -z (Blender -x and
+y) - and the roof are shadow-only (`_ShadowOnly` on the object, so on the
glTF node): they roof and wall the room for the sun and moon, but the camera
never draws them. Low plaster stubs (0.45 m) along their base are drawn, so
the room's outline reads.

The two far walls are drawn: engine +z (Blender -y), the back wall, holds
the stone hearth and three windows; engine +x (Blender +x) holds the doorway
for the inside door, one window, and the bar. Every window is a real hole,
crossed by a Wood mullion and transom so the light falls in four panes.
Posts and rails stand on the walls only: nothing crosses over the room, so
nothing hides the floor from the camera.

Contents, kept roomy: a wide-plank Wood floor (V-grooved, so the planks
read); a bar along the door wall with six stools and back shelves (bottles,
mugs); five barrels; the hearth; a small raised corner stage (empty); a
standing brazier; six tables - three square with four chairs, one square
with two, one round with three, one long trestle with two benches - and a
candle on each table and one on the bar. Furniture is sized at 1.6x real:
tables 1.2 m high, chair and bench seats 0.74 m, bar 1.7 m, stools 1.18 m.

The build checks the layout rules and fails if one breaks: every gap between
furniture groups (a table with its seats, the bar with its stools, the
shelves, the barrels, the hearth, the stage, the brazier) and to any wall a
group does not stand against is >= WALKWAY (2.0 m), and a PATH-wide (2.5 m)
disc swept from the hero's spawn inside the door reaches the bar, the hearth
and every table. It also checks the room fits the game's 40 x 22.5 m view at
the design yaw.

Each chair, stool, bench, table, barrel and candle is its own object
(Chair_A1, Stool_3, Bench_E1, ...), so it can be found by name; the build
prints every figure assets.md quotes, in engine coordinates.

Axes and origin (Blender): the floor's centre, its top at z = 0. Exported
+Y up: engine (x, y, z) = Blender (x, z, -y). Static, flat-shaded.

Material names are the contract with the engine: Wood, Oak, Plaster,
Stone, Dirt, and Roof on the shadow-only roof only. (The brazier's iron is
Dirt and its bowl Stone; the sconces' iron and dishes are Stone.)

Revision 3: the furniture is light Oak (the bar, every table, the trestle
and its benches, every chair and stool, the back shelves, the barrels' wood,
the stage's lip and the hearth's mantel); the floor, wall timbers, frames,
sill and skirting stay Wood. Four wall sconces (Sconce_1..4) hang on posts
of the drawn walls, their flames at 2.6 m. Nothing else moved.
"""

import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_TAVERN_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "scenes"))
OUT = os.path.join(OUT_DIR, "tavern_interior.glb")
PREVIEW = os.path.join(OUT_DIR, "tavern_interior.png")
OVERVIEW = os.path.join(OUT_DIR, "tavern_interior_overview.png")
COLLECTION = "Tavern_Interior"
SHADOW_ONLY = "_ShadowOnly"


def srgb(r, g, b):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b))


# Preview colours only (the engine colours static scenes by material name);
# the same as build_tavern_exterior.py.
MATERIALS = {
    "Wood": srgb(118, 76, 44),
    "Oak": srgb(180, 140, 90),       # light oak: the furniture (revision 3)
    "Plaster": srgb(230, 216, 184),
    "Roof": srgb(90, 102, 118),      # slate, as the exterior (shadow-only here)
    "Stone": (0.50, 0.42, 0.30),
    "Dirt": (0.25, 0.22, 0.19),
}


def material(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    colour = MATERIALS[name]
    mat.diffuse_color = (*colour, 1.0)
    if bpy.app.version < (5, 0, 0):
        mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.node_tree else None
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
        bsdf.inputs["Roughness"].default_value = 1.0
    return mat


# --------------------------------------------------------------------------
# Layout (Blender metres). Engine (x, z) = Blender (x, -y).

RX, RY = 11.3, 8.5         # inner half-extents: x in [-11.3, 11.3], y in [-8.5, 8.5]
WALL_T = 0.4
WALL_H = 4.6
STUB_H = 0.45              # the cutaway walls' drawn stubs
DESIGN_YAW = 45.0          # engine camera yaw the room is built for (degrees)
VIEW = (40.0, 22.5)        # the game's view at 24 px/m (960 x 540), metres
RAILS = [(0.0, 0.22), (1.30, 1.50), (3.20, 3.40), (4.32, 4.60)]  # skirting, low rail, high rail, wall plate
RAIL_PROUD = 0.08
POST_PROUD = 0.12
POST_W = 0.25
WIN_Z = (1.50, 3.20)
DOOR_W, DOOR_H, JAMB, HEAD = 2.2, 3.4, 0.2, 0.25
DOOR_Y = 1.1               # the doorway's centre along the +x wall
# Window openings: (s0, s1) along each wall's run (x for the y walls, y for
# the x walls). Only the drawn walls ("posx", "posz") have windows: four in
# all, on the walls the camera sees.
WINDOWS = {
    "posz": [(-10.6, -9.2), (0.95, 2.35), (4.0, 5.4)],
    "posx": [(5.9, 7.3)],
    "negx": [],
    "negz": [],
}
HEARTH_X = -4.2            # centre of the hearth on the back (+z engine) wall
FIRE_W = 2.2               # firebox opening
# The bar runs along the door wall (+x), its far end against the back wall:
# a frame on that wall, s along it (Blender y) and d out into the room (-x).
BAR_RUN = (-8.5, -2.6)     # s0, s1 of the bar body and the back shelves
BAR_D = (2.8, 3.7)         # the body's depth out from the wall
BAR_TOP = 1.7
SHELF_D = 0.6
STOOL_D = 4.25             # stools' centres out from the wall
STOOLS_S = [-7.8, -6.9, -6.0, -5.1, -4.2, -3.3]
BAR_CANDLE = (-5.55, 3.3)  # (s, d)
TABLE_H = 1.2
TABLES = [  # name, centre (x, y), kind, chair sides (+x, -x, +y, -y)
    ("A", (-7.3, -1.9), "square", ("+x", "-x", "+y", "-y")),
    ("B", (-2.2, -1.9), "square", ("+y", "-y")),
    ("C", (2.55, -1.9), "square", ("+x", "-x", "+y", "-y")),
    ("D", (-7.3, 4.1), "square", ("+x", "-x", "+y", "-y")),
    ("E", (-1.6, 4.1), "trestle", ()),
    ("F", (4.2, 4.1), "round", ("+x", "-x", "+y")),
]
TRESTLE = (3.8, 1.1)       # the trestle table's top, length (along x) and width
BENCH = (3.6, 0.45, 0.25)  # bench length, depth, and the gap to the table's edge
BARRELS = [(0.6, -7.85), (1.65, -7.85), (2.7, -7.85), (1.1, -6.95), (2.2, -6.95)]
BRAZIER = (8.6, 4.4)
STAGE = [(-RX, -RY), (-8.5, -RY), (-8.5, -6.9), (-9.5, -5.9), (-RX, -5.9)]  # counter-clockwise
STAGE_H = 0.72
SPAWN = (8.7, DOOR_Y)      # hero spawn inside the door (Blender xy); faces -x
TRI_BUDGET = 20000
WALKWAY = 2.0              # every gap between furniture groups, walls, stage and hearth
PATH = 2.5                 # clear path width from the door to the bar, hearth and every table
TOUCH = 0.1                # a group this close to a wall stands against it (no walkway)
# Revision 3: four wall sconces, each on a post of a drawn wall (wall, the
# post's centre along the wall's run), clear of the windows, the shelves and
# the hearth: over the stage, over the bar's far end, between the door and
# the +x window, and in the +x wall's corner by the -z cutaway.
SCONCES = [("posz", -11.175), ("posz", 7.408), ("posx", 4.15), ("posx", 8.375)]
SCONCE_FLAME_Z = 2.6       # the flame base, at about 2.6 m (hero scale)
SCONCE_OUT = 0.38          # the dish's centre out from the post's face
SCONCE_CLEAR = 0.3         # checked: from any part to a window, the shelves or the hearth
# Revision 3: the furniture is light Oak. These objects are Oak (some keep
# Dirt or Stone parts), and no other object has Oak. Stage and Hearth keep
# Wood too: the stage's planked top (a floor) and the hearth's logs; only
# the stage's lip and the hearth's mantel are Oak.
OAK_ONLY = ("Bar", "Shelves", "Table_A", "Table_B", "Table_C", "Table_D", "Table_E", "Table_F", "Chair_", "Bench_",
            "Stool_", "Barrel_")
OAK_AND_WOOD = ("Stage", "Hearth")
# These keep Wood and have no Oak: the floor, the wall timbers, the window
# and door frames, the sill, and the cutaway stubs' skirting.
WOOD_KEPT = ("Floor", "Timbers", "Wall_PosX", "Wall_PosZ", "Doorway", "Threshold", "WallStub_NegX", "WallStub_NegZ")


def engine(p):
    """Blender (x, y, z) -> engine (x, y, z): +Y up, +Z = Blender -Y."""
    return (round(p[0], 4), round(p[2], 4), round(-p[1], 4) + 0.0)


def wall_point(wall, s, d, z):
    """A point on a wall: s along its run, d out from its inner face (negative
    into the room), z up."""
    if wall == "posx":
        return (RX + d, s, z)
    if wall == "negx":
        return (-RX - d, s, z)
    if wall == "posz":   # Blender -y
        return (s, -RY - d, z)
    return (s, RY + d, z)  # negz, Blender +y


# Each wall's run along s; the drawn walls own the far corners, and the
# +x wall the corner it shares with the -z cutaway.
RUN = {
    "posz": (-RX - WALL_T, RX + WALL_T),
    "posx": (-RY, RY + WALL_T),
    "negx": (-RY, RY + WALL_T),
    "negz": (-RX, RX),
}


class Part:
    """One object's mesh, built face by face, with its materials."""

    def __init__(self, name):
        self.name = name
        self.mats = []
        self.bm = bmesh.new()

    def mi(self, mat):
        if mat not in self.mats:
            self.mats.append(mat)
        return self.mats.index(mat)

    def face(self, pts, mat, facing=None):
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        if facing is not None:
            f.normal_update()
            if f.normal.dot(Vector(facing)) < 0:
                f.normal_flip()
        f.material_index = self.mi(mat)
        return f

    def hexa(self, c, mat, skip=(), m=None):
        """Six faces over eight corners: 0-3 the bottom (x0y0, x1y0, x1y1,
        x0y1, counter-clockwise from above), 4-7 the top in the same order;
        `m` places them."""
        if m is not None:
            c = [m @ Vector(p) for p in c]
        vs = [self.bm.verts.new(p) for p in c]
        sides = {"-z": (0, 3, 2, 1), "+z": (4, 5, 6, 7), "-y": (0, 1, 5, 4), "+x": (1, 2, 6, 5),
                 "+y": (2, 3, 7, 6), "-x": (3, 0, 4, 7)}
        for key, idx in sides.items():
            if key not in skip:
                self.bm.faces.new([vs[i] for i in idx]).material_index = self.mi(mat)

    def box(self, lo, hi, mat, skip=(), m=None):
        (x0, y0, z0), (x1, y1, z1) = lo, hi
        self.hexa([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                   (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], mat, skip, m)

    def wall_box(self, wall, s0, s1, d0, d1, z0, z1, mat):
        a = wall_point(wall, s0, d0, z0)
        b = wall_point(wall, s1, d1, z1)
        self.box(tuple(min(a[i], b[i]) for i in range(3)), tuple(max(a[i], b[i]) for i in range(3)), mat)

    def prism_z(self, outline, z0, z1, mat, m=None, top=True, bottom=True):
        """A vertical prism over a counter-clockwise outline (x, y)."""
        lo = [Vector((x, y, z0)) for x, y in outline]
        hi = [Vector((x, y, z1)) for x, y in outline]
        if m is not None:
            lo = [m @ p for p in lo]
            hi = [m @ p for p in hi]
        lo = [self.bm.verts.new(p) for p in lo]
        hi = [self.bm.verts.new(p) for p in hi]
        n = len(outline)
        for i in range(n):
            j = (i + 1) % n
            self.bm.faces.new((lo[i], lo[j], hi[j], hi[i])).material_index = self.mi(mat)
        if top:
            self.bm.faces.new(hi).material_index = self.mi(mat)
        if bottom:
            self.bm.faces.new(list(reversed(lo))).material_index = self.mi(mat)

    def lathe(self, profile, sides, centre, mats, phase=0.0):
        """A turned shape round a vertical axis: profile [(z, r)], a material
        for each band between profile rings, capped top and bottom."""
        cx, cy, cz = centre
        rings = []
        for z, r in profile:
            rings.append([self.bm.verts.new((cx + r * math.cos(phase + k * math.tau / sides),
                                             cy + r * math.sin(phase + k * math.tau / sides), cz + z))
                          for k in range(sides)])
        for i in range(len(rings) - 1):
            for k in range(sides):
                j = (k + 1) % sides
                f = self.bm.faces.new((rings[i][k], rings[i][j], rings[i + 1][j], rings[i + 1][k]))
                f.material_index = self.mi(mats[i])
        self.bm.faces.new(list(reversed(rings[0]))).material_index = self.mi(mats[0])
        self.bm.faces.new(rings[-1]).material_index = self.mi(mats[-1])

    def finish(self, coll):
        mesh = bpy.data.meshes.new(self.name)
        self.bm.to_mesh(mesh)
        self.bm.free()
        for m in self.mats:
            mesh.materials.append(material(m))
        for p in mesh.polygons:
            p.use_smooth = False
        mesh.update()
        obj = bpy.data.objects.new(self.name, mesh)
        coll.objects.link(obj)
        return obj


def ngon(sides, r, phase=0.0, cx=0.0, cy=0.0):
    return [(cx + r * math.cos(phase + k * math.tau / sides), cy + r * math.sin(phase + k * math.tau / sides))
            for k in range(sides)]


def place(x, y, yaw=0.0, z=0.0):
    return Matrix.Translation((x, y, z)) @ Matrix.Rotation(yaw, 4, "Z")


class Builder:
    STEPS = ["start", "floor", "walls", "timbers", "doorway", "cutaway", "hearth", "stage", "bar", "shelves",
             "barrels", "tables", "brazier", "candles", "sconces"]

    def __init__(self):
        self.coll = None
        self.flames = []    # candle flame base points (Blender)
        self.seats = {}     # chair/stool/bench name -> (centre xy, radius, rectangle half extents, yaw)
        self.fire = None
        self.brazier_fire = None
        self.posts = {}     # drawn wall -> [(s0, s1)] post spans
        self.sconce_flames = []  # sconce flame base points (Blender)
        # Furniture groups for the walkway checks: a table with its chairs,
        # the bar with its stools, ... Gaps inside a group are not walkways.
        self.groups = {}

    def group(self, key, obj):
        self.groups.setdefault(key, []).append(obj.name)
        return obj

    def start(self):
        """Fresh Tavern_Interior collection; the rest of the scene is left alone."""
        old = bpy.data.collections.get(COLLECTION)
        if old:
            for ob in list(old.objects):
                data = ob.data
                bpy.data.objects.remove(ob)
                if isinstance(data, bpy.types.Mesh):
                    bpy.data.meshes.remove(data)
            bpy.data.collections.remove(old)
        self.coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(self.coll)
        for name in MATERIALS:
            material(name)

    # ---- shell ------------------------------------------------------------

    def floor(self):
        """Wide planks (0.5 m, 0.3 m real) running along x, each length a
        low frustum whose edges are bevelled into V-grooves, so every plank
        and every staggered butt joint reads. Plus the threshold through the
        doorway and a dirt step outside it."""
        part = Part("Floor")
        width = 0.5
        bevel, depth = 0.04, 0.048
        rows = int(round(2 * RY / width))
        lengths = [4.2, 3.6, 4.8, 3.9]
        for r in range(rows):
            y0 = -RY + r * width
            y1 = y0 + width
            x = -RX
            cut = -RX + (1.3 + 1.7 * ((r * 7) % 5) / 4.0)  # first joint, staggered row by row
            k = r
            while x < RX - 1e-6:
                x1 = min(RX, cut)
                if RX - x1 < 0.8:
                    x1 = RX
                lo = [(x, y0, -depth), (x1, y0, -depth), (x1, y1, -depth), (x, y1, -depth)]
                hi = [(x + bevel, y0 + bevel, 0.0), (x1 - bevel, y0 + bevel, 0.0), (x1 - bevel, y1 - bevel, 0.0),
                      (x + bevel, y1 - bevel, 0.0)]
                part.hexa(lo + hi, "Wood", skip=("-z",))
                x = x1
                cut = x + lengths[k % len(lengths)]
                k += 1
        part.finish(self.coll)

        sill = Part("Threshold")
        sill.box((RX, DOOR_Y - DOOR_W / 2, -0.12), (RX + WALL_T, DOOR_Y + DOOR_W / 2, 0.0), "Wood")
        sill.finish(self.coll)
        step = Part("DoorStep")
        step.box((RX + WALL_T, DOOR_Y - 1.5, -0.2), (RX + WALL_T + 2.4, DOOR_Y + 1.5, -0.01), "Dirt")
        step.finish(self.coll)

    def _wall(self, part, wall, holes, z_top=WALL_H, z_bottom=-0.1):
        """Plaster over the wall's run from z_bottom to z_top, less the holes
        (s0, s1, z0, z1)."""
        s_lo, s_hi = RUN[wall]
        cuts = sorted({s_lo, s_hi} | {h[0] for h in holes} | {h[1] for h in holes})
        for a, b in zip(cuts, cuts[1:]):
            if b - a < 1e-6:
                continue
            mid = (a + b) / 2
            spans = [(z_bottom, z_top)]
            for h0, h1, z0, z1 in holes:
                if h0 <= mid <= h1:
                    new = []
                    for lo, hi in spans:
                        if z1 <= lo or z0 >= hi:
                            new.append((lo, hi))
                            continue
                        if z0 > lo:
                            new.append((lo, z0))
                        if z1 < hi:
                            new.append((z1, hi))
                    spans = new
            for lo, hi in spans:
                part.wall_box(wall, a, b, 0.0, WALL_T, lo, hi, "Plaster")

    def _mullions(self, part, wall, a, b):
        """A Wood mullion and transom across a window hole, set midway in the
        wall, so the light through it falls in four panes."""
        z0, z1 = WIN_Z
        ms, mz = (a + b) / 2, (z0 + z1) / 2
        d0, d1 = WALL_T / 2 - 0.05, WALL_T / 2 + 0.05
        part.wall_box(wall, ms - 0.05, ms + 0.05, d0, d1, z0, z1, "Wood")
        part.wall_box(wall, a, ms - 0.05, d0, d1, mz - 0.05, mz + 0.05, "Wood")
        part.wall_box(wall, ms + 0.05, b, d0, d1, mz - 0.05, mz + 0.05, "Wood")

    def walls(self):
        """The two drawn walls, each its own object: plaster with real
        window holes (and the doorway in the +x wall), mullions, and a Wood
        sill board inside each window."""
        for wall, name in (("posz", "Wall_PosZ"), ("posx", "Wall_PosX")):
            part = Part(name)
            holes = [(a, b, WIN_Z[0], WIN_Z[1]) for a, b in WINDOWS[wall]]
            if wall == "posx":
                holes.append((DOOR_Y - DOOR_W / 2 - JAMB, DOOR_Y + DOOR_W / 2 + JAMB, -0.1, DOOR_H + HEAD))
            self._wall(part, wall, holes)
            for a, b in WINDOWS[wall]:
                self._mullions(part, wall, a, b)
                part.wall_box(wall, a - 0.1, b + 0.1, -0.14, WALL_T / 2 - 0.05, WIN_Z[0] - 0.06, WIN_Z[0] + 0.03, "Wood")
            part.finish(self.coll)

    def timbers(self):
        """Dark timber on the drawn walls' inside faces: skirting, two rails
        and the wall plate, and posts (standing further out, so they run
        over the rails) at the ends, either side of each window and the
        doorway, and between so no panel is wider than 2.6 m."""
        part = Part("Timbers")
        door = (DOOR_Y - DOOR_W / 2 - JAMB, DOOR_Y + DOOR_W / 2 + JAMB)
        for wall in ("posz", "posx"):
            t0 = -RX if wall == "posz" else -RY
            t1 = RX if wall == "posz" else RY
            for z0, z1 in RAILS:
                pieces = [(t0, t1)]
                if wall == "posx" and z0 < DOOR_H + HEAD:
                    pieces = [(t0, door[0]), (door[1], t1)]
                for a, b in pieces:
                    part.wall_box(wall, a, b, -RAIL_PROUD, 0.0, z0, z1, "Wood")
            # The back wall's post at x = +RX already stands in the corner.
            fixed = [(t0 - (POST_PROUD if wall == "posx" else 0.0), t0 + (0.0 if wall == "posx" else POST_W)),
                     (t1 - POST_W, t1)]
            for a, b in WINDOWS[wall]:
                fixed += [(a - POST_W, a), (b, b + POST_W)]
            if wall == "posx":
                fixed += [(door[0] - POST_W, door[0]), (door[1], door[1] + POST_W)]
            fixed.sort()
            posts = list(fixed)
            for (a0, a1), (b0, b1) in zip(fixed, fixed[1:]):
                gap = b0 - a1
                panels = max(1, math.ceil(gap / 2.6 - 1e-6))
                width = (gap - (panels - 1) * POST_W) / panels
                for k in range(1, panels):
                    s = a1 + k * width + (k - 1) * POST_W
                    posts.append((s, s + POST_W))
            self.posts[wall] = sorted((a, b) for a, b in posts if b - a > 1e-6 and not (wall == "posx" and a < t0))
            for a, b in posts:
                if b - a < 1e-6 or (wall == "posx" and a < t0):
                    continue
                # Just under the wall plate's top, so the two tops never share a plane.
                part.wall_box(wall, a, b, -POST_PROUD, 0.0, 0.0, WALL_H - 0.04, "Wood")
        part.finish(self.coll)

    def doorway(self):
        """The inside door's frame: Wood jambs and head through the +x wall,
        flush with its inside face, so the door (hinged on that face) swings
        100 degrees into the room clear of them."""
        part = Part("Doorway")
        y0, y1 = DOOR_Y - DOOR_W / 2, DOOR_Y + DOOR_W / 2
        part.box((RX, y0 - JAMB, -0.1), (RX + WALL_T, y0, DOOR_H), "Wood")
        part.box((RX, y1, -0.1), (RX + WALL_T, y1 + JAMB, DOOR_H), "Wood")
        part.box((RX, y0 - JAMB, DOOR_H), (RX + WALL_T, y1 + JAMB, DOOR_H + HEAD), "Wood")
        part.finish(self.coll)

    def cutaway(self):
        """The walls nearest the camera and the roof, shadow-only; and the
        drawn stubs along those walls' base."""
        for wall, name in (("negx", "Wall_NegX"), ("negz", "Wall_NegZ")):
            part = Part(name + SHADOW_ONLY)
            self._wall(part, wall, [(a, b, WIN_Z[0], WIN_Z[1]) for a, b in WINDOWS[wall]])
            for a, b in WINDOWS[wall]:
                self._mullions(part, wall, a, b)
            part.finish(self.coll)
            stub = Part(name.replace("Wall_", "WallStub_"))
            s0, s1 = RUN[wall]
            stub.wall_box(wall, s0, s1, 0.0, WALL_T, -0.1, STUB_H, "Plaster")
            stub.wall_box(wall, s0, s1, -0.06, 0.0, 0.0, 0.22, "Wood")
            stub.finish(self.coll)
        roof = Part("Roof" + SHADOW_ONLY)
        roof.box((-RX - WALL_T - 0.3, -RY - WALL_T - 0.3, WALL_H - 0.05), (RX + WALL_T + 0.3, RY + WALL_T + 0.3, WALL_H + 0.35),
                 "Roof")
        roof.finish(self.coll)

    # ---- hearth and stage -------------------------------------------------

    def hearth(self):
        """A stone fireplace on the back wall: a raised hearth slab, the
        firebox (sooty Dirt back) under a stone lintel, a Wood mantel beam,
        and the chimney breast up to the wall plate. In the firebox an ash
        bed and three logs; the hearth fire stands on the ash."""
        part = Part("Hearth")
        cx = HEARTH_X
        face = -RY + 1.3            # the breast's front face (y)
        part.box((cx - 2.1, -RY, -0.1), (cx + 2.1, face + 0.6, 0.3), "Stone")  # hearth slab
        fx0, fx1 = cx - FIRE_W / 2, cx + FIRE_W / 2
        top = 2.3
        part.box((cx - 1.8, -RY, 0.3), (fx0, face, top), "Stone")
        part.box((fx1, -RY, 0.3), (cx + 1.8, face, top), "Stone")
        part.box((cx - 1.8, -RY, top), (cx + 1.8, face, 2.75), "Stone")
        part.box((fx0, -RY, 0.3), (fx1, -RY + 0.2, top), "Dirt")         # sooty back
        part.box((cx - 2.0, face, 2.75), (cx + 2.0, face + 0.25, 2.95), "Oak")  # mantel
        part.box((cx - 1.5, -RY, 2.75), (cx + 1.5, -RY + 1.0, WALL_H), "Stone")  # breast
        part.box((cx - 1.6, -RY, 2.95), (cx + 1.6, -RY + 1.1, 3.15), "Stone")    # a course jutting out
        # Ash and logs.
        fy = (-RY + 0.2 + face) / 2
        part.box((cx - 0.75, fy - 0.42, 0.3), (cx + 0.75, fy + 0.42, 0.33), "Dirt")
        for yaw, off in ((0.35, -0.12), (-0.3, 0.14), (1.45, 0.0)):
            m = place(cx, fy + off, yaw, 0.33 + (0.12 if yaw > 1 else 0.0))
            part.box((-0.6, -0.08, 0.0), (0.6, 0.08, 0.14), "Wood", m=m)
        self.fire = (cx, fy, 0.33 + 0.08)
        self.group("Hearth", part.finish(self.coll))

    def stage(self):
        """A small raised stage in the corner between the back wall and the
        -x cutaway wall, its front corner cut off: a dark kick, a Wood body
        and a top of V-grooved planks. Empty. STAGE_H is above the hero's
        step, so its edge blocks."""
        part = Part("Stage")
        lip = STAGE_H - 0.06
        part.prism_z(STAGE, -0.05, 0.12, "Dirt", top=False)
        part.prism_z(STAGE, 0.12, lip, "Oak", top=False, bottom=False)  # the lip: Oak
        # Planks along x, the cut corner trimming their ends.
        (cx0, cy0), (cx1, cy1) = STAGE[2], STAGE[3]

        def x_end(y):
            if y <= cy0:
                return cx0
            return cx0 + (cx1 - cx0) * (y - cy0) / (cy1 - cy0)

        y_lo, y_hi = -RY, STAGE[4][1]
        rows = 5
        bevel = 0.035
        for r in range(rows):
            y0 = y_lo + (y_hi - y_lo) * r / rows
            y1 = y_lo + (y_hi - y_lo) * (r + 1) / rows
            lo = [(-RX, y0, lip), (x_end(y0), y0, lip), (x_end(y1), y1, lip), (-RX, y1, lip)]
            hi = [(-RX, y0 + bevel, STAGE_H), (x_end(y0 + bevel) - bevel, y0 + bevel, STAGE_H),
                  (x_end(y1 - bevel) - bevel, y1 - bevel, STAGE_H), (-RX, y1 - bevel, STAGE_H)]
            part.hexa(lo + hi, "Wood", skip=("-z",))
        self.group("Stage", part.finish(self.coll))

    # ---- furniture --------------------------------------------------------

    @staticmethod
    def _bar_frame():
        """The bar side of the door wall: local x runs along the wall
        (Blender y), local y out into the room (Blender -x)."""
        return place(RX, 0.0, math.pi / 2)

    def bar(self):
        """The bar counter along the door wall, its far end against the back
        wall: a panelled Wood body, an overhanging top, a foot rail; two
        mugs on the top; six stools in front."""
        f = self._bar_frame()
        part = Part("Bar")
        x0, x1 = BAR_RUN
        y0, y1 = BAR_D
        part.box((x0, y0, 0.0), (x1, y1, BAR_TOP - 0.1), "Oak", m=f)
        part.box((x0, y0 - 0.1, BAR_TOP - 0.1), (x1 + 0.1, y1 + 0.2, BAR_TOP), "Oak", m=f)
        n = 6
        for k in range(n + 1):
            x = x0 + 0.2 + (x1 - x0 - 0.2) * k / n
            part.box((x - 0.06, y1, 0.0), (x + 0.06, y1 + 0.05, BAR_TOP - 0.1), "Oak", m=f)
        part.box((x0, y1, 0.0), (x1, y1 + 0.04, 0.18), "Dirt", m=f)    # kick plinth
        part.box((x0 + 0.2, y1 + 0.28, 0.32), (x1 - 0.1, y1 + 0.36, 0.40), "Oak", m=f)  # foot rail
        for x in (x0 + 0.3, x1 - 0.2):
            part.box((x - 0.03, y1, 0.32), (x + 0.03, y1 + 0.36, 0.40), "Oak", m=f)
        self.group("Bar", part.finish(self.coll))
        mugs = Part("Bar_Mugs")
        for s, d in ((-6.8, 3.05), (-4.4, 3.45)):
            self._mug(mugs, s, d, BAR_TOP, frame=f)
        self.group("Bar", mugs.finish(self.coll))
        for i, s in enumerate(STOOLS_S):
            p = f @ Vector((s, STOOL_D, 0.0))
            self._stool(f"Stool_{i + 1}", p.x, p.y, yaw=0.15 * ((i % 3) - 1))

    def _mug(self, part, x, y, z, yaw=0.0, frame=None):
        m = place(x, y, yaw, z)
        if frame is not None:
            m = frame @ m
        part.prism_z(ngon(6, 0.085), 0.0, 0.17, "Wood", m=m)
        part.box((0.075, -0.02, 0.03), (0.15, 0.02, 0.13), "Wood", m=m)

    def _bottle(self, part, x, y, z, mat, frame):
        m = frame @ place(x, y, 0.3, z)
        part.prism_z(ngon(6, 0.085), 0.0, 0.3, mat, m=m)
        part.prism_z(ngon(6, 0.032), 0.3, 0.46, mat, m=m, bottom=False)

    def _stool(self, name, x, y, yaw=0.0):
        part = Part(name)
        m = place(x, y, yaw)
        part.prism_z(ngon(8, 0.28, math.pi / 8), 1.10, 1.18, "Oak", m=m)
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            cx, cy = 0.17 * math.cos(a), 0.17 * math.sin(a)
            part.box((cx - 0.035, cy - 0.035, 0.0), (cx + 0.035, cy + 0.035, 1.10), "Oak", m=m)
        part.box((-0.17, -0.025, 0.42), (0.17, 0.025, 0.48), "Oak", m=m)
        part.box((-0.025, -0.17, 0.62), (0.025, 0.17, 0.68), "Oak", m=m)
        self.group("Bar", part.finish(self.coll))
        self.seats[name] = ((x, y), 0.28, (0.28, 0.28), yaw)

    def shelves(self):
        """Back shelves on the door wall behind the bar: a low cupboard,
        three uprights, three boards; bottles (dark Dirt glass and pale Stone
        crocks) and mugs along them."""
        f = self._bar_frame()
        part = Part("Shelves")
        x0, x1 = BAR_RUN
        x0 += 0.15          # clear of the back wall's posts
        y0, y1 = 0.0, SHELF_D
        part.box((x0, y0, 0.0), (x1, y1, 1.1), "Oak", m=f)
        for x in (x0 + 0.05, (x0 + x1) / 2, x1 - 0.05):
            part.box((x - 0.05, y0, 1.1), (x + 0.05, y0 + 0.45, 3.7), "Oak", m=f)
        boards = [1.95, 2.75, 3.55]
        for z in boards:
            part.box((x0, y0, z), (x1, y0 + 0.45, z + 0.07), "Oak", m=f)
        self.group("Shelves", part.finish(self.coll))
        bottles = Part("Bottles")
        mugs = Part("Shelf_Mugs")
        yb = y0 + 0.22
        for i, z in enumerate([1.1] + [z + 0.07 for z in boards]):
            xs = [x0 + 0.35 + k * 0.42 for k in range(int((x1 - x0 - 0.5) / 0.42) + 1)]
            for k, x in enumerate(xs):
                if abs(x - (x0 + x1) / 2) < 0.15:
                    continue
                pick = (i * 5 + k * 3) % 7
                if i == 3 or (i == 0 and pick < 3):
                    if pick % 2 == 0 or i == 0:
                        self._mug(mugs, x, yb + (0.08 if i == 0 else 0.0), z, yaw=-1.2 + 0.3 * pick, frame=f)
                elif pick != 6:
                    self._bottle(bottles, x, yb, z, "Stone" if pick in (1, 4) else "Dirt", f)
        self.group("Shelves", bottles.finish(self.coll))
        self.group("Shelves", mugs.finish(self.coll))

    def barrels(self):
        """Five barrels in a huddle against the back wall: ten staves' worth
        of facets, bellied, with two dark hoops near each end."""
        profile = [(0.0, 0.42), (0.16, 0.45), (0.26, 0.465), (0.72, 0.5), (1.19, 0.465), (1.29, 0.45), (1.45, 0.42)]
        mats = ["Oak", "Dirt", "Oak", "Oak", "Dirt", "Oak"]
        for i, (x, y) in enumerate(BARRELS):
            part = Part(f"Barrel_{i + 1}")
            part.lathe(profile, 10, (x, y, 0.0), mats, phase=0.3 * i)
            self.group("Barrels", part.finish(self.coll))

    def _chair(self, name, x, y, yaw, key):
        """A chair whose front faces along local -y; yaw turns it about z."""
        part = Part(name)
        m = place(x, y, yaw)
        s = 0.33
        part.box((-s, -s, 0.66), (s, s, 0.74), "Oak", m=m)
        for cx in (-s + 0.05, s - 0.05):
            part.box((cx - 0.04, -s + 0.01, 0.0), (cx + 0.04, -s + 0.09, 0.66), "Oak", m=m)
            part.box((cx - 0.04, s - 0.09, 0.0), (cx + 0.04, s - 0.01, 1.52), "Oak", m=m)
        for z in (1.08, 1.38):
            part.box((-s + 0.01, s - 0.08, z), (s - 0.01, s - 0.02, z + 0.1), "Oak", m=m)
        self.group(key, part.finish(self.coll))
        self.seats[name] = ((x, y), math.hypot(s, s), (s, s), yaw)

    def _bench(self, name, x, y, key):
        """A plank bench along x: seat board on two slab legs and a rail."""
        part = Part(name)
        length, depth, _ = BENCH
        hl, hd = length / 2, depth / 2
        part.box((x - hl, y - hd, 0.66), (x + hl, y + hd, 0.74), "Oak")
        for lx in (x - hl + 0.3, x + hl - 0.3):
            part.box((lx - 0.05, y - hd + 0.04, 0.0), (lx + 0.05, y + hd - 0.04, 0.66), "Oak")
        part.box((x - hl + 0.35, y - 0.03, 0.3), (x + hl - 0.35, y + 0.03, 0.4), "Oak")
        self.group(key, part.finish(self.coll))
        self.seats[name] = ((x, y), math.hypot(hl, hd), (hl, hd), 0.0)

    def tables(self):
        """Six tables: three square (1.6 m) on four legs, one round on a
        pedestal, and a long trestle table with a bench either side; chairs
        pulled up to the others, each turned a little."""
        for name, (x, y), kind, sides in TABLES:
            key = f"Table_{name}"
            part = Part(key)
            if kind == "square":
                h = 0.8
                part.box((x - h, y - h, TABLE_H - 0.08), (x + h, y + h, TABLE_H), "Oak")
                for sx in (-1, 1):
                    for sy in (-1, 1):
                        lx, ly = x + sx * (h - 0.14), y + sy * (h - 0.14)
                        part.box((lx - 0.06, ly - 0.06, 0.0), (lx + 0.06, ly + 0.06, TABLE_H - 0.08), "Oak")
                for sy in (-1, 1):
                    part.box((x - h + 0.14, y + sy * (h - 0.14) - 0.03, TABLE_H - 0.24),
                             (x + h - 0.14, y + sy * (h - 0.14) + 0.03, TABLE_H - 0.08), "Oak")
                reach = h + 0.45
            elif kind == "round":
                part.prism_z(ngon(8, 0.85, math.pi / 8, x, y), TABLE_H - 0.08, TABLE_H, "Oak")
                part.prism_z(ngon(6, 0.11, 0.0, x, y), 0.08, TABLE_H - 0.08, "Oak", top=False)
                for yaw in (0.0, math.pi / 2):
                    part.box((-0.5, -0.07, 0.0), (0.5, 0.07, 0.1), "Oak", m=place(x, y, yaw + 0.4))
                reach = 0.85 + 0.42
            else:  # trestle
                hl, hw = TRESTLE[0] / 2, TRESTLE[1] / 2
                part.box((x - hl, y - hw, TABLE_H - 0.09), (x + hl, y + hw, TABLE_H), "Oak")
                for tx in (x - hl + 0.55, x + hl - 0.55):
                    # An A-frame trestle: two splayed legs, a cross bar and a foot.
                    for sy in (-1, 1):
                        quad = [(sy * (hw - 0.05), 0.0), (sy * (hw - 0.2), 0.0), (sy * 0.06, TABLE_H - 0.09),
                                (sy * 0.2, TABLE_H - 0.09)]
                        a = [Vector((tx - 0.06, y + q[0], q[1])) for q in quad]
                        b = [Vector((tx + 0.06, y + q[0], q[1])) for q in quad]
                        part.face(a, "Oak", (-1, 0, 0))
                        part.face(b, "Oak", (1, 0, 0))
                        for i in range(4):
                            j = (i + 1) % 4
                            part.face([a[i], a[j], b[j], b[i]], "Oak")
                    part.box((tx - 0.07, y - hw + 0.02, 0.0), (tx + 0.07, y + hw - 0.02, 0.1), "Oak")
                    part.box((tx - 0.05, y - 0.32, TABLE_H - 0.3), (tx + 0.05, y + 0.32, TABLE_H - 0.09), "Oak")
                part.box((x - hl + 0.6, y - 0.05, 0.35), (x + hl - 0.6, y + 0.05, 0.47), "Oak")  # stretcher
                off = hw + BENCH[2] + BENCH[1] / 2
                part.finish(self.coll)
                self.group(key, bpy.data.objects[key])
                self._bench(f"Bench_{name}1", x, y + off, key)
                self._bench(f"Bench_{name}2", x, y - off, key)
                part = None
            if part is not None:
                self.group(key, part.finish(self.coll))
            turn = {"+x": (1, 0), "-x": (-1, 0), "+y": (0, 1), "-y": (0, -1)}
            for k, side in enumerate(sides):
                dx, dy = turn[side]
                jig = (0.12, -0.08, 0.05, -0.14)[k]
                # The chair's front (local -y) faces the table.
                yaw = math.atan2(dy, dx) - math.pi / 2 + jig
                cx, cy = x + dx * reach + (0.12 if dy else 0.0) * (k - 1.5), y + dy * reach + (0.1 if dx else 0.0) * (k - 1.5)
                self._chair(f"Chair_{name}{k + 1}", cx, cy, yaw, key)
            mugs = {"A": [(0.4, -0.3, 0.6), (-0.45, 0.25, -2.0)], "C": [(-0.35, -0.4, 1.1)],
                    "E": [(-1.2, 0.25, 0.3), (0.5, -0.3, 2.2), (1.3, 0.2, -0.9)]}.get(name)
            if mugs:
                mp = Part(f"Table_{name}_Mugs")
                for dx, dy, yaw in mugs:
                    self._mug(mp, x + dx, y + dy, TABLE_H, yaw)
                self.group(key, mp.finish(self.coll))

    def brazier(self):
        """A standing brazier by the door: three splayed dark iron legs on a
        ring, a stone bowl, and a bed of coals the fire stands on."""
        part = Part("Brazier")
        x, y = BRAZIER
        for k in range(3):
            a = math.pi / 2 + k * math.tau / 3
            c, s = math.cos(a), math.sin(a)
            foot = Vector((x + 0.42 * c, y + 0.42 * s, 0.0))
            top = Vector((x + 0.24 * c, y + 0.24 * s, 0.8))
            side = Vector((-s, c, 0.0)) * 0.035
            out = Vector((c, s, 0.0)) * 0.035
            part.hexa([foot - side - out, foot + side - out, foot + side + out, foot - side + out,
                       top - side - out, top + side - out, top + side + out, top - side + out], "Dirt")
        part.prism_z(ngon(9, 0.33, 0.0, x, y), 0.34, 0.4, "Dirt")                  # ring
        part.lathe([(0.0, 0.16), (0.1, 0.32), (0.22, 0.42), (0.3, 0.45)], 9, (x, y, 0.74),
                   ["Stone", "Stone", "Stone"])                                     # bowl
        part.prism_z(ngon(7, 0.36, 0.2, x, y), 1.04, 1.08, "Dirt")                 # coals
        self.brazier_fire = (x, y, 1.08 + 0.08)
        self.group("Brazier", part.finish(self.coll))

    def candles(self):
        """A candle on each table and one on the bar: a stone dish and a
        stub of pale wax. The flame stands on the wax's top centre."""
        bar = self._bar_frame() @ Vector((BAR_CANDLE[0], BAR_CANDLE[1], 0.0))
        spots = [((x, y), TABLE_H, f"Table_{n}") for n, (x, y), _, _ in TABLES] + [((bar.x, bar.y), BAR_TOP, "Bar")]
        for i, ((x, y), z, key) in enumerate(spots):
            part = Part(f"Candle_{i + 1}")
            part.prism_z(ngon(8, 0.1, math.pi / 8, x, y), z, z + 0.03, "Stone")
            part.prism_z(ngon(6, 0.045, 0.0, x, y), z + 0.03, z + 0.25, "Plaster")
            self.group(key, part.finish(self.coll))
            self.flames.append((x, y, z + 0.25))

    def sconces(self):
        """Four wall sconces on posts of the drawn walls: an iron bracket
        (Stone) - a back plate on the post, an arm and a diagonal strut - and
        an open flame dish (Stone) holding a dark fuel bed (Dirt). The
        engine draws each flame at its base point (SCONCE_FLAME_Z)."""
        for i, (wall, s) in enumerate(SCONCES):
            part = Part(f"Sconce_{i + 1}")
            d_face = -POST_PROUD
            fz = SCONCE_FLAME_Z
            # Back plate and arm (wall_box: s along the wall, d into the room negative).
            part.wall_box(wall, s - 0.08, s + 0.08, d_face - 0.04, d_face, fz - 0.5, fz + 0.05, "Stone")
            part.wall_box(wall, s - 0.03, s + 0.03, d_face - SCONCE_OUT, d_face - 0.04, fz - 0.17, fz - 0.12, "Stone")
            # Strut: a diagonal from low on the plate to under the arm.
            quad = [(d_face - 0.04, fz - 0.46), (d_face - 0.04, fz - 0.38), (d_face - 0.26, fz - 0.17),
                    (d_face - 0.33, fz - 0.17)]
            a = [Vector(wall_point(wall, s - 0.025, d, z)) for d, z in quad]
            b = [Vector(wall_point(wall, s + 0.025, d, z)) for d, z in quad]
            side = b[0] - a[0]
            part.face(a, "Stone", -side)
            part.face(b, "Stone", side)
            centre = sum(a, Vector()) / 4
            for k in range(4):
                j = (k + 1) % 4
                part.face([a[k], a[j], b[j], b[k]], "Stone", (a[k] + a[j]) / 2 - centre)
            # The dish, with its dark fuel bed inside the rim.
            c = Vector(wall_point(wall, s, d_face - SCONCE_OUT, 0.0))
            part.lathe([(0.0, 0.05), (0.05, 0.11), (0.12, 0.16)], 8, (c.x, c.y, fz - 0.16),
                       ["Stone", "Stone"])
            part.prism_z(ngon(8, 0.12, math.pi / 8, c.x, c.y), fz - 0.04, fz - 0.02, "Dirt", bottom=False)
            part.finish(self.coll)
            self.sconce_flames.append((c.x, c.y, fz))

    def build_all(self):
        for step in self.STEPS:
            getattr(self, step)()


# --------------------------------------------------------------------------
# Checks, figures for assets.md, previews and export


def triangles(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def world_bounds(obj):
    vs = [obj.matrix_world @ v.co for v in obj.data.vertices]
    return (Vector([min(v[i] for v in vs) for i in range(3)]), Vector([max(v[i] for v in vs) for i in range(3)]))


# ---- layout checks: walkways and paths ------------------------------------
#
# Every furniture group (a table with its chairs or benches, the bar with its
# stools, the shelves, the barrels, the hearth, the stage, the brazier) is
# measured by the convex hulls of its objects' plan outlines. Walls are
# measured from what stands out of them: posts on the drawn walls, the
# skirting on the cutaway stubs.

WALL_LINES = {  # wall -> (axis, inner line, sign of "into the room")
    "posx": (0, RX - POST_PROUD, -1),
    "negx": (0, -RX + 0.06, 1),
    "posz": (1, -RY + POST_PROUD, 1),
    "negz": (1, RY - 0.06, -1),
}


def hull2d(pts):
    pts = sorted(set((round(p[0], 5), round(p[1], 5)) for p in pts))
    if len(pts) < 3:
        return pts

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in pts:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(pts):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]  # counter-clockwise


def seg_dist(p, a, b):
    ab = (b[0] - a[0], b[1] - a[1])
    t = ((p[0] - a[0]) * ab[0] + (p[1] - a[1]) * ab[1]) / max(ab[0] ** 2 + ab[1] ** 2, 1e-12)
    t = max(0.0, min(1.0, t))
    return math.hypot(p[0] - a[0] - t * ab[0], p[1] - a[1] - t * ab[1])


def hulls_overlap(a, b):
    for poly in (a, b):
        n = len(poly)
        for i in range(n):
            e = (poly[(i + 1) % n][0] - poly[i][0], poly[(i + 1) % n][1] - poly[i][1])
            ax = (-e[1], e[0])
            pa = [p[0] * ax[0] + p[1] * ax[1] for p in a]
            pb = [p[0] * ax[0] + p[1] * ax[1] for p in b]
            if max(pa) < min(pb) or max(pb) < min(pa):
                return False
    return True


def hull_dist(a, b):
    if hulls_overlap(a, b):
        return 0.0
    d = 1e9
    for p, poly in ((q, b) for q in a):
        d = min(d, min(seg_dist(p, poly[i], poly[(i + 1) % len(poly)]) for i in range(len(poly))))
    for p in b:
        d = min(d, min(seg_dist(p, a[i], a[(i + 1) % len(a)]) for i in range(len(a))))
    return d


def group_hulls(b):
    out = {}
    for key, names in b.groups.items():
        hs = []
        for name in names:
            obj = bpy.data.objects[name]
            hs.append(hull2d([(obj.matrix_world @ v.co).xy for v in obj.data.vertices]))
        out[key] = hs
    return out


def wall_dist(hulls, wall):
    axis, line, sign = WALL_LINES[wall]
    return max(0.0, min(sign * (p[axis] - line) for h in hulls for p in h))


def walkway_report(hulls):
    """Every gap between two groups, and between a group and a wall it does
    not stand against."""
    gaps = []
    keys = sorted(hulls)
    for i, a in enumerate(keys):
        for b_ in keys[i + 1:]:
            gaps.append((a, b_, min(hull_dist(p, q) for p in hulls[a] for q in hulls[b_])))
        for wall in WALL_LINES:
            gaps.append((a, "wall " + wall, wall_dist(hulls[a], wall)))
    return gaps


def distance_field(hulls, xs, ys):
    import numpy as np
    X, Y = np.meshgrid(xs, ys, indexing="xy")
    d = np.full(X.shape, 1e9)
    for h in hulls:
        n = len(h)
        inside = np.ones(X.shape, dtype=bool)
        dh = np.full(X.shape, 1e9)
        for i in range(n):
            a, c = h[i], h[(i + 1) % n]
            ex, ey = c[0] - a[0], c[1] - a[1]
            inside &= (ex * (Y - a[1]) - ey * (X - a[0])) >= 0
            t = np.clip(((X - a[0]) * ex + (Y - a[1]) * ey) / max(ex * ex + ey * ey, 1e-12), 0, 1)
            dh = np.minimum(dh, np.hypot(X - a[0] - t * ex, Y - a[1] - t * ey))
        dh[inside] = 0.0
        d = np.minimum(d, dh)
    return d


def path_report(hulls, cell=0.05):
    """Sweep a disc PATH wide from the spawn over the floor; report, for the
    bar, the hearth and every table, whether the disc reaches it."""
    import numpy as np
    xs = np.arange(-RX + cell / 2, RX, cell)
    ys = np.arange(-RY + cell / 2, RY, cell)
    X, Y = np.meshgrid(xs, ys, indexing="xy")
    walls = np.minimum.reduce([sign * ((X if axis == 0 else Y) - line) for axis, line, sign in WALL_LINES.values()])
    fields = {k: distance_field(h, xs, ys) for k, h in hulls.items()}
    clear = np.minimum.reduce([walls] + list(fields.values()))
    free = clear >= PATH / 2
    si, sj = int((SPAWN[1] + RY) / cell), int((SPAWN[0] + RX) / cell)
    reach = np.zeros_like(free)
    if not free[si, sj]:
        return {"spawn_clearance": float(clear[si, sj])}, reach
    reach[si, sj] = True
    while True:
        grow = reach.copy()
        grow[1:, :] |= reach[:-1, :]
        grow[:-1, :] |= reach[1:, :]
        grow[:, 1:] |= reach[:, :-1]
        grow[:, :-1] |= reach[:, 1:]
        grow &= free
        if grow.sum() == reach.sum():
            break
        reach = grow
    out = {"spawn_clearance": float(clear[si, sj])}
    for k in sorted(hulls):
        if k == "Bar" or k == "Hearth" or k.startswith("Table_"):
            near = reach & (fields[k] <= PATH / 2 + 1.5 * cell)
            out[k] = bool(near.any())
    return out, reach


def view_extent(b):
    """The visible room's extent on screen at the design yaw, in metres."""
    yaw, pitch = math.radians(DESIGN_YAW), math.radians(30)
    fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
    right = fwd.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(fwd).normalized()
    us, vs = [], []
    for obj in b.coll.objects:
        if obj.name.endswith(SHADOW_ONLY):
            continue
        for v in obj.data.vertices:
            p = obj.matrix_world @ v.co
            us.append(p.dot(right))
            vs.append(p.dot(up))
    return (max(us) - min(us), max(vs) - min(vs))


def figures(b):
    hinge = (RX, DOOR_Y - DOOR_W / 2, 0.0)
    seats = {}
    for name, ((x, y), r, half, yaw) in sorted(b.seats.items()):
        obj = bpy.data.objects[name]
        lo, hi = world_bounds(obj)
        # Measured: the farthest vertex from the centre in plan.
        rr = max(math.hypot(v.co.x - x, v.co.y - y) for v in obj.data.vertices)
        ex = engine((x, y, 0.0))
        seats[name] = {"centre": (ex[0], ex[2]), "radius": round(rr, 3), "half": tuple(round(h, 3) for h in half),
                       "yaw_deg": round(math.degrees(yaw), 1), "top": round(hi.z, 3)}
    braz = bpy.data.objects["Brazier"]
    bx, by = BRAZIER
    tables = {}
    for name, (x, y), kind, _ in TABLES:
        lo, hi = world_bounds(bpy.data.objects[f"Table_{name}"])
        tables[name] = {"kind": kind, "centre": (x, -y), "x": (round(lo.x, 3), round(hi.x, 3)),
                        "z": (round(-hi.y, 3), round(-lo.y, 3)), "top": round(hi.z, 3)}
    blocks = {}
    for name in ["Bar", "Shelves", "Hearth", "Stage"] + [f"Barrel_{i + 1}" for i in range(len(BARRELS))]:
        lo, hi = world_bounds(bpy.data.objects[name])
        blocks[name] = {"x": (round(lo.x, 3), round(hi.x, 3)), "z": (round(-hi.y, 3), round(-lo.y, 3)),
                        "top": round(hi.z, 3)}
    return {
        "design_yaw_deg": DESIGN_YAW,
        "inner_bounds_engine": {"x": (-RX, RX), "z": (-RY, RY)},
        "outer_bounds_engine": {"x": (-RX - WALL_T, RX + WALL_T), "z": (-RY - WALL_T, RY + WALL_T)},
        "hearth_fire_engine": engine(b.fire),
        "brazier_fire_engine": engine(b.brazier_fire),
        "brazier_footprint_engine": {"centre": (bx, -by), "radius": round(max(
            math.hypot(v.co.x - bx, v.co.y - by) for v in braz.data.vertices), 3),
            "top": round(world_bounds(braz)[1].z, 3)},
        "candles_engine": [engine(c) for c in b.flames],
        "sconces_engine": {f"Sconce_{i + 1}": engine(c) for i, c in enumerate(b.sconce_flames)},
        "materials_by_object": {o.name: [m.name for m in o.data.materials] for o in b.coll.objects},
        "hinge_engine": engine(hinge),
        "doorway_centre_engine": engine((RX + WALL_T / 2, DOOR_Y, 0.0)),
        "doorway_inner_face_centre_engine": engine((RX, DOOR_Y, 0.0)),
        "spawn_engine": engine((SPAWN[0], SPAWN[1], 0.0)),
        "stage_engine": {"outline": [(x, round(-y, 3)) for x, y in STAGE], "top": STAGE_H},
        "windows": WINDOWS,
        "tables": tables,
        "blocks": blocks,
        "view_extent_m": tuple(round(v, 2) for v in view_extent(b)),
        "seats": seats,
    }


def oak_checks(b):
    """Oak on exactly the furniture the task lists; Wood kept on the floor,
    wall timbers, frames, sill and skirting."""
    problems = []
    for obj in b.coll.objects:
        mats = {m.name for m in obj.data.materials}
        n = obj.name
        if n.startswith(OAK_ONLY) and not n.endswith("_Mugs"):
            if "Oak" not in mats or "Wood" in mats:
                problems.append(f"{n}: want Oak and no Wood, has {sorted(mats)}")
        elif n in OAK_AND_WOOD:
            if not {"Oak", "Wood"} <= mats:
                problems.append(f"{n}: want Oak (lip or mantel) and Wood, has {sorted(mats)}")
        elif "Oak" in mats:
            problems.append(f"{n}: Oak is only for the listed furniture")
        if n in WOOD_KEPT and "Wood" not in mats:
            problems.append(f"{n}: should keep Wood")
    for n in WOOD_KEPT + OAK_AND_WOOD:
        if n not in bpy.data.objects:
            problems.append(f"{n}: missing")
    return problems


def sconce_checks(b):
    """Four sconces, each on a post of a drawn wall, the flame at about
    2.6 m, every part SCONCE_CLEAR from the windows, the shelves and the
    hearth."""
    problems = []
    if len(b.sconce_flames) != 4:
        problems.append(f"{len(b.sconce_flames)} sconces, want 4")
    clear_of = [bpy.data.objects["Shelves"], bpy.data.objects["Hearth"]]
    boxes = [world_bounds(o) for o in clear_of]
    for i, (wall, s) in enumerate(SCONCES):
        name = f"Sconce_{i + 1}"
        if not any(abs((p0 + p1) / 2 - s) < 1e-3 for p0, p1 in b.posts[wall]):
            problems.append(f"{name}: s {s} is not on a post of {wall}: {b.posts[wall]}")
        obj = bpy.data.objects[name]
        lo, hi = world_bounds(obj)
        if abs(b.sconce_flames[i][2] - 2.6) > 0.1:
            problems.append(f"{name}: flame at {b.sconce_flames[i][2]}")
        # Windows on this wall: the gap along the run.
        s_lo, s_hi = (lo.x, hi.x) if wall == "posz" else (lo.y, hi.y)
        for w0, w1 in WINDOWS[wall]:
            gap = max(w0 - s_hi, s_lo - w1)
            if gap < SCONCE_CLEAR:
                problems.append(f"{name}: {gap:.2f} m from the window {w0}..{w1}")
        for o, (blo, bhi) in zip(clear_of, boxes):
            gx = max(blo.x - hi.x, lo.x - bhi.x, 0.0)
            gy = max(blo.y - hi.y, lo.y - bhi.y, 0.0)
            gz = max(blo.z - hi.z, lo.z - bhi.z, 0.0)
            if math.sqrt(gx * gx + gy * gy + gz * gz) < SCONCE_CLEAR:
                problems.append(f"{name}: within {SCONCE_CLEAR} m of {o.name}")
    return problems


def check(b):
    problems = []
    allowed = set(MATERIALS)
    total = 0
    per = {}
    for obj in b.coll.objects:
        if obj.type != "MESH":
            continue
        mats = {m.name for m in obj.data.materials}
        if not mats <= allowed:
            problems.append(f"{obj.name}: materials {sorted(mats - allowed)} not allowed")
        if "Roof" in mats and not obj.name.endswith(SHADOW_ONLY):
            problems.append(f"{obj.name}: Roof is only for the shadow-only roof")
        if "." in obj.name:
            problems.append(f"{obj.name}: a duplicate name")
        if obj.name.startswith("WallStub_"):
            if world_bounds(obj)[1].z > 0.5 + 1e-6:
                problems.append(f"{obj.name}: taller than 0.5 m")
        per[obj.name] = triangles(obj)
        total += per[obj.name]
    shadow = sorted(n for n in per if n.endswith(SHADOW_ONLY))
    if shadow != ["Roof" + SHADOW_ONLY, "Wall_NegX" + SHADOW_ONLY, "Wall_NegZ" + SHADOW_ONLY]:
        problems.append(f"shadow-only objects {shadow}")
    if total > TRI_BUDGET:
        problems.append(f"{total} triangles, budget {TRI_BUDGET}")
    if sum(len(w) for w in WINDOWS.values()) != 4 or WINDOWS["negx"] or WINDOWS["negz"]:
        problems.append("want four windows, all on the drawn walls")
    # The inside door's swing (radius 2.2 m round the hinge, from closed along
    # +y round through -x to 100 degrees) and the spawn must be clear of
    # every piece of furniture.
    hinge = Vector((RX, DOOR_Y - DOOR_W / 2))
    sweep = 2.2 + 0.05
    for obj in b.coll.objects:
        if obj.name.startswith(("Floor", "Wall", "Timbers", "Doorway", "Threshold", "DoorStep", "Roof")):
            continue
        for v in obj.data.vertices:
            p = (obj.matrix_world @ v.co).xy - hinge
            if p.length < sweep and p.x < 0.05 and math.degrees(math.atan2(-p.x, p.y)) <= 101:
                problems.append(f"{obj.name} is in the inside door's swing")
                break
            q = (obj.matrix_world @ v.co).xy - Vector(SPAWN)
            if q.length < 0.6:
                problems.append(f"{obj.name} is within 0.6 m of the spawn point")
                break
    # Walkways: every gap between groups >= WALKWAY; a group either stands
    # against a wall (<= TOUCH) or leaves a walkway to it.
    hulls = group_hulls(b)
    gaps = walkway_report(hulls)
    for a, c, d in gaps:
        if c.startswith("wall"):
            if TOUCH < d < WALKWAY:
                problems.append(f"walkway {a} - {c} is {d:.2f} m")
        elif d < WALKWAY:
            problems.append(f"walkway {a} - {c} is {d:.2f} m")
    paths, _ = path_report(hulls)
    if paths["spawn_clearance"] < PATH / 2:
        problems.append(f"the spawn has only {paths['spawn_clearance']:.2f} m clearance for a {PATH} m path")
    for k, ok in paths.items():
        if k != "spawn_clearance" and not ok:
            problems.append(f"no {PATH} m path from the door to {k}")
    problems += oak_checks(b)
    problems += sconce_checks(b)
    w, h = view_extent(b)
    if w > VIEW[0] or h > VIEW[1]:
        problems.append(f"the room is {w:.1f} x {h:.1f} m on screen, more than the {VIEW} view")
    return problems, total, per, gaps, paths


def render(scene, cam, path, yaw_deg, pitch_deg, target, scale, res):
    yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
    # As the engine's camera: looking along (sin yaw, cos yaw) in engine xz,
    # which is (sin yaw, -cos yaw) in Blender's ground plane.
    fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
    cam.data.ortho_scale = scale
    cam.location = Vector(target) - fwd * 70
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)


def render_previews(b):
    """tavern_interior.png: the design view - yaw 45, 30 degrees down,
    orthographic, framed exactly as the game's 960x540 view at 24 px/m (here
    at twice the pixels) - with the shadow-only pieces hidden, as the game
    draws it. tavern_interior_overview.png: the same view with the
    shadow-only roof and walls shown, and a plan from straight above with
    them hidden."""
    import numpy as np

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.film_transparent = False
    world = bpy.data.worlds.get("preview") or bpy.data.worlds.new("preview")
    world.color = srgb(24, 22, 28)
    scene.world = world
    cam = bpy.data.objects.new("preview", bpy.data.cameras.new("preview"))
    cam.data.type = "ORTHO"
    cam.data.clip_end = 300
    scene.collection.objects.link(cam)
    scene.camera = cam
    shadow = [o for o in b.coll.objects if o.name.endswith(SHADOW_ONLY)]
    target = (0.0, 0.0, 1.2)
    out_dir = os.path.dirname(PREVIEW)

    for o in shadow:
        o.hide_render = True
    render(scene, cam, PREVIEW, DESIGN_YAW, 30, target, 40.0, (1920, 1080))
    tmp = [os.path.join(out_dir, "_tavern_interior_shown.png"), os.path.join(out_dir, "_tavern_interior_plan.png")]
    render(scene, cam, tmp[1], 0.0, 90.0 - 1e-3, (0, 0, 0), 25.0, (1440, 1080))
    for o in shadow:
        o.hide_render = False
    render(scene, cam, tmp[0], DESIGN_YAW, 30, target, 36.0, (1440, 1080))
    bpy.data.objects.remove(cam)

    w, h = 1440, 1080
    sheet = np.zeros((h, w * 2, 4), dtype=np.float32)
    for k, path in enumerate(tmp):
        img = bpy.data.images.load(path)
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        sheet[:, k * w:(k + 1) * w, :] = px.reshape(h, w, 4)
        bpy.data.images.remove(img)
        os.remove(path)
    img = bpy.data.images.new("tavern_interior_overview", w * 2, h)
    img.pixels.foreach_set(sheet.ravel())
    img.filepath_raw = OVERVIEW
    img.file_format = "PNG"
    img.save()
    print("wrote", PREVIEW, "and", OVERVIEW)


def export():
    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_apply=True,
        export_yup=True,
        export_normals=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
    )
    print("wrote", OUT)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = Builder()
    b.build_all()
    problems, total, per, gaps, paths = check(b)
    print(f"tavern_interior: {total} tris in {len(per)} objects; " + ", ".join(f"{k} {v}" for k, v in per.items()))
    for a, c, d in sorted(gaps, key=lambda g: g[2]):
        if d > TOUCH and d < 3.5:
            print(f"tavern_interior: gap {a} - {c}: {d:.2f}")
    print(f"tavern_interior: groups touching walls: " + ", ".join(f"{a}-{c}" for a, c, d in gaps if d <= TOUCH))
    print(f"tavern_interior: paths: {paths}")
    for k, v in figures(b).items():
        if k == "seats":
            for name, s in v.items():
                print(f"tavern_interior: seat {name}: {s}")
        else:
            print(f"tavern_interior: {k}: {v}")
    if problems:
        raise RuntimeError("the interior fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_previews(b)


if __name__ == "__main__" and bpy.app.background:
    main()
