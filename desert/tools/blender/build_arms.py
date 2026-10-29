"""Build the desert player's arms and export desert/assets/characters/arms.glb.

Run headless (builds, checks, exports, then renders the previews beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_arms.py
    PXE_DESERT_OUT=/some/dir ... writes the glb and previews there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.

Or step by step in an open Blender, to watch it go together:
    import runpy; am = runpy.run_path(".../build_arms.py", run_name="arms")
    b = am["Builder"](); b.start(); b.arms(); b.gloves(); b.fingers(); b.bracer(); b.gauge(); b.rig()
    problems, m = am["check"](b)
Each step adds its pieces to the Arms collection of the current scene, leaving
the rest of the file alone.

A view model: the wanderer's two arms, modelled round the eye at real adult
scale (desert/docs/art-style.md). The origin is the eye. The model faces -Y
in Blender (engine +Z, the view's forward), +Z up; Blender +X (engine +X) is
the player's left, so the right arm is on -X. The game places the origin at
the eye with the view's axes, so this file's frame is the view's frame.

Built in the held pose, which is the rest (bind) pose: shoulders 0.38 m apart,
0.22 m below and 0.05 m behind the eye; elbows bent to 99 degrees, 8 cm below
the shoulders, 25 cm ahead of them and 15 cm out from them, so the forearms
rise from the bottom corners of the view, forward, up and inward; wrists 0.37 m
ahead, hands centred 0.42 m ahead, palms turned inward and down, fingers
relaxed and a little curled, thumbs resting along the index fingers. With real
proportions the forearm rises steeply, so only a few centimetres behind the
glove show; the cuff is kept short and the bracer's gauge sits at its wrist
end so the gauge is in view. The build checks the framing through the eye
camera (60 degree vertical field of view, 16:9), the near clearance and the
gauge, and fails if any is off.

Every piece is a closed volume (the engine culls back faces), flat-shaded,
one solid colour per material:
    arm tube      Arms_Wrap   shoulder cap to wrist; the forearm wrapped in
                              six stepped cloth bands, each lip standing
                              4-6 mm proud of the band under it, uneven in
                              width and angle
    glove         Arms_Glove  a flared cuff over the wraps, a faceted hand,
                              four fingers and a thumb in blocky segments
                              that overlap at the knuckles
    bracer (left) Arms_Brass  a brass plate in four facets over the forearm
                  Arms_Strap  two straps with square buckles
    gauge (left)  Arms_Brass  a bezel on the plate's wrist end, turned toward
                              the eye, its window recessed 3 mm, long side
                              level across the view
                  Arms_Glow   three emissive bars in the window

One skinned mesh (`Arms`) on one armature (`Arms_Rig`), 39 joints:
    root
    shoulder > upper_arm > forearm > hand              (.L / .R)
    hand > thumb.1 > thumb.2 > thumb.3, and the same for index, middle,
    ring and pinky
Weights blend over 4 cm across each elbow and wrist; every finger segment is
rigid to its joint, the bracer, straps and gauge to forearm.L. The attachment
check runs in the rest pose and in test poses: each wrist twisted, flexed and
bent sideways 30 degrees each way, every finger and thumb joint curled 60
degrees, and each elbow opened 30 degrees.

Material order is the contract with the engine: glTF material i is actor
slot firstMaterial + i (desert::Arms loads into slots 0-4).
"""

import json
import math
import os
import random
import struct

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_DESERT_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "characters"))
OUT = os.path.join(OUT_DIR, "arms.glb")
PREVIEW = os.path.join(OUT_DIR, "arms.png")
PREVIEW_EYE = os.path.join(OUT_DIR, "arms_eye.png")
COLLECTION = "Arms"
MESH = "Arms"
RIG = "Arms_Rig"

X = Vector((1, 0, 0))
Y = Vector((0, 1, 0))
Z = Vector((0, 0, 1))


def v(x, y, z):
    return Vector((x, y, z))


def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def to_srgb(c):
    """The engine's conversion back to display values (skinned.cpp toSrgb)."""
    c = max(0.0, min(1.0, c))
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1.0 / 2.4) - 0.055


def clamp01(x):
    return max(0.0, min(1.0, x))


def smoothstep(a, b, x):
    t = clamp01((x - a) / (b - a))
    return t * t * (3.0 - 2.0 * t)


def rot(axis, deg, vec):
    return Matrix.Rotation(math.radians(deg), 3, Vector(axis).normalized()) @ Vector(vec)


def mirror(p):
    return Vector((-p.x, p.y, p.z))


# --------------------------------------------------------------------------
# Materials, in the engine's order: (name, sRGB mid tone, emissive).

MATERIALS = [
    ("Arms_Wrap", (206, 194, 168), False),   # bleached cloth bands
    ("Arms_Glove", (78, 54, 40), False),     # dark leather gloves
    ("Arms_Strap", (118, 84, 56), False),    # bracer straps and buckles
    ("Arms_Brass", (176, 132, 64), False),   # worn brass bracer plate and bezel
    ("Arms_Glow", (90, 230, 210), True),     # the gauge's bars
]
M = {name: i for i, (name, _, _) in enumerate(MATERIALS)}
TRI_BUDGET = (2000, 3000)

# --------------------------------------------------------------------------
# The eye camera the framing is checked through, as the engine's view.

FOV_Y = math.radians(60.0)
ASPECT = 16.0 / 9.0
NEAR = 0.1
TAN_Y = math.tan(FOV_Y / 2.0)
VIEW_W, VIEW_H = 960, 540

FRAME_TOP = -0.30            # nothing in view above this NDC y (the lower 35%)
KNUCKLE_Y = (-0.70, -0.50)   # top of each index knuckle, NDC y
HAND_X = (0.40, 0.60)        # each hand's centre, |NDC x|
CLEARANCE = 0.15             # every vertex in view at least this far ahead (m)
BAR_MIN_PX = 3.0             # each gauge bar at least this wide on screen


def project(p):
    """NDC x (screen right +), NDC y (up +) and depth along the view, or None
    behind the eye."""
    d = -p.y
    if d <= 1e-9:
        return None
    return (-p.x / (d * TAN_Y * ASPECT), p.z / (d * TAN_Y), d)


def in_view(p):
    q = project(p)
    return q is not None and abs(q[0]) <= 1.0 and abs(q[1]) <= 1.0


# --------------------------------------------------------------------------
# The held pose, for the left arm (+X); the right arm mirrors it.

SHOULDER = v(0.19, 0.05, -0.22)     # 0.38 m apart, 0.22 below and 0.05 behind the eye
CLAVICLE = v(0.03, 0.075, -0.205)   # the shoulder joint's head, at the base of the neck
UPPER_LEN = 0.30
FORE_LEN = 0.26
WRIST = v(0.215, -0.370, -0.155)    # where the forearm meets the hand
ELBOW_SWING = 48.0                  # degrees the elbow swings out from straight below the shoulder-wrist line
HAND_YAW = 16.0                      # the hand points this far inward (degrees)
HAND_PITCH = 16.0                   # and this far down
HAND_ROLL = 24.0                    # the palm turns from facing down toward facing inward

# Forearm (cloth over it): radius at the wrist and at the elbow.
FORE_R0, FORE_R1 = 0.030, 0.044
UPPER_R = 0.050
SIDES = 12        # 30 degrees between sides: under the 40 degree crease ink
BAND_COUNT = 6
BAND_START = -0.026            # the first band's foot, under the cuff (m from the wrist along the forearm)
BAND_W = (0.024, 0.030)        # band widths
BAND_H = (0.0042, 0.0058)      # how far each lip stands proud of the band under it
BAND_TILT = (1.5, 4.5)         # degrees each band edge leans
BEVEL = math.radians(60.0)     # each lip's bevel down to the band under it, from the axis
CREASE_MIN = 40.0              # each band edge must turn more than this

