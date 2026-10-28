"""Build the Sleeping Fox tavern as seen from outside and export
assets/scenes/tavern_exterior.glb.

Run headless (builds, checks, exports, then renders the preview beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_exterior.py
    PXE_TAVERN_OUT=/some/dir ... writes the glb and preview there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.
    tavern_exterior_roof.png beside it: the roof close up from the game's
    default camera - at the game's 24 px/m as the engine would ink it (ray
    cast, toon-shaded, crease ink on Roof), the same with the ink solid, and
    a smooth render at 72 px/m.
    tavern_exterior_masonry.png beside it (revision 5): close views at the
    game's 24 px/m, toon-shaded with the engine's day palette - the plinth,
    threshold and a spalled patch from the default camera, the chimney from
    the front-right, and one spalled patch close up - each beside a smooth
    render of the same crop.
    tavern_exterior_hall.png beside it (revision 6): the hall through the
    doorway from the default camera (24 px/m toon and smooth), and a
    cutaway from above with the roof hidden.

Or step by step in an open Blender, to watch it go together:
    import runpy; tx = runpy.run_path(".../build_tavern_exterior.py", run_name="tavern")
    b = tx["Builder"](); b.start(); b.foundation(); b.walls(); ...  (see Builder.STEPS)
Each step adds its pieces to the Tavern_Exterior collection, leaving the rest
of the scene alone.

The building: a single-storey inn at hero scale (1.6x real), its stone
plinth 9 x 7 m (8.8 x 6.8 m over the timber faces). A plinth that reaches
0.6 m below the origin so it sits on uneven lawn; a timber frame (Wood) of
corner posts, posts, sole plate, two rails, wall plate and braces, standing
proud of cream plaster panels (Plaster); a hipped slate roof (Roof) at 45
degrees in stepped overlapping courses with ridge and hip rolls, its ridge
lower than the house is wide, with a cross-gable over the door and a small
gabled dormer on each long slope (revision 3: Plaster faces, Wood framing,
carved bargeboards and finials, slated as the roof; they stay inside the
revision-2 eaves and over the plinth, which the build checks); a stone chimney up the right-hand end wall (Stone); dark
recessed window panes (Dirt) behind Wood mullions; and a hanging sign by the
door, a Wood board with a sleeping fox in Plaster on both faces.
Revision 4 cuts every slate course - main slopes, hips, ridge, the gable's
and the dormers' slopes - into staggered tiles about TILE wide: a V notch at
each joint, its two faces turned more than 40 degrees from the tile, the
joints of each course half a tile along from the course below's, so the
engine's crease ink draws a line on every joint as well as every course lip. The doorway
is empty - the door model (build_tavern_door.py) hangs there - and opens on
a dark hall (Dirt side walls, Wood floor) that runs back to the rear wall's
inside face, so the open door shows a floor running into the building
rather than the back of the walls (revision 6: the vestibule's Dirt back
wall is gone; the game darkens the hall).

Axes and origin (Blender): the door faces -Y (+Z in the engine). The origin
is the middle of the doorway's threshold - centred across the doorway and
through the wall's thickness - at floor level. Everything is static, every
face flat-shaded.

Material names are the contract with the engine: Wood, Plaster, Roof,
Stone, Dirt, and nothing else.
"""

import math
import os

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_TAVERN_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "scenes"))
OUT = os.path.join(OUT_DIR, "tavern_exterior.glb")
PREVIEW = os.path.join(OUT_DIR, "tavern_exterior.png")
ROOF_PREVIEW = os.path.join(OUT_DIR, "tavern_exterior_roof.png")
MASONRY_PREVIEW = os.path.join(OUT_DIR, "tavern_exterior_masonry.png")
HALL_PREVIEW = os.path.join(OUT_DIR, "tavern_exterior_hall.png")
COLLECTION = "Tavern_Exterior"


def srgb(r, g, b):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b))


