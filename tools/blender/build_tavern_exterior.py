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
a short dark vestibule (Dirt walls, Wood floor) so the open door shows an
inside rather than the back of the walls.

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
VESTIBULE = (-1.9, 1.9, 3.0)        # x0, x1 and the back wall's face (y)
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
TRI_BUDGET = 8000                   # revision 4 (revision 3: 4,000)
PLINTH_SIZE = (9.0, 7.0)            # checked
MIN_VESTIBULE = 2.5                 # checked: the trigger zone's depth
MIN_SIGN_TO_JAMB = 0.3              # checked: the sign's nearest part to the jamb's outer edge
# Revision 3's roof rules (checked): everything the new roof adds stays inside
# revision 2's eaves in plan, its top at most MAX_RISE above revision 2's
# ridge, and nothing of it over the doorway or vestibule below HEADROOM.
REV2_RIDGE_TOP = 9.13
MAX_RISE = 0.5
HEADROOM = 3.3
REV2_PLAN = (-4.55, 5.59, -1.77, 7.1)   # the whole exterior's plan outline (Blender x0, x1, y0, y1)
ROOF_PARTS = ["Roof", CROSS_GABLE["name"]] + [d["name"] for d in DORMERS]
# Engine rectangles (assets.md / scene.cpp kTavernDoorway, kTavernVestibule) in Blender (x0, x1, y0, y1).
DOORWAY_RECT = (-1.1, 1.1, -0.85, 0.25)
VESTIBULE_RECT = (-1.9, 1.9, 0.25, 3.0)


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


