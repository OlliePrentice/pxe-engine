"""Build the hooded hero - mesh, rig and animations - and export
assets/characters/hooded_hero.glb.

Run headless (builds everything and exports):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_hooded_hero.py
    PXE_HERO_PREVIEW=/some/dir ... also renders preview images there.

Or step by step in an open Blender, to watch it go together:
    import runpy; hh = runpy.run_path(".../build_hooded_hero.py", run_name="hooded_hero")
    b = hh["Builder"](); b.start(); b.body(); b.legs(); ...  (see Builder.STEPS)
Each step adds its pieces to the HoodedHero collection, leaving the rest of
the scene alone.

The figure: a slim, faceted wanderer in a deep red hood and long cape with a
jagged hem, a red coat skirt split at the front over dark grey plate, black
gloves and boots, a teal buckle and chest chevron, a black void under the
hood with a glowing cyan chevron for eyes, and a glowing cyan crystal sword
in the right hand. Every face is flat, so it reads as cut paper.

The rig is made for smooth, blended animation rather than held frames:
    root > hips > spine > chest > neck > head > hood
    chest > clavicle > upperarm > forearm > hand (> sword on the right)
    hips > thigh > shin > foot > toe
    chest > cape.{R2,R1,C,L1,L2}.{1,2,3}   five chains round the back
    hips > tabard.{L,R}.{1,2}              the coat's front flaps
The cape and tabard chains are separate so the engine can lay secondary
motion (second-order dynamics) over the clips. Every clip loops seamlessly:
its keys are Bezier with cyclic handles, sampled every frame on export.
    Idle 4 s    Walk 1 s (two steps)    Run 0.66 s (two steps)

Material order is the contract with the engine: glTF material i is actor
material i. "Glow" is emissive.
"""

import math
import os

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "assets", "characters", "hooded_hero.glb"))
PREVIEW = os.environ.get("PXE_HERO_PREVIEW")
COLLECTION = "HoodedHero"
FPS = 30

X = Vector((1, 0, 0))
Y = Vector((0, 1, 0))
Z = Vector((0, 0, 1))
FWD = Vector((0, -1, 0))  # the figure faces -y; its left is +x


def v(x, y, z):
    return Vector((x, y, z))


def srgb(r, g, b):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b))


# --------------------------------------------------------------------------
# Materials, in the engine's order: (name, linear base colour, emissive)

MATERIALS = [
    ("Cloak", srgb(196, 52, 72), False),
    ("Armour", srgb(66, 66, 72), False),
    ("Glove", srgb(40, 40, 44), False),
    ("Accent", srgb(26, 128, 128), False),
    ("Void", srgb(6, 6, 8), False),
    ("Glow", srgb(120, 242, 248), True),
]
M = {name: i for i, (name, _, _) in enumerate(MATERIALS)}

# --------------------------------------------------------------------------
# Skeleton: name -> (head, tail, parent), in order (parents first)

BONES = {}
ORDER = []


def bone(name, head, tail, parent=None):
    BONES[name] = (v(*head), v(*tail), parent)
    ORDER.append(name)


bone("root", (0, 0, 0), (0, -0.25, 0))
bone("hips", (0, 0, 0.97), (0, 0, 1.08), "root")
bone("spine", (0, 0, 1.08), (0, 0, 1.24), "hips")
bone("chest", (0, 0, 1.24), (0, 0, 1.42), "spine")
bone("neck", (0, 0.01, 1.42), (0, 0, 1.52), "chest")
bone("head", (0, 0, 1.52), (0, 0, 1.74), "neck")
bone("hood", (0, 0.06, 1.74), (0, 0.12, 1.88), "head")
for s, S in ((1, "L"), (-1, "R")):
    bone(f"clavicle.{S}", (s * 0.03, 0, 1.39), (s * 0.16, 0.01, 1.41), "chest")
    bone(f"upperarm.{S}", (s * 0.18, 0.01, 1.40), (s * 0.335, 0.01, 1.17), f"clavicle.{S}")
    bone(f"forearm.{S}", (s * 0.335, 0.01, 1.17), (s * 0.465, -0.02, 0.955), f"upperarm.{S}")
    bone(f"hand.{S}", (s * 0.465, -0.02, 0.955), (s * 0.51, -0.03, 0.875), f"forearm.{S}")
    bone(f"thigh.{S}", (s * 0.09, 0, 0.95), (s * 0.10, -0.005, 0.53), "hips")
    bone(f"shin.{S}", (s * 0.10, -0.005, 0.53), (s * 0.10, 0.01, 0.11), f"thigh.{S}")
    bone(f"foot.{S}", (s * 0.10, 0.01, 0.11), (s * 0.10, -0.09, 0.03), f"shin.{S}")
    bone(f"toe.{S}", (s * 0.10, -0.09, 0.03), (s * 0.10, -0.16, 0.025), f"foot.{S}")
    bone(f"tabard.{S}.1", (s * 0.08, -0.12, 1.10), (s * 0.09, -0.14, 0.80), "hips")
    bone(f"tabard.{S}.2", (s * 0.09, -0.14, 0.80), (s * 0.09, -0.15, 0.48), f"tabard.{S}.1")

# The sword hangs from the right fist, point down and out, as in the
# concept: its bone runs from the grip along the blade.
GRIP = v(-0.49, -0.035, 0.91)
BLADE_DIR = v(-0.22, -0.28, -0.93).normalized()
bone("sword", tuple(GRIP), tuple(GRIP + BLADE_DIR * 0.3), "hand.R")

