"""Build the Sleeping Fox tavern as seen from outside and export
assets/scenes/tavern_exterior.glb.

Run headless (builds, checks, exports, then renders the preview beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_exterior.py
    PXE_TAVERN_OUT=/some/dir ... writes the glb and preview there instead.
    PXE_SKIP_PREVIEW=1 ... skips the preview.

Or step by step in an open Blender, to watch it go together:
    import runpy; tx = runpy.run_path(".../build_tavern_exterior.py", run_name="tavern")
    b = tx["Builder"](); b.start(); b.foundation(); b.walls(); ...  (see Builder.STEPS)
Each step adds its pieces to the Tavern_Exterior collection, leaving the rest
of the scene alone.

The building: a single-storey inn at hero scale (1.6x real), its stone
plinth 9 x 7 m (8.8 x 6.8 m over the timber faces). A plinth that reaches
0.6 m below the origin so it sits on uneven lawn; a timber frame (Wood) of
corner posts, posts, sole plate, two rails, wall plate and braces, standing
proud of cream plaster panels (Plaster); a hipped thatch roof (Roof) at 45
degrees in two courses with a ridge roll, its ridge lower than the house is
wide; a stone chimney up the right-hand end wall (Stone); dark
recessed window panes (Dirt) behind Wood mullions; and a hanging sign by the
door, a Wood board with a sleeping fox in Plaster on both faces. The doorway
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
    "Roof": srgb(120, 98, 58),
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
# Roof
EAVE_OUT = 0.55
EAVE_LOW, EAVE_TOP = 4.35, 4.9      # thatch edge, underside and top
PITCH = math.radians(45)
COURSE_STEP = 2.0                   # the upper course starts this far in from the eave
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
TRI_BUDGET = 4000
PLINTH_SIZE = (9.0, 7.0)            # checked
MIN_VESTIBULE = 2.5                 # checked: the trigger zone's depth
MIN_SIGN_TO_JAMB = 0.3              # checked: the sign's nearest part to the jamb's outer edge


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


def hip_roof(part, x0, x1, y0, y1, z_low, z_top, mat, chamfer=0.25):
    """A hipped roof solid over the rectangle at its eave: an underside at
    z_low, a thatch edge that rises vertically then rolls in (a chamfer) to
    z_top, then four slopes at PITCH up to the ridge. The roof surface meets
    the eave line at z_top, inset by `chamfer` so the edge reads as rounded."""
    t = math.tan(PITCH)
    ridge_inset = (y1 - y0) / 2
    ridge_z = z_top + (ridge_inset - chamfer) * t
    zc = z_top - chamfer * 0.6
    ring_low = [(x0, y0, z_low), (x1, y0, z_low), (x1, y1, z_low), (x0, y1, z_low)]
    ring_mid = [(x0, y0, zc), (x1, y0, zc), (x1, y1, zc), (x0, y1, zc)]
    c = chamfer
    ring_top = [(x0 + c, y0 + c, z_top), (x1 - c, y0 + c, z_top), (x1 - c, y1 - c, z_top), (x0 + c, y1 - c, z_top)]
    ym = (y0 + y1) / 2
    ridge = [(x0 + ridge_inset, ym, ridge_z), (x1 - ridge_inset, ym, ridge_z)]
    part.face(list(reversed(ring_low)), mat)
    for i in range(4):
        j = (i + 1) % 4
        part.face([ring_low[i], ring_low[j], ring_mid[j], ring_mid[i]], mat)
        part.face([ring_mid[i], ring_mid[j], ring_top[j], ring_top[i]], mat)
    # Slopes: front and back trapezoids, the hipped ends triangles.
    part.face([ring_top[0], ring_top[1], ridge[1], ridge[0]], mat)
    part.face([ring_top[1], ring_top[2], ridge[1]], mat)
    part.face([ring_top[2], ring_top[3], ridge[0], ridge[1]], mat)
    part.face([ring_top[3], ring_top[0], ridge[0]], mat)
    return ring_top, ridge


def roof_height(x, y, x0, x1, y0, y1, z_top, chamfer=0.25):
    """Height of hip_roof's slopes over (x, y)."""
    inset = min(x - x0, x1 - x, y - y0, y1 - y)
    return z_top + (inset - chamfer) * math.tan(PITCH)


class Builder:
    STEPS = ["start", "foundation", "walls", "timbers", "windows", "doorway", "roof", "chimney", "sign"]

    def __init__(self):
        self.coll = None
        self.posts = {}  # wall -> [(s0, s1)] post spans, for the braces and the sign

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
        """Steep hipped thatch in two courses - the upper one laps over the
        lower with a thick rolled edge, so the roof reads as layered thatch -
        and a ridge roll along the ridge and down the hips."""
        part = Part("Roof")
        ex0, ex1, ey0, ey1 = X0 - EAVE_OUT, X1 + EAVE_OUT, Y0 - EAVE_OUT, Y1 + EAVE_OUT
        hip_roof(part, ex0, ex1, ey0, ey1, EAVE_LOW, EAVE_TOP, "Roof")
        c = COURSE_STEP
        z_at = roof_height(ex0 + c, (ey0 + ey1) / 2, ex0, ex1, ey0, ey1, EAVE_TOP)
        lap = 0.12
        top2, ridge = hip_roof(part, ex0 + c - lap, ex1 - c + lap, ey0 + c - lap, ey1 - c + lap,
                               z_at - 0.3, z_at + 0.3, "Roof", chamfer=0.18)
        # Ridge roll: a triangular-section roll along the ridge and each hip.
        r = 0.32

        def roll(a, b):
            a, b = Vector(a), Vector(b)
            d = (b - a).normalized()
            side = d.cross(Vector((0, 0, 1)))
            if side.length < 1e-4:
                side = Vector((1, 0, 0))
            side.normalize()
            up = side.cross(d).normalized()
            if up.z < 0:
                up = -up
            ring_a = [a + side * r - up * 0.12, a + up * r * 0.75, a - side * r - up * 0.12]
            ring_b = [b + side * r - up * 0.12, b + up * r * 0.75, b - side * r - up * 0.12]
            for i in range(2):
                part.face([ring_a[i], ring_b[i], ring_b[i + 1], ring_a[i + 1]], "Roof", up + (side if i == 0 else -side))
            part.face(ring_a, "Roof", -d)
            part.face(ring_b, "Roof", d)

        lift = Vector((0, 0, 0.05))
        r0, r1 = Vector(ridge[0]) + lift, Vector(ridge[1]) + lift
        roll(r0, r1)
        for corner, end in ((top2[0], r0), (top2[3], r0), (top2[1], r1), (top2[2], r1)):
            roll(Vector(corner) + Vector((0, 0, 0.08)), end)
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
    if total > TRI_BUDGET * 1.05:
        problems.append(f"{total} triangles, budget about {TRI_BUDGET}")
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
    roof_at = max(roof_height(min(v.co.x, ex1), v.co.y, ex0, ex1, ey0, ey1, EAVE_TOP)
                  for v in chimney.data.vertices if v.co.x < ex1)
    if CHIMNEY_TOP < roof_at + 1.0:
        problems.append(f"chimney top {CHIMNEY_TOP} is less than 1 m above the roof ({roof_at:.2f})")
    return problems, total, per, lo, hi


def render_preview(b):
    """tavern_exterior.png: three views at 30 degrees down, orthographic, as
    the engine's camera sees it - from the front-right, front-left and back-left -
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
    if problems:
        raise RuntimeError("the exterior fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_preview(b)


if __name__ == "__main__" and bpy.app.background:
    main()