# Glove cuff: (s from the wrist along the forearm, radius); the first is the rear face.
CUFF = [(-0.036, 0.045), (-0.031, 0.046), (-0.019, 0.041), (-0.004, 0.036), (0.012, 0.034)]
# Hand: (s along the hand, half width, dorsal height, volar height). The last
# ring is the knuckle line, slanted (see palm_front).
PALM = [(-0.012, 0.023, 0.011, 0.011), (0.022, 0.037, 0.0165, 0.014), (0.058, 0.0435, 0.0175, 0.013),
        (None, 0.0445, 0.0135, 0.0105)]
PALM_SHAPE = [(1.0, 0.05), (0.5, 1.0), (0.02, 1.1), (-0.5, 0.95), (-1.0, 0.0), (-0.45, -1.0), (0.45, -1.0)]
HAND_LEN = 0.088   # wrist to the middle knuckle


def palm_front(t):
    """The knuckle line's reach along the hand at lateral offset t (thumb side +)."""
    return 0.084 + 0.075 * t


# Fingers: name, lateral offset (thumb side +), dorsal offset, splay (degrees,
# toward the thumb +), segment lengths, rest curls (degrees, each joint),
# width at the base, thickness at the base.
FINGERS = [
    ("index", 0.0315, 0.002, 2.5, (0.043, 0.025, 0.021), (12.0, 20.0, 13.0), 0.0210, 0.0175),
    ("middle", 0.0105, 0.003, 0.5, (0.047, 0.028, 0.023), (15.0, 23.0, 15.0), 0.0215, 0.0180),
    ("ring", -0.0105, 0.002, -2.0, (0.044, 0.027, 0.022), (18.0, 26.0, 16.0), 0.0210, 0.0172),
    ("pinky", -0.0305, 0.000, -5.0, (0.035, 0.021, 0.019), (22.0, 29.0, 18.0), 0.0180, 0.0150),
]
OVERLAP = 0.004   # each segment reaches this far past its joints, so knuckles overlap
# Thumb: base (along hand, lateral, dorsal), first direction (f, t, n), segment
# lengths, bends toward the index (degrees), widths, thicknesses.
THUMB_BASE = (0.020, 0.022, -0.011)
THUMB_DIR = (0.80, 0.40, -0.45)
THUMB_LEN = (0.046, 0.033, 0.027)
THUMB_BEND = (0.0, 30.0, 16.0)
THUMB_W = (0.026, 0.0205, 0.0185)
THUMB_T = (0.022, 0.0180, 0.0160)

# Bracer, left forearm: along the forearm (s from the wrist), angular span
# about the back of the forearm, thickness.
BRACER_S = (-0.160, -0.040)
BRACER_SPAN = 70.0     # degrees either side of the back of the forearm: four 35 degree facets
BRACER_T = 0.004
BRACER_SIT = 0.0045    # the plate's inside sits this far out from a band foot, into the lips
STRAPS = (-0.148, -0.090)   # strap centres along the forearm, behind the gauge
STRAP_W = 0.014
STRAP_T = 0.003
BUCKLE = (0.013, 0.013, 0.0035)
# Gauge: centre along the forearm, angle round from the back of the forearm
# toward the eye side, how far its face turns from the plate toward the eye.
GAUGE_S = -0.0585
GAUGE_TURN = 0.55
WINDOW = (0.045, 0.022)
BEZEL = 0.005
RECESS = 0.003
BAR = (0.010, 0.015)
BAR_GAP = 0.0035


def ring_frame(t, ref):
    t = t.normalized()
    f = ref - t * ref.dot(t)
    if f.length < 1e-5:
        f = Z - t * Z.dot(t)
    f.normalize()
    return f.cross(t).normalized(), f


class Side:
    """The held pose's joints and frames for one arm, in world space."""

    def __init__(self, tag):
        self.tag = tag
        mir = tag == "R"
        P = mirror if mir else (lambda p: Vector(p))
        S, W = SHOULDER.copy(), WRIST.copy()
        # Elbow: where the 0.30 upper arm and 0.26 forearm meet, swung out
        # from straight below the shoulder-wrist line.
        d = W - S
        L = d.length
        d.normalize()
        a = (UPPER_LEN ** 2 - FORE_LEN ** 2 + L ** 2) / (2 * L)
        r = math.sqrt(max(UPPER_LEN ** 2 - a * a, 0.0))
        down = (-Z - d * (-Z).dot(d)).normalized()
        out = d.cross(down).normalized()
        if out.x < 0:
            out = -out
        sw = math.radians(ELBOW_SWING)
        E = S + d * a + (down * math.cos(sw) + out * math.sin(sw)) * r
        # Hand frame: f forward, n the back of the hand, t toward the thumb.
        yaw, pitch = math.radians(HAND_YAW), math.radians(HAND_PITCH)
        f = v(-math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch))
        n0 = (Z - f * Z.dot(f)).normalized()
        o = n0.cross(f).normalized()   # with f forward and n0 up, this points outward (+X) for the left
        if o.x < 0:
            o = -o
        roll = math.radians(HAND_ROLL)
        n = (n0 * math.cos(roll) + o * math.sin(roll)).normalized()
        t = f.cross(n).normalized()    # toward the thumb: inward (-X) on the left hand
        self.S, self.E, self.W = P(S), P(E), P(W)
        self.C = P(CLAVICLE)
        self.f, self.n, self.t = P(f), P(n), P(t)
        self.au = (self.E - self.S).normalized()
        self.af = (self.W - self.E).normalized()
        self.K_mid = self.W + self.f * HAND_LEN + self.n * 0.003
        # Fingers: joint positions and per-segment frames.
        self.fingers = {}
        for name, lat, dors, splay, lens, curls, w0, t0 in FINGERS:
            reach = palm_front(lat) - 0.004
            K = self.W + self.f * reach + self.t * lat + self.n * dors
            d0 = rot(self.n, splay if not mir else -splay, self.f)
            hinge = self.n.cross(d0).normalized()      # curls toward the palm for a positive angle
            pts, frames = [K], []
            dirv, up = d0.copy(), self.n.copy()
            for L_, c in zip(lens, curls):
                dirv = rot(hinge, c, dirv)
                up = rot(hinge, c, up)
                frames.append((dirv.copy(), up.copy()))
                pts.append(pts[-1] + dirv * L_)
            self.fingers[name] = (pts, frames, hinge, w0, t0)
        # Thumb.
        fb, tb, nb = THUMB_BASE
        T0 = self.W + self.f * fb + self.t * tb + self.n * nb
        fd, td, nd = THUMB_DIR
        dirv = (self.f * fd + self.t * td + self.n * nd).normalized()
        side_ax = self.n.cross(dirv).normalized()
        up = dirv.cross(side_ax).normalized()
        if up.dot(self.n) < 0:
            up = -up
        pts, frames = [T0], []
        for L_, bend in zip(THUMB_LEN, THUMB_BEND):
            # Each joint bends the thumb toward the index finger's line.
            ax = dirv.cross(self.f)
            if ax.length > 1e-6 and bend:
                dirv = rot(ax, bend, dirv)
                up = rot(ax, bend, up)
            frames.append((dirv.copy(), up.copy()))
            pts.append(pts[-1] + dirv * L_)
        self.thumb = (pts, frames, self.n.cross(frames[0][0]).normalized())
        # The top of the forearm, where the bracer sits: square to the
        # forearm, halfway between straight up and toward the eye from the gauge.
        g = self.on_forearm(GAUGE_S)
        up = Z + (-g).normalized()
        self.back = (up - self.af * up.dot(self.af)).normalized()

    def s_wrist(self, p):
        return (p - self.W).dot(self.af)

    def s_elbow(self, p):
        b = (self.au + self.af).normalized()
        return (p - self.E).dot(b)

    def on_forearm(self, s):
        """The forearm axis point s metres from the wrist (s < 0 toward the elbow)."""
        return self.W + self.af * s

    def fore_r(self, s):
        return FORE_R0 + (FORE_R1 - FORE_R0) * clamp01(-s / FORE_LEN)