# Cape: five chains of three round the back, at these angles from straight
# back (+y) toward the left (+x), each joint at these heights, the cape
# flaring as it falls.
CAPE_CHAINS = [("R2", -100), ("R1", -50), ("C", 0), ("L1", 50), ("L2", 100)]
CAPE_Z = [1.40, 1.07, 0.70, 0.30]
CAPE_RX = [0.21, 0.27, 0.36, 0.46]
CAPE_RY = [0.15, 0.22, 0.29, 0.36]


def cape_radius(z):
    """The cape's half-width and half-depth at height z."""
    zs, rx, ry = CAPE_Z, CAPE_RX, CAPE_RY
    if z >= zs[0]:
        return rx[0], ry[0]
    for i in range(len(zs) - 1):
        if zs[i + 1] <= z <= zs[i]:
            f = (zs[i] - z) / (zs[i] - zs[i + 1])
            return rx[i] + (rx[i + 1] - rx[i]) * f, ry[i] + (ry[i + 1] - ry[i]) * f
    f = (zs[-1] - z) / (zs[-2] - zs[-1])
    return rx[-1] + (rx[-1] - rx[-2]) * f, ry[-1] + (ry[-1] - ry[-2]) * f


def cape_point(angle_deg, z, out=0.0):
    a = math.radians(angle_deg)
    rx, ry = cape_radius(z)
    return v((rx + out) * math.sin(a), (ry + out) * math.cos(a), z)


for name, ang in CAPE_CHAINS:
    for i in range(3):
        bone(f"cape.{name}.{i + 1}", tuple(cape_point(ang, CAPE_Z[i], 0.01)),
             tuple(cape_point(ang, CAPE_Z[i + 1], 0.01)),
             "chest" if i == 0 else f"cape.{name}.{i}")


def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / ab.length_squared))
    return (p - (a + ab * t)).length


def ring_frame(t, ref):
    t = t.normalized()
    f = ref - t * ref.dot(t)
    if f.length < 1e-5:
        f = Z - t * Z.dot(t)
    f.normalize()
    return f.cross(t).normalized(), f


class Piece:
    def __init__(self, verts=(), faces=()):
        self.verts = list(verts)
        self.faces = list(faces)

    def add(self, other):
        self.verts += other.verts
        self.faces += other.faces
        return self


# --------------------------------------------------------------------------
# The builder: one bmesh, one mesh object, weights in the deform layer