# Preview colours only: the engine colours static scenes from its own
# palettes by material name. Stone and Dirt match build_ruins.py.
MATERIALS = {
    "Wood": srgb(118, 76, 44),
    "Plaster": srgb(230, 216, 184),
    "Roof": srgb(90, 102, 118),      # slate: the engine's day mid tone (5a6676)
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
# Layout (Blender metres; x across the front, y back into the building)

DOOR_W, DOOR_H = 2.2, 3.4          # clear opening
JAMB = 0.2                          # Wood jambs either side, the head above
HEAD = 0.25
X0, X1 = -4.0, 4.8                  # timber faces, left and right (plinth 9.0 m)
Y0, Y1 = -0.25, 6.55                # timber faces, front and back (plinth 7.0 m)
PROUD = 0.08                        # rails and braces stand off the plaster
POST_PROUD = 0.10                   # posts stand off further, so they run over the rails
POST_W = 0.25
CORNER = 0.33                       # corner posts, square
WALL_T = 0.42                       # plaster wall, face to inside
PX0, PX1 = X0 + PROUD, X1 - PROUD   # plaster faces
PY0, PY1 = Y0 + PROUD, Y1 - PROUD
PLINTH_TOP = 0.45
PLINTH_OUT = 0.10                   # beyond the timber faces
FOOT = -0.6                         # the foundation's bottom
PLATE_TOP = 4.6                     # wall plate's top
RAILS = [(0.45, 0.70), (1.30, 1.50), (3.20, 3.40), (4.35, 4.60)]  # sole plate, low rail, high rail, wall plate
WIN_Z = (1.50, 3.20)                # windows fill the panel between the rails
# Window openings along each wall: (s0, s1) on the wall's run (x for the
# front and back, y for the sides).
WINDOWS = {
    "front": [(-3.1, -1.9), (2.7, 3.9)],
    "back": [(-2.4, -1.2), (2.0, 3.2)],
    "left": [(2.5, 3.7)],
    "right": [(0.9, 2.1)],
}
DOORWAY = (-DOOR_W / 2 - JAMB, DOOR_W / 2 + JAMB)  # the hole the jambs and door fill, on the front
VESTIBULE = (-1.9, 1.9, PY1 - WALL_T)  # x0, x1 and the hall's back (y): the rear wall's inside face (revision 6; was 3.0)
# Roof (revision 3: slate). The eave line and underside are revision 2's.
EAVE_OUT = 0.55
EAVE_LOW, EAVE_TOP = 4.35, 4.6      # the slate eave's underside and the top of its edge
EAVE_CHAMFER = 0.1                  # the slopes start this far in from the eave line
PITCH = math.radians(45)
COURSES = 10                        # slate courses up each main slope, eave to ridge
COURSE_T = 0.12                     # the step at each course's lower edge (revision 3: 0.08)
COURSE_E = 0.02                     # each course's top edge stands this far above its bed
# Revision 4: tiles. Along every course a V notch every TILE, half a tile
# along from the course below's; the notch is JOINT_W wide across the top and
# its bottom JOINT_DZ below the tile, straight down, so each of its faces turns
# more than CREASE_DEG from the tile (the engine inks normals more than 40
# degrees apart). The course lip shows COURSE_T - COURSE_E (0.1 m, two pixels
# at 24 px/m from the game's camera); revision 3's 0.06 m lip was barely one.
TILE = 0.5
JOINT_W = 0.12
JOINT_DZ = 0.095
JOINT_MARGIN = 0.02                 # a notch keeps this far in from a hip
SKIRT = 0.09                        # each lip reaches this far below its bed, closing the notches above it
ROLL_JOINT_HW = 0.04                # ridge and hip rolls: a pinch every TILE, this half-width
CREASE_DEG = 40.0                   # checked: every notch face and lip turns more than this from its tile
PX_PER_M = 24.0                     # the game's pixel scale
GABLE_PITCH = math.radians(50)      # the cross-gable and the dormers, a little steeper
# The cross-gable over the door (its face in the front wall's plaster plane,
# facing -y) and one dormer on each long slope. hw: the face's half-width;
# rise: its eave above the main slates where the face meets them.
CROSS_GABLE = dict(name="Gable_Front", cx=0.0, face_y=Y0 + PROUD, out=-1, hw=1.4, rise=0.45,
                   ov_side=0.2, ov_front=0.25, courses=5, window=False)
DORMERS = [
    dict(name="Dormer_Front", cx=2.6, face_y=Y0 + 0.4, out=-1, hw=0.5, rise=0.85,
         ov_side=0.14, ov_front=0.18, courses=2, window=True),
    dict(name="Dormer_Back", cx=(X0 + X1) / 2, face_y=Y1 - 0.4, out=1, hw=0.5, rise=0.85,
         ov_side=0.14, ov_front=0.18, courses=2, window=True),
]
# Chimney, up the right-hand end wall, standing a little past the eaves
CHIMNEY_Y = (3.6, 5.2)
CHIMNEY_X = (X1 - 0.3, X1 + 0.75)
CHIMNEY_TOP = 8.2
# Sign, hung from a bracket off the front wall right of the door, on its own
# post (SIGN_POST) between the jamb and the window, clear of the jamb
SIGN_POST = (1.9, 2.15)
SIGN_X = (SIGN_POST[0] + SIGN_POST[1]) / 2
SIGN_Y = (-0.45, -1.65)             # the board's span out from the wall
SIGN_Z = (2.75, 3.75)
BRACKET_Z = 4.05
TRI_BUDGET = 14000                  # revision 5 (revision 4: 8,000)
PLINTH_SIZE = (9.0, 7.0)            # checked
MIN_VESTIBULE = 2.5                 # checked: the trigger zone's depth
HALL_TRI_BUDGET = 100               # revision 6: the Vestibule node stays under this
MIN_SIGN_TO_JAMB = 0.3              # checked: the sign's nearest part to the jamb's outer edge
# Revision 3's roof rules (checked): everything the new roof adds stays inside
# revision 2's eaves in plan, its top at most MAX_RISE above revision 2's
# ridge, and nothing of it over the doorway or vestibule below HEADROOM.
REV2_RIDGE_TOP = 9.13
MAX_RISE = 0.5
HEADROOM = 3.3
REV2_PLAN = (-4.55, 5.59, -1.77, 7.1)   # the whole exterior's plan outline (Blender x0, x1, y0, y1)
# Revision 5: masonry and render. The plinth (the threshold included) and
# the chimney are laid as individual Stone stones, each STONE_PROUD (0.03 to
# 0.05 m) proud of a Plaster mortar bed set BED behind the revision-4 outline,
# in courses with JOINT-wide joints (a pixel at 24 px/m), each course's
# vertical joints at least STAGGER from the course below's. No stone passes
# the revision-4 outline, so the footprint, plan and chimney figures hold.
JOINT = 0.06
BED = 0.05
STONE_PROUD = [0.05, 0.04, 0.045, 0.035]   # stone face over its bed, cycled
TOP_DROP = [0.0, 0.01, 0.02, 0.005]        # an exposed top course's stones sit this far under the top, cycled
STONE_T = 0.15                      # a facing stone's depth in from the outline
PLINTH_T = 0.22                     # the plinth's run under the timber face
STONE_LEN = [0.62, 0.78, 0.55, 0.70, 0.66, 0.85, 0.58, 0.74]  # stone lengths along a course, cycled
STONE_MIN = 0.25                    # the shortest stone
STAGGER = 0.12                      # checked
PLINTH_COURSES = [FOOT, -0.25, 0.10, PLINTH_TOP + 0.02]
# Plaster panels: each panel between the timbers is cut into a few facets,
# recessed at most FACET_MAX into the wall (never proud of the revision-4
# plaster face, so nothing grows), its edges at the face.
FACET_MAX = 0.03
FACET_MIN = 0.012
FACET_STRIP = 0.5                   # about one strip of four facets per this much of a panel's long side
# Spalled patches: where the render has fallen away, an irregular hole in a
# panel down to a mortar bed PATCH_DEPTH behind the face, the stones beneath
# standing STONE_PROUD off it (so still behind the render's face). Each on a
# panel the game's default camera faces, clear of the windows, the doorway
# and the sign. (wall, centre s, centre z, half-sizes s and z, seed)
PATCH_DEPTH = 0.08
PATCH_EDGE_MAX = 0.02               # the broken render's edge: recessed at most this
PATCH_COURSE = 0.19                 # stone course height (stone 0.13 + joint)
PATCH_LEN = [0.30, 0.25, 0.34, 0.27, 0.32, 0.24]
PATCHES = [
    ("front", -2.5, 1.0, 0.42, 0.21, 3),     # under the left front window
    ("front", 3.25, 3.72, 0.38, 0.20, 7),    # over the right front window
    ("front", 3.42, 0.99, 0.36, 0.20, 11),   # under the right front window, toward the corner
]
PATCH_COUNT = (3, 4)                # checked
PATCH_PANEL_MARGIN = 0.06           # checked: a patch keeps this inside its panel
PATCH_WINDOW_CLEAR = 0.15           # checked: from a window opening or its sill board
PATCH_DOOR_CLEAR = 0.3              # checked: from the doorway (jambs included) and the sign
PATCH_FACING = 0.5                  # checked: the wall's normal . the way to the default camera

ROOF_PARTS = ["Roof", CROSS_GABLE["name"]] + [d["name"] for d in DORMERS]
# Engine rectangles (assets.md / scene.cpp kTavernDoorway, kTavernVestibule) in Blender (x0, x1, y0, y1).
DOORWAY_RECT = (-1.1, 1.1, -0.85, 0.25)
VESTIBULE_RECT = (-1.9, 1.9, 0.25, PY1 - WALL_T)  # revision 6: the whole hall (was y 0.25 to 3.0)


def wall_point(wall, s, v, z):
    """A point on a wall: s along its run, v out from its plaster face, z up."""
    if wall == "front":
        return (s, PY0 - v, z)
    if wall == "back":
        return (s, PY1 + v, z)
    if wall == "left":
        return (PX0 - v, s, z)
    return (PX1 + v, s, z)


# Each wall's run: the plaster spans these, between the walls it meets.
PLASTER_RUN = {
    "front": (PX0, PX1),
    "back": (PX0, PX1),
    "left": (PY0 + WALL_T, PY1 - WALL_T),
    "right": (PY0 + WALL_T, PY1 - WALL_T),
}
# Timbers run between the corner posts.
TIMBER_RUN = {
    "front": (X0 + CORNER, X1 - CORNER),
    "back": (X0 + CORNER, X1 - CORNER),
    "left": (Y0 + CORNER, Y1 - CORNER),
    "right": (Y0 + CORNER, Y1 - CORNER),
}


def post_layout(wall):
    """A wall's posts: either side of each window (the doorway's jambs stand
    in for posts there), then fill so no panel is wider than 2.4 m. Returns
    every span (corners and jambs included) and the posts drawn on the wall."""
    t0, t1 = TIMBER_RUN[wall]
    fixed = [(t0 - CORNER, t0), (t1, t1 + CORNER)]
    for a, b in WINDOWS[wall]:
        fixed += [(a - POST_W, a), (b, b + POST_W)]
    if wall == "front":
        fixed += [(DOORWAY[0], DOORWAY[0] + JAMB), (DOORWAY[1] - JAMB, DOORWAY[1]), SIGN_POST]
    fixed.sort()
    fill = []
    for (a0, a1), (b0, b1) in zip(fixed, fixed[1:]):
        gap = b0 - a1
        panels = max(1, math.ceil(gap / 2.4 - 1e-6))
        width = (gap - (panels - 1) * POST_W) / panels
        for k in range(1, panels):
            s = a1 + k * width + (k - 1) * POST_W
            fill.append((s, s + POST_W))
    drawn = fill + [p for a, b in WINDOWS[wall] for p in ((a - POST_W, a), (b, b + POST_W))]
    if wall == "front":
        drawn.append(SIGN_POST)
    return sorted(set(fixed + fill)), drawn


def timber_posts(wall):
    """The spans of a wall's face covered by an upright timber: its drawn
    posts and the corner posts (not the jambs, which stop at the door head)."""
    _, drawn = post_layout(wall)
    lo, hi = (X0, X1) if wall in ("front", "back") else (Y0, Y1)
    corners = [(lo - (POST_PROUD - PROUD), lo + CORNER), (hi - CORNER, hi + (POST_PROUD - PROUD))]
    return sorted(drawn + corners)


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

    def hexa(self, c, mat, skip=()):
        """Six faces over eight corners: 0-3 the bottom (x0y0, x1y0, x1y1,
        x0y1, counter-clockwise from above), 4-7 the top in the same order."""
        vs = [self.bm.verts.new(p) for p in c]
        sides = {"-z": (0, 3, 2, 1), "+z": (4, 5, 6, 7), "-y": (0, 1, 5, 4), "+x": (1, 2, 6, 5),
                 "+y": (2, 3, 7, 6), "-x": (3, 0, 4, 7)}
        for key, idx in sides.items():
            if key not in skip:
                f = self.bm.faces.new([vs[i] for i in idx])
                f.material_index = self.mi(mat)

    def box(self, lo, hi, mat, skip=()):
        (x0, y0, z0), (x1, y1, z1) = lo, hi
        self.hexa([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                   (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)], mat, skip)

    def wall_box(self, wall, s0, s1, v0, v1, z0, z1, mat):
        a = wall_point(wall, s0, v0, z0)
        b = wall_point(wall, s1, v1, z1)
        self.box(tuple(min(a[i], b[i]) for i in range(3)), tuple(max(a[i], b[i]) for i in range(3)), mat)

    def wall_quad_prism(self, wall, quad, v0, v1, mat):
        """A prism through the wall's v from v0 to v1 over a quad (s, z) in the wall's plane."""
        front = [Vector(wall_point(wall, s, v1, z)) for s, z in quad]
        back = [Vector(wall_point(wall, s, v0, z)) for s, z in quad]
        out = Vector(wall_point(wall, 0, 1, 0)) - Vector(wall_point(wall, 0, 0, 0))
        self.face(front, mat, out)
        self.face(back, mat, -out)
        n = len(quad)
        for i in range(n):
            j = (i + 1) % n
            mid = (front[i] + front[j] + back[i] + back[j]) / 4
            centre = sum(front, Vector()) / n
            side = mid - centre
            side -= out * side.dot(out)
            self.face([front[i], front[j], back[j], back[i]], mat, side)

    def prism_z(self, outline, z0, z1, mat, top=True, bottom=True):
        """A vertical prism over a counter-clockwise outline (x, y)."""
        lo = [self.bm.verts.new((x, y, z0)) for x, y in outline]
        hi = [self.bm.verts.new((x, y, z1)) for x, y in outline]
        n = len(outline)
        for i in range(n):
            j = (i + 1) % n
            f = self.bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
            f.material_index = self.mi(mat)
        if top:
            self.bm.faces.new(hi).material_index = self.mi(mat)
        if bottom:
            self.bm.faces.new(list(reversed(lo))).material_index = self.mi(mat)

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


def roof_base(inset):
    """Height of the main roof's slate bed (under the courses) at `inset` in
    from the eave line. Each course stands up to COURSE_T above it."""
    return EAVE_TOP + (inset - EAVE_CHAMFER) * math.tan(PITCH)


EAVES = (X0 - EAVE_OUT, X1 + EAVE_OUT, Y0 - EAVE_OUT, Y1 + EAVE_OUT)


def roof_inset(x, y):
    ex0, ex1, ey0, ey1 = EAVES
    return min(x - ex0, ex1 - x, y - ey0, ey1 - y)


def roof_height(x, y):
    """The top of the main roof's slates over (x, y), at most (a course's step)."""
    return roof_base(roof_inset(x, y)) + COURSE_T


def face_normal(pts, facing):
    n = (pts[1] - pts[0]).cross(pts[2] - pts[0])
    if n.length < 1e-9:
        n = (pts[2] - pts[0]).cross(pts[3] - pts[0])
    n.normalize()
    return n if n.dot(Vector(facing)) >= 0 else -n


def angle_deg(a, b):
    return math.degrees(math.acos(max(-1.0, min(1.0, a.dot(b)))))


def roll(part, a, b, r, mat="Roof", tiled=None):
    """A triangular-section roll (ridge or hip tiles) from a to b. With
    `tiled` (a list for the record), cut into tiles: a pinch every TILE, its
    faces turned about 50 degrees from the roll's, so each joint is inked."""
    a, b = Vector(a), Vector(b)
    length = (b - a).length
    d = (b - a).normalized()
    side = d.cross(Vector((0, 0, 1)))
    if side.length < 1e-4:
        side = Vector((1, 0, 0))
    side.normalize()
    up = side.cross(d).normalized()
    if up.z < 0:
        up = -up
    prof = [side * r - up * r * 0.4, up * r * 0.75, -side * r - up * r * 0.4]
    stations = [(0.0, 1.0)]
    joints = []
    if tiled is not None:
        # Each side face lies 0.492 r from the axis; pinch it in by enough
        # to turn the notch's faces 50 degrees over ROLL_JOINT_HW.
        f = 1.0 - ROLL_JOINT_HW * math.tan(math.radians(50)) / (0.492 * r)
        s = TILE / 2
        while s + ROLL_JOINT_HW < length - 0.1:
            if s - ROLL_JOINT_HW > 0.1:
                joints.append(s)
                stations += [(s - ROLL_JOINT_HW, 1.0), (s, f), (s + ROLL_JOINT_HW, 1.0)]
            s += TILE
    stations.append((length, 1.0))
    rings = [[a + d * s + p * f for p in prof] for s, f in stations]
    faces = []
    for ra, rb in zip(rings, rings[1:]):
        row = []
        for i in range(2):
            facing = up + (side if i == 0 else -side)
            pts = [ra[i], rb[i], rb[i + 1], ra[i + 1]]
            part.face(pts, mat, facing)
            row.append(face_normal(pts, facing))
        faces.append(row)
    part.face(rings[0], mat, -d)
    part.face(rings[-1], mat, d)
    if tiled is not None:
        # stations: 0, then (in, pinch, out) per joint: faces 3j+1 and 3j+2
        # are the notch's, 3j and 3j+3 the tiles either side.
        angles = []
        for j in range(len(joints)):
            for i in range(2):
                tile = faces[3 * j][i]
                angles += [angle_deg(tile, faces[3 * j + 1][i]), angle_deg(tile, faces[3 * j + 2][i])]
        tiled.append({"length": length, "joints": joints, "angles": angles, "r": r})


# --------------------------------------------------------------------------
# Revision 5: masonry, faceted render and spalled patches


def stable_rng(*key):
    """A random stream fixed by `key` (numbers and strings), the same every build."""
    import random
    seed = 7
    for k in key:
        if isinstance(k, str):
            v = sum(ord(c) * (i + 1) for i, c in enumerate(k))
        else:
            v = int(round(k * 1000))
        seed = (seed * 1000003 + v) % (2 ** 31)
    return random.Random(seed)


def inner_key(face):
    """The box side of a stone on `face` that faces into the bed."""
    return ("+" if face["out"] < 0 else "-") + face["axis"]


def face_box(face, u0, u1, z0, z1, d0, d1):
    """A box on a vertical face: u along the face, z up, d in from the
    face's outline (d0 < d1)."""
    c, o = face["coord"], face["out"]
    n0, n1 = c - o * d0, c - o * d1
    lo_n, hi_n = min(n0, n1), max(n0, n1)
    if face["axis"] == "y":
        return (u0, lo_n, z0), (u1, hi_n, z1)
    return (lo_n, u0, z0), (hi_n, u1, z1)


def lay_courses(part, faces, zs, records, bottom_gap=0.0, top_gap=0.0, top_open=False, forbid=None):
    """Lay coursed stones on vertical faces that share courses (one
    structure). zs: the course lines, bottom to top. Each face is a dict:
    name; axis ("x" or "y") and coord, the plane of its outline; out (+-1),
    which way it faces; u, the run (along y for an x face, x for a y face);
    ends, "corner" or "flat" at each end of u; phase (0 or 1); T, the
    stones' depth in from the outline. At a corner, faces take turns course
    by course: a face owns the corner on courses where (course + phase) is
    even, its end stone running to the corner; otherwise its stones stop a
    joint short of the other face's stone. Each stone stands STONE_PROUD off
    the bed (BED in from the outline), a JOINT between stones and courses,
    every course's vertical joints at least STAGGER from the course below's
    (and from the corner joints of the course above). With top_open the top
    course's stones sit TOP_DROP under the top line, so the bed shows in
    their joints from above too. `forbid` (face name -> joint positions)
    seeds the stagger of the first course. Records each course."""
    n = len(zs) - 1
    prev = {f["name"]: list((forbid or {}).get(f["name"], [])) for f in faces}

    def ends_for(f, k):
        if k >= n:
            return None, None, []
        own = (k + f["phase"]) % 2 == 0
        ua, ub = f["u"]
        a = ua + (f["T"] + JOINT if f["ends"][0] == "corner" and not own else 0.0)
        b = ub - (f["T"] + JOINT if f["ends"][1] == "corner" and not own else 0.0)
        js = ([a - JOINT / 2] if a > ua + 1e-9 else []) + ([b + JOINT / 2] if b < ub - 1e-9 else [])
        return a, b, js
    for k in range(n):
        z0 = zs[k] + (JOINT / 2 if k > 0 else bottom_gap)
        z1 = zs[k + 1] - (JOINT / 2 if k < n - 1 else top_gap)
        for fi, f in enumerate(faces):
            a, b, corner_js = ends_for(f, k)
            next_js = ends_for(f, k + 1)[2]
            forbidden = prev[f["name"]] + next_js
            cuts = []
            pos, i = a, k * 3 + fi * 5
            while True:
                c = pos + STONE_LEN[i % len(STONE_LEN)]
                i += 1
                tries = 0
                while any(abs(c - q) < STAGGER for q in forbidden) and tries < 40:
                    c += 0.03
                    tries += 1
                if b - c < STONE_MIN + JOINT / 2:
                    break
                cuts.append(c)
                pos = c
            edges = [a] + [x for c in cuts for x in (c - JOINT / 2, c + JOINT / 2)] + [b]
            stones = []
            for j in range(0, len(edges), 2):
                u0, u1 = edges[j], edges[j + 1]
                idx = k + j // 2 + fi
                proud = STONE_PROUD[idx % len(STONE_PROUD)]
                top = z1 - (TOP_DROP[idx % len(TOP_DROP)] if top_open and k == n - 1 else 0.0)
                lo, hi = face_box(f, u0, u1, z0, top, BED - proud, f["T"])
                part.box(lo, hi, "Stone", skip=(inner_key(f), "-z"))
                stones.append({"u": (u0, u1), "z": (z0, top), "proud": proud})
            records.append({"face": f["name"], "course": k, "z": (z0, z1), "stones": stones,
                            "joints": sorted(cuts + corner_js), "top": top_open and k == n - 1})
            prev[f["name"]] = cuts + corner_js
    return prev


def lay_slabs(part, x0, x1, ys, z0, z1, records, name, out_edges, forbid=()):
    """Flat-topped slabs (a threshold, a hearth) over x0..x1, in rows between
    the lines ys (along y), their tops flush at z1 and their bottoms at z0.
    A JOINT between rows and between slabs, each row's joints at least
    STAGGER from the row before's (the first row's from `forbid`). Slab
    edges on out_edges ("-y", "+y", "-x", "+x": the outline sides) stand
    STONE_PROUD off a bed BED in; other edges run to the line."""
    prev = list(forbid)
    for r in range(len(ys) - 1):
        ya = ys[r] + (JOINT / 2 if r > 0 else 0.0)
        yb = ys[r + 1] - (JOINT / 2 if r < len(ys) - 2 else 0.0)
        cuts = []
        pos, i = x0, r * 3 + 2
        while True:
            c = pos + STONE_LEN[i % len(STONE_LEN)] * 1.15
            i += 1
            tries = 0
            while any(abs(c - q) < STAGGER for q in prev) and tries < 40:
                c += 0.03
                tries += 1
            if x1 - c < STONE_MIN + JOINT / 2:
                break
            cuts.append(c)
            pos = c
        edges = [x0] + [x for c in cuts for x in (c - JOINT / 2, c + JOINT / 2)] + [x1]
        stones = []
        for j in range(0, len(edges), 2):
            u0, u1 = edges[j], edges[j + 1]
            proud = STONE_PROUD[(r + j // 2) % len(STONE_PROUD)]
            nud = BED - proud
            lo = [u0 + (nud if "-x" in out_edges and j == 0 else 0.0),
                  ya + (nud if "-y" in out_edges and r == 0 else 0.0), z0]
            hi = [u1 - (nud if "+x" in out_edges and j == len(edges) - 2 else 0.0),
                  yb - (nud if "+y" in out_edges and r == len(ys) - 2 else 0.0), z1]
            part.box(tuple(lo), tuple(hi), "Stone", skip=("-z",))
            stones.append({"u": (lo[0], hi[0]), "y": (lo[1], hi[1]), "z": (z0, z1), "proud": proud})
        records.append({"face": name, "course": r, "z": (z0, z1), "stones": stones, "joints": sorted(cuts),
                        "row": (ya, yb), "slab": True})
        prev = cuts
    return prev


def lay_plane(part, quad, n, courses, records, name, T=STONE_T):
    """Stones on a sloped, planar face (the chimney's shoulder): quad is its
    corners (bottom-left, bottom-right, top-right, top-left), n its outward
    normal. Courses up the slant, a JOINT between them and along each; the
    stones keep JOINT / 2 in from the face's edges, so a mortar line runs
    along each hip. Each stone stands STONE_PROUD off the bed (the face
    moved BED in)."""
    BL, BR, TR, TL = [Vector(p) for p in quad]
    n = Vector(n).normalized()

    def P(su, t):
        return (BL.lerp(BR, su)).lerp(TL.lerp(TR, su), t)
    slant = ((TL + TR) / 2 - (BL + BR) / 2).length
    dt = JOINT / 2 / slant
    for c in range(courses):
        t0 = c / courses + dt
        t1 = (c + 1) / courses - dt
        w0 = (P(1, t0) - P(0, t0)).length
        w1 = (P(1, t1) - P(0, t1)).length
        wm = (w0 + w1) / 2
        k = max(1, round(wm / 0.6))
        fr = [(i + (0.5 if c % 2 else 0.0)) / k for i in range(1, k + (1 if c % 2 else 0))]
        fr = [f for f in fr if 0.0 < f < 1.0]
        edges0 = [JOINT / 2 / w0] + [x for f in fr for x in (f - JOINT / 2 / w0, f + JOINT / 2 / w0)] + [1 - JOINT / 2 / w0]
        edges1 = [JOINT / 2 / w1] + [x for f in fr for x in (f - JOINT / 2 / w1, f + JOINT / 2 / w1)] + [1 - JOINT / 2 / w1]
        stones = []
        for j in range(0, len(edges0), 2):
            proud = STONE_PROUD[(c + j // 2) % len(STONE_PROUD)]
            face_pts = [P(edges0[j], t0), P(edges0[j + 1], t0), P(edges1[j + 1], t1), P(edges1[j], t1)]
            outer = [p - n * (BED - proud) for p in face_pts]
            inner = [p - n * T for p in face_pts]
            part.hexa(inner + outer, "Stone")
            stones.append({"u": (edges0[j] * wm, edges0[j + 1] * wm), "z": (t0 * slant, t1 * slant), "proud": proud})
        records.append({"face": name, "course": c, "z": (t0 * slant, t1 * slant), "stones": stones,
                        "joints": [f * wm for f in fr], "plane": True})


def facet_rect(part, P, out, s0, s1, z0, z1, rng, mat="Plaster"):
    """A plaster panel s0..s1 x z0..z1 cut into gentle facets: strips across
    its long side, slanted a little, each fanned into four facets round a
    point recessed FACET_MIN to FACET_MAX. P(s, z, d) maps the panel (d
    into the wall) to Blender. The panel's edges stay on its face. Returns
    the deepest recess and the facet normals."""
    w, h = s1 - s0, z1 - z0
    along_s = w >= h
    m = max(1, round((w if along_s else h) / FACET_STRIP))
    lines = [(0.0, 0.0)]
    for i in range(1, m):
        lines.append((i / m + rng.uniform(-0.2, 0.2) / m, i / m + rng.uniform(-0.2, 0.2) / m))
    lines.append((1.0, 1.0))

    def pt(fu, fv, d):
        if along_s:
            return Vector(P(s0 + fu * w, z0 + fv * h, d))
        return Vector(P(s0 + fv * w, z0 + fu * h, d))
    deepest, normals = 0.0, []
    for i in range(m):
        (a0, a1), (b0, b1) = lines[i], lines[i + 1]
        quad = [(a0, 0.0), (b0, 0.0), (b1, 1.0), (a1, 1.0)]
        cu = (a0 + b0 + a1 + b1) / 4 + rng.uniform(-0.15, 0.15) * ((b0 + b1) - (a0 + a1)) / 2
        cv = 0.5 + rng.uniform(-0.2, 0.2)
        depth = rng.uniform(FACET_MIN, FACET_MAX)
        deepest = max(deepest, depth)
        corners = [pt(fu, fv, 0.0) for fu, fv in quad]
        c = pt(cu, cv, depth)
        for k in range(4):
            tri = [corners[k], corners[(k + 1) % 4], c]
            part.face(tri, mat, out)
            normals.append(face_normal(tri + [c], out))
    return deepest, normals


def facet_tri(part, P, out, a, b, c, rng, mat="Plaster"):
    """A triangular plaster panel (corners (s, z)) fanned into three facets
    round a recessed point."""
    w = rng.uniform(0.25, 0.42)
    v = rng.uniform(0.25, 0.42)
    centre = (a[0] + (b[0] - a[0]) * w + (c[0] - a[0]) * v, a[1] + (b[1] - a[1]) * w + (c[1] - a[1]) * v)
    depth = rng.uniform(FACET_MIN, FACET_MAX)
    pc = Vector(P(centre[0], centre[1], depth))
    corners = [Vector(P(x, z, 0.0)) for x, z in (a, b, c)]
    normals = []
    for k in range(3):
        tri = [corners[k], corners[(k + 1) % 3], pc]
        part.face(tri, mat, out)
        normals.append(face_normal(tri + [pc], out))
    return depth, normals


def point_in_poly(p, poly):
    inside = False
    n = len(poly)
    for i in range(n):
        (x0, y0), (x1, y1) = poly[i], poly[(i + 1) % n]
        if (y0 > p[1]) != (y1 > p[1]):
            if p[0] < x0 + (p[1] - y0) * (x1 - x0) / (y1 - y0):
                inside = not inside
    return inside


def patch_outline(cs, cz, rs, rz, rng):
    """An irregular outline round (cs, cz), about rs by rz: eleven points
    at uneven angles and radii, a few bitten in."""
    pts = []
    n = 11
    for k in range(n):
        a = math.tau * k / n + rng.uniform(-0.12, 0.12)
        r = rng.uniform(0.78, 1.08) * (0.8 if k % 4 == 1 else 1.0)
        pts.append((cs + rs * r * math.cos(a), cz + rz * r * math.sin(a)))
    return pts


def spalled_panel(part, P, out, cell, outline, rng, mat_render="Plaster"):
    """A panel (cell s0, s1, z0, z1) where the render has fallen away inside
    `outline` (s, z): the render round it, faceted by the triangulation
    between the panel's edge and the hole, its broken edge recessed up to
    PATCH_EDGE_MAX; the hole's sides down to a mortar bed (Plaster)
    PATCH_DEPTH behind the face; and the stones beneath in courses, each
    STONE_PROUD off the bed, JOINT apart. Returns the patch's record."""
    from mathutils.geometry import tessellate_polygon
    s0, s1, z0, z1 = cell
    rect = [(s0, z0), ((s0 + s1) / 2, z0), (s1, z0), (s1, (z0 + z1) / 2), (s1, z1), ((s0 + s1) / 2, z1),
            (s0, z1), (s0, (z0 + z1) / 2)]
    edge_d = [rng.uniform(0.0, PATCH_EDGE_MAX) for _ in outline]
    tris = tessellate_polygon([[(x, z, 0.0) for x, z in rect], [(x, z, 0.0) for x, z in outline]])
    pts = rect + outline
    depth = [0.0] * len(rect) + edge_d
    normals = []
    for i, j, k in tris:
        tri = [Vector(P(pts[q][0], pts[q][1], depth[q])) for q in (i, j, k)]
        part.face(tri, mat_render, out)
        normals.append(face_normal(tri + [tri[0]], out))
    cs = sum(x for x, _ in outline) / len(outline)
    cz = sum(z for _, z in outline) / len(outline)
    n = len(outline)
    for k in range(n):
        j = (k + 1) % n
        (xa, za), (xb, zb) = outline[k], outline[j]
        quad = [Vector(P(xa, za, edge_d[k])), Vector(P(xb, zb, edge_d[j])), Vector(P(xb, zb, PATCH_DEPTH)),
                Vector(P(xa, za, PATCH_DEPTH))]
        inward = Vector(P(cs, cz, 0.0)) - Vector(P((xa + xb) / 2, (za + zb) / 2, 0.0))
        part.face(quad, mat_render, inward)
    part.face([Vector(P(x, z, PATCH_DEPTH)) for x, z in outline], "Plaster", out)
    lo_s, hi_s = min(x for x, _ in outline), max(x for x, _ in outline)
    lo_z, hi_z = min(z for _, z in outline), max(z for _, z in outline)
    stones = []
    row = 0
    # Courses centred on the patch, a bed joint at its middle; the stones'
    # joints staggered row to row.
    zc = cz - PATCH_COURSE * math.ceil((cz - lo_z) / PATCH_COURSE)
    while zc < hi_z:
        za, zb = zc + JOINT / 2, zc + PATCH_COURSE - JOINT / 2
        u = cs - PATCH_LEN[0] * (1.0 if row % 2 else 0.5)
        while u > lo_s:
            u -= PATCH_LEN[0]
        i = row * 2
        while u < hi_s:
            L = PATCH_LEN[i % len(PATCH_LEN)]
            i += 1
            ua, ub = u + JOINT / 2, u + L - JOINT / 2
            u += L
            samples = [(ua + (ub - ua) * fx, za + (zb - za) * fz) for fx in (0.15, 0.5, 0.85) for fz in (0.2, 0.8)]
            if sum(point_in_poly(q, outline) for q in samples) < 2:
                continue
            proud = STONE_PROUD[(row + i) % len(STONE_PROUD)]
            a = Vector(P(ua, za, PATCH_DEPTH - proud))
            b = Vector(P(ub, zb, PATCH_DEPTH + 0.1))
            part.box(tuple(min(a[q], b[q]) for q in range(3)), tuple(max(a[q], b[q]) for q in range(3)), "Stone")
            stones.append({"u": (ua, ub), "z": (za, zb), "proud": proud,
                           "centre_in": point_in_poly(((ua + ub) / 2, (za + zb) / 2), outline),
                           "centre": Vector(P((ua + ub) / 2, (za + zb) / 2, PATCH_DEPTH - proud))})
        zc += PATCH_COURSE
        row += 1
    return {"cell": cell, "outline": outline, "edge_max": max(edge_d), "stones": stones, "normals": normals}


def prism_uz(part, outline, v0, v1, mat, P, out):
    """A prism over an outline (u, z) in a gable's face plane, from v0 to v1
    (v into the roof). P maps (u, v, z) to Blender; `out` is the face's
    outward direction."""
    front = [P(u, v0, z) for u, z in outline]
    back = [P(u, v1, z) for u, z in outline]
    part.face(front, mat, out)
    part.face(back, mat, -out)
    n = len(outline)
    area = sum(outline[i][0] * outline[(i + 1) % n][1] - outline[(i + 1) % n][0] * outline[i][1] for i in range(n))
    for i in range(n):
        j = (i + 1) % n
        du, dz = outline[j][0] - outline[i][0], outline[j][1] - outline[i][1]
        nu, nz = (dz, -du) if area > 0 else (-dz, du)
        part.face([front[i], front[j], back[j], back[i]], mat, (nu, 0.0, nz))


def gable_frame(g):
    """The gable's or dormer's local frame and heights: u across the face,
    v into the roof, z up."""
    cx, face_y, out = g["cx"], g["face_y"], g["out"]

    def P(u, v, z):
        return Vector((cx + u, face_y - out * v, z))
    tp = math.tan(GABLE_PITCH)
    seat = roof_base(roof_inset(cx, face_y))      # the main slate bed where the face meets it
    eave = seat + g["rise"]
    ridge = eave + g["hw"] * tp                   # the gable roof's underside at the ridge
    we = g["hw"] + g["ov_side"]

    def zb(u):
        return ridge - abs(u) * tp
    # Run the gable roof back until its ridge and both eaves are buried under
    # the main slates.
    depth = 0.3
    while depth < 6.0:
        ok = all(roof_base(roof_inset(*P(u, depth, 0).xy)) > z
                 for u, z in ((0.0, ridge + 0.3), (-we, zb(we) + COURSE_T + 0.1), (we, zb(we) + COURSE_T + 0.1)))
        if ok:
            break
        depth += 0.05
    return P, Vector((0, out, 0)), seat, eave, ridge, we, zb, depth


def build_gable(part, g, tiling=None, roll_tiling=None, panels=None):
    """A gable on the roof: a Plaster body under a slated gable roof in
    stepped courses with a ridge roll; Wood framing on its face (or a dark
    window for a dormer); carved bargeboards with stepped notches under the
    rake, a pendant at the apex and a finial above it."""
    P, OUT, seat, eave, ridge, we, zb, depth = gable_frame(g)
    hw = g["hw"]
    foot = seat - 0.15
    t, e = COURSE_T, COURSE_E
    v0 = -g["ov_front"]
    # Body: the face and cheeks, just under the roof's underside. Revision 5:
    # the face and both cheeks are faceted plaster panels (recessed at most
    # FACET_MAX into the body); the back, bottom and rakes, under the roof,
    # stay flat. The outline is revision 4's.
    b = 0.03
    outline = [(-hw, foot), (hw, foot), (hw, eave - b), (0.0, ridge - b), (-hw, eave - b)]
    back = [P(u, depth, z) for u, z in outline]
    part.face(back, "Plaster", -OUT)
    for i in (0, 2, 3):  # the bottom and the two rakes
        j = (i + 1) % 5
        du, dz = outline[j][0] - outline[i][0], outline[j][1] - outline[i][1]
        part.face([P(outline[i][0], 0.0, outline[i][1]), P(outline[j][0], 0.0, outline[j][1]),
                   P(outline[j][0], depth, outline[j][1]),
                   P(outline[i][0], depth, outline[i][1])], "Plaster", (dz, 0.0, -du))
    rng = stable_rng(g["name"], "body")
    recs = []

    def PF(u, z, d):  # the face: d into the body
        return P(u, d, z)
    recs.append(facet_rect(part, PF, OUT, -hw, hw, foot, eave - b, rng))
    recs.append(facet_tri(part, PF, OUT, (-hw, eave - b), (0.0, eave - b), (0.0, ridge - b), rng))
    recs.append(facet_tri(part, PF, OUT, (0.0, eave - b), (hw, eave - b), (0.0, ridge - b), rng))
    for sg in (-1, 1):
        def PC(v, z, d, sg=sg):  # a cheek: d in toward the middle
            return P(sg * (hw - d), v, z)
        recs.append(facet_rect(part, PC, Vector(P(sg, 0, 0)) - Vector(P(0, 0, 0)), 0.0, depth, foot, eave - b, rng))
    if panels is not None:
        for k, (deep, normals) in enumerate(recs):
            panels.append({"part": g["name"], "wall": ["face", "face apex left", "face apex right", "cheek left",
                                                       "cheek right"][k], "depth": deep, "normals": normals,
                           "patched": False})
    # Slates: courses up each side, a lip at each course's lower edge, the
    # front end of each course, an underside, and a ridge roll. Revision 4:
    # each course cut into tiles along v by V notches, half a tile along from
    # the course below's; a notch that would be buried under the main slates
    # is left out. Where a notch's bottom dips below the next course's lip,
    # a small cap closes it.
    n = g["courses"]
    for sg in (-1, 1):
        us = [sg * we * (1 - k / n) for k in range(n + 1)]
        part.face([P(us[0], v0, zb(us[0])), P(us[0], depth, zb(us[0])), P(0, depth, ridge), P(0, v0, ridge)],
                  "Roof", (0, 0, -1))
        for k in range(n):
            ua, ub = us[k], us[k + 1]
            za, zu = zb(ua), zb(ub)
            facing = (sg * 0.5, 0, 1)
            part.face([P(ua, v0, za), P(ua, depth, za), P(ua, depth, za + t), P(ua, v0, za + t)], "Roof", (sg, 0, -0.2))
            joints = []
            for jj in range(0, 40):
                vj = v0 + TILE / 2 + (jj + 0.5 * (k % 2)) * TILE
                if vj + JOINT_W / 2 > depth - JOINT_MARGIN:
                    break
                buried = all(roof_base(roof_inset(*P(u, vj, 0).xy)) > z for u, z in ((ua, za + t), (ub, zu + e)))
                if not buried:
                    joints.append(vj)
            cuts = [(v0, 0.0)]
            for vj in joints:
                cuts += [(vj - JOINT_W / 2, 0.0), (vj, JOINT_DZ), (vj + JOINT_W / 2, 0.0)]
            cuts.append((depth, 0.0))
            normals = []
            for (va, da), (vb, db) in zip(cuts, cuts[1:]):
                pts = [P(ua, va, za + t - da), P(ua, vb, za + t - db), P(ub, vb, zu + e - db), P(ub, va, zu + e - da)]
                part.face(pts, "Roof", facing)
                normals.append(face_normal(pts, facing))
            if k < n - 1:
                h = JOINT_W / 2 * (JOINT_DZ - e) / JOINT_DZ
                for vj in joints:
                    part.face([P(ub, vj - h, zu), P(ub, vj, zu + e - JOINT_DZ), P(ub, vj + h, zu)], "Roof", (sg, 0, 0))
            part.face([P(ua, v0, za), P(ua, v0, za + t), P(ub, v0, zu + e), P(ub, v0, zu)], "Roof", OUT)
            tile_n = normals[0]
            angles = []
            for jj in range(len(joints)):
                angles += [angle_deg(tile_n, normals[3 * jj + 1]), angle_deg(tile_n, normals[3 * jj + 2])]
            mv = (ua + ub) / 2
            mz = (za + t + zu + e) / 2
            if tiling is not None:
                tiling.append({
                    "part": g["name"], "slope": "left" if sg < 0 else "right", "course": k, "joints": joints,
                    "upper": (v0, depth), "angles": angles,
                    "lip_angle": angle_deg(tile_n, Vector((sg, 0, 0))), "tile_normal": tile_n,
                    "notch": [(normals[3 * jj + 1], normals[3 * jj + 2]) for jj in range(len(joints))],
                    "groove_pts": [(P(mv, vj - JOINT_W / 2, mz), P(mv, vj, mz - JOINT_DZ), P(mv, vj + JOINT_W / 2, mz))
                                   for vj in joints],
                    "lip": (P(ua, v0, za + t), P(ua, depth, za + t), t - e),
                })
    roll(part, P(0, v0, ridge + e + 0.02), P(0, depth, ridge + e + 0.02), 0.13,
         tiled=None if roll_tiling is None else roll_tiling.setdefault(g["name"] + " ridge", []))
    # Face timbers (proud of the plaster) or the dormer's window.
    tv0, tv1 = -0.06, 0.0
    for sg in (-1, 1):
        prism_uz(part, [(sg * (hw - 0.14), foot), (sg * hw, foot), (sg * hw, eave - b), (sg * (hw - 0.14), eave - b)],
                 tv0, tv1, "Wood", P, OUT)
    if g["window"]:
        sill = seat + 0.15
        lintel = eave - 0.17
        prism_uz(part, [(-hw, foot), (hw, foot), (hw, sill), (-hw, sill)], tv0, tv1, "Wood", P, OUT)
        prism_uz(part, [(-hw, lintel), (hw, lintel), (hw, eave - b), (-hw, eave - b)], tv0, tv1, "Wood", P, OUT)
        prism_uz(part, [(-(hw - 0.14), sill), (hw - 0.14, sill), (hw - 0.14, lintel), (-(hw - 0.14), lintel)],
                 -0.03, 0.0, "Dirt", P, OUT)
        prism_uz(part, [(-0.04, sill), (0.04, sill), (0.04, lintel), (-0.04, lintel)], tv0, -0.03, "Wood", P, OUT)
        post = [(-0.06, eave - b), (0.06, eave - b), (0.06, zb(0.06) - 0.05), (0.0, ridge - 0.05),
                (-0.06, zb(0.06) - 0.05)]
        prism_uz(part, post, tv0, tv1, "Wood", P, OUT)
    else:
        beam = eave - 0.2
        prism_uz(part, [(-hw, beam), (hw, beam), (hw, eave - b), (-hw, eave - b)], tv0, tv1, "Wood", P, OUT)
        post = [(-0.08, eave - b), (0.08, eave - b), (0.08, zb(0.08) - 0.05), (0.0, ridge - 0.05),
                (-0.08, zb(0.08) - 0.05)]
        prism_uz(part, post, tv0, tv1, "Wood", P, OUT)
        hb = (ridge - eave) * 0.45
        for sg in (-1, 1):
            quad = [(sg * (hw - 0.2), eave - b), (sg * (hw - 0.45), eave - b), (sg * 0.08, eave + hb),
                    (sg * 0.08, eave + hb + 0.25)]
            prism_uz(part, quad, tv0, tv1, "Wood", P, OUT)
    # Bargeboards: a board under each rake, its lower edge carved in steps
    # (tooth, notch, tooth...), ending in a short tail at the eave.
    bv0, bv1 = -0.11, -0.06
    depth_b = 0.26 if hw > 1 else 0.18
    notch = 0.09 if hw > 1 else 0.06
    tooth, gap = (0.22, 0.13) if hw > 1 else (0.16, 0.1)
    s_end = we - 0.02
    for sg in (-1, 1):
        top = [(0.0, ridge - 0.01), (s_end, zb(s_end) - 0.01)]
        bottom = [(s_end, zb(s_end) - depth_b - 0.12)]  # the tail at the eave
        s, deep = s_end - 0.1, True
        bottom.append((s, zb(s) - depth_b))
        while True:
            s2 = s - (tooth if deep else gap)
            if s2 < 0.12:
                break
            d, d2 = (depth_b, depth_b - notch) if deep else (depth_b - notch, depth_b)
            bottom += [(s2, zb(s2) - d), (s2, zb(s2) - d2)]
            s, deep = s2, not deep
        bottom.append((0.0, ridge - (depth_b if deep else depth_b - notch)))
        pts = [(sg * u, z) for u, z in top + bottom]
        # top goes apex -> eave, bottom eave -> apex: one loop.
        prism_uz(part, pts, bv0, bv1, "Wood", P, OUT)
    # Pendant under the apex and the finial above it.
    dz = ridge - depth_b
    prism_uz(part, [(-0.06, dz + 0.02), (0.06, dz + 0.02), (0.06, dz - 0.22), (0.0, dz - 0.36), (-0.06, dz - 0.22)],
             bv0, bv1, "Wood", P, OUT)
    vc, sh = (bv0 + bv1) / 2, 0.045
    base = ridge - 0.1
    lo, hi = P(-sh, vc - sh, base), P(sh, vc + sh, ridge + 0.42)
    part.box(tuple(min(lo[i], hi[i]) for i in range(3)), tuple(max(lo[i], hi[i]) for i in range(3)), "Wood")
    k = 0.09 if hw > 1 else 0.07
    zb_, zm, zt = ridge + 0.36, ridge + 0.54, ridge + (0.85 if hw > 1 else 0.72)
    ring = [P(k, vc, zm), P(0, vc + k, zm), P(-k, vc, zm), P(0, vc - k, zm)]
    cb, ct = P(0, vc, zb_), P(0, vc, zt)
    centre = P(0, vc, zm)
    for i in range(4):
        j = (i + 1) % 4
        mid = (ring[i] + ring[j]) / 2 - centre
        part.face([ring[i], ring[j], ct], "Wood", mid + Vector((0, 0, 0.5)))
        part.face([ring[j], ring[i], cb], "Wood", mid - Vector((0, 0, 0.5)))
    return {"face_y": g["face_y"], "seat": seat, "eave": eave, "ridge": ridge, "depth": depth,
            "finial_top": zt, "we": we}


class Builder:
    STEPS = ["start", "foundation", "walls", "timbers", "windows", "doorway", "roof", "gables", "chimney", "sign"]

    def __init__(self):
        self.coll = None
        self.posts = {}  # wall -> [(s0, s1)] post spans, for the braces and the sign
        self.gable_figs = {}  # gable or dormer name -> its heights (build_gable)
        self.tiling = []  # one record per slate course per slope (revision 4's tiles)
        self.roll_tiling = {}  # roll name -> [its tiling record]
        self.masonry = []  # revision 5: one record per course of stones (or row of slabs)
        self.beds = []     # each structure's mortar bed top, for the proudness check
        self.panels = []   # every faceted plaster panel
        self.patches = []  # the spalled patches
        self.envelopes = {}  # part -> revision 4's boxes, which no stone passes

    def start(self):
        """Fresh Tavern_Exterior collection; the rest of the scene is left alone."""
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

    # ---- stone ----------------------------------------------------------

    def foundation(self):
        """The plinth round the walls, top about PLINTH_TOP, bottom at FOOT,
        and the threshold stone through the doorway flush with the floor.
        Revision 5: laid as individual stones in three courses, proud of a
        Plaster mortar bed BED inside the revision-4 outline (which no stone
        passes), joints staggered course to course and the courses taking
        turns at each corner. The threshold is three slabs, flush with the
        floor, over a course of stones."""
        part = Part("Foundation")
        ox0, ox1 = X0 - PLINTH_OUT, X1 + PLINTH_OUT
        oy0, oy1 = Y0 - PLINTH_OUT, Y1 + PLINTH_OUT
        depth = (PY0 + WALL_T) - oy0  # flush with the walls' inside faces
        dl, dr = DOORWAY
        bed_top = PLINTH_TOP + 0.02 - BED  # under the stones' tops by 0.03 to 0.05
        # The mortar bed: revision 4's runs, BED in from the outline.
        part.box((ox0 + BED, oy0 + BED, FOOT), (dl, oy0 + depth, bed_top), "Plaster")
        part.box((dr, oy0 + BED, FOOT), (ox1 - BED, oy0 + depth, bed_top), "Plaster")
        part.box((ox0 + BED, oy1 - depth, FOOT), (ox1 - BED, oy1 - BED, bed_top), "Plaster")
        part.box((ox0 + BED, oy0 + depth, FOOT), (ox0 + depth, oy1 - depth, bed_top), "Plaster")
        part.box((ox1 - depth, oy0 + depth, FOOT), (ox1 - BED, oy1 - depth, bed_top), "Plaster")
        T = PLINTH_T
        faces = [
            dict(name="plinth front-left", axis="y", coord=oy0, out=-1, u=(ox0, dl), ends=("corner", "flat"), phase=0, T=T),
            dict(name="plinth front-right", axis="y", coord=oy0, out=-1, u=(dr, ox1), ends=("flat", "corner"), phase=0, T=T),
            dict(name="plinth back", axis="y", coord=oy1, out=1, u=(ox0, ox1), ends=("corner", "corner"), phase=0, T=T),
            dict(name="plinth left", axis="x", coord=ox0, out=-1, u=(oy0, oy1), ends=("corner", "corner"), phase=1, T=T),
            # The right end, either side of the chimney's base (which stands over it).
            dict(name="plinth right-front", axis="x", coord=ox1, out=1, u=(oy0, CHIMNEY_Y[0]), ends=("corner", "flat"),
                 phase=1, T=T),
            dict(name="plinth right-back", axis="x", coord=ox1, out=1, u=(CHIMNEY_Y[1], oy1), ends=("flat", "corner"),
                 phase=1, T=T),
        ]
        lay_courses(part, faces, PLINTH_COURSES, self.masonry, top_open=True)
        self.beds.append({"part": "Foundation", "top": bed_top, "faces": [f["name"] for f in faces]})
        # Threshold: a course of stones on its face, then three slabs over
        # it, their tops flush with the floor (y = 0).
        ty0 = oy0 - 0.12
        slab_lo = -0.25
        part.box((dl + BED, ty0 + BED, FOOT), (dr - BED, PY0 + WALL_T, -BED), "Plaster")
        tface = dict(name="threshold face", axis="y", coord=ty0, out=-1, u=(dl, dr), ends=("flat", "flat"), phase=0,
                     T=STONE_T)
        below = lay_courses(part, [tface], [FOOT, slab_lo], self.masonry, top_gap=JOINT / 2)
        lay_slabs(part, dl, dr, [ty0, PY0 + WALL_T], slab_lo + JOINT / 2, 0.0, self.masonry, "threshold slabs",
                  ("-y",), forbid=below["threshold face"])
        self.beds.append({"part": "Foundation", "top": -BED, "faces": ["threshold slabs"]})
        self.envelopes["Foundation"] = [((ox0, oy0, FOOT), (ox1, oy1, PLINTH_TOP + 0.02)),
                                        ((dl, ty0, FOOT), (dr, PY0 + WALL_T, 0.0))]
        part.finish(self.coll)

    # ---- plaster ----------------------------------------------------------

    def walls(self):
        """Plaster walls from the plinth to the wall plate, with real holes
        for the windows and the doorway (the panes sit back in them).
        Revision 5: each wall slab's outer face is built panel by panel -
        flat where a timber covers it, every plaster panel between the
        timbers cut into gentle facets recessed at most FACET_MAX - and on
        PATCHES panels a spalled patch shows the stones beneath."""
        part = Part("Walls")
        for wall, (s0, s1) in PLASTER_RUN.items():
            holes = [(a, b, WIN_Z[0], WIN_Z[1]) for a, b in WINDOWS[wall]]
            if wall == "front":
                holes.append((DOORWAY[0], DOORWAY[1], PLINTH_TOP, DOOR_H + HEAD))
            cuts = sorted({s0, s1} | {h[0] for h in holes} | {h[1] for h in holes})
            for a, b in zip(cuts, cuts[1:]):
                if b - a < 1e-6:
                    continue
                mid = (a + b) / 2
                spans = [(PLINTH_TOP, PLATE_TOP)]
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
                    self.wall_slab(part, wall, a, b, lo, hi)
        part.finish(self.coll)

    def wall_slab(self, part, wall, a, b, lo, hi):
        """One revision-4 wall box (s a..b, z lo..hi, through the wall's
        thickness), its outer face cut at the timbers' edges into cells:
        flat behind a timber, faceted on a plaster panel, spalled where a
        patch is."""
        def P(s_, z_, d):
            return wall_point(wall, s_, -d, z_)
        out = Vector(wall_point(wall, 0, 1, 0)) - Vector(wall_point(wall, 0, 0, 0))
        along = Vector(wall_point(wall, 1, 0, 0)) - Vector(wall_point(wall, 0, 0, 0))
        V = lambda s_, z_, d: Vector(P(s_, z_, d))
        part.face([V(a, lo, WALL_T), V(b, lo, WALL_T), V(b, hi, WALL_T), V(a, hi, WALL_T)], "Plaster", -out)
        part.face([V(a, lo, 0), V(a, hi, 0), V(a, hi, WALL_T), V(a, lo, WALL_T)], "Plaster", -along)
        part.face([V(b, lo, 0), V(b, hi, 0), V(b, hi, WALL_T), V(b, lo, WALL_T)], "Plaster", along)
        part.face([V(a, lo, 0), V(b, lo, 0), V(b, lo, WALL_T), V(a, lo, WALL_T)], "Plaster", (0, 0, -1))
        part.face([V(a, hi, 0), V(b, hi, 0), V(b, hi, WALL_T), V(a, hi, WALL_T)], "Plaster", (0, 0, 1))
        posts = timber_posts(wall)
        t0, t1 = TIMBER_RUN[wall]
        S = sorted({a, b} | {e for p in posts for e in p if a < e < b})
        Z = sorted({lo, hi} | {e for r in RAILS for e in r if lo < e < hi})
        for sa, sb in zip(S, S[1:]):
            sm = (sa + sb) / 2
            for za, zb in zip(Z, Z[1:]):
                zm = (za + zb) / 2
                covered = any(p0 <= sm <= p1 for p0, p1 in posts) or not (t0 <= sm <= t1)
                for r0, r1 in RAILS:
                    if r0 <= zm <= r1 and not (wall == "front" and r1 <= DOOR_H + HEAD and DOORWAY[0] <= sm <= DOORWAY[1]):
                        covered = True
                if covered:
                    part.face([V(sa, za, 0), V(sb, za, 0), V(sb, zb, 0), V(sa, zb, 0)], "Plaster", out)
                    continue
                cell = (sa, sb, za, zb)
                rng = stable_rng(wall, sa, za)
                patch = next((pt for pt in PATCHES if pt[0] == wall and sa < pt[1] < sb and za < pt[2] < zb), None)
                if patch:
                    _, cs, cz, rs, rz, seed = patch
                    outline = patch_outline(cs, cz, rs, rz, stable_rng("patch", seed))
                    rec = spalled_panel(part, P, out, cell, outline, rng)
                    rec.update({"part": "Walls", "wall": wall, "out": out.copy()})
                    self.patches.append(rec)
                    self.panels.append({"part": "Walls", "wall": wall, "cell": cell, "depth": rec["edge_max"],
                                        "normals": rec["normals"], "patched": True})
                else:
                    deepest, normals = facet_rect(part, P, out, sa, sb, za, zb, rng)
                    self.panels.append({"part": "Walls", "wall": wall, "cell": cell, "depth": deepest,
                                        "normals": normals, "patched": False})

    # ---- timber frame -----------------------------------------------------

    def timbers(self):
        """Corner posts; on each wall the rails (sole plate, low rail, high
        rail, wall plate), posts either side of every opening and between,
        no panel wider than 2.4 m, and a brace in each plain end panel."""
        part = Part("Timbers")
        # Corner posts, square, through both faces.
        for x0, x1 in ((X0 - (POST_PROUD - PROUD), X0 + CORNER), (X1 - CORNER, X1 + (POST_PROUD - PROUD))):
            for y0, y1 in ((Y0 - (POST_PROUD - PROUD), Y0 + CORNER), (Y1 - CORNER, Y1 + (POST_PROUD - PROUD))):
                part.box((x0, y0, PLINTH_TOP), (x1, y1, PLATE_TOP), "Wood")
        for wall, (t0, t1) in TIMBER_RUN.items():
            # Rails, broken by the doorway on the front (all but the wall plate).
            for z0, z1 in RAILS:
                pieces = [(t0, t1)]
                if wall == "front" and z1 <= DOOR_H + HEAD:
                    pieces = [(t0, DOORWAY[0]), (DOORWAY[1], t1)]
                for a, b in pieces:
                    part.wall_box(wall, a, b, 0.0, PROUD, z0, z1, "Wood")
            every, drawn = post_layout(wall)
            for a, b in drawn:
                part.wall_box(wall, a, b, 0.0, POST_PROUD, PLINTH_TOP, PLATE_TOP, "Wood")
            self.posts[wall] = every
            # Braces between the low and high rails in the two end panels,
            # rising toward the corner, when the panel holds no window.
            z0, z1 = RAILS[1][1], RAILS[2][0]
            for (a0, a1), (b0, b1), corner_left in ((every[0], every[1], True), (every[-2], every[-1], False)):
                if b0 - a1 < 1.0:
                    continue
                if any(a1 - 0.01 <= w0 and w1 <= b0 + 0.01 for w0, w1 in WINDOWS[wall]):
                    continue
                w = 0.26
                if corner_left:  # low on the inner side, high against the corner post
                    quad = [(b0 - w, z0), (b0, z0), (a1 + w, z1), (a1, z1)]
                else:
                    quad = [(a1, z0), (a1 + w, z0), (b0, z1), (b0 - w, z1)]
                part.wall_quad_prism(wall, quad, 0.0, PROUD, "Wood")
        part.finish(self.coll)

    # ---- openings ---------------------------------------------------------

    def windows(self):
        """Each window: a dark pane (Dirt) set 0.2 m back in the opening, a
        Wood mullion and transom just in front of it, and a Wood sill board
        on the low rail. No glow."""
        part = Part("Windows")
        z0, z1 = WIN_Z
        for wall, spans in WINDOWS.items():
            for a, b in spans:
                part.wall_box(wall, a, b, -0.24, -0.20, z0, z1, "Dirt")
                mid_s, mid_z = (a + b) / 2, (z0 + z1) / 2
                part.wall_box(wall, mid_s - 0.05, mid_s + 0.05, -0.20, -0.14, z0, z1, "Wood")
                part.wall_box(wall, a, mid_s - 0.05, -0.20, -0.14, mid_z - 0.05, mid_z + 0.05, "Wood")
                part.wall_box(wall, mid_s + 0.05, b, -0.20, -0.14, mid_z - 0.05, mid_z + 0.05, "Wood")
                part.wall_box(wall, a - 0.12, b + 0.12, -0.05, 0.16, z0 - 0.06, z0 + 0.03, "Wood")
        part.finish(self.coll)

    def doorway(self):
        """The door's frame (Wood jambs and head, through the wall), and the
        hall behind it (the `Vestibule` node): a Wood floor and two dark Dirt
        side walls, open to the doorway, running back to the rear wall's
        inside face, which closes it (revision 6: no back wall of its own)."""
        part = Part("Doorway")
        dl, dr = DOORWAY
        yo, yi = PY0 - (POST_PROUD - PROUD) - PROUD, PY0 + WALL_T  # proud as the posts; the wall's inside face
        part.box((dl, yo, 0.0), (dl + JAMB, yi, DOOR_H), "Wood")
        part.box((dr - JAMB, yo, 0.0), (dr, yi, DOOR_H), "Wood")
        part.box((dl, yo, DOOR_H), (dr, yi, DOOR_H + HEAD), "Wood")
        part.finish(self.coll)

        vest = Part("Vestibule")
        vx0, vx1, vy1 = VESTIBULE
        vest.box((vx0, yi, -0.1), (vx1, vy1, 0.0), "Wood")
        vest.box((vx0 - 0.2, yi, 0.0), (vx0, vy1, PLATE_TOP - 0.2), "Dirt")
        vest.box((vx1, yi, 0.0), (vx1 + 0.2, vy1, PLATE_TOP - 0.2), "Dirt")
        vest.finish(self.coll)

    # ---- roof -------------------------------------------------------------

    def roof(self):
        """Hipped slate at 45 degrees over revision 2's eave line: a thin
        eave edge, then COURSES stepped courses up every slope - each course
        tilts out a little from its bed, so its lower edge stands COURSE_T
        proud of the course below and the slope reads as bands - each course
        cut into staggered tiles by V notches (revision 4), and ridge and hip
        rolls, tiled the same way."""
        part = Part("Roof")
        x0, x1, y0, y1 = EAVES
        c = EAVE_CHAMFER
        R = (y1 - y0) / 2
        ym = (y0 + y1) / 2

        def ring(a, dz):
            z = roof_base(a) + dz
            return [Vector((x0 + a, y0 + a, z)), Vector((x1 - a, y0 + a, z)), Vector((x1 - a, y1 - a, z)),
                    Vector((x0 + a, y1 - a, z))]
        outs = [Vector((0, -1, 0)), Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((-1, 0, 0))]
        low = [Vector((x0, y0, EAVE_LOW)), Vector((x1, y0, EAVE_LOW)), Vector((x1, y1, EAVE_LOW)), Vector((x0, y1, EAVE_LOW))]
        zc = EAVE_TOP - c * 0.6
        mid = [Vector((p.x, p.y, zc)) for p in low]
        bed = ring(c, 0.0)
        part.face(list(reversed(low)), "Roof", (0, 0, -1))
        for i in range(4):
            j = (i + 1) % 4
            part.face([low[i], low[j], mid[j], mid[i]], "Roof", outs[i])
            part.face([mid[i], mid[j], bed[j], bed[i]], "Roof", outs[i] + Vector((0, 0, 1)))
        insets = [c + (R - c) * k / COURSES for k in range(COURSES + 1)]
        e = COURSE_E
        ridge_z = roof_base(R) + e
        ridge = [Vector((x0 + R, ym, ridge_z)), Vector((x1 - R, ym, ridge_z))]
        # Each slope's own frame: s along its courses, t in from its eave
        # line (plan). The hips run at 45 degrees in plan, so a course at
        # inset t spans s from s0 + t to s1 - t.
        spans = [(x0, x1), (y0, y1), (x0, x1), (y0, y1)]
        slope_names = ["front", "right", "back", "left"]

        def plan(i, s, t):
            return [(s, y0 + t), (x1 - t, s), (s, y1 - t), (x0 + t, s)][i]
        for k in range(COURSES):
            a, b = insets[k], insets[k + 1]
            za, zu = roof_base(a) + COURSE_T, roof_base(b) + e
            # The lip, all round; above the eave course it reaches SKIRT below
            # its bed, so the notches of the course below end against it.
            lo, lo_t = ring(a, -SKIRT if k else 0.0), ring(a, COURSE_T)
            for i in range(4):
                j = (i + 1) % 4
                part.face([lo[i], lo[j], lo_t[j], lo_t[i]], "Roof", outs[i] - Vector((0, 0, 0.2)))
            for i in range(4):
                s0, s1 = spans[i]
                sc = (s0 + s1) / 2
                up_lo, up_hi = s0 + b, s1 - b
                joints = []
                for jj in range(-40, 41):
                    sj = sc + (jj + 0.5 * (k % 2)) * TILE
                    if sj - JOINT_W / 2 >= up_lo + JOINT_MARGIN and sj + JOINT_W / 2 <= up_hi - JOINT_MARGIN:
                        joints.append(sj)
                cuts = [(s0 + a, up_lo, 0.0)]
                for sj in joints:
                    cuts += [(sj - JOINT_W / 2,) * 2 + (0.0,), (sj, sj, JOINT_DZ), (sj + JOINT_W / 2,) * 2 + (0.0,)]
                cuts.append((s1 - a, up_hi, 0.0))
                facing = outs[i] + Vector((0, 0, 1))

                def pt(s, t, z, sink):
                    px, py = plan(i, s, t)
                    return Vector((px, py, z - sink))
                normals = []
                for (sa_lo, sa_up, da), (sb_lo, sb_up, db) in zip(cuts, cuts[1:]):
                    pts = [pt(sa_lo, a, za, da), pt(sb_lo, a, za, db), pt(sb_up, b, zu, db)]
                    if sb_up - sa_up > 1e-6:
                        pts.append(pt(sa_up, b, zu, da))
                    part.face(pts, "Roof", facing)
                    normals.append(face_normal(pts, facing))
                tile_n = normals[0]
                lip_n = outs[i]
                angles = []
                for jj in range(len(joints)):
                    angles += [angle_deg(tile_n, normals[3 * jj + 1]), angle_deg(tile_n, normals[3 * jj + 2])]
                self.tiling.append({
                    "part": "Roof", "slope": slope_names[i], "course": k, "joints": joints,
                    "upper": (up_lo, up_hi), "angles": angles, "lip_angle": angle_deg(tile_n, lip_n),
                    "tile_normal": tile_n, "notch": [(normals[3 * jj + 1], normals[3 * jj + 2]) for jj in range(len(joints))],
                    "groove_pts": [(pt(sj - JOINT_W / 2, (a + b) / 2, (za + zu) / 2, 0.0),
                                    pt(sj, (a + b) / 2, (za + zu) / 2, JOINT_DZ),
                                    pt(sj + JOINT_W / 2, (a + b) / 2, (za + zu) / 2, 0.0)) for sj in joints],
                    "lip": (lo_t[i], lo_t[(i + 1) % 4], COURSE_T - e),
                })
        # Ridge and hip rolls, the hips starting a little in from the corners
        # so the rolls stay inside the eave line; tiled.
        lift = Vector((0, 0, 0.04))
        r0, r1 = ridge[0] + lift, ridge[1] + lift
        roll(part, r0, r1, 0.2, tiled=self.roll_tiling.setdefault("Roof ridge", []))
        start = ring(c + 0.3, COURSE_T + 0.04)
        for name, corner, end in (("hip front-left", start[0], r0), ("hip back-left", start[3], r0),
                                  ("hip front-right", start[1], r1), ("hip back-right", start[2], r1)):
            roll(part, corner, end, 0.17, tiled=self.roll_tiling.setdefault("Roof " + name, []))
        part.finish(self.coll)

    def gables(self):
        """The cross-gable over the door, facing the street (-y), and a small
        gabled dormer on each long slope: Plaster faces, Wood framing,
        carved bargeboards and finials, slated as the roof."""
        for g in [CROSS_GABLE] + DORMERS:
            part = Part(g["name"])
            self.gable_figs[g["name"]] = build_gable(part, g, self.tiling, self.roll_tiling, self.panels)
            part.finish(self.coll)

    def chimney(self):
        """A stone stack up the right-hand end wall: a broad base to above the
        eave, a sloped shoulder, a slimmer flue stack in courses, a cap and a
        pot. Revision 5: every stage of it - base, the two stages of the
        stack, the shoulder's four slopes and the four jogged flue courses -
        laid as individual stones proud of a Plaster mortar bed BED inside
        revision 4's faces (no stone passes them). The cap stays one stone
        and the pot Dirt."""
        part = Part("Chimney")
        x0, x1 = CHIMNEY_X
        y0, y1 = CHIMNEY_Y
        T = STONE_T
        wall_x = X1  # the stack's faces run back to the timber face; behind that it is in the wall
        boxes = [((x0, y0, FOOT), (x1, y1, PLINTH_TOP + 0.05)),
                 ((x0 + 0.04, y0 + 0.04, PLINTH_TOP + 0.05), (x1 - 0.04, y1 - 0.04, 2.6)),
                 ((x0 + 0.02, y0 + 0.06, 2.6), (x1 - 0.06, y1 - 0.02, 4.9))]
        stage_courses = [[FOOT, -0.25, 0.10, PLINTH_TOP + 0.05], 6, 6]
        prev = {}
        for k, ((bx0, by0, bz0), (bx1, by1, bz1)) in enumerate(boxes):
            part.box((bx0, by0 + BED, bz0), (bx1 - BED, by1 - BED, bz1), "Plaster")
            zs = stage_courses[k] if isinstance(stage_courses[k], list) else \
                [bz0 + (bz1 - bz0) * i / stage_courses[k] for i in range(stage_courses[k] + 1)]
            u0 = X1 + PLINTH_OUT if k == 0 else wall_x  # the base meets the plinth's face (x 4.9), the stack the wall's
            faces = [
                dict(name="chimney stage %d front" % k, axis="y", coord=by0, out=-1, u=(u0, bx1), ends=("flat", "corner"),
                     phase=0, T=T),
                dict(name="chimney stage %d back" % k, axis="y", coord=by1, out=1, u=(u0, bx1), ends=("flat", "corner"),
                     phase=0, T=T),
                dict(name="chimney stage %d end" % k, axis="x", coord=bx1, out=1, u=(by0, by1), ends=("corner", "corner"),
                     phase=1, T=T),
            ]
            lay_courses(part, faces, zs, self.masonry, bottom_gap=JOINT / 2 if k else 0.0, top_gap=JOINT / 2)
            self.beds.append({"part": "Chimney", "top": None, "faces": [f["name"] for f in faces]})
        # Shoulder: the stack narrows to the flue over 0.6 m. Its bed is the
        # revision-4 shoulder with each slope moved BED in; two courses of
        # stones up each slope.
        fx0, fx1, fy0, fy1 = x0 + 0.15, x1 - 0.08, y0 + 0.28, y1 - 0.28
        b0, b1 = (x0 + 0.02, y0 + 0.06), (x1 - 0.06, y1 - 0.02)
        zb, zt = 4.9, 5.5
        lo_r = {"x0": b0[0], "x1": b1[0], "y0": b0[1], "y1": b1[1]}
        hi_r = {"x0": fx0, "x1": fx1, "y0": fy0, "y1": fy1}
        normals = {}
        for key, sign in (("x0", -1), ("x1", 1), ("y0", -1), ("y1", 1)):
            run = hi_r[key] - lo_r[key]
            axis = Vector((1, 0, 0)) if key[0] == "x" else Vector((0, 1, 0))
            nrm = (axis * sign * (zt - zb) + Vector((0, 0, -sign * run))).normalized()
            normals[key] = nrm
        def inset(r, key):
            nrm = normals[key]
            h = math.hypot(nrm.x, nrm.y)
            return r[key] - (BED / h) * (1 if key[1] == "1" else -1)
        blo = {k: inset(lo_r, k) for k in lo_r}
        bhi = {k: inset(hi_r, k) for k in hi_r}
        part.hexa([(blo["x0"], blo["y0"], zb), (blo["x1"], blo["y0"], zb), (blo["x1"], blo["y1"], zb), (blo["x0"], blo["y1"], zb),
                   (bhi["x0"], bhi["y0"], zt), (bhi["x1"], bhi["y0"], zt), (bhi["x1"], bhi["y1"], zt), (bhi["x0"], bhi["y1"], zt)],
                  "Plaster", skip=("-z",))
        L = lambda x, y, z: (x, y, z)
        quads = {  # corners as seen from outside: bottom-left, bottom-right, top-right, top-left
            "y0": [L(b0[0], b0[1], zb), L(b1[0], b0[1], zb), L(fx1, fy0, zt), L(fx0, fy0, zt)],
            "x1": [L(b1[0], b0[1], zb), L(b1[0], b1[1], zb), L(fx1, fy1, zt), L(fx1, fy0, zt)],
            "y1": [L(b1[0], b1[1], zb), L(b0[0], b1[1], zb), L(fx0, fy1, zt), L(fx1, fy1, zt)],
            "x0": [L(b0[0], b1[1], zb), L(b0[0], b0[1], zb), L(fx0, fy0, zt), L(fx0, fy1, zt)],
        }
        for key, quad in quads.items():
            lay_plane(part, quad, normals[key], 2, self.masonry, "chimney shoulder " + key)
        self.beds.append({"part": "Chimney", "top": None, "faces": ["chimney shoulder " + k for k in quads]})
        z = zt
        jog = [(0.0, 0.0), (0.03, -0.02), (-0.02, 0.03), (0.02, 0.01), (-0.01, -0.03)]
        courses = 4
        step = (CHIMNEY_TOP - z) / courses
        flue = []
        for k in range(courses):
            dx, dy = jog[k]
            ax0, ax1, ay0, ay1 = fx0 + dx, fx1 + dx, fy0 + dy, fy1 + dy
            part.box((ax0 + BED, ay0 + BED, z), (ax1 - BED, ay1 - BED, z + step), "Plaster")
            faces = [
                dict(name="flue %d front" % k, axis="y", coord=ay0, out=-1, u=(ax0, ax1), ends=("corner", "corner"), phase=0, T=T),
                dict(name="flue %d back" % k, axis="y", coord=ay1, out=1, u=(ax0, ax1), ends=("corner", "corner"), phase=0, T=T),
                dict(name="flue %d left" % k, axis="x", coord=ax0, out=-1, u=(ay0, ay1), ends=("corner", "corner"), phase=1, T=T),
                dict(name="flue %d right" % k, axis="x", coord=ax1, out=1, u=(ay0, ay1), ends=("corner", "corner"), phase=1, T=T),
            ]
            lay_courses(part, faces, [z, z + step / 2, z + step], self.masonry, bottom_gap=JOINT / 2,
                        top_gap=JOINT / 2 if k < courses - 1 else JOINT)
            self.beds.append({"part": "Chimney", "top": None, "faces": [f["name"] for f in faces]})
            flue.append(((ax0, ay0, z), (ax1, ay1, z + step)))
            z += step
        part.box((fx0 - 0.12, fy0 - 0.12, z), (fx1 + 0.12, fy1 + 0.12, z + 0.25), "Stone")
        cx, cy = (fx0 + fx1) / 2, (fy0 + fy1) / 2
        part.box((cx - 0.2, cy - 0.2, z + 0.25), (cx + 0.2, cy + 0.2, z + 0.7), "Dirt")
        self.envelopes["Chimney"] = boxes + [((min(b0[0], fx0), min(b0[1], fy0), zb), (max(b1[0], fx1), max(b1[1], fy1), zt))] \
            + flue + [((fx0 - 0.12, fy0 - 0.12, z), (fx1 + 0.12, fy1 + 0.12, z + 0.25)),
                      ((cx - 0.2, cy - 0.2, z + 0.25), (cx + 0.2, cy + 0.2, z + 0.7))]
        part.finish(self.coll)

    # ---- sign -------------------------------------------------------------

    def sign(self):
        """A hanging sign right of the door: a Wood bracket and strut off the
        wall, two dark hangers, and a Wood board standing edge-on to the wall
        with a curled, sleeping fox in Plaster on both faces. No text."""
        part = Part("Sign")
        x = SIGN_X
        y_wall = PY0 - POST_PROUD  # the post's face
        y_far = SIGN_Y[1] - 0.12
        # Bracket arm, a back plate on the wall, and a diagonal strut.
        part.box((x - 0.07, y_far, BRACKET_Z), (x + 0.07, y_wall, BRACKET_Z + 0.16), "Wood")
        part.box((x - 0.1, y_wall - 0.06, BRACKET_Z - 1.1), (x + 0.1, PY0, BRACKET_Z + 0.3), "Wood")
        sz0, sy1 = BRACKET_Z - 0.9, (SIGN_Y[0] + SIGN_Y[1]) / 2
        w = 0.1
        strut = [(y_wall - 0.06, sz0), (y_wall - 0.06, sz0 + w * 1.4), (sy1 + w, BRACKET_Z), (sy1, BRACKET_Z)]
        a = [Vector((x + 0.05, y, z)) for y, z in strut]
        b = [Vector((x - 0.05, y, z)) for y, z in strut]
        part.face(a, "Wood", (1, 0, 0))
        part.face(b, "Wood", (-1, 0, 0))
        for i in range(4):
            j = (i + 1) % 4
            mid = (a[i] + a[j]) / 2
            centre = sum(a, Vector()) / 4
            part.face([a[i], a[j], b[j], b[i]], "Wood", mid - centre)
        # Hangers from the arm to the board's top corners.
        for y in (SIGN_Y[0] - 0.12, SIGN_Y[1] + 0.12):
            part.box((x - 0.02, y - 0.02, SIGN_Z[1]), (x + 0.02, y + 0.02, BRACKET_Z), "Dirt")
        # The board, edge-on to the wall (its faces look along +-x).
        t = 0.04
        ya, yb = sorted(SIGN_Y)
        part.box((x - t, ya, SIGN_Z[0]), (x + t, yb, SIGN_Z[1]), "Wood")
        # The fox: one outline, read as (along the board toward the street, up).
        fox = [(-0.42, -0.24), (-0.47, -0.12), (-0.42, 0.0), (-0.40, 0.17), (-0.31, 0.07), (-0.24, 0.19),
               (-0.17, 0.06), (0.02, 0.15), (0.22, 0.14), (0.37, 0.05), (0.45, -0.08), (0.41, -0.20),
               (0.28, -0.27), (0.05, -0.29), (-0.22, -0.29)]
        yc, zc = (SIGN_Y[0] + SIGN_Y[1]) / 2, (SIGN_Z[0] + SIGN_Z[1]) / 2
        scale = 1.05
        for side in (1, -1):
            x_face, x_out = x + side * t, x + side * (t + 0.015)
            pts = [(yc - u * scale, zc + v * scale) for u, v in fox]  # toward -y (the street) is forward
            front = [Vector((x_out, y, z)) for y, z in pts]
            back = [Vector((x_face, y, z)) for y, z in pts]
            part.face(front, "Plaster", (side, 0, 0))
            n = len(pts)
            # The outline's winding in (y, z) says which side of each edge is out.
            area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1] for i in range(n))
            for i in range(n):
                j = (i + 1) % n
                dy, dz = pts[j][0] - pts[i][0], pts[j][1] - pts[i][1]
                out = Vector((0, dz, -dy)) if area > 0 else Vector((0, -dz, dy))
                part.face([front[i], front[j], back[j], back[i]], "Plaster", out)
        part.finish(self.coll)

    def build_all(self):
        for step in self.STEPS:
            getattr(self, step)()


# --------------------------------------------------------------------------
# Checks, figures for assets.md, preview and export


def engine(p):
    """Blender (x, y, z) -> engine (x, y, z): +Y up, +Z = Blender -Y."""
    return (p[0], p[2], -p[1])


def triangles(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)


def figures(b):
    """Everything assets.md quotes, from the same constants the build uses."""
    yi = PY0 + WALL_T
    return {
        "doorway": {"width": DOOR_W, "height": DOOR_H, "x": (-DOOR_W / 2, DOOR_W / 2),
                    "wall_z_engine": (-yi, -(PY0 - (POST_PROUD - PROUD) - PROUD))},
        "hinge_engine": engine((-DOOR_W / 2, yi, 0.0)),
        "vestibule_engine": {"x": VESTIBULE[:2], "z": (-VESTIBULE[2], -yi)},
        "footprint_engine": [engine(p) for p in [
            (X0 - PLINTH_OUT, Y0 - PLINTH_OUT, 0), (DOORWAY[0], Y0 - PLINTH_OUT, 0), (DOORWAY[0], Y0 - PLINTH_OUT - 0.12, 0),
            (DOORWAY[1], Y0 - PLINTH_OUT - 0.12, 0), (DOORWAY[1], Y0 - PLINTH_OUT, 0), (X1 + PLINTH_OUT, Y0 - PLINTH_OUT, 0),
            (X1 + PLINTH_OUT, CHIMNEY_Y[0], 0), (CHIMNEY_X[1], CHIMNEY_Y[0], 0), (CHIMNEY_X[1], CHIMNEY_Y[1], 0),
            (X1 + PLINTH_OUT, CHIMNEY_Y[1], 0), (X1 + PLINTH_OUT, Y1 + PLINTH_OUT, 0),
            (X0 - PLINTH_OUT, Y1 + PLINTH_OUT, 0)]],
        "eaves_engine": {"x": (X0 - EAVE_OUT, X1 + EAVE_OUT), "z": (-(Y1 + EAVE_OUT), -(Y0 - EAVE_OUT)),
                         "underside": EAVE_LOW},
        "sign_engine": {"x": (SIGN_X - 0.1, SIGN_X + 0.1), "z": (-Y0, -(SIGN_Y[1] - 0.12)),
                        "y": (SIGN_Z[0], BRACKET_Z + 0.3), "board_y": SIGN_Z,
                        "clear_of_jamb": round(SIGN_X - 0.1 - DOORWAY[1], 3)},
        "ridge_top_engine_y": round(max(v.co.z for v in bpy.data.objects["Roof"].data.vertices), 3),
        "roof_top_engine_y": round(max(v.co.z for n in ROOF_PARTS for v in bpy.data.objects[n].data.vertices), 3),
        "roof_parts_plan_engine": {n: (lambda vs: {"x": (round(min(v.x for v in vs), 3), round(max(v.x for v in vs), 3)),
                                                   "z": (round(-max(v.y for v in vs), 3), round(-min(v.y for v in vs), 3)),
                                                   "y": (round(min(v.z for v in vs), 3), round(max(v.z for v in vs), 3))})(
            [v.co for v in bpy.data.objects[n].data.vertices]) for n in ROOF_PARTS},
        "gables": {n: {k: round(v, 3) for k, v in g.items()} for n, g in b.gable_figs.items()},
        "chimney_engine": {"x": CHIMNEY_X, "z": (-CHIMNEY_Y[1], -CHIMNEY_Y[0]),
                           "cap_x": tuple(round(v, 3) for v in (
                               min(v.co.x for v in bpy.data.objects["Chimney"].data.vertices),
                               max(v.co.x for v in bpy.data.objects["Chimney"].data.vertices))),
                           "cap_z": tuple(round(-v, 3) for v in (
                               max(v.co.y for v in bpy.data.objects["Chimney"].data.vertices),
                               min(v.co.y for v in bpy.data.objects["Chimney"].data.vertices))),
                           "top_y": round(max(v.co.z for v in bpy.data.objects["Chimney"].data.vertices), 3)},
        "vestibule_depth": round(VESTIBULE[2] - (PY0 + WALL_T), 3),
        "posts": b.posts,
    }


def check(b):
    problems = []
    allowed = set(MATERIALS)
    total = 0
    lo = Vector((1e9, 1e9, 1e9))
    hi = Vector((-1e9, -1e9, -1e9))
    per = {}
    for obj in b.coll.objects:
        if obj.type != "MESH":
            continue
        mats = {m.name for m in obj.data.materials}
        if not mats <= allowed:
            problems.append(f"{obj.name}: materials {sorted(mats - allowed)} not allowed")
        if "." in obj.name:
            problems.append(f"{obj.name}: a duplicate name")
        n = triangles(obj)
        per[obj.name] = n
        total += n
        for v in obj.data.vertices:
            for i in range(3):
                lo[i] = min(lo[i], v.co[i])
                hi[i] = max(hi[i], v.co[i])
    if total > TRI_BUDGET:
        problems.append(f"{total} triangles, budget {TRI_BUDGET}")
    if not any(a <= SIGN_X <= b for a, b in b.posts["front"]):
        problems.append(f"the sign at x = {SIGN_X} hangs off no post: front posts {b.posts['front']}")
    if abs(lo.z - FOOT) > 1e-4:
        problems.append(f"foundation bottom at {lo.z:.3f}, want {FOOT}")
    plinth = (X1 - X0 + 2 * PLINTH_OUT, Y1 - Y0 + 2 * PLINTH_OUT)
    if any(abs(a - b) > 1e-6 for a, b in zip(plinth, PLINTH_SIZE)):
        problems.append(f"plinth {plinth}, want {PLINTH_SIZE}")
    vest_depth = VESTIBULE[2] - (PY0 + WALL_T)
    if vest_depth < MIN_VESTIBULE - 1e-6:
        problems.append(f"vestibule {vest_depth:.2f} m deep, want >= {MIN_VESTIBULE}")
    # The sign (bracket, back plate, board) must stay clear of the jamb and
    # never reach over the doorway.
    sign = bpy.data.objects["Sign"]
    sx0 = min(v.co.x for v in sign.data.vertices)
    if sx0 - DOORWAY[1] < MIN_SIGN_TO_JAMB - 1e-6:
        problems.append(f"the sign comes to x = {sx0:.3f}, {sx0 - DOORWAY[1]:.3f} m from the jamb")
    # The roof's ridge must stay lower than the house is wide.
    roof = bpy.data.objects["Roof"]
    ridge = max(v.co.z for v in roof.data.vertices)
    if ridge - EAVE_TOP > (Y1 - Y0):
        problems.append(f"the roof rises {ridge - EAVE_TOP:.2f} m, more than the house's {Y1 - Y0:.2f} m depth")
    # The chimney's top stands clear of the roof where it passes.
    chimney = bpy.data.objects["Chimney"]
    ex0, ex1, ey0, ey1 = X0 - EAVE_OUT, X1 + EAVE_OUT, Y0 - EAVE_OUT, Y1 + EAVE_OUT
    roof_at = max(roof_height(min(v.co.x, ex1), v.co.y)
                  for v in chimney.data.vertices if v.co.x < ex1)
    if CHIMNEY_TOP < roof_at + 1.0:
        problems.append(f"chimney top {CHIMNEY_TOP} is less than 1 m above the roof ({roof_at:.2f})")
    problems += roof_checks(b)
    problems += tile_checks(b)
    problems += hall_checks(b)
    mp, b.masonry_summary = masonry_checks(b)
    rp, b.render_summary = render_checks(b)
    problems += mp + rp
    return problems, total, per, lo, hi


def hall_checks(b):
    """Revision 6's hall (the Vestibule node):
    - its back (VESTIBULE[2]) is the rear wall's inside face, measured from
      the Walls mesh behind the hall;
    - the Wood floor (top y 0) and both Dirt side walls reach that face;
    - no back wall: nothing of it inside x -1.9..1.9 above the floor;
    - under HALL_TRI_BUDGET triangles."""
    problems = []
    eps = 1e-5
    vx0, vx1, vy1 = VESTIBULE
    walls = bpy.data.objects["Walls"].data.vertices
    behind = [v.co.y for v in walls if v.co.y > (Y0 + Y1) / 2 and vx0 - 0.2 - eps <= v.co.x <= vx1 + 0.2 + eps]
    face = min(behind)
    if abs(face - vy1) > eps:
        problems.append(f"the hall's back is y {vy1:.3f}, but the rear wall's inside face is y {face:.3f}")
    vest = bpy.data.objects["Vestibule"]
    mesh = vest.data
    vs = [v.co for v in mesh.vertices]
    if abs(max(v.y for v in vs) - face) > eps:
        problems.append(f"the hall reaches y {max(v.y for v in vs):.3f}, not the rear wall's {face:.3f}")
    for mat, x0, x1 in (("Wood", vx0, vx1), ("Dirt", vx0 - 0.2, vx0), ("Dirt", vx1, vx1 + 0.2)):
        mi = [m.name for m in mesh.materials].index(mat)
        reach = max((mesh.vertices[i].co.y for p in mesh.polygons if p.material_index == mi
                     for i in p.vertices if x0 - eps <= mesh.vertices[i].co.x <= x1 + eps), default=-1e9)
        if abs(reach - face) > eps:
            problems.append(f"the hall's {mat} at x {x0}..{x1} reaches y {reach:.3f}, not {face:.3f}")
    for v in vs:
        if vx0 + eps < v.x < vx1 - eps and v.z > eps:
            problems.append(f"Vestibule: a vertex over the hall floor at {tuple(round(c, 3) for c in v)} (no back wall)")
            break
    n = triangles(vest)
    if n >= HALL_TRI_BUDGET:
        problems.append(f"Vestibule: {n} triangles, budget under {HALL_TRI_BUDGET}")
    return problems


def masonry_checks(b):
    """Revision 5's stonework (plinth, threshold, chimney):
    - every joint, between stones and between courses, at least 0.05 m;
    - every stone 0.03 to 0.05 m proud of its bed (and an exposed top
      course's tops 0.03 to 0.05 m over the bed's top);
    - each course's vertical joints at least STAGGER from the course below's;
    - no stone past revision 4's outline (the part's envelope boxes).
    Returns problems and a summary."""
    problems = []
    eps = 1e-6
    by_face = {}
    n_stones = 0
    min_joint, proud_lo, proud_hi, min_stagger = 9.0, 9.0, 0.0, 9.0
    bed_top = {f: bd["top"] for bd in b.beds for f in bd["faces"]}
    for rec in b.masonry:
        by_face.setdefault(rec["face"], []).append(rec)
        st = sorted(rec["stones"], key=lambda q: q["u"][0])
        n_stones += len(st)
        for a, c in zip(st, st[1:]):
            gap = c["u"][0] - a["u"][1]
            min_joint = min(min_joint, gap)
            if gap < 0.05 - eps:
                problems.append(f"{rec['face']} course {rec['course']}: a joint {gap:.3f} m wide at u {a['u'][1]:.2f}")
        for q in st:
            proud_lo, proud_hi = min(proud_lo, q["proud"]), max(proud_hi, q["proud"])
            if not 0.03 - eps <= q["proud"] <= 0.05 + eps:
                problems.append(f"{rec['face']}: a stone {q['proud']:.3f} m proud")
            if rec.get("top") and bed_top.get(rec["face"]) is not None:
                tp = q["z"][1] - bed_top[rec["face"]]
                if not 0.03 - eps <= tp <= 0.05 + eps:
                    problems.append(f"{rec['face']}: a top {tp:.3f} m over its bed")
    for face, recs in by_face.items():
        recs.sort(key=lambda r: r["course"])
        for lo, hi in zip(recs, recs[1:]):
            top_lo = max(q["z"][1] for q in lo["stones"]) if not lo.get("slab") else lo["row"][1]
            bot_hi = min(q["z"][0] for q in hi["stones"]) if not hi.get("slab") else hi["row"][0]
            gap = bot_hi - top_lo
            min_joint = min(min_joint, gap)
            if gap < 0.05 - eps:
                problems.append(f"{face}: courses {lo['course']} and {hi['course']} only {gap:.3f} m apart")
            for j in hi["joints"]:
                for q in lo["joints"]:
                    min_stagger = min(min_stagger, abs(j - q))
                    if abs(j - q) < STAGGER - 1e-6:
                        problems.append(f"{face} course {hi['course']}: a joint at {j:.2f} is {abs(j - q):.2f} m "
                                        f"from one in the course below")
    for name, boxes in b.envelopes.items():
        obj = bpy.data.objects[name]
        for v in obj.data.vertices:
            p = v.co
            if not any(all(lo[i] - eps <= p[i] <= hi[i] + eps for i in range(3)) for lo, hi in boxes):
                problems.append(f"{name}: a vertex at {tuple(round(c, 3) for c in p)} is outside revision 4's outline")
                break
    summary = {"stones": n_stones, "courses": len(b.masonry), "min_joint": round(min_joint, 3),
               "proud": (round(proud_lo, 3), round(proud_hi, 3)), "min_stagger": round(min_stagger, 3)}
    return problems, summary


def render_checks(b):
    """Revision 5's render:
    - every plaster panel (walls, gable and dormer faces and cheeks) faceted,
      at least three facet tones, recessed at most FACET_MAX, never proud of
      the revision-4 face;
    - PATCH_COUNT spalled patches, each inside its panel, clear of the
      windows, the doorway and the sign, on a wall the default camera
      faces, its stones seen from that camera, at least three stones
      inside its outline, JOINT apart, 0.03 to 0.05 proud of the bed and
      behind the render's face;
    - nothing of the foundation, walls, chimney or gables in the doorway or
      the vestibule below HEADROOM except at or under the floor."""
    problems = []
    eps = 1e-6
    fwd, right, up = view_axes()
    for pn in b.panels:
        tag = f"{pn['part']} {pn['wall']} panel {tuple(round(c, 2) for c in pn.get('cell', ()))}"
        limit = PATCH_EDGE_MAX if pn["patched"] else FACET_MAX
        if pn["depth"] > limit + eps or (not pn["patched"] and pn["depth"] < FACET_MIN - eps):
            problems.append(f"{tag}: recessed {pn['depth']:.3f}")
        tones = []
        for nrm in pn["normals"]:
            if all(angle_deg(nrm, t) > 1.0 for t in tones):
                tones.append(nrm)
        if len(tones) < 3:
            problems.append(f"{tag}: only {len(tones)} facet directions")
    walls = bpy.data.objects["Walls"]
    for v in walls.data.vertices:
        if not (PX0 - eps <= v.co.x <= PX1 + eps and PY0 - eps <= v.co.y <= PY1 + eps):
            problems.append(f"Walls: a vertex at {tuple(round(c, 3) for c in v.co)} is proud of the plaster face")
            break
    for g in [CROSS_GABLE] + DORMERS:
        obj = bpy.data.objects[g["name"]]
        pi = [i for i, m in enumerate(obj.data.materials) if m.name == "Plaster"]
        vs = {i for poly in obj.data.polygons if poly.material_index in pi for i in poly.vertices}
        for i in vs:
            co = obj.data.vertices[i].co
            vdepth = (g["face_y"] - co.y) / g["out"]
            if vdepth < -eps or abs(co.x - g["cx"]) > g["hw"] + eps:
                problems.append(f"{g['name']}: plaster at {tuple(round(c, 3) for c in co)} is proud of revision 4's body")
                break
    if not PATCH_COUNT[0] <= len(b.patches) <= PATCH_COUNT[1]:
        problems.append(f"{len(b.patches)} spalled patches, want {PATCH_COUNT[0]}-{PATCH_COUNT[1]}")
    sign = bpy.data.objects["Sign"]
    sign_s = (min(v.co.x for v in sign.data.vertices), max(v.co.x for v in sign.data.vertices))
    scene = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()

    def rect_gap(a, c):
        """Gap between two rectangles (s0, s1, z0, z1); 0 if they touch."""
        ds = max(c[0] - a[1], a[0] - c[1], 0.0)
        dz = max(c[2] - a[3], a[2] - c[3], 0.0)
        return math.hypot(ds, dz)
    report = []
    for pt in b.patches:
        s0, s1, z0, z1 = pt["cell"]
        ol = pt["outline"]
        box = (min(x for x, _ in ol), max(x for x, _ in ol), min(z for _, z in ol), max(z for _, z in ol))
        tag = f"patch on the {pt['wall']} wall at s {box[0]:.2f}-{box[1]:.2f}, z {box[2]:.2f}-{box[3]:.2f}"
        if (box[0] - s0 < PATCH_PANEL_MARGIN or s1 - box[1] < PATCH_PANEL_MARGIN
                or box[2] - z0 < PATCH_PANEL_MARGIN or z1 - box[3] < PATCH_PANEL_MARGIN):
            problems.append(f"{tag}: not {PATCH_PANEL_MARGIN} m inside its panel {pt['cell']}")
        for a, c in WINDOWS[pt["wall"]]:
            for win in ((a, c, WIN_Z[0], WIN_Z[1]), (a - 0.12, c + 0.12, WIN_Z[0] - 0.06, WIN_Z[0] + 0.03)):
                if rect_gap(box, win) < PATCH_WINDOW_CLEAR:
                    problems.append(f"{tag}: {rect_gap(box, win):.2f} m from a window")
        if pt["wall"] == "front":
            if rect_gap(box, (DOORWAY[0], DOORWAY[1], 0.0, DOOR_H + HEAD)) < PATCH_DOOR_CLEAR:
                problems.append(f"{tag}: too near the doorway")
            if max(sign_s[0] - box[1], box[0] - sign_s[1]) < PATCH_DOOR_CLEAR:
                problems.append(f"{tag}: too near the sign")
        facing = pt["out"].dot(-fwd)
        if facing < PATCH_FACING:
            problems.append(f"{tag}: the default camera sees it at {facing:.2f}, want >= {PATCH_FACING}")
        inside = [q for q in pt["stones"] if q["centre_in"]]
        if len(inside) < 3:
            problems.append(f"{tag}: only {len(inside)} stones inside its outline")
        seen = 0
        for q in inside:
            origin = q["centre"] - fwd * 60.0
            hit, loc, *_ = scene.ray_cast(dg, origin, fwd)
            seen += bool(hit and (loc - q["centre"]).length < 0.03)
        if seen < max(3, 0.75 * len(inside)):
            problems.append(f"{tag}: the default camera sees {seen} of its {len(inside)} stones")
        rows = {}
        for q in pt["stones"]:
            if not 0.03 - eps <= q["proud"] <= 0.05 + eps:
                problems.append(f"{tag}: a stone {q['proud']:.3f} proud")
            if PATCH_DEPTH - q["proud"] < pt["edge_max"] + 0.005:
                problems.append(f"{tag}: a stone stands in front of the render's broken edge")
            rows.setdefault(round(q["z"][0], 4), []).append(q)
        zs = sorted(rows)
        for za, zb in zip(zs, zs[1:]):
            if zb - max(q["z"][1] for q in rows[za]) < 0.05 - eps:
                problems.append(f"{tag}: its courses are under 0.05 m apart")
        for row in rows.values():
            row.sort(key=lambda q: q["u"][0])
            for a, c in zip(row, row[1:]):
                if c["u"][0] - a["u"][1] < 0.05 - eps:
                    problems.append(f"{tag}: a joint under 0.05 m")
        report.append({"wall": pt["wall"], "s": (round(box[0], 2), round(box[1], 2)), "z": (round(box[2], 2), round(box[3], 2)),
                       "stones_inside": len(inside), "seen": seen, "facing": round(facing, 3)})
    for name in ["Foundation", "Walls", "Chimney"] + [CROSS_GABLE["name"]] + [d["name"] for d in DORMERS]:
        for v in bpy.data.objects[name].data.vertices:
            p = v.co
            for label, (rx0, rx1, ry0, ry1) in (("doorway", DOORWAY_RECT), ("vestibule", VESTIBULE_RECT)):
                if rx0 + 1e-4 < p.x < rx1 - 1e-4 and ry0 + 1e-4 < p.y < ry1 - 1e-4 and 1e-6 < p.z < HEADROOM:
                    problems.append(f"{name}: a vertex in the {label} at {tuple(round(c, 3) for c in p)}")
                    break
    return problems, {"panels": len(b.panels), "patches": report}


def roof_checks(b):
    """Revision 3's rules for everything the new roof adds (ROOF_PARTS):
    - in plan, inside revision 2's eaves;
    - its top at most MAX_RISE above revision 2's ridge;
    - nothing over the doorway or the vestibule below HEADROOM;
    - its timber, plaster and glass (which, unlike Roof, go into the
      engine's scene.ground) over the plinth, so the ground's highest
      surface grows nowhere past the walls;
    - each gable's and dormer's roof buried under the main slates at its back;
    - the whole exterior's plan outline still revision 2's."""
    problems = []
    ex0, ex1, ey0, ey1 = EAVES
    px0, px1, py0, py1 = X0 - PLINTH_OUT, X1 + PLINTH_OUT, Y0 - PLINTH_OUT, Y1 + PLINTH_OUT
    eps = 1e-5
    top = -1e9
    for name in ROOF_PARTS:
        obj = bpy.data.objects[name]
        mesh = obj.data
        vs = [v.co for v in mesh.vertices]
        for v in vs:
            top = max(top, v.z)
            if not (ex0 - eps <= v.x <= ex1 + eps and ey0 - eps <= v.y <= ey1 + eps):
                problems.append(f"{name}: a vertex at ({v.x:.3f}, {v.y:.3f}) is outside revision 2's eaves")
                break
        for poly in mesh.polygons:
            pts = [vs[i] for i in poly.vertices]
            lo_x, hi_x = min(p.x for p in pts), max(p.x for p in pts)
            lo_y, hi_y = min(p.y for p in pts), max(p.y for p in pts)
            low = min(p.z for p in pts)
            for label, (rx0, rx1, ry0, ry1) in (("doorway", DOORWAY_RECT), ("vestibule", VESTIBULE_RECT)):
                if hi_x > rx0 and lo_x < rx1 and hi_y > ry0 and lo_y < ry1 and low < HEADROOM:
                    problems.append(f"{name}: a face over the {label} comes down to y {low:.2f}")
            mat = mesh.materials[poly.material_index].name
            if mat != "Roof":
                for p in pts:
                    inside = px0 - eps <= p.x <= px1 + eps and py0 - eps <= p.y <= py1 + eps
                    over_door = (DOORWAY_RECT[0] <= p.x <= DOORWAY_RECT[1] and DOORWAY_RECT[2] <= p.y <= DOORWAY_RECT[3]
                                 and p.z > HEADROOM)
                    if not (inside or over_door):
                        problems.append(f"{name}: {mat} at ({p.x:.3f}, {p.y:.3f}, {p.z:.2f}) is past the plinth")
                        break
    if top > REV2_RIDGE_TOP + MAX_RISE + eps:
        problems.append(f"the roof's top is at {top:.3f}, more than {MAX_RISE} m above revision 2's {REV2_RIDGE_TOP}")
    for g in [CROSS_GABLE] + DORMERS:
        P, OUT, seat, eave, ridge, we, zb, depth = gable_frame(g)
        for u, z in ((0.0, ridge + 0.3), (-we, zb(we) + COURSE_T + 0.1), (we, zb(we) + COURSE_T + 0.1)):
            p = P(u, depth, 0)
            if roof_base(roof_inset(p.x, p.y)) <= z:
                problems.append(f"{g['name']}: its roof's back end at u {u:.2f} stands out of the main slates")
    lo = Vector((1e9, 1e9, 0))
    hi = Vector((-1e9, -1e9, 0))
    for obj in b.coll.objects:
        for v in obj.data.vertices:
            lo.x, lo.y = min(lo.x, v.co.x), min(lo.y, v.co.y)
            hi.x, hi.y = max(hi.x, v.co.x), max(hi.y, v.co.y)
    plan = (lo.x, hi.x, lo.y, hi.y)
    if any(abs(a - c) > 0.006 for a, c in zip(plan, REV2_PLAN)):
        problems.append(f"the plan outline is {tuple(round(v, 3) for v in plan)}, revision 2's was {REV2_PLAN}")
    return problems




DEFAULT_VIEW_YAW = 180.0 - 12.5


def view_axes(yaw_deg=DEFAULT_VIEW_YAW, pitch_deg=30.0):
    """The game camera's forward, right and up in Blender, as render_preview
    aims it: looking along yaw, pitch degrees down."""
    yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
    fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
    right = fwd.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(fwd).normalized()
    return fwd, right, up


def tile_stats(b):
    """Joint and tile counts per part, for the log and assets.md."""
    out = {}
    for rec in b.tiling:
        o = out.setdefault(rec["part"], {"courses": 0, "joints": 0, "min_notch_deg": 180.0, "min_lip_deg": 180.0})
        o["courses"] += 1
        o["joints"] += len(rec["joints"])
        o["min_notch_deg"] = round(min([o["min_notch_deg"]] + rec["angles"]), 1)
        o["min_lip_deg"] = round(min(o["min_lip_deg"], rec["lip_angle"]), 1)
    for name, recs in b.roll_tiling.items():
        for rec in recs:
            out[name] = {"joints": len(rec["joints"]), "min_notch_deg": round(min(rec["angles"] or [180.0]), 1)}
    return out


def tile_checks(b):
    """Revision 4's tile joint pattern, on every slate course of every slope
    (main roof, cross-gable, dormers) and every roll:
    - joints exactly TILE apart along each course;
    - each course's joints half a tile along from the course below's;
    - no stretch of a course wider than a tile (plus the notch and the hip
      margin) left without a joint, except where it is buried;
    - both faces of every notch, and every course lip, turned more than
      CREASE_DEG from the tile, so the engine's crease test (normals more
      than 40 degrees apart) fires on each;
    - on the front slope, the one the game's default camera faces, each
      notch at least 2 px across and each lip at least 2 px tall at 24 px/m."""
    problems = []
    eps = 1e-6
    fwd, right, up = view_axes()
    by = {}
    for rec in b.tiling:
        by.setdefault((rec["part"], rec["slope"]), {})[rec["course"]] = rec
    if not by:
        return ["no tiled courses recorded"]
    for (part, slope), courses in sorted(by.items()):
        tag = f"{part} {slope}"
        for k in sorted(courses):
            rec = courses[k]
            js = rec["joints"]
            for a, c in zip(js, js[1:]):
                if abs((c - a) - TILE) > eps:
                    problems.append(f"{tag} course {k}: joints {a:.3f} and {c:.3f} are not {TILE} apart")
            lo, hi = rec["upper"]
            if part == "Roof":
                reach = TILE + JOINT_W / 2 + JOINT_MARGIN + eps
                if js:
                    if js[0] - lo > reach or hi - js[-1] > reach:
                        problems.append(f"{tag} course {k}: an end tile wider than a tile ({js[0] - lo:.2f}, {hi - js[-1]:.2f})")
                elif hi - lo >= JOINT_W + 2 * JOINT_MARGIN + TILE:
                    problems.append(f"{tag} course {k}: {hi - lo:.2f} m with no joint")
            else:
                if not js and k == 0:
                    problems.append(f"{tag} course {k}: no joints")
                if js and js[0] - lo > TILE + eps:
                    problems.append(f"{tag} course {k}: its front tile is {js[0] - lo:.2f} m")
            for ang in rec["angles"]:
                if ang <= CREASE_DEG:
                    problems.append(f"{tag} course {k}: a notch face turns only {ang:.1f} degrees")
                    break
            if rec["lip_angle"] <= CREASE_DEG:
                problems.append(f"{tag} course {k}: its lip turns only {rec['lip_angle']:.1f} degrees")
            below = courses.get(k - 1)
            if below and below["joints"] and js:
                for sj in js:
                    off = (sj - below["joints"][0]) % TILE
                    if abs(off - TILE / 2) > 1e-4:
                        problems.append(f"{tag} course {k}: joint at {sj:.3f} is {off:.3f} along from the course "
                                        f"below's, not half a tile")
                        break
            if part == "Roof" and slope == "front":
                for p0, pc, p1 in rec["groove_pts"]:
                    d = p1 - p0
                    px = math.hypot(d.dot(right), d.dot(up)) * PX_PER_M
                    if px < 2.0:
                        problems.append(f"{tag} course {k}: a notch is {px:.2f} px across at {PX_PER_M:g} px/m")
                        break
                for n0, n1 in rec["notch"]:
                    if n0.dot(-fwd) <= 0 or n1.dot(-fwd) <= 0:
                        problems.append(f"{tag} course {k}: a notch face turns away from the default camera")
                        break
                a, c, h = rec["lip"]
                along = c - a
                sa = Vector((along.dot(right), along.dot(up))).normalized()
                hv = Vector((0, 0, h))
                sh = Vector((hv.dot(right), hv.dot(up)))
                tall = abs(sa.x * sh.y - sa.y * sh.x) * PX_PER_M
                if tall < 2.0:
                    problems.append(f"{tag} course {k}: its lip is {tall:.2f} px tall at {PX_PER_M:g} px/m")
    for name, recs in b.roll_tiling.items():
        for rec in recs:
            js = rec["joints"]
            if rec["length"] > TILE + 0.3 and not js:
                problems.append(f"{name}: no joints along {rec['length']:.2f} m")
            for a, c in zip(js, js[1:]):
                if abs((c - a) - TILE) > eps:
                    problems.append(f"{name}: joints not {TILE} apart")
                    break
            if any(ang <= CREASE_DEG for ang in rec["angles"]):
                problems.append(f"{name}: a pinch turns only {min(rec['angles']):.1f} degrees")
    return problems


def render_preview(b):
    """tavern_exterior.png: four views at 30 degrees down, orthographic, as
    the engine's camera sees it - from the game's default camera as the
    tavern is placed, then from the front-right, front-left and back-left -
    each at twice the game's pixel scale (48 px/m)."""
    import numpy as np

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = 760, 640
    scene.render.film_transparent = False
    world = bpy.data.worlds.get("preview") or bpy.data.worlds.new("preview")
    world.color = srgb(110, 128, 96)
    scene.world = world
    cam_data = bpy.data.cameras.new("preview")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 760 / 48
    cam = bpy.data.objects.new("preview", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    paths = []

    def shoot(name, yaw_deg, target):
        yaw, pitch = math.radians(yaw_deg), math.radians(30)
        fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
        cam.location = Vector(target) - fwd * 80
        cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(os.path.dirname(PREVIEW), f"_tavern_exterior_{name}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)

    centre = ((X0 + X1) / 2, (Y0 + Y1) / 2, 3.8)
    # The ruins' default camera (yaw 180) as it sees the tavern placed at
    # kTavernYaw (12.5 degrees): 167.5 degrees in the tavern's own frame.
    shoot("default_camera", DEFAULT_VIEW_YAW, centre)
    shoot("front_right", 180 + 45, centre)
    shoot("front_left", 180 - 45, centre)
    shoot("back_left", 45, centre)
    bpy.data.objects.remove(cam)

    w, h = scene.render.resolution_x, scene.render.resolution_y
    sheet = np.zeros((h, w * len(paths), 4), dtype=np.float32)
    for k, path in enumerate(paths):
        img = bpy.data.images.load(path)
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        sheet[:, k * w:(k + 1) * w, :] = px.reshape(h, w, 4)
        bpy.data.images.remove(img)
        os.remove(path)
    out = bpy.data.images.new("tavern_exterior_sheet", w * len(paths), h)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = PREVIEW
    out.file_format = "PNG"
    out.save()
    print("wrote", PREVIEW)


def render_roof_preview(b):
    """tavern_exterior_roof.png: the roof close up from the game's default
    camera, three panels of the same crop (12 x 7.5 m):
    1. at the game's 24 px/m, ray cast pixel by pixel and toon-shaded with
       the engine's day palette, with its edge pass's outline (depth step
       > 0.35 m) and crease ink on Roof pixels (neighbouring normals more
       than 40 degrees apart, one pixel wide, on the side turned from the
       camera), mixed in as the engine mixes it (0.35 of the day ink);
    2. the same with the ink drawn solid, to show where every line falls;
    3. a smooth Workbench render at 72 px/m.
    Panels 1 and 2 are scaled up 3x, nearest neighbour.
    Returns how many joints and lips on the front slope, seen in the crop,
    take ink."""
    import numpy as np

    scene = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()
    fwd, right, up = view_axes()
    centre = Vector((0.4, 3.0, 6.7))
    width_m, height_m = 12.0, 7.5
    W, H = int(width_m * PX_PER_M), int(height_m * PX_PER_M)
    scale = 3
    day = {"Roof": ((0x3c, 0x44, 0x54), (0x5a, 0x66, 0x76), (0x7e, 0x8a, 0x98))}
    for name, col in MATERIALS.items():
        if name not in day:
            # Others: the preview colour as mid, darker and lighter round it.
            c = [((v ** (1 / 2.2)) * 255) for v in col]
            day[name] = (tuple(v * 0.7 for v in c), tuple(c), tuple(min(255, v * 1.2) for v in c))
    sun = Vector((-0.35, -0.55, 0.76)).normalized()
    ink_col = np.array((0x4c, 0x4c, 0x66), dtype=np.float32)
    mats = list(MATERIALS)
    N = np.zeros((H, W, 3), dtype=np.float32)
    D = np.full((H, W), 1e9, dtype=np.float32)
    S = np.full((H, W), -1, dtype=np.int32)
    C = np.zeros((H, W, 3), dtype=np.float32)
    C[:] = (110, 128, 96)
    for row in range(H):
        sy = ((row + 0.5) / H - 0.5) * height_m
        for col in range(W):
            sx = ((col + 0.5) / W - 0.5) * width_m
            origin = centre + right * sx + up * sy - fwd * 60.0
            hit, loc, nrm, idx, obj, _ = scene.ray_cast(dg, origin, fwd)
            if not hit:
                continue
            mat = obj.data.materials[obj.data.polygons[idx].material_index].name
            n = nrm.normalized()
            if n.dot(fwd) > 0:
                n = -n
            N[row, col] = n
            D[row, col] = (loc - origin).dot(fwd)
            S[row, col] = mats.index(mat)
            ndl = n.dot(sun)
            tone = 2 if ndl > 0.6 else (1 if ndl > 0.2 else 0)
            C[row, col] = day[mat][tone]
    roof_i = mats.index("Roof")
    timbered = {mats.index(m) for m in ("Wood", "Plaster", "Roof", "Stone")}
    ink = np.zeros((H, W), dtype=bool)
    f = np.array(fwd, dtype=np.float32)
    for row in range(H):
        for col in range(W):
            sp = S[row, col]
            if sp not in timbered:
                continue
            n, d = N[row, col], D[row, col]
            for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                r2, c2 = row + dr, col + dc
                if not (0 <= r2 < H and 0 <= c2 < W):
                    continue
                dq, nq, sq = D[r2, c2], N[r2, c2], S[r2, c2]
                if dq - d > 0.35:
                    ink[row, col] = True
                    break
                if sp == roof_i and sq >= 0 and abs(dq - d) < 0.35 and float(n @ nq) < math.cos(math.radians(40)):
                    to_cam = float(nq @ f) - float(n @ f)
                    if sq != roof_i or to_cam < -1e-3 or (abs(to_cam) <= 1e-3 and n[0] > nq[0]):
                        ink[row, col] = True
                        break
    mixed = C.copy()
    mixed[ink] = mixed[ink] * 0.65 + ink_col * 0.35
    solid = C.copy()
    solid[ink] = (20, 18, 30)

    # How many of the front slope's joints and lips, seen in the crop, take ink.
    def pixel_of(p):
        v = p - centre
        col = int(math.floor((v.dot(right) / width_m + 0.5) * W))
        row = int(math.floor((v.dot(up) / height_m + 0.5) * H))
        return row, col

    def seen(p):
        origin = p - fwd * 60.0
        hit, loc, *_ = scene.ray_cast(dg, origin, fwd)
        return hit and (loc - p).length < 0.15

    def inked_near(p, reach=1):
        row, col = pixel_of(p)
        if not (reach <= row < H - reach and reach <= col < W - reach):
            return None
        return bool(ink[row - reach:row + reach + 1, col - reach:col + reach + 1].any())
    joints_seen = joints_inked = lips_seen = lips_inked = 0
    for rec in b.tiling:
        if rec["part"] != "Roof" or rec["slope"] != "front":
            continue
        for p0, pc, p1 in rec["groove_pts"]:
            if not seen(pc):
                continue
            got = inked_near(pc)
            if got is None:
                continue
            joints_seen += 1
            joints_inked += got
        a, c, h = rec["lip"]
        steps = max(1, int((c - a).length / 0.5))
        for k in range(1, steps):
            p = a.lerp(c, k / steps) - Vector((0, 0, h / 2))
            if not seen(p + Vector((0, 0, h / 2 - 0.01))):
                continue
            got = inked_near(p)
            if got is None:
                continue
            lips_seen += 1
            lips_inked += got

    # Panel 3: Workbench at 72 px/m, the same crop.
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = W * scale, H * scale
    scene.render.resolution_percentage = 100
    world = bpy.data.worlds.get("preview") or bpy.data.worlds.new("preview")
    world.color = srgb(110, 128, 96)
    scene.world = world
    cam_data = bpy.data.cameras.new("roof_preview")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = width_m
    cam = bpy.data.objects.new("roof_preview", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = centre - fwd * 60.0
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    path = os.path.join(os.path.dirname(ROOF_PREVIEW), "_tavern_exterior_roof_smooth.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
    bpy.data.cameras.remove(cam_data)
    img = bpy.data.images.load(path)
    smooth = np.empty(W * scale * H * scale * 4, dtype=np.float32)
    img.pixels.foreach_get(smooth)
    smooth = smooth.reshape(H * scale, W * scale, 4)
    bpy.data.images.remove(img)
    os.remove(path)

    # A new (byte) image's pixels are its display values, as the loaded
    # render's are: no colour transform either way.
    def to_rgba(c):
        big = np.repeat(np.repeat(c / 255.0, scale, axis=0), scale, axis=1)
        return np.concatenate([big, np.ones(big.shape[:2] + (1,), dtype=np.float32)], axis=2)
    gap = np.ones((H * scale, 8, 4), dtype=np.float32)
    sheet = np.concatenate([to_rgba(mixed), gap, to_rgba(solid), gap, smooth], axis=1)
    out = bpy.data.images.new("tavern_exterior_roof", sheet.shape[1], sheet.shape[0])
    out.pixels.foreach_set(sheet.astype(np.float32).ravel())
    out.filepath_raw = ROOF_PREVIEW
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    print("wrote", ROOF_PREVIEW)
    return {"front_joints_seen": joints_seen, "front_joints_inked": joints_inked,
            "front_lip_points_seen": lips_seen, "front_lip_points_inked": lips_inked,
            "roof_ink_pixels": int(ink[S == roof_i].sum()), "roof_pixels": int((S == roof_i).sum())}


# The engine's day palette (game/src/scene.cpp): shadow, mid and lit tones.
DAY = {
    "Stone": ((0x74, 0x74, 0x80), (0xa4, 0x9c, 0x8c), (0xbc, 0xac, 0x94)),
    "Plaster": ((0x9c, 0x8a, 0x70), (0xc8, 0xbc, 0xa2), (0xe2, 0xd6, 0xb6)),
    "Wood": ((0x4c, 0x36, 0x26), (0x86, 0x66, 0x4e), (0xa8, 0x80, 0x58)),
    "Roof": ((0x3c, 0x44, 0x54), (0x5a, 0x66, 0x76), (0x7e, 0x8a, 0x98)),
    "Dirt": ((0x5c, 0x64, 0x68), (0x7c, 0x78, 0x74), (0x94, 0x8c, 0x80)),
}


def game_crop(centre, width_m, height_m, yaw_deg, scale, smooth_name):
    """One crop as the game's camera at yaw_deg sees it: ray cast pixel by
    pixel at 24 px/m, toon-shaded with the day palette (the sun as the roof
    preview's), with the edge pass's outline (a depth step over 0.35 m);
    scaled `scale` times, nearest neighbour. Beside it a smooth Workbench
    render of the same crop at `scale` x 24 px/m. Returns an RGBA array
    (rows bottom-up, as Blender images)."""
    import numpy as np

    scene = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()
    fwd, right, up = view_axes(yaw_deg)
    W, H = int(round(width_m * PX_PER_M)), int(round(height_m * PX_PER_M))
    sun = Vector((-0.35, -0.55, 0.76)).normalized()
    D = np.full((H, W), 1e9, dtype=np.float32)
    C = np.zeros((H, W, 3), dtype=np.float32)
    C[:] = (110, 128, 96)
    centre = Vector(centre)
    for row in range(H):
        sy = ((row + 0.5) / H - 0.5) * height_m
        for col in range(W):
            sx = ((col + 0.5) / W - 0.5) * width_m
            origin = centre + right * sx + up * sy - fwd * 60.0
            hit, loc, nrm, idx, obj, _ = scene.ray_cast(dg, origin, fwd)
            if not hit:
                continue
            mat = obj.data.materials[obj.data.polygons[idx].material_index].name
            n = nrm.normalized()
            if n.dot(fwd) > 0:
                n = -n
            D[row, col] = (loc - origin).dot(fwd)
            ndl = n.dot(sun)
            C[row, col] = DAY[mat][2 if ndl > 0.6 else (1 if ndl > 0.2 else 0)]
    ink = np.zeros((H, W), dtype=bool)
    for dr, dc in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        shifted = np.full((H, W), 1e9, dtype=np.float32)
        r0, r1 = max(0, dr), H + min(0, dr)
        c0, c1 = max(0, dc), W + min(0, dc)
        shifted[r0 - dr:r1 - dr, c0 - dc:c1 - dc] = D[r0:r1, c0:c1]
        ink |= (shifted - D > 0.35) & (D < 1e8)
    C[ink] = C[ink] * 0.65 + np.array((0x4c, 0x4c, 0x66), dtype=np.float32) * 0.35
    toon = np.repeat(np.repeat(C / 255.0, scale, axis=0), scale, axis=1)
    toon = np.concatenate([toon, np.ones(toon.shape[:2] + (1,), dtype=np.float32)], axis=2)

    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = W * scale, H * scale
    scene.render.resolution_percentage = 100
    world = bpy.data.worlds.get("preview") or bpy.data.worlds.new("preview")
    world.color = srgb(110, 128, 96)
    scene.world = world
    cam_data = bpy.data.cameras.new(smooth_name)
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = max(width_m, height_m)
    cam = bpy.data.objects.new(smooth_name, cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cam.location = centre - fwd * 60.0
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    path = os.path.join(os.path.dirname(MASONRY_PREVIEW), f"_{smooth_name}.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
    bpy.data.cameras.remove(cam_data)
    img = bpy.data.images.load(path)
    smooth = np.empty(W * scale * H * scale * 4, dtype=np.float32)
    img.pixels.foreach_get(smooth)
    smooth = smooth.reshape(H * scale, W * scale, 4)
    bpy.data.images.remove(img)
    os.remove(path)
    gap = np.ones((H * scale, 6, 4), dtype=np.float32)
    return np.concatenate([toon, gap, smooth], axis=1)


def render_masonry_preview(b):
    """tavern_exterior_masonry.png: three rows, each a 24 px/m toon crop
    (scaled up) beside a smooth render of the same crop -
    1. the plinth, threshold and the patch under the left window, from the
       game's default camera (8 x 3 m, 3x);
    2. the chimney from the front-right (yaw 225; 3.2 x 9.6 m, 3x) beside
       the patch over the right window from the default camera (2 x 1.2 m, 6x)."""
    import numpy as np

    rows = [
        [game_crop((-1.5, -0.4, 0.9), 8.0, 3.0, DEFAULT_VIEW_YAW, 3, "m_plinth")],
        [game_crop((5.1, 4.4, 4.0), 3.2, 9.6, 180.0 + 45.0, 3, "m_chimney"),
         game_crop((3.25, -0.17, 3.72), 2.0, 1.2, DEFAULT_VIEW_YAW, 6, "m_patch")],
    ]
    lines = []
    for row in rows:
        h = max(p.shape[0] for p in row)
        padded = []
        for p in row:
            pad = np.ones((h - p.shape[0], p.shape[1], 4), dtype=np.float32)
            padded += [np.concatenate([pad, p], axis=0), np.ones((h, 16, 4), dtype=np.float32)]
        lines.append(np.concatenate(padded[:-1], axis=1))
    w = max(l.shape[1] for l in lines)
    out_rows = []
    for l in reversed(lines):  # image rows are bottom-up
        out_rows.append(np.concatenate([l, np.ones((l.shape[0], w - l.shape[1], 4), dtype=np.float32)], axis=1))
        out_rows.append(np.ones((16, w, 4), dtype=np.float32))
    sheet = np.concatenate(out_rows[:-1], axis=0)
    out = bpy.data.images.new("tavern_exterior_masonry", sheet.shape[1], sheet.shape[0])
    out.pixels.foreach_set(sheet.astype(np.float32).ravel())
    out.filepath_raw = MASONRY_PREVIEW
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    print("wrote", MASONRY_PREVIEW)


def render_hall_preview(b):
    """tavern_exterior_hall.png (revision 6): the hall through the doorway.
    Top: the doorway from the game's default camera (as the dark-hall
    capture sees it), 24 px/m toon (3x) beside a smooth render - with no
    door and no darkening, so the floor and side walls show plainly.
    Bottom: the same yaw from 60 degrees down with the roof, gables and
    dormers hidden, so the whole hall shows inside the shell."""
    import numpy as np

    top = game_crop((0.0, 1.2, 1.9), 6.0, 4.5, DEFAULT_VIEW_YAW, 3, "h_doorway")
    scene = bpy.context.scene
    hidden = [bpy.data.objects[n] for n in ROOF_PARTS]
    for o in hidden:
        o.hide_render = True
    W, H = top.shape[1], 720
    scene.render.resolution_x, scene.render.resolution_y = W, H
    cam_data = bpy.data.cameras.new("h_cutaway")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 12.0 * W / H if W > H else 12.0
    cam = bpy.data.objects.new("h_cutaway", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    yaw, pitch = math.radians(DEFAULT_VIEW_YAW), math.radians(60)
    fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
    cam.location = Vector((0.3, 3.0, 1.5)) - fwd * 80
    cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
    path = os.path.join(os.path.dirname(HALL_PREVIEW), "_h_cutaway.png")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    bpy.data.objects.remove(cam)
    bpy.data.cameras.remove(cam_data)
    for o in hidden:
        o.hide_render = False
    img = bpy.data.images.load(path)
    low = np.empty(W * H * 4, dtype=np.float32)
    img.pixels.foreach_get(low)
    bpy.data.images.remove(img)
    os.remove(path)
    low = low.reshape(H, W, 4)
    sheet = np.concatenate([low, np.ones((16, W, 4), dtype=np.float32), top], axis=0)  # rows bottom-up
    out = bpy.data.images.new("tavern_exterior_hall", W, sheet.shape[0])
    out.pixels.foreach_set(sheet.astype(np.float32).ravel())
    out.filepath_raw = HALL_PREVIEW
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    print("wrote", HALL_PREVIEW)


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
    problems, total, per, lo, hi = check(b)
    print(f"tavern_exterior: {total} tris; " + ", ".join(f"{k} {v}" for k, v in per.items()))
    print(f"tavern_exterior: bounds (Blender) {tuple(round(c, 3) for c in lo)} .. {tuple(round(c, 3) for c in hi)}")
    for k, v in figures(b).items():
        print(f"tavern_exterior: {k}: {v}")
    for k, v in tile_stats(b).items():
        print(f"tavern_exterior: tiles: {k}: {v}")
    print(f"tavern_exterior: masonry: {b.masonry_summary}")
    print(f"tavern_exterior: render: {b.render_summary}")
    if problems:
        raise RuntimeError("the exterior fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_preview(b)
        print(f"tavern_exterior: roof preview ink: {render_roof_preview(b)}")
        render_masonry_preview(b)
        render_hall_preview(b)


if __name__ == "__main__" and bpy.app.background:
    main()