def bone_names():
    names = ["root"]
    for S in "LR":
        names += [f"shoulder.{S}", f"upper_arm.{S}", f"forearm.{S}", f"hand.{S}"]
        for digit in ("thumb", "index", "middle", "ring", "pinky"):
            names += [f"{digit}.{i}.{S}" for i in (1, 2, 3)]
    return names


JOINTS = bone_names()


class Piece:
    def __init__(self, name, mat):
        self.name = name
        self.mat = mat
        self.verts = []
        self.faces = []


# --------------------------------------------------------------------------
# The builder: one bmesh, one mesh object, weights in the deform layer


class Builder:
    STEPS = ["start", "arms", "gloves", "fingers", "bracer", "gauge", "rig"]

    def __init__(self):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.group = {name: i for i, name in enumerate(JOINTS)}
        self.sides = {S: Side(S) for S in "LR"}
        self.pieces = {}
        self.attach = []     # (piece, piece) that must touch in every pose
        self.covered = []    # (piece, vertex list, [pieces]): verts that must stay inside one of the pieces
        self.band_edges = []  # per forearm, ring loops whose edges must crease
        self.bars = []
        self.obj = None
        self.rig_obj = None
        self.coll = None

    # ---- scene plumbing ---------------------------------------------------

    def start(self):
        """A fresh Arms collection and the five materials; the rest of the file is left alone."""
        old = bpy.data.collections.get(COLLECTION)
        if old:
            for ob in list(old.objects):
                data = ob.data
                bpy.data.objects.remove(ob)
                if isinstance(data, bpy.types.Mesh):
                    bpy.data.meshes.remove(data)
                elif isinstance(data, bpy.types.Armature):
                    bpy.data.armatures.remove(data)
            bpy.data.collections.remove(old)
        self.coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(self.coll)
        mats = []
        for name, srgb, glow in MATERIALS:
            colour = tuple(lin(c) for c in srgb)
            mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
            mat.diffuse_color = (*colour, 1.0)
            if mat.node_tree is None:
                mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Roughness"].default_value = 1.0
                bsdf.inputs["Metallic"].default_value = 0.0
                bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Emission Strength"].default_value = 1.0 if glow else 0.0
            mats.append(mat)
        mesh = bpy.data.meshes.new(MESH)
        for mat in mats:
            mesh.materials.append(mat)
        self.obj = bpy.data.objects.new(MESH, mesh)
        self.coll.objects.link(self.obj)
        for name in JOINTS:
            self.obj.vertex_groups.new(name=name)
        self.refresh()

    def refresh(self):
        self.bm.to_mesh(self.obj.data)
        self.obj.data.update()

    # ---- geometry helpers -------------------------------------------------

    def piece(self, name, mat):
        p = Piece(name, mat)
        self.pieces[name] = p
        return p

    def loft(self, piece, loops, cap0=True, cap1=True):
        """Faces between consecutive loops of points (each the same length),
        capped flat at the ends. Returns the BMVert loops."""
        bm = self.bm
        vl = [[bm.verts.new(Vector(p)) for p in loop] for loop in loops]
        for loop in vl:
            piece.verts += loop
        n = len(vl[0])
        for a, b in zip(vl, vl[1:]):
            for k in range(n):
                j = (k + 1) % n
                piece.faces.append(bm.faces.new((a[k], a[j], b[j], b[k])))
        if cap0:
            piece.faces.append(bm.faces.new(list(reversed(vl[0]))))
        if cap1:
            piece.faces.append(bm.faces.new(vl[-1]))
        return vl

    def outward(self, piece):
        """Wind a closed piece consistently, then turn its faces outward (positive volume)."""
        bm = self.bm
        bmesh.ops.recalc_face_normals(bm, faces=piece.faces)
        bm.normal_update()
        vol = 0.0
        for f in piece.faces:
            co = [x.co for x in f.verts]
            for i in range(1, len(co) - 1):
                vol += co[0].dot(co[i].cross(co[i + 1]))
        if vol < 0:
            for f in piece.faces:
                f.normal_flip()
            bm.normal_update()

    def finish(self, piece, weights):
        """Material, flat shading, outward faces, and weights(point) -> [(joint, w)]."""
        self.outward(piece)
        for f in piece.faces:
            f.material_index = M[piece.mat]
            f.smooth = False
        for x in piece.verts:
            self.set_weights(x, weights(x.co))

    def set_weights(self, x, ws):
        acc = {}
        for n, w in ws:
            acc[n] = acc.get(n, 0.0) + w
        items = sorted(acc.items(), key=lambda kv: -kv[1])[:4]
        total = sum(w for _, w in items) or 1.0
        x[self.deform].clear()
        for n, w in items:
            if w / total > 1e-3:
                x[self.deform][self.group[n]] = w / total

    def box_loops(self, a, b, d, up, w, th, wb=None, tb=None):
        """Two rectangular loops from a to b along d, square to `up`."""
        side = d.cross(up).normalized()
        up = side.cross(d).normalized()
        wb = w if wb is None else wb
        tb = th if tb is None else tb

        def rect(c, ww, tt):
            return [c + side * (sx * ww / 2) + up * (sy * tt / 2) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        return rect(a, w, th), rect(b, wb, tb)

    # ---- weights ------------------------------------------------------------

    def arm_weights(self, sd):
        S = sd.tag

        def ws(p):
            tu = (p - sd.S).dot(sd.au)
            w_sh = 1.0 - smoothstep(-0.03, 0.03, tu)
            w_f = smoothstep(-0.02, 0.02, sd.s_elbow(p))    # 4 cm across the elbow
            w_h = smoothstep(-0.03, 0.01, sd.s_wrist(p))    # 4 cm across the wrist
            return [(f"shoulder.{S}", w_sh * (1 - w_f)), (f"upper_arm.{S}", (1 - w_sh) * (1 - w_f)),
                    (f"forearm.{S}", w_f * (1 - w_h)), (f"hand.{S}", w_f * w_h)]
        return ws

    def wrist_weights(self, sd):
        S = sd.tag

        def ws(p):
            w_h = smoothstep(-0.03, 0.01, sd.s_wrist(p))
            return [(f"forearm.{S}", 1 - w_h), (f"hand.{S}", w_h)]
        return ws

    @staticmethod
    def rigid(name):
        return lambda p: [(name, 1.0)]

    # ---- the arms -----------------------------------------------------------

    def _ring(self, c, axis, r, ref, tilt=None):
        u, w = ring_frame(axis, ref)
        R = Matrix.Identity(3)
        if tilt:
            ang, deg = tilt
            R = Matrix.Rotation(math.radians(deg), 3, u * math.cos(ang) + w * math.sin(ang))
        out = []
        for k in range(SIDES):
            a = 2 * math.pi * (k + 0.5) / SIDES
            out.append(c + R @ ((u * math.cos(a) + w * math.sin(a)) * r))
        return out

    def arms(self):
        """Each arm tube, capped at the shoulder and inside the glove at the
        wrist, with the forearm's stepped cloth bands."""
        for S, seed in (("L", 7), ("R", 19)):
            sd = self.sides[S]
            rng = random.Random(seed)
            ref = X
            loops = []
            loops.append(self._ring(sd.S - sd.au * 0.035, sd.au, 0.044, ref))      # shoulder cap
            loops.append(self._ring(sd.S, sd.au, 0.054, ref))
            loops.append(self._ring(sd.S + sd.au * 0.13, sd.au, UPPER_R, ref))
            loops.append(self._ring(sd.E - sd.au * 0.045, sd.au, 0.047, ref))
            loops.append(self._ring(sd.E, (sd.au + sd.af).normalized(), 0.049, ref))
            loops.append(self._ring(sd.on_forearm(-FORE_LEN + 0.025), sd.af, sd.fore_r(-FORE_LEN + 0.025) + 0.001, ref))
            # Bands, elbow to wrist: band k runs from its foot at b[k] (wrist
            # side) up to its lip just short of b[k+1] (elbow side), where a
            # 60 degree bevel drops to the next band's foot.
            b = [BAND_START]
            for _ in range(BAND_COUNT):
                b.append(b[-1] - rng.uniform(*BAND_W))
            hs = [rng.uniform(*BAND_H) for _ in range(BAND_COUNT)]
            tilts = [(rng.uniform(0, 2 * math.pi), rng.uniform(*BAND_TILT) * rng.choice((-1, 1)))
                     for _ in range(BAND_COUNT + 1)]
            band_loops = []   # the band edges that show: behind the cuff's rear face
            for k in range(BAND_COUNT, -1, -1):
                foot = self._ring(sd.on_forearm(b[k]), sd.af, sd.fore_r(b[k]), ref, tilts[k])
                loops.append(foot)
                if b[k] < CUFF[0][0]:
                    band_loops.append(foot)
                if k > 0:
                    # Band k-1's lip stands over this foot, a bevel's run toward the wrist.
                    h = hs[k - 1]
                    sl = b[k] + h / math.tan(BEVEL)
                    lip = self._ring(sd.on_forearm(sl), sd.af, sd.fore_r(sl) + h, ref, tilts[k])
                    loops.append(lip)
                    if sl < CUFF[0][0]:
                        band_loops.append(lip)
            loops.append(self._ring(sd.on_forearm(0.002), sd.af, sd.fore_r(0.0) - 0.001, ref))   # inside the glove
            sd.bands = (b, hs)
            p = self.piece(f"{S}.arm", "Arms_Wrap")
            vl = self.loft(p, loops)
            self.finish(p, self.arm_weights(sd))
            self.band_edges.append((S, [vl[i] for i in range(len(loops)) if any(loops[i] is bl for bl in band_loops)], vl))
            self.covered.append((p.name, vl[-1], [f"{S}.cuff", f"{S}.palm"]))
            self.refresh()

    def gloves(self):
        """The flared cuffs over the wraps and the faceted hands."""
        for S in "LR":
            sd = self.sides[S]
            loops = [self._ring(sd.on_forearm(s), sd.af, r, X) for s, r in CUFF]
            p = self.piece(f"{S}.cuff", "Arms_Glove")
            self.loft(p, loops)
            self.finish(p, self.wrist_weights(sd))
            self.attach.append((p.name, f"{S}.arm"))
            # The hand: rings across it, the knuckle line slanted.
            loops = []
            for s, hw, hd, hv in PALM:
                loop = []
                for tx, ny in PALM_SHAPE:
                    lat = tx * hw
                    h = ny * (hd if ny > 0 else hv)
                    reach = palm_front(lat) if s is None else s
                    loop.append(sd.W + sd.f * reach + sd.t * lat + sd.n * h)
                loops.append(loop)
            p = self.piece(f"{S}.palm", "Arms_Glove")
            vl = self.loft(p, loops)
            self.finish(p, self.wrist_weights(sd))
            self.attach.append((p.name, f"{S}.cuff"))
            self.covered.append((p.name, vl[0], [f"{S}.cuff"]))
        self.refresh()

    def _segments(self, S, digit, pts, frames, widths, thicks, anchor):
        n = len(frames)
        for i, ((d, up), w, th) in enumerate(zip(frames, widths, thicks)):
            a = pts[i] - d * OVERLAP
            last = i == n - 1
            b = pts[i + 1] + (d * OVERLAP if not last else Vector())
            name = f"{S}.{digit}.{i + 1}"
            p = self.piece(name, "Arms_Glove")
            w1, t1 = w * 0.93, th * 0.93
            # A chamfered back end (the knuckle), full size 4 mm in.
            a2 = a + d * 0.004
            l0, _ = self.box_loops(a, b, d, up, w * 0.8, th * 0.78)
            la, lb = self.box_loops(a2, b, d, up, w, th, w1, t1)
            if last:
                # A chamfered fingertip: full size to just short of the tip, then smaller.
                mid = b - d * 0.006
                _, lm = self.box_loops(a2, mid, d, up, w, th, w1, t1)
                _, lt = self.box_loops(a2, b, d, up, w, th, w1 * 0.62, t1 * 0.55)
                lt = [q - up * (t1 * 0.12) for q in lt]
                self.loft(p, [l0, la, lm, lt])
            else:
                self.loft(p, [l0, la, lb])
            self.finish(p, self.rigid(f"{digit}.{i + 1}.{S}"))
            self.attach.append((name, anchor))
            anchor = name

    def fingers(self):
        """Four fingers and a thumb a hand, each a chain of three blocky
        segments overlapping at the knuckles."""
        for S in "LR":
            sd = self.sides[S]
            for name, lat, dors, splay, lens, curls, w0, t0 in FINGERS:
                pts, frames, hinge, w0, t0 = sd.fingers[name]
                widths = [w0, w0 * 0.92, w0 * 0.86]
                thicks = [t0, t0 * 0.92, t0 * 0.85]
                self._segments(S, name, pts, frames, widths, thicks, f"{S}.palm")
            pts, frames, _ = sd.thumb
            self._segments(S, "thumb", pts, frames, THUMB_W, THUMB_T, f"{S}.palm")
        self.refresh()

    # ---- bracer and gauge (left forearm) -----------------------------------

    def _bracer_point(self, sd, s, ang, r):
        """A point on the left forearm at s, `ang` degrees round from its back, radius r."""
        side = sd.af.cross(sd.back).normalized()
        a = math.radians(ang)
        return sd.on_forearm(s) + (sd.back * math.cos(a) + side * math.sin(a)) * r

    def outside_angle(self, sd):
        """The angle round the left forearm (from its back) facing most outward, +X, off the plate."""
        side = sd.af.cross(sd.back).normalized()
        best = max((a for a in range(int(BRACER_SPAN) + 15, 360 - int(BRACER_SPAN) - 15)),
                   key=lambda a: (sd.back * math.cos(math.radians(a)) + side * math.sin(math.radians(a))).x)
        return float(best)

    def radius_at(self, sd, p):
        q = p - sd.W
        return (q - sd.af * q.dot(sd.af)).length

    def bracer_r(self, sd, s):
        return sd.fore_r(s) + BRACER_SIT

    def bracer(self):
        """A brass plate curved over the back of the left forearm in four flat
        facets, held by two straps with square buckles."""
        sd = self.sides["L"]
        angs = [-BRACER_SPAN + 2 * BRACER_SPAN * i / 4 for i in range(5)]
        s0, s1 = BRACER_S
        p = self.piece("L.bracer", "Arms_Brass")
        # Closed slab: a loop of outer then inner points at each end.
        loops = []
        for s in (s0, s1):
            ri = self.bracer_r(sd, s)
            outer = [self._bracer_point(sd, s, a, ri + BRACER_T) for a in angs]
            inner = [self._bracer_point(sd, s, a, ri) for a in reversed(angs)]
            loops.append(outer + inner)
        self.loft(p, loops)
        self.finish(p, self.rigid("forearm.L"))
        self.attach.append(("L.bracer", "L.arm"))
        # Straps: rings round the forearm, over the plate on the back.
        under = [BRACER_SPAN + (360 - 2 * BRACER_SPAN) * i / 5 for i in range(1, 5)]
        strap_angs = angs + under
        for k, sc in enumerate(STRAPS):
            p = self.piece(f"L.strap.{k + 1}", "Arms_Strap")
            # Cross-section loop per angle: (front inner, front outer, back outer, back inner).
            sections = []
            for a in strap_angs:
                on_plate = abs(a) <= BRACER_SPAN + 1e-6
                sec = []
                for s in (sc - STRAP_W / 2, sc + STRAP_W / 2):
                    if on_plate:
                        rin = self.bracer_r(sd, s) + BRACER_T - 0.001
                    else:
                        rin = sd.fore_r(s) + 0.0025
                    sec.append((s, rin))
                (sa, ra), (sb, rb) = sec
                sections.append([self._bracer_point(sd, sa, a, ra), self._bracer_point(sd, sa, a, ra + STRAP_T + 0.001),
                                 self._bracer_point(sd, sb, a, rb + STRAP_T + 0.001), self._bracer_point(sd, sb, a, rb)])
            bm = self.bm
            vs = [[bm.verts.new(q) for q in sec] for sec in sections]
            for sec in vs:
                p.verts += sec
            n = len(vs)
            for i in range(n):
                A, B = vs[i], vs[(i + 1) % n]
                for j in range(4):
                    jj = (j + 1) % 4
                    p.faces.append(bm.faces.new((A[j], A[jj], B[jj], B[j])))
            self.finish(p, self.rigid("forearm.L"))
            self.attach.append((p.name, "L.arm"))
            self.attach.append((p.name, "L.bracer"))
            # Buckle: a small square block on the strap, on the outside of the forearm.
            a = self.outside_angle(sd)
            r = sd.fore_r(sc) + 0.0025 + STRAP_T + 0.001
            c = self._bracer_point(sd, sc, a, r + BUCKLE[2] / 2 - 0.001)
            radial = (c - sd.on_forearm(sc)).normalized()
            q = self.piece(f"L.buckle.{k + 1}", "Arms_Strap")
            la, lb = self.box_loops(c - radial * BUCKLE[2] / 2, c + radial * BUCKLE[2] / 2, radial, sd.af,
                                    BUCKLE[0], BUCKLE[1])
            self.loft(q, [la, lb])
            self.finish(q, self.rigid("forearm.L"))
            self.attach.append((q.name, p.name))
        self.refresh()

    def gauge(self):
        """A brass bezel on the plate, turned toward the eye, its window
        recessed 3 mm, with three glowing bars in the window."""
        sd = self.sides["L"]
        ri = self.bracer_r(sd, GAUGE_S) + BRACER_T
        base = self._bracer_point(sd, GAUGE_S, 0.0, ri)
        plate_n = sd.back
        to_eye = (-base).normalized()
        n = plate_n.slerp(to_eye, GAUGE_TURN).normalized()
        # The window's long side lies level across the view, so the bars
        # stand side by side on screen; its short side runs up the forearm.
        w = (X - n * X.dot(n)).normalized()
        u = n.cross(w).normalized()
        if u.dot(sd.af) < 0:
            u, w = -u, -w
        hl, hw = WINDOW[1] / 2 + BEZEL, WINDOW[0] / 2 + BEZEL
        il, iw = WINDOW[1] / 2, WINDOW[0] / 2
        # Lift the bezel's face along n until its corners and the window's
        # floor clear the plate by 1.5 mm.
        def clear(lam):
            top = base + n * lam
            pts = [top + u * x + w * y for x in (-hl, hl) for y in (-hw, hw)]
            pts += [top - n * RECESS + u * x + w * y for x in (-il, il) for y in (-iw, iw)]
            return all(self.radius_at(sd, q) >= ri + 0.0015 for q in pts)
        lam = 0.0
        while not clear(lam):
            lam += 0.0002
            if lam > 0.03:
                raise RuntimeError("the gauge's bezel can't clear the plate within 3 cm")
        top = base + n * lam
        self.gauge_lift = lam
        bm = self.bm
        p = self.piece("L.gauge", "Arms_Brass")

        def tp(x, y, h=0.0):
            return top + u * x + w * y + n * h

        def on_plate(x, y):
            # Straight in toward the forearm's axis from the bezel's face, to
            # 4 mm inside the plate's outer surface.
            q = top + u * x + w * y
            rel = q - sd.W
            axial = sd.W + sd.af * rel.dot(sd.af)
            radial = q - axial
            return axial + radial.normalized() * (ri - 0.004)
        outer = [(-hl, -hw), (hl, -hw), (hl, hw), (-hl, hw)]
        inner = [(-il, -iw), (il, -iw), (il, iw), (-il, iw)]
        O = [bm.verts.new(tp(x, y)) for x, y in outer]
        I = [bm.verts.new(tp(x, y)) for x, y in inner]
        F = [bm.verts.new(tp(x, y, -RECESS)) for x, y in inner]
        B = [bm.verts.new(on_plate(x, y)) for x, y in outer]
        p.verts += O + I + F + B
        for i in range(4):
            j = (i + 1) % 4
            p.faces.append(bm.faces.new((O[i], O[j], I[j], I[i])))   # bezel face
            p.faces.append(bm.faces.new((I[i], I[j], F[j], F[i])))   # window walls
            p.faces.append(bm.faces.new((B[i], B[j], O[j], O[i])))   # sides
        p.faces.append(bm.faces.new(F))                              # window floor
        p.faces.append(bm.faces.new(list(reversed(B))))              # underside, in the plate
        self.finish(p, self.rigid("forearm.L"))
        self.attach.append(("L.gauge", "L.bracer"))
        self.gauge_top = top
        # Bars, side by side along the window's long side.
        floor = top - n * RECESS
        for k in range(3):
            y = (k - 1) * (BAR[0] + BAR_GAP)
            c = floor + w * y
            q = self.piece(f"L.bar.{k + 1}", "Arms_Glow")
            la, lb = self.box_loops(c - n * 0.0006, c + n * 0.0016, n, u, BAR[0], BAR[1])
            self.loft(q, [la, lb])
            self.finish(q, self.rigid("forearm.L"))
            self.attach.append((q.name, "L.gauge"))
            self.bars.append((q.name, c + n * 0.0016, w, n))
        self.refresh()

    # ---- rig ------------------------------------------------------------------

    def bone_table(self):
        """name -> (head, tail, parent, roll reference)."""
        t = {"root": (v(0, 0, 0), v(0, -0.1, 0), None, Z)}
        for S in "LR":
            sd = self.sides[S]
            t[f"shoulder.{S}"] = (sd.C, sd.S, "root", Z)
            t[f"upper_arm.{S}"] = (sd.S, sd.E, f"shoulder.{S}", sd.back)
            t[f"forearm.{S}"] = (sd.E, sd.W, f"upper_arm.{S}", sd.back)
            t[f"hand.{S}"] = (sd.W, sd.K_mid, f"forearm.{S}", sd.n)
            for name, *_ in FINGERS:
                pts, frames, *_ = sd.fingers[name]
                parent = f"hand.{S}"
                for i in range(3):
                    t[f"{name}.{i + 1}.{S}"] = (pts[i], pts[i + 1], parent, frames[i][1])
                    parent = f"{name}.{i + 1}.{S}"
            pts, frames, _ = sd.thumb
            parent = f"hand.{S}"
            for i in range(3):
                t[f"thumb.{i + 1}.{S}"] = (pts[i], pts[i + 1], parent, frames[i][1])
                parent = f"thumb.{i + 1}.{S}"
        return t

    def rig(self):
        arm = bpy.data.armatures.new(RIG)
        rig = bpy.data.objects.new(RIG, arm)
        self.coll.objects.link(rig)
        arm.display_type = "STICK"
        rig.show_in_front = True
        view_layer = bpy.context.view_layer
        prev_active = view_layer.objects.active
        for ob in view_layer.objects:
            if ob.select_get():
                ob.select_set(False)
        view_layer.objects.active = rig
        rig.select_set(True)
        bpy.ops.object.mode_set(mode="EDIT")
        table = self.bone_table()
        for name in JOINTS:
            head, tail, parent, ref = table[name]
            eb = arm.edit_bones.new(name)
            eb.head, eb.tail = head, tail
            eb.align_roll(ref)
            if parent:
                eb.parent = arm.edit_bones[parent]
                eb.use_connect = False
        bpy.ops.object.mode_set(mode="OBJECT")
        rig.select_set(False)
        if prev_active and prev_active.name in view_layer.objects:
            view_layer.objects.active = prev_active
        self.refresh()
        self.obj.parent = rig
        mod = self.obj.modifiers.new("Armature", "ARMATURE")
        mod.object = rig
        self.rig_obj = rig

    def build_all(self):
        for step in self.STEPS:
            getattr(self, step)()

    # ---- poses ----------------------------------------------------------------

    def clear_pose(self):
        for pb in self.rig_obj.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion()
            pb.location = Vector()
            pb.scale = Vector((1, 1, 1))

    def turn(self, name, world_axis, deg):
        """Turn a bone about a world-space axis (taken at rest), after its parent."""
        pb = self.rig_obj.pose.bones[name]
        local = self.rig_obj.data.bones[name].matrix_local.to_3x3().inverted() @ Vector(world_axis)
        pb.rotation_mode = "QUATERNION"
        pb.rotation_quaternion = Quaternion(local.normalized(), math.radians(deg)) @ pb.rotation_quaternion

    def test_poses(self):
        """name -> function setting the pose: the attachment check's test poses."""
        def wrist(kind, deg):
            def f():
                for S in "LR":
                    sd = self.sides[S]
                    ax = {"twist": sd.af, "flex": sd.t, "sideways": sd.n}[kind]
                    self.turn(f"hand.{S}", ax, deg)
            return f

        def curl():
            for S in "LR":
                sd = self.sides[S]
                for name, *_ in FINGERS:
                    hinge = sd.fingers[name][2]
                    for i in range(3):
                        self.turn(f"{name}.{i + 1}.{S}", hinge, 60.0)
                hinge = sd.thumb[2]
                for i in range(3):
                    self.turn(f"thumb.{i + 1}.{S}", hinge, 60.0)

        def elbow():
            for S in "LR":
                sd = self.sides[S]
                hinge = sd.au.cross(sd.af).normalized()
                self.turn(f"forearm.{S}", hinge, -30.0)   # opens the elbow (checked in check())

        poses = {"rest": lambda: None}
        for kind in ("twist", "flex", "sideways"):
            for deg in (30.0, -30.0):
                poses[f"wrist {kind} {deg:+.0f}"] = wrist(kind, deg)
        poses["fingers curled 60"] = curl
        poses["elbows opened 30"] = elbow
        return poses


# --------------------------------------------------------------------------
# Checks and measurements


def piece_indices(b):
    """piece name -> (vertex indices, polygon indices), from the built mesh."""
    bm = b.bm
    bm.verts.index_update()
    bm.faces.index_update()
    return {name: (sorted(x.index for x in p.verts), sorted(f.index for f in p.faces)) for name, p in b.pieces.items()}


def closed_problems(b):
    problems = []
    for name, p in b.pieces.items():
        faces = set(p.faces)
        for f in p.faces:
            for e in f.edges:
                n = sum(1 for g in e.link_faces if g in faces)
                if n != 2:
                    problems.append(f"{name} is not closed (an edge with {n} faces)")
                    break
            else:
                continue
            break
        vol = 0.0
        for f in p.faces:
            co = [x.co for x in f.verts]
            for i in range(1, len(co) - 1):
                vol += co[0].dot(co[i].cross(co[i + 1]))
        if vol <= 0:
            problems.append(f"{name} faces inward (volume {vol / 6:.3g})")
    return problems


def band_crease_problems(b):
    """Each band's lip and foot edges turn more than CREASE_MIN degrees."""
    problems, worst = [], {}
    for S, band_loops, all_loops in b.band_edges:
        idx = {id(l[0]): i for i, l in enumerate(all_loops)}
        lo = 180.0
        for loop in band_loops:
            i = idx[id(loop[0])]
            if i == 0 or i == len(all_loops) - 1:
                continue
            A, B, C = all_loops[i - 1], loop, all_loops[i + 1]
            for k in range(SIDES):
                j = (k + 1) % SIDES
                n1 = (A[j].co - A[k].co).cross(B[k].co - A[k].co)
                n2 = (B[j].co - B[k].co).cross(C[k].co - B[k].co)
                ang = math.degrees(n1.angle(n2))
                lo = min(lo, ang)
        worst[S] = round(lo, 1)
        if lo <= CREASE_MIN:
            problems.append(f"{S} forearm: a band edge turns only {lo:.1f} degrees (needs > {CREASE_MIN})")
    return problems, worst


def evaluated_points(b):
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    ev = b.obj.evaluated_get(dg)
    me = ev.to_mesh()
    pts = [b.obj.matrix_world @ x.co for x in me.vertices]
    ev.to_mesh_clear()
    return pts


def attachment_problems(b, pose_name, pts, idx, polys):
    problems = []
    trees = {name: BVHTree.FromPolygons(pts, [polys[i] for i in pi]) for name, (vi, pi) in idx.items()}

    def inside(p, name):
        hits, o, d = 0, p.copy(), Vector((0.31, 0.22, 0.925)).normalized()
        for _ in range(32):
            hit, _, _, _ = trees[name].ray_cast(o, d)
            if hit is None:
                break
            hits += 1
            o = hit + d * 1e-6
        return hits % 2 == 1

    def touch(a, c):
        if trees[a].overlap(trees[c]):
            return True
        return inside(pts[idx[a][0][0]], c) or inside(pts[idx[c][0][0]], a)

    for a, c in b.attach:
        if not touch(a, c):
            problems.append(f"{pose_name}: {a} does not touch {c} (a gap or a floating piece)")
    for name, loop, hosts in b.covered:
        loose = [x for x in loop if not any(inside(pts[x.index], h) for h in hosts)]
        if loose:
            c = pts[loose[0].index]
            problems.append(f"{pose_name}: {len(loose)} end verts of {name} show outside {hosts} "
                            f"near ({c.x:.3f}, {c.y:.3f}, {c.z:.3f})")
    # Every piece is reached from its arm tube through the attachments.
    links = {}
    for a, c in b.attach:
        links.setdefault(a, set()).add(c)
        links.setdefault(c, set()).add(a)
    for S in "LR":
        reach, stack = {f"{S}.arm"}, [f"{S}.arm"]
        while stack:
            for j in links.get(stack.pop(), ()):
                if j not in reach:
                    reach.add(j)
                    stack.append(j)
        for name in idx:
            if name.startswith(S + ".") and name not in reach:
                problems.append(f"{pose_name}: {name} is attached to nothing")
    return problems


def framing(b, pts, idx):
    """The framing through the eye camera, and the problems with it."""
    problems = []
    m = {}
    # Every vertex, and points along every edge, inside the view: in the lower 35% and 0.15 m ahead.
    samples = list(pts)
    me = b.obj.data
    for e in me.edges:
        a, c = pts[e.vertices[0]], pts[e.vertices[1]]
        for k in (0.25, 0.5, 0.75):
            samples.append(a.lerp(c, k))
    top, near = -2.0, 10.0
    for p in samples:
        q = project(p)
        if q is None or abs(q[0]) > 1 or abs(q[1]) > 1:
            continue
        top = max(top, q[1])
        near = min(near, q[2])
    m["highest_ndc_y_in_view"] = round(top, 3)
    m["nearest_in_view_m"] = round(near, 3)
    if top > FRAME_TOP:
        problems.append(f"something in view reaches NDC y {top:.3f}, above {FRAME_TOP}")
    if near < CLEARANCE:
        problems.append(f"something in view is only {near:.3f} m ahead of the eye (needs {CLEARANCE})")
    for S in "LR":
        sd = b.sides[S]
        # Index knuckle: the top of index.1 near its joint.
        K = sd.fingers["index"][0][0]
        d = sd.fingers["index"][1][0][0]
        ys = [project(pts[i])[1] for i in idx[f"{S}.index.1"][0] if (pts[i] - K).dot(d) <= 0.012]
        ky = max(ys)
        m[f"index_knuckle_top_ndc_y.{S}"] = round(ky, 3)
        if not KNUCKLE_Y[0] <= ky <= KNUCKLE_Y[1]:
            problems.append(f"{S} index knuckle top at NDC y {ky:.3f}, outside {KNUCKLE_Y}")
        # Hand centre: the middle of the hand's screen box (palm, fingers, thumb).
        hand = [n for n in idx if n.startswith(S + ".") and n.split(".")[1] in
                ("palm", "thumb", "index", "middle", "ring", "pinky")]
        qs = [project(pts[i]) for n in hand for i in idx[n][0]]
        xs, ys2 = [q[0] for q in qs], [q[1] for q in qs]
        cx, cy = (min(xs) + max(xs)) / 2, (min(ys2) + max(ys2)) / 2
        m[f"hand_centre_ndc.{S}"] = [round(cx, 3), round(cy, 3)]
        m[f"hand_box_ndc.{S}"] = [round(min(xs), 3), round(max(xs), 3), round(min(ys2), 3), round(max(ys2), 3)]
        want_sign = -1 if S == "L" else 1
        if cx * want_sign <= 0 or not HAND_X[0] <= abs(cx) <= HAND_X[1]:
            problems.append(f"{S} hand centre at NDC x {cx:.3f}, wants {'-' if S == 'L' else '+'}{HAND_X}")
        # Forearm leaves through the bottom edge: walk its axis from the wrist to the elbow.
        exit_q = None
        for i in range(1, 401):
            p = sd.W.lerp(sd.E, i / 400)
            q = project(p)
            if q is None or abs(q[0]) > 1 or abs(q[1]) > 1:
                exit_q = q
                break
        if exit_q is None or exit_q[1] >= -1.0 or abs(exit_q[0]) > 1.0:
            problems.append(f"{S} forearm does not leave through the bottom edge (at {exit_q})")
        else:
            m[f"forearm_exit_ndc_x.{S}"] = round(exit_q[0], 3)
        m[f"wrist_ndc.{S}"] = [round(c, 3) for c in project(sd.W)]
    # Gauge bars: in view, facing the eye, wide enough, not hidden.
    tree = BVHTree.FromPolygons(pts, [p.vertices for p in me.polygons])
    poly_piece = {}
    for name, (vi, pi) in idx.items():
        for i in pi:
            poly_piece[i] = name
    for name, top_c, across, n in b.bars:
        vs = [pts[i] for i in idx[name][0]]
        if not all(in_view(p) for p in vs):
            problems.append(f"{name} is not wholly in view")
        to_eye = (-top_c).normalized()
        facing = n.dot(to_eye)
        a, c = project(top_c - across * BAR[0] / 2), project(top_c + across * BAR[0] / 2)
        px = math.hypot((a[0] - c[0]) * VIEW_W / 2, (a[1] - c[1]) * VIEW_H / 2)
        hit, _, pidx, _ = tree.ray_cast(Vector((0, 0, 0)), top_c.normalized(), top_c.length + 0.01)
        seen = hit is not None and poly_piece.get(pidx) == name
        m[f"{name}_px_wide"] = round(px, 1)
        m[f"{name}_facing_eye"] = round(facing, 3)
        if facing <= 0.2 or px < BAR_MIN_PX or not seen:
            problems.append(f"{name}: facing {facing:.2f}, {px:.1f} px wide, seen from the eye {seen}")
    return problems, m


def check(b):
    """Every binding check. Returns (problems, measurements)."""
    problems = []
    m = {}
    # Names and material order.
    objs = sorted(o.name for o in b.coll.objects)
    if objs != sorted([MESH, RIG]):
        problems.append(f"objects {objs} are not {sorted([MESH, RIG])}")
    mats = [mt.name for mt in b.obj.data.materials]
    if mats != [n for n, _, _ in MATERIALS]:
        problems.append(f"materials {mats} are not in the order {[n for n, _, _ in MATERIALS]}")
    colours = {}
    for (name, srgb, glow), mt in zip(MATERIALS, b.obj.data.materials):
        bsdf = mt.node_tree.nodes["Principled BSDF"]
        back = tuple(round(to_srgb(c) * 255) for c in bsdf.inputs["Base Color"].default_value[:3])
        emits = bsdf.inputs["Emission Strength"].default_value > 0
        colours[name] = {"srgb": back, "emissive": emits}
        if back != srgb:
            problems.append(f"{name} converts back to {back}, not {srgb}")
        if emits != glow:
            problems.append(f"{name} emissive {emits}, wants {glow}")
    m["materials"] = colours
    bones = [bn.name for bn in b.rig_obj.data.bones]
    if sorted(bones) != sorted(JOINTS) or len(bones) != 39:
        problems.append(f"joints {sorted(set(bones) ^ set(JOINTS))} differ from the list ({len(bones)} joints)")
    root = b.rig_obj.data.bones["root"]
    if root.head_local.length > 1e-6:
        problems.append(f"root joint at {tuple(root.head_local)}, not the origin")
    for o in (b.obj, b.rig_obj):
        if any(abs(o.matrix_world[i][j] - (1.0 if i == j else 0.0)) > 1e-6 for i in range(4) for j in range(4)):
            problems.append(f"{o.name} has a transform; the origin must be the eye")
    unweighted = [x.index for x in b.obj.data.vertices if not x.groups]
    if unweighted:
        problems.append(f"{len(unweighted)} vertices have no weights")
    # Triangles.
    tris = sum(len(p.vertices) - 2 for p in b.obj.data.polygons)
    m["triangles"] = tris
    by = {}
    for name, p in b.pieces.items():
        key = name.split(".")[1]
        by[key] = by.get(key, 0) + sum(len(f.verts) - 2 for f in p.faces)
    m["triangles_by_part"] = by
    if not TRI_BUDGET[0] <= tris <= TRI_BUDGET[1]:
        problems.append(f"{tris} triangles, outside {TRI_BUDGET}")
    if any(p.use_smooth for p in b.obj.data.polygons):
        problems.append("some faces are smooth-shaded")
    # Closed volumes and band creases.
    problems += closed_problems(b)
    cp, worst = band_crease_problems(b)
    problems += cp
    m["band_edge_min_turn_deg"] = worst
    m["bands"] = {S: {"boundaries_m": [round(x, 4) for x in b.sides[S].bands[0]],
                      "lips_mm": [round(h * 1000, 1) for h in b.sides[S].bands[1]]} for S in "LR"}
    # Poses: framing at rest, attachment in every test pose.
    idx = piece_indices(b)
    polys = [tuple(p.vertices) for p in b.obj.data.polygons]
    m["pieces"] = len(idx)
    poses = b.test_poses()
    b.clear_pose()
    rest_pts = evaluated_points(b)
    fp, fm = framing(b, rest_pts, idx)
    problems += fp
    m["framing"] = fm
    for name, setter in poses.items():
        b.clear_pose()
        setter()
        pts = evaluated_points(b)
        if name == "elbows opened 30":
            for S in "LR":
                sd = b.sides[S]
                pb = b.rig_obj.pose.bones[f"forearm.{S}"]
                head = b.rig_obj.matrix_world @ pb.head
                tail = b.rig_obj.matrix_world @ pb.tail
                before = math.degrees((sd.S - sd.E).angle(sd.W - sd.E))
                after = math.degrees((sd.S - head).angle(tail - head))
                m[f"elbow_angle_deg.{S}"] = [round(before, 1), round(after, 1)]
                if after < before + 25:
                    problems.append(f"{S} elbow test pose does not open it ({before:.1f} -> {after:.1f})")
        problems += attachment_problems(b, name, pts, idx, polys)
    b.clear_pose()
    m["poses_checked"] = list(poses)
    # Joints, for the record (engine coordinates).
    m["joints_engine"] = {n: [round(c, 3) for c in to_engine(b.rig_obj.data.bones[n].head_local)]
                          for n in ("shoulder.L", "upper_arm.L", "forearm.L", "hand.L", "index.1.L")}
    return problems, m


def to_engine(p):
    return (p[0], p[2], -p[1])


# --------------------------------------------------------------------------
# Export, glb check and previews


def export():
    os.makedirs(OUT_DIR, exist_ok=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_yup=True,
        export_normals=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_skins=True,
        export_animations=False,
        export_def_bones=False,
        export_apply=False,
    )
    print("wrote", OUT)


def check_glb(path, tris_expected):
    """Read the glb back: one skinned mesh, 39 named joints, the five
    materials in order with their colours, the glow emissive, no clips."""
    with open(path, "rb") as fh:
        data = fh.read()
    magic, _, _ = struct.unpack_from("<III", data, 0)
    clen, ctype = struct.unpack_from("<II", data, 12)
    if magic != 0x46546C67 or ctype != 0x4E4F534A:
        return ["not a glb"], {}
    gj = json.loads(data[20:20 + clen])
    problems = []
    m = {}
    meshes = gj.get("meshes", [])
    skins = gj.get("skins", [])
    if len(meshes) != 1 or len(skins) != 1:
        problems.append(f"{len(meshes)} meshes and {len(skins)} skins, wants one of each")
    joints = [gj["nodes"][j]["name"] for j in skins[0]["joints"]] if skins else []
    if sorted(joints) != sorted(JOINTS):
        problems.append(f"glb joints differ: {sorted(set(joints) ^ set(JOINTS))}")
    mats = gj.get("materials", [])
    names = [mt["name"] for mt in mats]
    if names != [n for n, _, _ in MATERIALS]:
        problems.append(f"glb materials {names}")
    got = {}
    for mt, (name, srgb, glow) in zip(mats, MATERIALS):
        c = mt.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1, 1, 1, 1])
        back = tuple(round(to_srgb(x) * 255) for x in c[:3])
        e = mt.get("emissiveFactor", [0, 0, 0])
        glows = sum(e) > 0.01
        got[name] = {"srgb": back, "glow": glows}
        if back != srgb or glows != glow:
            problems.append(f"glb {name}: {back} glow {glows}, wants {srgb} glow {glow}")
    if gj.get("animations"):
        problems.append(f"glb has {len(gj['animations'])} animations, wants none")
    tris = 0
    for mesh in meshes:
        for prim in mesh["primitives"]:
            tris += gj["accessors"][prim["indices"]]["count"] // 3
            for att in ("JOINTS_0", "WEIGHTS_0", "NORMAL"):
                if att not in prim["attributes"]:
                    problems.append(f"glb primitive lacks {att}")
    if tris != tris_expected:
        problems.append(f"glb has {tris} triangles, the build {tris_expected}")
    m.update({"glb_triangles": tris, "glb_joints": len(joints), "glb_materials": got,
              "glb_nodes": sorted(n.get("name", "") for n in gj["nodes"] if "mesh" in n or "skin" in n)})
    return problems, m