class Builder:
    STEPS = ["start", "body", "legs", "arms", "head", "cowl", "skirt", "cape", "sword", "rig", "animate"]

    def __init__(self):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.group = {name: i for i, name in enumerate(ORDER)}
        self.obj = None
        self.rig = None
        self.coll = None

    # ---- scene plumbing ---------------------------------------------------

    def start(self):
        """Fresh HoodedHero collection and materials; the rest of the scene is left alone."""
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
        for act in [a for a in bpy.data.actions if a.name.startswith("HH_")]:
            bpy.data.actions.remove(act)
        self.coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(self.coll)

        mats = []
        for name, colour, glow in MATERIALS:
            mname = "HH_" + name
            mat = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
            mat.diffuse_color = (*colour, 1.0)
            mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF")
            if bsdf:
                bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Roughness"].default_value = 1.0
                bsdf.inputs["Emission Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Emission Strength"].default_value = 2.0 if glow else 0.0
            mats.append(mat)

        mesh = bpy.data.meshes.new("HoodedHero")
        for mat in mats:
            mesh.materials.append(mat)
        self.obj = bpy.data.objects.new("HoodedHero", mesh)
        self.coll.objects.link(self.obj)
        for name in ORDER:
            self.obj.vertex_groups.new(name=name)
        self.refresh()

    def refresh(self):
        self.bm.to_mesh(self.obj.data)
        self.obj.data.update()

    # ---- geometry helpers -------------------------------------------------

    def loft(self, rings, sides=8, cap0=True, cap1=True, ref=FWD, phase=0.5, tip0=None, tip1=None):
        """A tube through rings of (centre, rx, ry): rx across, ry along `ref`.
        Caps close an end flat, or to a point at tip0 / tip1."""
        bm = self.bm
        centres = [Vector(r[0]) for r in rings]
        loops = []
        for i, (c, rx, ry) in enumerate(rings):
            c = Vector(c)
            a, b = centres[max(i - 1, 0)], centres[min(i + 1, len(rings) - 1)]
            u, f = ring_frame(b - a, ref)
            loop = []
            for k in range(sides):
                ang = 2 * math.pi * (k + phase) / sides
                loop.append(bm.verts.new(c + u * (rx * math.cos(ang)) + f * (ry * math.sin(ang))))
            loops.append(loop)
        piece = Piece([x for loop in loops for x in loop])
        for i in range(len(loops) - 1):
            for k in range(sides):
                j = (k + 1) % sides
                piece.faces.append(bm.faces.new((loops[i][k], loops[i][j], loops[i + 1][j], loops[i + 1][k])))

        def cap(loop, tip, reverse):
            if tip is None:
                f = bm.faces.new(list(reversed(loop)) if reverse else loop)
                piece.faces.append(f)
                return
            t = bm.verts.new(Vector(tip))
            piece.verts.append(t)
            for k in range(sides):
                tri = (loop[k], loop[(k + 1) % sides], t)
                piece.faces.append(bm.faces.new(tuple(reversed(tri)) if reverse else tri))

        if cap0:
            cap(loops[0], tip0, False)
        if cap1:
            cap(loops[-1], tip1, True)
        self.outward(piece)
        return piece

    def outward(self, piece, ref=None):
        """Turn a piece's faces to face away from its own middle (or from
        ref(point), a point inside it near that point)."""
        bm = self.bm
        bm.normal_update()
        if not piece.faces:
            return
        mid = sum((f.calc_center_median() for f in piece.faces), Vector()) / len(piece.faces)
        score = 0.0
        for f in piece.faces:
            c = f.calc_center_median()
            inside = ref(c) if ref else None
            if inside is None:
                inside = mid
            score += f.normal.dot(c - inside) * f.calc_area()
        if score < 0:
            for f in piece.faces:
                f.normal_flip()
            bm.normal_update()

    def sheet(self, grid):
        """Quads through a grid of points, rows top to bottom."""
        bm = self.bm
        vs = [[bm.verts.new(p) for p in row] for row in grid]
        piece = Piece([x for row in vs for x in row])
        for i in range(len(vs) - 1):
            for j in range(len(vs[i]) - 1):
                piece.faces.append(bm.faces.new((vs[i][j], vs[i + 1][j], vs[i + 1][j + 1], vs[i][j + 1])))
        return piece

    def thicken(self, piece, th):
        """Give a one-sided shell a back: an inset copy facing in, and its
        boundary closed, so it reads from both sides."""
        bm = self.bm
        bm.normal_update()
        inner = {}
        for x in piece.verts:
            inner[x] = bm.verts.new(x.co - x.normal * th)
        fs = set(piece.faces)
        new = Piece(inner.values())
        for f in piece.faces:
            new.faces.append(bm.faces.new([inner[x] for x in reversed(f.verts)]))
        for f in piece.faces:
            for loop in f.loops:
                e = loop.edge
                if sum(1 for lf in e.link_faces if lf in fs) == 1:
                    a, b = loop.vert, loop.link_loop_next.vert
                    new.faces.append(bm.faces.new((b, a, inner[a], inner[b])))
        return piece.add(new)

    def box(self, centre, axes, half):
        """A box from a centre, three axes and half-sizes along them."""
        bm = self.bm
        c = Vector(centre)
        ax = [Vector(a).normalized() for a in axes]
        vs = []
        for sz in (-1, 1):
            for sy in (-1, 1):
                for sx in (-1, 1):
                    vs.append(bm.verts.new(c + ax[0] * (sx * half[0]) + ax[1] * (sy * half[1]) + ax[2] * (sz * half[2])))
        quads = [(0, 2, 3, 1), (4, 5, 7, 6), (0, 1, 5, 4), (2, 6, 7, 3), (0, 4, 6, 2), (1, 3, 7, 5)]
        piece = Piece(vs, [bm.faces.new([vs[i] for i in q]) for q in quads])
        self.outward(piece)
        return piece

    def blob(self, centre, radii, segs=8, rings=5, fwd_scale=None):
        """A low-poly ellipsoid."""
        bm = self.bm
        res = bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0)
        piece = Piece(res["verts"], list({f for x in res["verts"] for f in x.link_faces}))
        for x in piece.verts:
            x.co = v(x.co.x * radii[0], x.co.y * radii[1], x.co.z * radii[2]) + Vector(centre)
        self.outward(piece)
        return piece

    def finish(self, piece, mat, bones, power=4.0, keep=3, adjust=None):
        """Material, flat shading, and weights to the nearest of `bones`."""
        for f in piece.faces:
            f.material_index = M[mat]
            f.smooth = False
        for x in piece.verts:
            p = x.co
            ws = []
            for name in bones:
                h, t, _ = BONES[name]
                ws.append([name, 1.0 / (seg_dist(p, h, t) + 0.02) ** power])
            ws.sort(key=lambda w: -w[1])
            ws = ws[:keep]
            total = sum(w for _, w in ws)
            ws = [(n, w / total) for n, w in ws]
            if adjust:
                ws = adjust(p, ws)
            self.set_weights(x, ws)
        return piece

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

    def limb(self, bone_name, radii, sides=7, cap0=False, cap1=False, sink=(0.0, 0.0)):
        """A tube along a bone: radii are (fraction along, rx, ry)."""
        h, t, _ = BONES[bone_name]
        d = t - h
        h = h - d.normalized() * sink[0]
        t = t + d.normalized() * sink[1]
        rings = [(h + (t - h) * f, rx, ry) for f, rx, ry in radii]
        return self.loft(rings, sides=sides, cap0=cap0, cap1=cap1)

    # ---- the figure -------------------------------------------------------

    def body(self):
        """Torso and pelvis in plate, the belt and buckle, the chest chevron."""
        torso = self.loft([
            (v(0, 0.0, 0.86), 0.10, 0.075),
            (v(0, 0.0, 0.93), 0.14, 0.10),
            (v(0, 0.0, 1.01), 0.15, 0.105),
            (v(0, 0.0, 1.11), 0.125, 0.09),
            (v(0, 0.0, 1.22), 0.145, 0.10),
            (v(0, -0.005, 1.32), 0.165, 0.112),
            (v(0, 0.0, 1.41), 0.15, 0.10),
            (v(0, 0.005, 1.47), 0.07, 0.065),
            (v(0, 0.01, 1.57), 0.055, 0.055),
        ], sides=8)
        self.finish(torso, "Armour", ["hips", "spine", "chest", "neck", "thigh.L", "thigh.R", "clavicle.L", "clavicle.R"])

        belt = self.loft([(v(0, 0, 1.085), 0.133, 0.097), (v(0, 0, 1.135), 0.132, 0.096)], sides=8, cap0=False, cap1=False)
        self.thicken(belt, 0.01)
        self.finish(belt, "Cloak", ["hips", "spine"])
        buckle = self.box(v(0, -0.103, 1.11), (X, Y, Z), (0.03, 0.012, 0.03))
        self.finish(buckle, "Accent", ["hips"])

        # The chevron on the chest: two plates meeting in a V below the collar.
        for s in (1, -1):
            p = self.box(v(s * 0.035, -0.112, 1.35), (v(s * 0.7, 0, 0.7), Y, v(-s * 0.7, 0, 0.7)), (0.042, 0.008, 0.016))
            self.finish(p, "Accent", ["chest"])
        self.refresh()

    def legs(self):
        for s, S in ((1, "L"), (-1, "R")):
            thigh = self.limb(f"thigh.{S}", [(0.0, 0.085, 0.085), (0.45, 0.075, 0.08), (1.0, 0.058, 0.062)], sides=7, sink=(0.04, 0.0))
            self.finish(thigh, "Armour", [f"thigh.{S}", "hips", f"shin.{S}"])
            shin = self.limb(f"shin.{S}", [(0.0, 0.058, 0.062), (0.35, 0.06, 0.066), (1.0, 0.045, 0.05)], sides=7)
            self.finish(shin, "Armour", [f"shin.{S}", f"thigh.{S}"])
            # Knee guard: a faceted wedge on the front of the knee.
            kh = BONES[f"shin.{S}"][0]
            knee = self.blob(kh + v(0, -0.055, 0.0), (0.05, 0.03, 0.065), segs=5, rings=3)
            self.finish(knee, "Armour", [f"shin.{S}", f"thigh.{S}"], keep=2)
            # Boot: a plated cuff, the shaft, then the foot with a heel.
            cuff = self.loft([(v(s * 0.10, 0.005, 0.33), 0.066, 0.07), (v(s * 0.10, 0.006, 0.27), 0.062, 0.066)], sides=7)
            self.finish(cuff, "Glove", [f"shin.{S}"])
            shaft = self.loft([(v(s * 0.10, 0.008, 0.27), 0.055, 0.06), (v(s * 0.10, 0.012, 0.10), 0.052, 0.062)], sides=7, cap0=False)
            self.finish(shaft, "Glove", [f"shin.{S}", f"foot.{S}"])
            foot = self.loft([
                (v(s * 0.10, 0.055, 0.045), 0.045, 0.04),
                (v(s * 0.10, 0.0, 0.05), 0.05, 0.05),
                (v(s * 0.10, -0.08, 0.035), 0.048, 0.035),
                (v(s * 0.10, -0.15, 0.025), 0.035, 0.022),
            ], sides=6, ref=Z, phase=0.0)
            for x in foot.verts:  # flatten the sole onto the ground
                x.co.z = max(x.co.z, 0.0)
            self.finish(foot, "Glove", [f"foot.{S}", f"toe.{S}", f"shin.{S}"])
            heel = self.box(v(s * 0.10, 0.045, 0.02), (X, Y, Z), (0.04, 0.03, 0.02))
            self.finish(heel, "Glove", [f"foot.{S}"])
        self.refresh()

    def arms(self):
        for s, S in ((1, "L"), (-1, "R")):
            shoulder = self.blob(BONES[f"upperarm.{S}"][0] + v(s * 0.01, 0, 0.0), (0.06, 0.058, 0.055), segs=6, rings=4)
            self.finish(shoulder, "Armour", [f"upperarm.{S}", f"clavicle.{S}", "chest"])
            upper = self.limb(f"upperarm.{S}", [(0.0, 0.055, 0.058), (0.5, 0.05, 0.052), (1.0, 0.043, 0.045)], sides=6)
            self.finish(upper, "Armour", [f"upperarm.{S}", f"forearm.{S}", f"clavicle.{S}"])
            fore = self.limb(f"forearm.{S}", [(0.0, 0.043, 0.045), (0.55, 0.05, 0.052), (0.85, 0.052, 0.054), (1.0, 0.038, 0.04)], sides=6)
            self.finish(fore, "Armour", [f"forearm.{S}", f"upperarm.{S}", f"hand.{S}"])
            # Gauntlet flare and the glove: a squared mitten with a thumb.
            h, t, _ = BONES[f"hand.{S}"]
            d = (t - h).normalized()
            side = d.cross(FWD).normalized()
            palm = self.box(h + d * 0.055, (side, side.cross(d), d), (0.022, 0.034, 0.055))
            self.finish(palm, "Glove", [f"hand.{S}", f"forearm.{S}"], keep=2)
            thumb = self.box(h + d * 0.035 + FWD * 0.028, (side, side.cross(d), d), (0.014, 0.014, 0.03))
            self.finish(thumb, "Glove", [f"hand.{S}"], keep=1)
        self.refresh()

    def head(self):
        """The void under the hood, the glowing eyes, and the hood itself."""
        void = self.blob(v(0, 0.01, 1.625), (0.095, 0.10, 0.115), segs=8, rings=5)
        self.finish(void, "Void", ["head", "neck"], keep=1)

        # Eyes: one cyan chevron pointing down, just in front of the void.
        eyes = Piece()
        bm = self.bm
        y0, y1 = -0.098, -0.086
        outline = [v(-0.058, 0, 1.662), v(0, 0, 1.636), v(0.058, 0, 1.662), v(0, 0, 1.600)]
        front = [bm.verts.new(p + v(0, y0, 0)) for p in outline]
        back = [bm.verts.new(p + v(0, y1, 0)) for p in outline]
        eyes.verts = front + back
        eyes.faces.append(bm.faces.new([front[0], front[3], front[2], front[1]]))
        eyes.faces.append(bm.faces.new([back[0], back[1], back[2], back[3]]))
        for i in range(4):
            j = (i + 1) % 4
            eyes.faces.append(bm.faces.new([front[i], front[j], back[j], back[i]]))
        self.outward(eyes)
        self.finish(eyes, "Glow", ["head"], keep=1)

        # Hood: lofted up from the shoulders to a point leaning back, open in front.
        hood = self.loft([
            (v(0, 0.005, 1.46), 0.165, 0.155),
            (v(0, 0.005, 1.55), 0.158, 0.15),
            (v(0, 0.012, 1.645), 0.152, 0.152),
            (v(0, 0.035, 1.735), 0.125, 0.132),
            (v(0, 0.075, 1.81), 0.075, 0.085),
            (v(0, 0.11, 1.862), 0.032, 0.036),
        ], sides=12, cap0=False, cap1=True, tip1=v(0, 0.135, 1.892), ref=FWD, phase=0.5)
        cut = [f for f in hood.faces
               if (lambda c: c.y < -0.06 and abs(c.x) < 0.085 and 1.52 < c.z < 1.78)(f.calc_center_median())]
        for f in cut:
            hood.faces.remove(f)
        bmesh.ops.delete(bm, geom=cut, context="FACES_ONLY")
        hood.verts = [x for x in hood.verts if x.is_valid and x.link_faces]
        self.outward(hood, lambda c: v(0, 0.03, min(max(c.z, 1.5), 1.8)))
        self.thicken(hood, 0.014)

        def hood_weights(p, ws):
            # The hood's crown and point lag on their own bone; the rim sits on the head.
            up = max(0.0, min(1.0, (p.z - 1.70) / 0.12))
            low = max(0.0, min(1.0, (1.52 - p.z) / 0.06))
            out = [("head", 1.0 - up - low), ("hood", up), ("neck", low * 0.6), ("chest", low * 0.4)]
            return [(n, w) for n, w in out if w > 0]

        self.finish(hood, "Cloak", ["head"], adjust=hood_weights)
        self.refresh()

    def cowl(self):
        """The layered collar over the shoulders, its lower edge cut in points."""
        rings = [
            (v(0, 0.01, 1.53), 0.135, 0.12),
            (v(0, 0.01, 1.47), 0.21, 0.16),
            (v(0, 0.012, 1.37), 0.31, 0.21),
        ]
        piece = self.loft(rings, sides=12, cap0=False, cap1=False, ref=FWD, phase=0.5)
        # A jagged hem: drop every other vertex of the bottom ring into a point.
        bottom = piece.verts[-12:]
        for k, x in enumerate(bottom):
            x.co.z -= 0.075 if k % 2 == 0 else 0.0
            x.co.x *= 1.12 if k % 2 == 0 else 1.0
            x.co.y = (x.co.y - 0.012) * (1.08 if k % 2 == 0 else 1.0) + 0.012
        self.outward(piece, lambda c: v(0, 0.01, c.z))
        self.thicken(piece, 0.012)

        def cowl_weights(p, ws):
            k = max(0.0, min(1.0, (1.50 - p.z) / 0.12))
            side = max(0.0, min(1.0, (abs(p.x) - 0.12) / 0.12)) * k
            S = "L" if p.x > 0 else "R"
            out = [("neck", (1 - k) * 0.6), ("chest", (1 - k) * 0.4 + k * (1 - side)), (f"clavicle.{S}", side * 0.6),
                   (f"upperarm.{S}", side * 0.4)]
            return [(n, w) for n, w in out if w > 0]

        self.finish(piece, "Cloak", ["chest"], adjust=cowl_weights)
        self.refresh()

    def skirt(self):
        """The coat below the belt, split up the front into two flaps."""
        rows = [1.10, 0.96, 0.80, 0.64, 0.50]
        for s, S in ((1, "L"), (-1, "R")):
            # Angles from straight ahead round this side to the back.
            angles = [10, 35, 65, 100, 140, 178]
            grid = []
            for r, z in enumerate(rows):
                row = []
                for c, a in enumerate(angles):
                    t = (1.10 - z) / 0.60
                    rx = 0.14 + 0.06 * t
                    ry = 0.105 + 0.05 * t
                    zz = z
                    if r == len(rows) - 1:  # pointed hem, lowest at the front edge
                        zz = z - (0.06 if c % 2 == 0 else -0.04) - (0.04 if c == 0 else 0.0)
                    ar = math.radians(a)
                    row.append(v(s * rx * math.sin(ar), -ry * math.cos(ar), zz))
                grid.append(row)
            piece = self.sheet(grid)
            self.outward(piece, lambda c: v(0, 0, c.z))
            self.thicken(piece, 0.01)

            def skirt_weights(p, ws, s=s, S=S):
                depth = max(0.0, min(1.0, (1.10 - p.z) / 0.62))
                ang = math.degrees(math.atan2(abs(p.x), -p.y))  # 0 front .. 180 back
                front = max(0.0, min(1.0, 1.0 - (ang - 20) / 110))
                hips = max(0.0, 1.0 - depth * 3.0)
                rest = 1.0 - hips
                upper = max(0.0, min(1.0, 1.0 - (depth - 0.45) / 0.4))
                flap = rest * front
                out = [("hips", hips + rest * (1 - front) * 0.5),
                       (f"thigh.{S}", rest * (1 - front) * 0.5),
                       (f"tabard.{S}.1", flap * upper),
                       (f"tabard.{S}.2", flap * (1 - upper))]
                return [(n, w) for n, w in out if w > 0]

            self.finish(piece, "Cloak", ["hips"], adjust=skirt_weights)
        self.refresh()

    def cape(self):
        """The long cape round the back, folded, with a jagged hem."""
        angles = [-112 + 14 * i for i in range(17)]
        rows = [1.40, 1.25, 1.07, 0.88, 0.70, 0.50, 0.32]
        grid = []
        for r, z in enumerate(rows):
            row = []
            for c, a in enumerate(angles):
                fold = (0.03 if c % 2 else -0.01) * (r / (len(rows) - 1))  # folds deepen as it falls
                zz = z
                if r == len(rows) - 1:
                    zz = z - (0.07 if c % 2 == 0 else -0.09)
                row.append(cape_point(a, zz, fold))
            grid.append(row)
        piece = self.sheet(grid)
        self.outward(piece, lambda c: v(0, 0, c.z))
        self.thicken(piece, 0.012)

        chain_angles = [a for _, a in CAPE_CHAINS]
        names = [n for n, _ in CAPE_CHAINS]
        mids = [(CAPE_Z[i] + CAPE_Z[i + 1]) / 2 for i in range(3)]

        def cape_weights(p, ws):
            a = math.degrees(math.atan2(p.x, p.y))
            a = max(chain_angles[0], min(chain_angles[-1], a))
            for i in range(len(chain_angles) - 1):
                if chain_angles[i] <= a <= chain_angles[i + 1]:
                    f = (a - chain_angles[i]) / (chain_angles[i + 1] - chain_angles[i])
                    pair = [(names[i], 1 - f), (names[i + 1], f)]
                    break
            # Down the chain: chest at the top, then each segment by height.
            nodes = [(CAPE_Z[0], None)] + [(mids[i], i) for i in range(3)]
            z = p.z
            vert = []
            if z >= nodes[0][0]:
                vert = [(None, 1.0)]
            elif z <= nodes[-1][0]:
                vert = [(2, 1.0)]
            else:
                for i in range(len(nodes) - 1):
                    if nodes[i + 1][0] <= z <= nodes[i][0]:
                        f = (nodes[i][0] - z) / (nodes[i][0] - nodes[i + 1][0])
                        vert = [(nodes[i][1], 1 - f), (nodes[i + 1][1], f)]
                        break
            out = []
            for seg, wv in vert:
                if seg is None:
                    out.append(("chest", wv))
                    continue
                for name, wa in pair:
                    out.append((f"cape.{name}.{seg + 1}", wv * wa))
            return out

        self.finish(piece, "Cloak", ["chest"], adjust=cape_weights)
        self.refresh()

    def sword(self):
        """A glowing crystal blade, dark guard, wrapped grip and pommel."""
        d = BLADE_DIR
        side = d.cross(FWD).normalized()
        flat = side.cross(d).normalized()
        g = GRIP
        bones = ["sword"]
        guard_at = g + d * 0.07
        blade = self.loft([
            (guard_at + d * 0.01, 0.022, 0.009),
            (guard_at + d * 0.07, 0.034, 0.014),
            (guard_at + d * 0.13, 0.024, 0.01),
            (guard_at + d * 0.40, 0.03, 0.011),
            (guard_at + d * 0.62, 0.026, 0.01),
        ], sides=4, cap0=True, cap1=True, tip1=guard_at + d * 0.74, ref=flat, phase=0.0)
        self.finish(blade, "Glow", bones, keep=1)
        guard = self.box(guard_at, (side, flat, d), (0.075, 0.02, 0.014))
        self.finish(guard, "Armour", bones, keep=1)
        collar = self.loft([(guard_at - d * 0.005, 0.03, 0.026), (guard_at + d * 0.035, 0.018, 0.016)], sides=6,
                           ref=flat, cap1=True, tip1=guard_at + d * 0.05)
        self.finish(collar, "Armour", bones, keep=1)
        grip = self.loft([(g - d * 0.09, 0.014, 0.014), (g + d * 0.065, 0.016, 0.016)], sides=6, ref=flat)
        self.finish(grip, "Glove", bones, keep=1)
        pommel = self.blob(g - d * 0.105, (0.022, 0.022, 0.022), segs=6, rings=3)
        self.finish(pommel, "Armour", bones, keep=1)
        self.refresh()

    # ---- rig ---------------------------------------------------------------

    def rig_up(self):
        arm = bpy.data.armatures.new("HoodedHeroRig")
        rig = bpy.data.objects.new("HoodedHeroRig", arm)
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
        for name in ORDER:
            head, tail, parent = BONES[name]
            eb = arm.edit_bones.new(name)
            eb.head, eb.tail = head, tail
            # Each bone's z axis faces forward (up, for bones lying along y), so the axes read the same on every limb.
            eb.align_roll(Z if abs((tail - head).normalized().y) > 0.7 else FWD)
            if parent:
                eb.parent = arm.edit_bones[parent]
                eb.use_connect = False
        bpy.ops.object.mode_set(mode="OBJECT")
        rig.select_set(False)
        if prev_active and prev_active.name in view_layer.objects:
            view_layer.objects.active = prev_active
        self.obj.parent = rig
        mod = self.obj.modifiers.new("Armature", "ARMATURE")
        mod.object = rig
        self.rig = rig

    # ---- poses and clips ---------------------------------------------------

    def rest_rot(self, name):
        return self.rig.data.bones[name].matrix_local.to_3x3()

    def set_pose(self, p):
        """Pose from angles in radians about the figure's own axes, at rest:
        thighS / armS swing forward, kneeS / elbowS bend, footS lifts the toes,
        armDownS brings the arm in from the A-pose, legOutS spreads a leg;
        spine / chest / neck / head lean forward, twistHips / twistChest /
        headTurn turn to the figure's left; bob (m) raises the hips and sway (m)
        moves them to the left; cape lifts the cape out behind, capeWave
        ripples it down the chains, capeSide swings it to the left; tabardS
        lifts a front flap; hood swings the hood's point back; swordX / swordZ
        tip the sword's point back / out."""
        rig = self.rig
        for pb in rig.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion()
            pb.location = Vector()

        def turn(name, axis, angle):
            if abs(angle) < 1e-6:
                return
            pb = rig.pose.bones[name]
            local = self.rest_rot(name).inverted() @ axis
            pb.rotation_quaternion = Quaternion(local, angle) @ pb.rotation_quaternion

        g = p.get
        for s, S in ((1, "L"), (-1, "R")):
            turn(f"thigh.{S}", X, -g("thigh" + S, 0.0))
            turn(f"thigh.{S}", Y, -s * g("legOut" + S, 0.0))
            turn(f"shin.{S}", X, g("knee" + S, 0.0))
            turn(f"foot.{S}", X, -g("foot" + S, 0.0))
            turn(f"upperarm.{S}", Y, s * g("armDown" + S, 0.0))
            turn(f"upperarm.{S}", X, -g("arm" + S, 0.0))
            turn(f"forearm.{S}", X, -g("elbow" + S, 0.0))
            turn(f"tabard.{S}.1", X, -g("tabard" + S, 0.0))
            turn(f"tabard.{S}.2", X, -0.5 * g("tabard" + S, 0.0))
        turn("hips", Z, g("twistHips", 0.0))
        turn("spine", X, g("spine", 0.0))
        turn("chest", X, g("chest", 0.0))
        turn("chest", Z, g("twistChest", 0.0))
        turn("neck", X, g("neck", 0.0))
        turn("head", X, g("head", 0.0))
        turn("head", Z, g("headTurn", 0.0))
        turn("hood", X, g("hood", 0.0))
        turn("sword", X, g("swordX", 0.0))
        turn("sword", Y, g("swordZ", 0.0))
        lift, wave, swing = g("cape", 0.0), g("capeWave", 0.0), g("capeSide", 0.0)
        for name, ang in CAPE_CHAINS:
            a = math.radians(ang)
            axis = v(math.cos(a), -math.sin(a), 0)  # turns a hanging chain out, away from the body
            for i in range(3):
                amount = lift * (1.0 if i == 0 else 0.35) + wave * (0.0, 0.6, 1.0)[i] * (1 if i % 2 else -1)
                turn(f"cape.{name}.{i + 1}", axis, amount)
                turn(f"cape.{name}.{i + 1}", Y, -swing * (0.5 if i == 0 else 0.25))
        pb = rig.pose.bones["hips"]
        pb.location = self.rest_rot("hips").inverted() @ v(g("sway", 0.0), -g("shift", 0.0), g("bob", 0.0))

    def animate(self):
        rig = self.rig
        bpy.context.scene.render.fps = FPS
        rig.animation_data_create()
        made = []
        for name, keys, frames in clips():
            act = bpy.data.actions.new("HH_" + name)
            act.use_fake_user = True
            rig.animation_data.action = act
            for frame, p in keys + [(frames, keys[0][1])]:
                self.set_pose(p)
                for pb in rig.pose.bones:
                    pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
                    pb.keyframe_insert("location", frame=frame, group=pb.name)
            # Smooth, seamless loops: Bezier keys whose handles see the cycle.
            for fc in action_fcurves(act, rig):
                if not any(m.type == "CYCLES" for m in fc.modifiers):
                    fc.modifiers.new("CYCLES")
                for kp in fc.keyframe_points:
                    kp.interpolation = "BEZIER"
                    kp.handle_left_type = kp.handle_right_type = "AUTO"
                fc.update()
            act.use_frame_range = True
            act.frame_start, act.frame_end = 0, frames
            act.use_cyclic = True
            made.append(act)
            rig.animation_data.action = None
        # Leave Walk playing, for a look in the viewport.
        rig.animation_data.action = made[1]
        bpy.context.scene.frame_start = 0
        bpy.context.scene.frame_end = int(made[1].frame_range[1]) - 1
        return made

    def build_all(self):
        for step in self.STEPS:
            getattr(self, "rig_up" if step == "rig" else step)()