def build_gable(part, g, tiling=None, roll_tiling=None):
    """A gable on the roof: a Plaster body under a slated gable roof in
    stepped courses with a ridge roll; Wood framing on its face (or a dark
    window for a dormer); carved bargeboards with stepped notches under the
    rake, a pendant at the apex and a finial above it."""
    P, OUT, seat, eave, ridge, we, zb, depth = gable_frame(g)
    hw = g["hw"]
    foot = seat - 0.15
    t, e = COURSE_T, COURSE_E
    v0 = -g["ov_front"]
    # Body: the face and cheeks, just under the roof's underside.
    b = 0.03
    prism_uz(part, [(-hw, foot), (hw, foot), (hw, eave - b), (0.0, ridge - b), (-hw, eave - b)],
             0.0, depth, "Plaster", P, OUT)
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
        """A plinth of stone blocks round the walls, top at PLINTH_TOP, bottom
        at FOOT, with a threshold stone through the doorway flush with the
        floor. Each block's face and top are nudged a little, so the course
        reads as laid stone; neighbours never share a face plane."""
        part = Part("Foundation")
        ox0, ox1 = X0 - PLINTH_OUT, X1 + PLINTH_OUT
        oy0, oy1 = Y0 - PLINTH_OUT, Y1 + PLINTH_OUT
        depth = (PY0 + WALL_T) - oy0  # flush with the walls' inside faces
        nudges = [0.0, -0.025, -0.01, -0.035, -0.015, -0.02, -0.03, -0.005]  # inward only: the outline is exact
        heights = [0.0, 0.03, -0.02, 0.04, 0.01, -0.03, 0.02, -0.01]
        k = 0

        def run(s0, s1, place, target=1.35):
            nonlocal k
            n = max(1, round((s1 - s0) / target))
            for i in range(n):
                a = s0 + (s1 - s0) * i / n
                b = s0 + (s1 - s0) * (i + 1) / n
                place(a, b, nudges[k % len(nudges)], PLINTH_TOP + heights[k % len(heights)])
                k += 1

        # Front, either side of the threshold; back; then the ends between them.
        dl, dr = DOORWAY
        run(ox0, dl, lambda a, b, d, top: part.box((a, oy0 - d, FOOT), (b, oy0 + depth, top), "Stone"))
        run(dr, ox1, lambda a, b, d, top: part.box((a, oy0 - d, FOOT), (b, oy0 + depth, top), "Stone"))
        run(ox0, ox1, lambda a, b, d, top: part.box((a, oy1 - depth, FOOT), (b, oy1 + d, top), "Stone"))
        run(oy0 + depth, oy1 - depth,
            lambda a, b, d, top: part.box((ox0 - d, a, FOOT), (ox0 + depth, b, top), "Stone"), target=1.5)
        run(oy0 + depth, oy1 - depth,
            lambda a, b, d, top: part.box((ox1 - depth, a, FOOT), (ox1 + d, b, top), "Stone"), target=1.5)
        # Threshold: a step stone through the wall, a little proud of the plinth.
        part.box((dl, oy0 - 0.12, FOOT), (dr, PY0 + WALL_T, 0.0), "Stone")
        part.finish(self.coll)

    # ---- plaster ----------------------------------------------------------

    def walls(self):
        """Plaster walls from the plinth to the wall plate, with real holes
        for the windows and the doorway (the panes sit back in them)."""
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
                    part.wall_box(wall, a, b, -WALL_T, 0.0, lo, hi, "Plaster")
        part.finish(self.coll)

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
            # Posts: either side of each window (the doorway's jambs stand in
            # for posts there), then fill so no panel is wider than 2.4 m.
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
            for a, b in drawn:
                part.wall_box(wall, a, b, 0.0, POST_PROUD, PLINTH_TOP, PLATE_TOP, "Wood")
            every = sorted(set(fixed + fill))
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
        vestibule behind it: a Wood floor and dark Dirt walls, open to the
        doorway, deep enough for the door to swing 100 degrees inward."""
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
        vest.box((vx0 - 0.2, yi, 0.0), (vx0, vy1 + 0.2, PLATE_TOP - 0.2), "Dirt")
        vest.box((vx1, yi, 0.0), (vx1 + 0.2, vy1 + 0.2, PLATE_TOP - 0.2), "Dirt")
        vest.box((vx0, vy1, 0.0), (vx1, vy1 + 0.2, PLATE_TOP - 0.2), "Dirt")
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
            self.gable_figs[g["name"]] = build_gable(part, g, self.tiling, self.roll_tiling)
            part.finish(self.coll)

    def chimney(self):
        """A stone stack up the right-hand end wall: a broad base to above the
        eave, a sloped shoulder, a slimmer flue stack in courses, a cap and a
        pot."""
        part = Part("Chimney")
        x0, x1 = CHIMNEY_X
        y0, y1 = CHIMNEY_Y
        part.box((x0, y0, FOOT), (x1, y1, PLINTH_TOP + 0.05), "Stone")
        part.box((x0 + 0.04, y0 + 0.04, PLINTH_TOP + 0.05), (x1 - 0.04, y1 - 0.04, 2.6), "Stone")
        part.box((x0 + 0.02, y0 + 0.06, 2.6), (x1 - 0.06, y1 - 0.02, 4.9), "Stone")
        # Shoulder: the base narrows to the flue over 0.6 m.
        fx0, fx1, fy0, fy1 = x0 + 0.15, x1 - 0.08, y0 + 0.28, y1 - 0.28
        b0, b1 = (x0 + 0.02, y0 + 0.06), (x1 - 0.06, y1 - 0.02)
        part.hexa([(b0[0], b0[1], 4.9), (b1[0], b0[1], 4.9), (b1[0], b1[1], 4.9), (b0[0], b1[1], 4.9),
                   (fx0, fy0, 5.5), (fx1, fy0, 5.5), (fx1, fy1, 5.5), (fx0, fy1, 5.5)], "Stone", skip=("-z",))
        z = 5.5
        jog = [(0.0, 0.0), (0.03, -0.02), (-0.02, 0.03), (0.02, 0.01), (-0.01, -0.03)]
        courses = 4
        step = (CHIMNEY_TOP - z) / courses
        for k in range(courses):
            dx, dy = jog[k]
            part.box((fx0 + dx, fy0 + dy, z), (fx1 + dx, fy1 + dy, z + step), "Stone")
            z += step
        part.box((fx0 - 0.12, fy0 - 0.12, z), (fx1 + 0.12, fy1 + 0.12, z + 0.25), "Stone")
        cx, cy = (fx0 + fx1) / 2, (fy0 + fy1) / 2
        part.box((cx - 0.2, cy - 0.2, z + 0.25), (cx + 0.2, cy + 0.2, z + 0.7), "Dirt")
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
    return problems, total, per, lo, hi


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
    if problems:
        raise RuntimeError("the exterior fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_preview(b)
        print(f"tavern_exterior: roof preview ink: {render_roof_preview(b)}")


if __name__ == "__main__" and bpy.app.background:
    main()