def render_previews(b):
    """Two previews, Workbench with back faces culled as the engine draws:
    arms_eye.png  through the eye camera (60 degrees, 960x540): what the player sees
    arms.png      a three-quarter view from outside, front left and above
    A sand-coloured ground plane 1.7 m below the eye is added for the previews only."""
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
    world.color = (lin(156) , lin(192), lin(220))
    scene.world = world
    sand = bpy.data.materials.new("Preview_Sand")
    sand.diffuse_color = (lin(214), lin(176), lin(118), 1.0)
    gm = bpy.data.meshes.new("Preview_Ground")
    gm.from_pydata([(-200, -200, -1.7), (200, -200, -1.7), (200, 200, -1.7), (-200, 200, -1.7)], [], [(0, 1, 2, 3)])
    gm.materials.append(sand)
    ground = bpy.data.objects.new("Preview_Ground", gm)
    scene.collection.objects.link(ground)
    cam_data = bpy.data.cameras.new("Preview_Camera")
    cam_data.sensor_fit = "VERTICAL"
    cam_data.angle_y = FOV_Y
    cam_data.clip_start = NEAR
    cam_data.clip_end = 400.0
    cam = bpy.data.objects.new("Preview_Camera", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam

    def shoot(path, eye, target, res, up="Y"):
        scene.render.resolution_x, scene.render.resolution_y = res
        scene.render.resolution_percentage = 100
        cam.location = Vector(eye)
        cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat("-Z", up).to_euler()
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        print("wrote", path)

    shoot(PREVIEW_EYE, (0.0, 0.0, 0.0), (0.0, -1.0, 0.0), (VIEW_W, VIEW_H))
    cam_data.clip_start = 0.01
    cam_data.angle_y = math.radians(38.0)
    shoot(PREVIEW, (0.62, -0.98, 0.12), (0.03, -0.20, -0.20), (960, 720))
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
    print("arms:", json.dumps(m, indent=1))
    if problems:
        raise RuntimeError("the arms fail their checks:\n  " + "\n  ".join(problems))
    if os.environ.get("PXE_SKIP_EXPORT"):
        return
    export()
    gp, gm = check_glb(OUT, m["triangles"])
    print("arms.glb:", json.dumps(gm))
    if gp:
        raise RuntimeError("arms.glb fails its checks:\n  " + "\n  ".join(gp))
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_previews(b)


if __name__ == "__main__" and bpy.app.background:
    main()