def action_fcurves(act, rig):
    """An action's F-curves, under Blender 5's slotted actions or older ones."""
    if hasattr(act, "fcurves") and len(getattr(act, "fcurves")):
        return list(act.fcurves)
    out = []
    for layer in act.layers:
        for strip in layer.strips:
            for slot in act.slots:
                bag = strip.channelbag(slot)
                if bag:
                    out += list(bag.fcurves)
    return out


# --------------------------------------------------------------------------
# Clips: (name, [(frame, pose)], length in frames). Walk and run are half
# cycles off the right foot - contact, down, passing, up - then mirrored.


def mirror(p):
    out = {}
    for k, val in p.items():
        if k.endswith("L"):
            out[k[:-1] + "R"] = val
        elif k.endswith("R"):
            out[k[:-1] + "L"] = val
        elif k in ("twistHips", "twistChest", "headTurn", "sway", "capeSide"):
            out[k] = -val
        else:
            out[k] = val
    return out


# The right hand keeps the sword, so its arm swings less and the left takes
# up the difference.
HOLD = dict(armDownL=0.38, armDownR=0.30, elbowL=0.2, elbowR=0.3)


def with_hold(p, hold=HOLD, sword_swing=0.0):
    q = dict(hold)
    q.update(p)
    if "armR" in q:
        q["armR"] *= 0.55
    q["swordX"] = q.get("swordX", 0.0) + sword_swing
    # The flaps follow the thighs forward (never back into the legs).
    for S in "LR":
        q.setdefault("tabard" + S, 0.08 + 0.75 * max(0.0, q.get("thigh" + S, 0.0)))
    return q


def clips():
    walk_half = [
        (0, dict(thighR=0.42, kneeR=0.05, footR=0.25, thighL=-0.30, kneeL=0.2, footL=-0.3,
                 armL=0.32, armR=-0.26, bob=-0.012, twistHips=0.09, twistChest=-0.1, sway=-0.006,
                 cape=0.10, capeWave=0.04, hood=0.06)),
        (4, dict(thighR=0.28, kneeR=0.28, footR=0.04, thighL=-0.24, kneeL=0.6, footL=-0.45,
                 armL=0.22, armR=-0.18, bob=-0.035, twistHips=0.06, twistChest=-0.07, sway=-0.012,
                 cape=0.12, capeWave=-0.03, hood=0.1)),
        (8, dict(thighR=0.0, kneeR=0.08, footR=0.0, thighL=0.26, kneeL=0.9, footL=-0.05,
                 armL=0.0, armR=0.02, bob=0.012, sway=-0.016, cape=0.09, capeWave=0.0, hood=0.04)),
        (11, dict(thighR=-0.24, kneeR=0.06, footR=-0.25, thighL=0.38, kneeL=0.38, footL=0.12,
                  armL=-0.2, armR=0.2, bob=0.026, twistHips=-0.06, twistChest=0.07, sway=-0.01,
                  cape=0.11, capeWave=0.04, hood=0.02)),
    ]
    run_half = [
        (0, dict(thighR=0.62, kneeR=0.25, footR=0.2, thighL=-0.48, kneeL=0.65, footL=-0.5,
                 armL=0.75, armR=-0.6, elbowL=1.2, elbowR=1.0, spine=0.12, chest=0.06, head=-0.12,
                 bob=-0.02, twistHips=0.12, twistChest=-0.16, cape=0.42, capeWave=0.12, hood=0.3)),
        (2, dict(thighR=0.36, kneeR=0.7, footR=0.02, thighL=-0.32, kneeL=1.3, footL=-0.35,
                 armL=0.45, armR=-0.4, elbowL=1.25, elbowR=1.0, spine=0.14, chest=0.06, head=-0.13,
                 bob=-0.08, twistHips=0.08, twistChest=-0.1, cape=0.36, capeWave=-0.1, hood=0.38)),
        (5, dict(thighR=-0.05, kneeR=0.3, footR=-0.1, thighL=0.58, kneeL=1.6, footL=-0.15,
                 armL=0.05, armR=0.0, elbowL=1.2, elbowR=1.0, spine=0.13, chest=0.05, head=-0.12,
                 bob=0.0, cape=0.45, capeWave=0.08, hood=0.32)),
        (7, dict(thighR=-0.48, kneeR=0.45, footR=-0.45, thighL=0.66, kneeL=0.9, footL=0.12,
                 armL=-0.45, armR=0.5, elbowL=1.1, elbowR=1.0, spine=0.12, chest=0.06, head=-0.12,
                 bob=0.06, twistHips=-0.1, twistChest=0.12, cape=0.52, capeWave=-0.12, hood=0.26)),
    ]
    walk = [(f, with_hold(p, sword_swing=0.45)) for f, p in walk_half] + \
           [(f + 15, with_hold(mirror(p), sword_swing=0.45)) for f, p in walk_half]
    run = [(f, with_hold(p, sword_swing=1.0)) for f, p in run_half] + \
          [(f + 10, with_hold(mirror(p), sword_swing=1.0)) for f, p in run_half]
    stand = dict(legOutL=0.03, legOutR=0.03, thighL=0.02, thighR=-0.03, kneeL=0.05, kneeR=0.04)
    idle = [
        (0, with_hold(dict(stand, chest=-0.01, head=0.02, bob=-0.004, sway=0.004, cape=0.02, hood=0.0))),
        (60, with_hold(dict(stand, chest=-0.045, neck=-0.01, head=0.0, armDownL=0.35, armDownR=0.28,
                            bob=0.003, sway=0.008, cape=0.035, capeWave=0.02, hood=0.03))),
    ]
    return [("Idle", idle, 120), ("Walk", walk, 30), ("Run", run, 20)]


# --------------------------------------------------------------------------
# Headless: build, preview, export


def render_previews(b):
    os.makedirs(PREVIEW, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = 520, 640
    scene.world = bpy.data.worlds.new("w")
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 2.3
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam

    def shoot(name, act, frame, yaw_deg, pitch_deg=10):
        b.rig.animation_data.action = act
        scene.frame_set(frame)
        yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
        d = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
        cam.location = v(0, 0, 0.95) + d * 6
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        scene.render.filepath = os.path.join(PREVIEW, name + ".png")
        bpy.ops.render.render(write_still=True)

    acts = {a.name[3:]: a for a in bpy.data.actions if a.name.startswith("HH_")}
    shoot("idle_front", acts["Idle"], 0, 0)
    shoot("idle_side", acts["Idle"], 0, 90)
    shoot("idle_back", acts["Idle"], 0, 180)
    shoot("idle_34", acts["Idle"], 0, 35, 30)
    for f in (0, 4, 8, 11):
        shoot(f"walk{f:02d}", acts["Walk"], f, 70, 12)
    for f in (0, 2, 5, 7):
        shoot(f"run{f:02d}", acts["Run"], f, 70, 12)


def export():
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=OUT,
        export_format="GLB",
        export_yup=True,
        export_normals=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_skins=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_force_sampling=True,
        export_frame_step=1,
        export_optimize_animation_size=False,
        export_def_bones=False,
        export_reset_pose_bones=True,
        export_apply=False,
    )
    print("wrote", OUT)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = Builder()
    b.build_all()
    tris = sum(len(p.vertices) - 2 for p in b.obj.data.polygons)
    print("hooded hero:", len(b.obj.data.vertices), "verts,", tris, "tris,", len(ORDER), "bones")
    if PREVIEW:
        render_previews(b)
    b.rig.animation_data.action = None
    b.set_pose({})
    export()


if __name__ == "__main__" and bpy.app.background:
    main()
