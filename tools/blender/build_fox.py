"""Build the fox - mesh, rig, its sleeping clip and its waking clips - and
export assets/characters/fox.glb.

Run headless (builds everything and exports):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_fox.py
    PXE_FOX_PREVIEW=/some/dir ... also renders preview images there (see
    render_previews), fox.png among them.
The build checks the fox holds together (see check) in the bind pose and
through all five clips, that the walk's planted paws move back at the
walk's speed without slipping and its legs bend the standing way (see
check_walk), and that every clip moves smoothly between its keys
as the engine plays it - no joint turning more than 45 degrees from one key
to the next, nothing more than 1 cm below the ground at half-keys (see
check_motion) - and stops if not.

Or step by step in an open Blender, to watch it go together:
    import runpy; fx = runpy.run_path(".../build_fox.py", run_name="fox")
    b = fx["Builder"](); b.start(); b.body(); b.head(); ...  (see Builder.STEPS)
Each step adds its pieces to the Fox collection, leaving the rest of the
scene alone.

The figure: a stylised red fox, modelled standing at real size (0.41 m at
the shoulder, 1.1 m nose to tail tip) and bound in that pose, so walk and run
clips can be added later. Rust-orange coat; cream muzzle, cheeks, throat,
chest, belly and tail tip; dark brown-black stockings and ear backs; black
nose and closed eyes. Ears and the brush of the tail are a little oversized
so they read at pixel scale. Every face is flat, so it reads as cut paper.

The rig, a full quadruped skeleton:
    root > hips > spine.1 > spine.2 > chest > neck > head > ear.{L,R}
    spine.2 > ribcage                         (a leaf, scaled to breathe)
    chest > upperarm > forearm > forepaw      (front legs, .L/.R)
    hips > thigh > shin > hindpaw             (hind legs, .L/.R)
    hips > tail.1 > tail.2 > tail.3 > tail.4 > tail.5
The fox faces -y; its left is +x.

Five clips. Fox_Rest (4 s, loops seamlessly): curled up asleep in a tight
ball on its right side, legs folded in, the head on the ground with its nose
at the root of the brush, and the brush wrapped round the outside, across
the face; breathing slowly, with a flick of the ear and a stir of the tail
tip. In the clip the ball is turned and moved so the head, behind the brush,
is at the front (-y) and the ball sits over the origin.

Three one-shot clips wake it, strung together in the game as Rest ->
StandUp -> LookAround -> LieDown -> Rest (see "Waking" below):
    Fox_StandUp (1.5 s): from Fox_Rest's first pose, the head lifts out from
        under the brush, the ring unrolls onto its belly, and it rises front
        first onto all four paws, turned a quarter turn to its left (+x).
    Fox_LookAround (4 s): standing, ears up, it looks left, then right, then
        sniffs the ground; the tail sways low. Starts and ends on StandUp's
        last pose; the paws and body don't move.
    Fox_LieDown (2 s): hind end down, then the front, onto its belly, then it
        rolls and curls back into exactly Fox_Rest's first pose.
One more loops: Fox_Walk (1.2 s), an unhurried four-beat walk in place,
turned as standing; see "Walking" below.

Standing paws are planted (Builder.plant): they don't slide or lift, and
the legs bend as they do standing (elbows behind the forepaws, knees in front
of the hind paws) all the while.

Each leg is one tube from inside the body to the toes, and the top of each
leg and both ends of the neck are weighted into the body and head, so no
open end or seam shows however far a joint folds.

Material order is the contract with the engine: glTF material i is the
fox's actor material i (the engine offsets them into its own slots).
"""

import math
import os

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "assets", "characters", "fox.glb"))
PREVIEW = os.environ.get("PXE_FOX_PREVIEW")
COLLECTION = "Fox"
PREFIX = "Fox_"
FPS = 30
REST_FRAMES = 120  # Fox_Rest: 4 s

X = Vector((1, 0, 0))
Y = Vector((0, 1, 0))
Z = Vector((0, 0, 1))
FWD = Vector((0, -1, 0))  # the fox faces -y; its left is +x


def v(x, y, z):
    return Vector((x, y, z))


def srgb(r, g, b):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b))


def clamp01(x):
    return max(0.0, min(1.0, x))


# --------------------------------------------------------------------------
# Materials, in the engine's order: (name, linear base colour, emissive).
# Base colours are the mid tone; the engine's toon ramp makes light and shade.

MATERIALS = [
    ("Fur", srgb(208, 98, 40), False),     # rust-orange coat
    ("Cream", srgb(238, 220, 188), False),  # muzzle, cheeks, chest, belly, tail tip, inner ears
    ("Dark", srgb(54, 38, 33), False),      # stockings, ear backs
    ("Black", srgb(20, 17, 17), False),     # nose, closed eyes
]
M = {name: i for i, (name, _, _) in enumerate(MATERIALS)}

# --------------------------------------------------------------------------
# Skeleton: name -> (head, tail, parent), in order (parents first)

BONES = {}
ORDER = []


def bone(name, head, tail, parent=None):
    BONES[name] = (v(*head), v(*tail), parent)
    ORDER.append(name)


# The tail's joints, from the rump out to the tip of the brush, held low.
TAIL = [v(0, 0.235, 0.36), v(0, 0.315, 0.335), v(0, 0.395, 0.295), v(0, 0.47, 0.255), v(0, 0.545, 0.22),
        v(0, 0.625, 0.195)]

bone("root", (0, 0, 0), (0, -0.2, 0))
bone("hips", (0, 0.16, 0.335), (0, 0.06, 0.34), "root")
bone("spine.1", (0, 0.06, 0.34), (0, -0.04, 0.345), "hips")
bone("spine.2", (0, -0.04, 0.345), (0, -0.13, 0.35), "spine.1")
bone("chest", (0, -0.13, 0.35), (0, -0.215, 0.355), "spine.2")
bone("ribcage", (0, -0.0, 0.31), (0, -0.16, 0.31), "spine.2")
bone("neck", (0, -0.215, 0.355), (0, -0.30, 0.44), "chest")
bone("head", (0, -0.30, 0.44), (0, -0.48, 0.42), "neck")
for s, S in ((1, "L"), (-1, "R")):
    bone(f"ear.{S}", (s * 0.036, -0.318, 0.50), (s * 0.06, -0.302, 0.625), "head")
for s, S in ((1, "L"), (-1, "R")):
    bone(f"upperarm.{S}", (s * 0.058, -0.185, 0.315), (s * 0.062, -0.165, 0.19), "chest")
    bone(f"forearm.{S}", (s * 0.062, -0.165, 0.19), (s * 0.058, -0.18, 0.05), f"upperarm.{S}")
    bone(f"forepaw.{S}", (s * 0.058, -0.18, 0.05), (s * 0.058, -0.235, 0.012), f"forearm.{S}")
    bone(f"thigh.{S}", (s * 0.062, 0.155, 0.31), (s * 0.068, 0.10, 0.19), "hips")
    bone(f"shin.{S}", (s * 0.068, 0.10, 0.19), (s * 0.062, 0.175, 0.09), f"thigh.{S}")
    bone(f"hindpaw.{S}", (s * 0.062, 0.175, 0.09), (s * 0.06, 0.12, 0.012), f"shin.{S}")
for i in range(5):
    bone(f"tail.{i + 1}", tuple(TAIL[i]), tuple(TAIL[i + 1]), "hips" if i == 0 else f"tail.{i}")

SPINE = ["hips", "spine.1", "spine.2", "chest"]
GROUND_SLACK = 0.001  # how far into the ground a one-shot clip may reach (m)
IK_EASE = 0.4  # over the first 40% of a step the leg eases from its posed angles to the IK's
STEP_LIFT = 0.04  # how high a paw lifts on its way to where it plants (m)
# Each leg's (upper, lower, paw) bones, for planting the paws.
LEGS = [(f"upperarm.{S}", f"forearm.{S}", f"forepaw.{S}") for S in "LR"] + \
       [(f"thigh.{S}", f"shin.{S}", f"hindpaw.{S}") for S in "LR"]
# The parts the one-shot clips time separately.
PARTS = {
    "root": ["root"],
    "body": SPINE + ["ribcage"],
    "head": ["neck", "head", "ear.L", "ear.R"],
    "tail": [f"tail.{i + 1}" for i in range(5)],
    "fore": [n for leg in LEGS[:2] for n in leg],
    "hind": [n for leg in LEGS[2:] for n in leg],
}


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
    STEPS = ["start", "body", "head", "ears", "legs", "tail", "rig", "animate"]

    def __init__(self):
        self.bm = bmesh.new()
        self.deform = self.bm.verts.layers.deform.verify()
        self.group = {name: i for i, name in enumerate(ORDER)}
        self.obj = None
        self.rig = None
        self.coll = None
        self.walk_contact = None  # each paw's contact point in its bone's rest frame (walk_clip)

    # ---- scene plumbing ---------------------------------------------------

    def start(self):
        """Fresh Fox collection and materials; the rest of the scene is left alone."""
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
        for act in [a for a in bpy.data.actions if a.name.startswith(PREFIX)]:
            bpy.data.actions.remove(act)
        self.coll = bpy.data.collections.new(COLLECTION)
        bpy.context.scene.collection.children.link(self.coll)

        mats = []
        for name, colour, glow in MATERIALS:
            mname = PREFIX + name
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

        mesh = bpy.data.meshes.new("Fox")
        for mat in mats:
            mesh.materials.append(mat)
        self.obj = bpy.data.objects.new("Fox", mesh)
        self.coll.objects.link(self.obj)
        for name in ORDER:
            self.obj.vertex_groups.new(name=name)
        self.refresh()

    def refresh(self):
        self.bm.to_mesh(self.obj.data)
        self.obj.data.update()

    # ---- geometry helpers -------------------------------------------------

    def loft(self, rings, sides=8, cap0=True, cap1=True, ref=Z, phase=0.5, tip0=None, tip1=None):
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

    def outward(self, piece):
        """Wind a piece's faces consistently (so a tube's caps agree with its
        sides), then turn them all to face away from its middle."""
        bm = self.bm
        if not piece.faces:
            return
        bmesh.ops.recalc_face_normals(bm, faces=piece.faces)
        bm.normal_update()
        mid = sum((f.calc_center_median() for f in piece.faces), Vector()) / len(piece.faces)
        score = sum(f.normal.dot(f.calc_center_median() - mid) * f.calc_area() for f in piece.faces)
        if score < 0:
            for f in piece.faces:
                f.normal_flip()
            bm.normal_update()

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

    def blob(self, centre, radii, segs=8, rings=5):
        """A low-poly ellipsoid."""
        bm = self.bm
        res = bmesh.ops.create_uvsphere(bm, u_segments=segs, v_segments=rings, radius=1.0)
        piece = Piece(res["verts"], list({f for x in res["verts"] for f in x.link_faces}))
        for x in piece.verts:
            x.co = v(x.co.x * radii[0], x.co.y * radii[1], x.co.z * radii[2]) + Vector(centre)
        self.outward(piece)
        return piece

    def finish(self, piece, mat, bones, power=4.0, keep=3, adjust=None, paint=None):
        """Material (or paint(face) -> material name, None for `mat`), flat
        shading, and weights to the nearest of `bones`."""
        for f in piece.faces:
            name = paint(f) if paint else None
            f.material_index = M[name or mat]
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

    def limb(self, bone_name, radii, sides=6, cap0=False, cap1=False, sink=(0.0, 0.0), ref=FWD):
        """A tube along a bone: radii are (fraction along, rx, ry)."""
        h, t, _ = BONES[bone_name]
        d = t - h
        h = h - d.normalized() * sink[0]
        t = t + d.normalized() * sink[1]
        rings = [(h + (t - h) * f, rx, ry) for f, rx, ry in radii]
        return self.loft(rings, sides=sides, cap0=cap0, cap1=cap1, ref=ref)

    def surface(self, origin, target):
        """Where a ray from origin toward target first meets the mesh so far:
        (point, normal), for setting features onto a surface."""
        bm = self.bm
        bm.normal_update()
        bvh = BVHTree.FromBMesh(bm)
        hit, normal, _, _ = bvh.ray_cast(Vector(origin), (Vector(target) - Vector(origin)).normalized())
        if hit is None:
            raise RuntimeError(f"no surface between {origin} and {target}")
        return hit, normal

    # ---- the figure -------------------------------------------------------

    def body(self):
        """The trunk, rump to chest, and the neck: orange above, cream under
        the belly, up the chest and the throat. The ribcage swells to breathe."""
        trunk = self.loft([
            (v(0, 0.275, 0.335), 0.05, 0.05),
            (v(0, 0.245, 0.33), 0.074, 0.08),
            (v(0, 0.18, 0.325), 0.086, 0.094),
            (v(0, 0.10, 0.318), 0.082, 0.09),
            (v(0, 0.01, 0.312), 0.077, 0.088),
            (v(0, -0.08, 0.312), 0.082, 0.096),
            (v(0, -0.16, 0.315), 0.087, 0.1),
            (v(0, -0.22, 0.33), 0.078, 0.092),
            (v(0, -0.255, 0.345), 0.055, 0.065),
        ], sides=12)

        def trunk_paint(f):
            c, n = f.calc_center_median(), f.normal
            if n.z < -0.55:
                return "Cream"  # belly
            if c.y < -0.15 and n.z < 0.25 and n.y < -0.1:
                return "Cream"  # chest
            if c.y < -0.2 and n.z < 0.55 and n.y < -0.3:
                return "Cream"  # the front of the chest, up to the throat
            return None

        def breathe(p, ws):
            # The ribcage takes a share round the middle of the trunk, most
            # on the flanks and belly, least along the spine.
            along = clamp01(1.0 - abs(p.y + 0.07) / 0.15)
            low = clamp01((0.36 - p.z) / 0.1)
            share = 0.75 * along * (0.35 + 0.65 * low)
            return [(n, w * (1 - share)) for n, w in ws] + [("ribcage", share)]

        self.finish(trunk, "Fur", SPINE, power=3.0, adjust=breathe, paint=trunk_paint)

        neck = self.loft([
            (v(0, -0.15, 0.33), 0.055, 0.065),  # deep in the chest
            (v(0, -0.2, 0.35), 0.07, 0.082),
            (v(0, -0.245, 0.385), 0.063, 0.07),
            (v(0, -0.29, 0.435), 0.052, 0.054),
            (v(0, -0.322, 0.45), 0.04, 0.04),  # deep in the head
        ], sides=10, cap0=False, cap1=False, ref=Z)

        def neck_paint(f):
            n = f.normal
            return "Cream" if (n.y < -0.25 and n.z < 0.35) or n.z < -0.3 else None

        def neck_weights(p, ws):
            # The ends sit inside the chest and the head and stay there; the
            # neck bends between them.
            t = clamp01((-0.2 - p.y) / 0.085)
            return [("chest", (1 - t) ** 2), ("neck", 2 * t * (1 - t)), ("head", t * t)]

        self.finish(neck, "Fur", ["chest", "neck", "head"], adjust=neck_weights, paint=neck_paint)
        self.refresh()

    def head(self):
        """A wedge of a head: wide cheeks to a slim muzzle, cream below and
        on the cheeks, the black nose and closed eyes set on its surface."""
        head = self.loft([
            (v(0, -0.282, 0.455), 0.048, 0.048),
            (v(0, -0.305, 0.463), 0.074, 0.062),
            (v(0, -0.34, 0.462), 0.072, 0.058),
            (v(0, -0.372, 0.45), 0.052, 0.046),
            (v(0, -0.415, 0.433), 0.032, 0.032),
            (v(0, -0.455, 0.423), 0.022, 0.024),
        ], sides=8, ref=Z, phase=0.0, tip1=v(0, -0.488, 0.418))

        def head_paint(f):
            c, n = f.calc_center_median(), f.normal
            if c.y < -0.36 and n.z < 0.2:
                return "Cream"  # sides and underside of the muzzle
            if n.z < -0.25:
                return "Cream"  # cheeks and jaw
            return None

        def face_on_head(p, ws):
            # All but the back of the skull is on the head alone, so the eyes,
            # nose and ruffs (on the head too) stay set in it.
            return ws if p.y > -0.3 else [("head", 1.0)]

        self.finish(head, "Fur", ["head", "neck"], keep=2, adjust=face_on_head, paint=head_paint)

        # Cheek ruffs: a flared tuft of cream fur behind each jaw.
        bm = self.bm
        for s in (1, -1):
            pts = [v(s * 0.05, -0.30, 0.43), v(s * 0.05, -0.345, 0.425), v(s * 0.092, -0.31, 0.405),
                   v(s * 0.06, -0.30, 0.455)]
            vs = [bm.verts.new(p) for p in pts]
            ruff = Piece(vs, [bm.faces.new((vs[0], vs[1], vs[2])), bm.faces.new((vs[0], vs[2], vs[3])),
                              bm.faces.new((vs[1], vs[3], vs[2])), bm.faces.new((vs[0], vs[3], vs[1]))])
            self.outward(ruff)
            self.finish(ruff, "Cream", ["head"], keep=1)

        nose = self.blob(v(0, -0.486, 0.42), (0.015, 0.012, 0.012), segs=6, rings=4)
        self.finish(nose, "Black", ["head"], keep=1)

        # Closed eyes: a short dark slit each side, slanting up to the outer corner.
        for s in (1, -1):
            aim = v(s * 0.036, -0.37, 0.476)
            p, n = self.surface(aim + v(s * 0.08, -0.06, 0.08), aim)
            along = (v(s * 0.85, 0.3, 0.42) - n * n.dot(v(s * 0.85, 0.3, 0.42))).normalized()
            eye = self.box(p, (along, n, n.cross(along)), (0.014, 0.004, 0.0035))
            self.finish(eye, "Black", ["head"], keep=1)
        self.refresh()

    def ears(self):
        """Big pointed ears: cream inside, dark on the back, standing up and
        a little out."""
        bm = self.bm
        for s, S in ((1, "L"), (-1, "R")):
            h, t, _ = BONES[f"ear.{S}"]
            base = h - (t - h).normalized() * 0.02
            up = (t - h).normalized()
            out = v(s, 0, 0)
            out = (out - up * out.dot(up)).normalized()
            back = up.cross(out).normalized() * s  # toward +y
            pts = [base - out * 0.034 - back * 0.012, base + out * 0.036 - back * 0.012,
                   base + out * 0.03 + back * 0.02, base - out * 0.028 + back * 0.02]
            vs = [bm.verts.new(p) for p in pts]
            apex = bm.verts.new(t + back * 0.008)
            # The front is a shallow cup: its middle pushed back toward the tip.
            cup = bm.verts.new(base + up * 0.05 + back * 0.004)
            faces = [
                bm.faces.new((vs[0], vs[1], cup)), bm.faces.new((vs[1], apex, cup)), bm.faces.new((apex, vs[0], cup)),
                bm.faces.new((vs[1], vs[2], apex)), bm.faces.new((vs[2], vs[3], apex)), bm.faces.new((vs[3], vs[0], apex)),
                bm.faces.new((vs[3], vs[2], vs[1], vs[0])),
            ]
            ear = Piece(vs + [apex, cup], faces)
            self.outward(ear)
            front = set(faces[:3])
            self.finish(ear, "Dark", [f"ear.{S}"], keep=1, paint=lambda f, front=front: "Cream" if f in front else None)
        self.refresh()

    def legs(self):
        """Slim legs in dark stockings from the elbow and hock down, on small
        paws. Each leg is one tube from inside the body to the toes, so it
        bends at every joint without coming apart."""
        def leg(rings, bones, dark_z, joint):
            # Rings are (point, depth, width): depth in the leg's own plane,
            # width across it (along x), so the tube never twists.
            piece = self.loft(rings[:-1], sides=6, cap0=False, cap1=True, ref=X, tip1=rings[-1][0])
            for x in piece.verts:
                x.co.z = max(x.co.z, 0.0)  # the pad flat on the ground
            body = bones[-1]

            def anchored(p, ws):
                # Above the joint the leg is part of the body, so its top stays
                # inside the body however the leg folds; it blends across the joint.
                h = p.z - joint.z
                f = clamp01((h + 0.02) / 0.05)
                return [(n, w * (1 - f)) for n, w in ws] + [(body, f)]

            self.finish(piece, "Fur", bones, adjust=anchored,
                        paint=lambda f: "Dark" if f.calc_center_median().z < dark_z else None)

        for s, S in ((1, "L"), (-1, "R")):
            sh, el, _ = BONES[f"upperarm.{S}"]
            wr, toe = BONES[f"forepaw.{S}"][0], BONES[f"forepaw.{S}"][1]
            leg([(sh + v(-s * 0.03, 0, 0.05), 0.035, 0.03), (sh, 0.05, 0.042), (sh.lerp(el, 0.45), 0.04, 0.034),
                 (el, 0.027, 0.025), (el.lerp(wr, 0.5), 0.021, 0.019), (wr, 0.018, 0.017),
                 (wr.lerp(toe, 0.55) + v(0, 0, -0.004), 0.015, 0.022), (toe + v(0, 0.006, -0.002), 0.009, 0.017),
                 (toe + v(0, -0.012, -0.008), 0, 0)],
                [f"upperarm.{S}", f"forearm.{S}", f"forepaw.{S}", "chest"], 0.2, sh)
            hp, kn, _ = BONES[f"thigh.{S}"]
            hk, toe = BONES[f"hindpaw.{S}"][0], BONES[f"hindpaw.{S}"][1]
            leg([(hp + v(-s * 0.03, 0, 0.05), 0.04, 0.032), (hp, 0.068, 0.056), (hp.lerp(kn, 0.5), 0.054, 0.046),
                 (kn, 0.034, 0.03), (kn.lerp(hk, 0.5), 0.026, 0.023), (hk, 0.02, 0.018),
                 (hk.lerp(toe, 0.6) + v(0, 0, -0.006), 0.016, 0.016), (toe + v(0, 0.008, -0.002), 0.01, 0.017),
                 (toe + v(0, -0.014, -0.008), 0, 0)],
                [f"thigh.{S}", f"shin.{S}", f"hindpaw.{S}", "hips"], 0.135, hp)
        self.refresh()

    def tail(self):
        """The big brush, fat in the middle and tipped in cream."""
        # Rings along the tail's joints: (distance along, radius).
        lengths = [0.0]
        for i in range(5):
            lengths.append(lengths[-1] + (TAIL[i + 1] - TAIL[i]).length)
        total = lengths[-1]

        def at(d):
            for i in range(5):
                if d <= lengths[i + 1] or i == 4:
                    f = (d - lengths[i]) / (lengths[i + 1] - lengths[i])
                    return TAIL[i].lerp(TAIL[i + 1], f)

        profile = [(-0.02, 0.03), (0.04, 0.05), (0.11, 0.07), (0.19, 0.08), (0.27, 0.077), (0.34, 0.066),
                   (0.39, 0.048)]
        rings = [(at(max(d, 0.0)) + (v(0, d, 0) if d < 0 else Vector()), r, r * 0.92) for d, r in profile]
        tip = at(total) + (at(total) - at(total - 0.02)).normalized() * 0.03
        brush = self.loft(rings, sides=10, cap0=True, cap1=True, ref=Z, tip1=tip)
        # Cream from here to the tip.
        start = at(0.325)
        axis = (at(total) - at(total - 0.05)).normalized()
        self.finish(brush, "Fur", ["hips"] + [f"tail.{i + 1}" for i in range(5)], power=3.0,
                    paint=lambda f: "Cream" if (f.calc_center_median() - start).dot(axis) > 0 else None)
        self.refresh()

    # ---- rig ---------------------------------------------------------------

    def rig_up(self):
        arm = bpy.data.armatures.new("FoxRig")
        rig = bpy.data.objects.new("FoxRig", arm)
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
            # Each bone's z axis faces up (for bones lying along y) or forward, so the axes read the same on every limb.
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
        """Pose from angles in radians about the fox's own axes, at rest
        (each turn is carried by the bone's parent):
          face turns the whole fox to its left about the vertical; cx, cy
          move it (m) along x and y; drop (m) lowers the hips, sway (m) moves
          them to the left and shift (m) forward; pitch tips the body nose-up
          about the rump; roll lays it onto its right side (belly toward its
          left);
          bend curls each of spine.1, spine.2 and chest to the left;
          neckBend / headBend curl the neck and head left, neckDip / headDip
          drop them, headRoll rolls the head onto its right cheek;
          tailCurl curls each tail bone round to the right (the way that
          follows a leftward curl of the body round), tailPitch raises each
          (negative drops it), tailTip flicks the tip;
          foreS / elbowS / wristS swing the upper arm back, bend the forearm
          forward and fold the paw; thighS / kneeS / hockS swing the thigh
          forward, bend the shin back and bring the paw forward; tuckS /
          hipTuckS swing a folded front / hind leg in under the body (toward
          its middle);
          earS turns an ear back (a flick); breath (0..1) swells the ribcage
          and lifts the back;
          aim maps bones to a direction, or (direction, up), in the fox's own
          frame (before face): each is laid along it with its z axis (the
          crown, for the head) toward up, after the turns above - for resting
          the head and the end of the brush just so."""
        rig = self.rig
        for pb in rig.pose.bones:
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion = Quaternion()
            pb.location = Vector()
            pb.scale = Vector((1, 1, 1))

        def turn(name, axis, angle):
            if abs(angle) < 1e-6:
                return
            pb = rig.pose.bones[name]
            local = self.rest_rot(name).inverted() @ axis
            pb.rotation_quaternion = Quaternion(local, angle) @ pb.rotation_quaternion

        g = p.get
        turn("root", Z, g("face", 0.0))
        rig.pose.bones["root"].location = self.rest_rot("root").inverted() @ v(g("cx", 0.0), g("cy", 0.0), 0.0)
        rig.pose.bones["hips"].location = self.rest_rot("hips").inverted() @ v(g("sway", 0.0), -g("shift", 0.0),
                                                                                -g("drop", 0.0))
        turn("hips", X, -g("pitch", 0.0))
        turn("spine.1", Y, -g("roll", 0.0))
        b = g("breath", 0.0)
        for name in ("spine.1", "spine.2", "chest"):
            turn(name, Z, g("bend", 0.0))
        turn("spine.1", X, 0.025 * b)
        turn("spine.2", X, -0.02 * b)
        turn("neck", Z, g("neckBend", 0.0))
        turn("neck", X, g("neckDip", 0.0))
        turn("head", Z, g("headBend", 0.0))
        turn("head", X, g("headDip", 0.0))
        turn("head", Y, -g("headRoll", 0.0))
        rib = rig.pose.bones["ribcage"]
        rib.scale = Vector((1.0 + 0.07 * b, 1.0, 1.0 + 0.05 * b))
        curl, pitch = g("tailCurl", [0.0] * 5), g("tailPitch", [0.0] * 5)
        for i in range(5):
            turn(f"tail.{i + 1}", X, pitch[i])
            turn(f"tail.{i + 1}", Z, -curl[i])
        for s, S in ((1, "L"), (-1, "R")):
            turn(f"upperarm.{S}", X, g("fore" + S, 0.0))
            turn(f"upperarm.{S}", Y, s * g("tuck" + S, 0.0))
            turn(f"forearm.{S}", X, -g("elbow" + S, 0.0))
            turn(f"forepaw.{S}", X, g("wrist" + S, 0.0))
            turn(f"thigh.{S}", X, -g("thigh" + S, 0.0))
            turn(f"thigh.{S}", Y, s * g("hipTuck" + S, 0.0))
            turn(f"shin.{S}", X, g("knee" + S, 0.0))
            turn(f"hindpaw.{S}", X, -g("hock" + S, 0.0))
            turn(f"ear.{S}", X, -g("ear" + S, 0.0))
        # Laid in place: bones aimed along directions in the fox's own frame
        # (before `face`), parents first, each with its z axis toward an up.
        aims = g("aim", {})
        for name in ORDER:
            if name in aims:
                fwd, up = aims[name] if len(aims[name]) == 2 else (aims[name], Z)
                self.aim(name, Vector(fwd), Vector(up))
        turn("tail.5", Z, g("tailTip", 0.0))  # after any aim, so the tip still stirs

    def aim(self, name, fwd, up=Z):
        """Turn a bone so it points along fwd, with its z axis as near `up`
        as it can be, both in the fox's own frame (turned with the root),
        keeping its children's own turns."""
        rig = self.rig
        bpy.context.view_layer.update()
        frame = rig.pose.bones["root"].matrix.to_3x3() @ self.rest_rot("root").inverted()
        y = (frame @ fwd).normalized()
        z = frame @ up
        z = (z - y * z.dot(y)).normalized()
        x = y.cross(z)
        pb = rig.pose.bones[name]
        m = Matrix((x, y, z)).transposed().to_4x4()
        m.translation = pb.matrix.translation
        pb.matrix = m
        bpy.context.view_layer.update()

    # ---- one-shot clips: key poses, blended per part, paws planted ---------

    def snapshot(self):
        """Every bone's local (rotation, location, scale) as posed now."""
        return {pb.name: (pb.rotation_quaternion.copy(), pb.location.copy(), pb.scale.copy())
                for pb in self.rig.pose.bones}

    def apply(self, snap):
        for pb in self.rig.pose.bones:
            q, loc, sc = snap[pb.name]
            pb.rotation_mode = "QUATERNION"
            pb.rotation_quaternion, pb.location, pb.scale = q.copy(), loc.copy(), sc.copy()
        bpy.context.view_layer.update()

    def pose_of(self, params):
        self.set_pose(params)
        bpy.context.view_layer.update()
        return self.snapshot()

    def lowest(self):
        """The lowest point of the posed mesh (m)."""
        dg = bpy.context.evaluated_depsgraph_get()
        ev = self.obj.evaluated_get(dg)
        me = ev.to_mesh()
        low = min((self.obj.matrix_world @ x.co).z for x in me.vertices)
        ev.to_mesh_clear()
        return low

    def legs_planted(self):
        """Each paw's armature-space matrix as posed now (where it is
        planted), and each leg's bend: the way its knee or elbow stands off
        the line from hip or shoulder to paw, in the frame of the body bone
        the leg hangs from (chest or hips). Taken from the standing pose, so
        planted legs bend as they do standing - elbows behind the forepaws,
        knees in front of the hind paws - however the body moves."""
        rig = self.rig
        planted = {}
        for leg in LEGS:
            upper, lower, paw = (rig.pose.bones[n] for n in leg)
            line = (paw.head - upper.head).normalized()
            off = lower.head - upper.head
            off -= line * off.dot(line)
            planted[leg[2]] = (paw.matrix.copy(), upper.parent.matrix.to_3x3().inverted() @ off.normalized())
        return planted

    def plant(self, leg, planted, w):
        """Swing a leg's two upper bones (2-bone IK, bending the way the
        standing leg bends, from `planted`) so its paw joint reaches a goal,
        and turn the paw to the goal's orientation. With w = 1 the goal is
        where the paw is planted; between 0 and 1 the paw is on its way from
        where the pose puts it, lifted in a low arc (a step), so it doesn't
        drag through the ground; at 0 the leg is left as it is. Each bone is
        swung from where it would point unturned on its parent, so it never
        rolls round however it was posed before."""
        if w <= 0.0:
            return
        target, bend = planted
        rig = self.rig
        upper, lower, paw = (rig.pose.bones[n] for n in leg)
        before = {pb.name: pb.location.copy() for pb in (upper, lower, paw)}
        posed = {pb.name: pb.rotation_quaternion.copy() for pb in (upper, lower, paw)}
        hip = upper.head.copy()
        if w < 1.0:
            src = paw.matrix
            goal = src.translation.lerp(target.translation, w) + Z * (STEP_LIFT * math.sin(math.pi * w))
            goal.z = max(goal.z, target.translation.z)
            turn = src.to_quaternion().slerp(target.to_quaternion(), w).to_matrix()
        else:
            goal, turn = target.translation.copy(), target.to_3x3()
        if (paw.head - goal).length < 1e-6 and \
                (paw.matrix.to_quaternion().rotation_difference(turn.to_quaternion())).angle < 1e-6:
            return  # already there (the key pose itself)
        a, b = upper.bone.length, lower.bone.length
        to = goal - hip
        d = max(abs(a - b) + 1e-4, min(a + b - 1e-4, to.length))
        dirn = to.normalized()
        pole = upper.parent.matrix.to_3x3() @ bend
        pole -= dirn * pole.dot(dirn)
        pole.normalize()
        cos_a = (a * a + d * d - b * b) / (2 * a * d)
        sin_a = math.sqrt(max(0.0, 1 - cos_a * cos_a))
        knee = hip + dirn * (a * cos_a) + pole * (a * sin_a)
        foot = hip + dirn * d

        def swing(pb, y_new, head):
            # Unturned on its parent, then the least turn to lie along y_new.
            r = pb.parent.matrix.to_3x3() @ pb.parent.bone.matrix_local.to_3x3().inverted() @ \
                pb.bone.matrix_local.to_3x3()
            r = (r @ Y).rotation_difference(y_new).to_matrix() @ r
            m = r.to_4x4()
            m.translation = head
            pb.matrix = m
            bpy.context.view_layer.update()

        swing(upper, (knee - hip).normalized(), hip)
        swing(lower, (foot - knee).normalized(), knee)
        m = turn.to_4x4()
        m.translation = foot
        paw.matrix = m
        # At the start of a step the joints ease over from the posed leg to
        # the IK's, so the leg doesn't jump as the IK takes it over (or, in
        # reverse, hands it back).
        ease = smooth(w / IK_EASE)
        for pb in (upper, lower, paw):
            pb.location = before[pb.name]  # legs never move off their joints
            if ease < 1.0:
                q0, q1 = posed[pb.name], pb.rotation_quaternion.copy()
                pb.rotation_quaternion = q0.slerp(-q1 if q0.dot(q1) < 0 else q1, ease)
        bpy.context.view_layer.update()

    def blend_keys(self, keys, times, u):
        """Pose between key snapshots: each part (PARTS) passes through the
        keys at its own times (times[part], one per key), eased in and out
        of each. Exactly a key at its time."""
        out = {}
        for part, names in PARTS.items():
            ts = times[part]
            i = 0
            while i < len(ts) - 2 and u >= ts[i + 1]:
                i += 1
            span = ts[i + 1] - ts[i]
            f = smooth((u - ts[i]) / span) if span > 0 else (1.0 if u >= ts[i + 1] else 0.0)
            for name in names:
                if f <= 0.0:
                    out[name] = keys[i][name]
                elif f >= 1.0:
                    out[name] = keys[i + 1][name]
                else:
                    q0, l0, s0 = keys[i][name]
                    q1, l1, s1 = keys[i + 1][name]
                    q1 = -q1 if q0.dot(q1) < 0 else q1
                    out[name] = (q0.slerp(q1, f), l0.lerp(l1, f), s0.lerp(s1, f))
        return out

    def animate(self):
        rig = self.rig
        bpy.context.scene.render.fps = FPS
        rig.animation_data_create()
        made = {}
        for name, pose_at, frames, step in clips():
            act = bpy.data.actions.new(PREFIX + name)
            act.use_fake_user = True
            rig.animation_data.action = act
            prev = {}
            # The clip is a periodic function of time, keyed every `step`
            # frames; the last key is the first again, so it loops.
            for frame in list(range(0, frames, step)) + [frames]:
                self.set_pose(pose_at((frame % frames) / frames))
                for pb in rig.pose.bones:
                    # Keep each key's quaternion on the same side as the last,
                    # so the curves between them take the short way round.
                    q = pb.rotation_quaternion
                    if pb.name in prev and q.dot(prev[pb.name]) < 0.0:
                        pb.rotation_quaternion = -q
                    prev[pb.name] = pb.rotation_quaternion.copy()
                    pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
                    pb.keyframe_insert("location", frame=frame, group=pb.name)
                    pb.keyframe_insert("scale", frame=frame, group=pb.name)
            # Bezier handles that see the cycle, so it runs on without a seam.
            for fc in action_fcurves(act):
                if not any(m.type == "CYCLES" for m in fc.modifiers):
                    fc.modifiers.new("CYCLES")
                for kp in fc.keyframe_points:
                    kp.interpolation = "BEZIER"
                    kp.handle_left_type = kp.handle_right_type = "AUTO"
                fc.update()
            act.use_frame_range = True
            act.frame_start, act.frame_end = 0, frames
            act.use_cyclic = True
            made[name] = act
            rig.animation_data.action = None
        made.update(self.animate_once())
        rig.animation_data.action = made["Rest"]
        bpy.context.scene.frame_start = 0
        bpy.context.scene.frame_end = REST_FRAMES - 1
        return made

    def animate_once(self):
        """The one-shot clips and Fox_Walk (once_clips), keyed on every frame
        from 0 to their length inclusive, linear between keys, so the
        export samples exactly the designed poses and nothing past the end
        (Fox_Walk's last key is its first, so it loops).
        Quaternion signs carry on from Fox_Rest's first key through the
        clips in order, so each key takes the short way from the last."""
        rig = self.rig
        made = {}
        self.set_pose(rest_at(0.0))
        bpy.context.view_layer.update()
        prev = {pb.name: pb.rotation_quaternion.copy() for pb in rig.pose.bones}
        for name, pose_at, frames, cyclic in once_clips(self):
            act = bpy.data.actions.new(PREFIX + name)
            act.use_fake_user = True
            rig.animation_data.action = act
            for frame in range(frames + 1):
                pose_at(frame / frames)
                for pb in rig.pose.bones:
                    q = pb.rotation_quaternion
                    if q.dot(prev[pb.name]) < 0.0:
                        pb.rotation_quaternion = -q
                    prev[pb.name] = pb.rotation_quaternion.copy()
                    pb.keyframe_insert("rotation_quaternion", frame=frame, group=pb.name)
                    pb.keyframe_insert("location", frame=frame, group=pb.name)
                    pb.keyframe_insert("scale", frame=frame, group=pb.name)
            for fc in action_fcurves(act):
                for kp in fc.keyframe_points:
                    kp.interpolation = "LINEAR"
                fc.update()
            act.use_frame_range = True
            act.frame_start, act.frame_end = 0, frames
            act.use_cyclic = cyclic
            made[name] = act
            rig.animation_data.action = None
        return made

    def build_all(self):
        for step in self.STEPS:
            getattr(self, "rig_up" if step == "rig" else step)()


def action_fcurves(act):
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


def parts_of(mesh):
    """The mesh's separate pieces: (vertex indices, polygon indices, closed?,
    boundary vertex indices), found once from the rest mesh."""
    adj = {}
    for e in mesh.edges:
        a, b = e.vertices
        adj.setdefault(a, []).append(b)
        adj.setdefault(b, []).append(a)
    seen, parts = set(), []
    for start in range(len(mesh.vertices)):
        if start in seen:
            continue
        stack, comp = [start], set()
        seen.add(start)
        while stack:
            x = stack.pop()
            comp.add(x)
            for y in adj.get(x, ()):
                if y not in seen:
                    seen.add(y)
                    stack.append(y)
        parts.append(comp)
    edge_faces = {}
    for poly in mesh.polygons:
        for ek in poly.edge_keys:
            edge_faces[ek] = edge_faces.get(ek, 0) + 1
    out = []
    for comp in parts:
        polys = [poly.index for poly in mesh.polygons if poly.vertices[0] in comp]
        border = {x for ek, n in edge_faces.items() if n == 1 and ek[0] in comp for x in ek}
        out.append((sorted(comp), polys, not border, sorted(border)))
    return out


def check_frames():
    """The frames check() samples in each clip: every 6th through the loop
    and the look, every 3rd through standing up and lying down, every frame
    of the walk, both ends always."""
    return {
        "Rest": list(range(0, REST_FRAMES + 1, 6)),
        "StandUp": list(range(0, STAND_FRAMES + 1, 3)),
        "LookAround": list(range(0, LOOK_FRAMES + 1, 6)),
        "LieDown": list(range(0, DOWN_FRAMES + 1, 3)),
        "Walk": list(range(0, WALK_FRAMES + 1)),
    }


def check(b, frames=None, bind=False, clip="Rest"):
    """Check the fox (in a clip, or with bind, the standing pose) holds
    together through the clip: at each frame, every
    open end of a piece (the top of a leg, the ends of the neck) is inside a
    closed piece, and every piece touches the rest, so nothing comes apart or
    floats. Returns the problems found and, per frame, the lowest point of
    the head and of the whole fox (for resting on the ground)."""
    rig, obj = b.rig, b.obj
    if bind:  # the standing pose the mesh was built in
        rig.animation_data.action = None
        b.set_pose({})
        frames = ["bind"]
    else:
        rig.animation_data.action = bpy.data.actions[PREFIX + clip]
    pieces = parts_of(obj.data)
    head_group = obj.vertex_groups["head"].index
    head_verts = {vx.index for vx in obj.data.vertices
                  if any(g.group == head_group and g.weight > 0.5 for g in vx.groups)}
    frames = frames if frames is not None else check_frames()[clip]
    problems, ground = [], {}
    scene = bpy.context.scene
    for frame in frames:
        if not bind:
            scene.frame_set(frame)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = obj.evaluated_get(dg)
        me = ev.to_mesh()
        pts = [obj.matrix_world @ vx.co for vx in me.vertices]
        trees = []
        for verts, polys, closed, border in pieces:
            trees.append(BVHTree.FromPolygons(pts, [me.polygons[i].vertices for i in polys]))

        def inside(p, k):
            # Odd crossings of a closed piece along a ray: inside it.
            hits, o, d = 0, p.copy(), Vector((0.3, 0.2, 0.93)).normalized()
            for _ in range(32):
                hit, _, _, dist = trees[k].ray_cast(o, d)
                if hit is None:
                    break
                hits += 1
                o = hit + d * 1e-5
            return hits % 2 == 1

        closed = [k for k, piece in enumerate(pieces) if piece[2]]
        for k, (verts, polys, is_closed, border) in enumerate(pieces):
            loose = [x for x in border if not any(inside(pts[x], j) for j in closed if j != k)]
            if loose:
                c = pts[loose[0]]
                problems.append(f"{clip} frame {frame}: open end of piece {k} showing at "
                                f"({c.x:.3f}, {c.y:.3f}, {c.z:.3f}), {len(loose)} verts")
        # Pieces that touch (overlap, or one inside another) are joined.
        links = {k: set() for k in range(len(pieces))}
        for i in range(len(pieces)):
            for j in range(i + 1, len(pieces)):
                if trees[i].overlap(trees[j]) or \
                        (pieces[j][2] and inside(pts[pieces[i][0][0]], j)) or \
                        (pieces[i][2] and inside(pts[pieces[j][0][0]], i)):
                    links[i].add(j)
                    links[j].add(i)
        reach, stack = {0}, [0]
        while stack:
            for j in links[stack.pop()]:
                if j not in reach:
                    reach.add(j)
                    stack.append(j)
        for k in range(len(pieces)):
            if k not in reach:
                c = sum((pts[x] for x in pieces[k][0]), Vector()) / len(pieces[k][0])
                problems.append(f"{clip} frame {frame}: piece {k} floats free near ({c.x:.3f}, {c.y:.3f}, {c.z:.3f})")
        ground[frame] = (min(pts[x].z for x in head_verts), min(q.z for q in pts))
        ev.to_mesh_clear()
    return problems, ground


MAX_KEY_TURN = math.radians(45)  # the most any joint may turn from one key to the next
MAX_SINK = 0.01  # how far below the ground any vertex may go between keys (m)


def check_motion(b, clip):
    """Check a clip between its keys, as the engine plays it (each joint's
    local rotation slerped the short way, translation and scale lerped): no
    joint turns more than MAX_KEY_TURN from one key to the next (a flip,
    which the game would draw as a pop or a limb swinging through the body),
    and halfway between every pair of keys no vertex is more than MAX_SINK
    below the ground. Returns the problems found, and the largest turn
    (degrees, joint, key) and lowest half-key point (m, key)."""
    rig, scene = b.rig, bpy.context.scene
    act = bpy.data.actions[PREFIX + clip]
    rig.animation_data.action = act
    first, last = (int(round(f)) for f in act.frame_range)
    keys = []
    for frame in range(first, last + 1):
        scene.frame_set(frame)
        keys.append(b.snapshot())
    rig.animation_data.action = None
    problems, worst_turn, worst_low = [], (0.0, "", 0), (1.0, 0.0)
    for k in range(len(keys) - 1):
        a, c = keys[k], keys[k + 1]
        mid = {}
        for name in a:
            q0, l0, s0 = a[name]
            q1, l1, s1 = c[name]
            q1 = -q1 if q0.dot(q1) < 0 else q1
            turn = q0.rotation_difference(q1).angle
            if turn > worst_turn[0]:
                worst_turn = (turn, name, first + k)
            if turn > MAX_KEY_TURN:
                problems.append(f"{clip} keys {first + k}-{first + k + 1}: {name} turns "
                                f"{math.degrees(turn):.1f} degrees")
            mid[name] = (q0.slerp(q1, 0.5), l0.lerp(l1, 0.5), s0.lerp(s1, 0.5))
        b.apply(mid)
        low = b.lowest()
        if low < worst_low[0]:
            worst_low = (low, first + k + 0.5)
        if low < -MAX_SINK:
            problems.append(f"{clip} key {first + k + 0.5}: {-low * 1000:.0f} mm below the ground")
    rig.animation_data.action = act
    return problems, (math.degrees(worst_turn[0]), worst_turn[1], worst_turn[2]), worst_low


WALK_SPEED_TOL = 0.03  # how far a stance paw's speed may stray from WALK_SPEED (fraction)
WALK_SLIP = 0.001  # how far a planted paw may stray from a point fixed on the ground (m)
WALK_TRACK_TOL = 0.001  # how far a stance paw may miss its track at a key (m)


def keys_of(b, clip):
    """A clip's keys: each bone's local (rotation, location, scale)."""
    rig, scene = b.rig, bpy.context.scene
    act = bpy.data.actions[PREFIX + clip]
    rig.animation_data.action = act
    first, last = (int(round(f)) for f in act.frame_range)
    keys = []
    for frame in range(first, last + 1):
        scene.frame_set(frame)
        keys.append(b.snapshot())
    rig.animation_data.action = None
    return keys


def between(keys, t):
    """The pose `t` keys in, as the engine samples it: local rotations
    slerped the short way, translation and scale lerped."""
    k = min(int(math.floor(t)), len(keys) - 2)
    f = t - k
    a, c = keys[k], keys[k + 1]
    out = {}
    for name in a:
        q0, l0, s0 = a[name]
        q1, l1, s1 = c[name]
        q1 = -q1 if q0.dot(q1) < 0 else q1
        out[name] = (q0.slerp(q1, f), l0.lerp(l1, f), s0.lerp(s1, f))
    return out


def check_walk(b, rate=240):
    """Check Fox_Walk as the engine plays it, sampled `rate` times a second
    (rotations slerped between keys):
      - while a paw is on the ground its contact point moves back at
        WALK_SPEED, within WALK_SPEED_TOL; it strays no more than WALK_SLIP
        from a point that moves straight back at exactly WALK_SPEED (a point
        fixed on the ground once the engine moves the fox), and at the keys
        it is on its track (walk_track) within WALK_TRACK_TOL (so the IK
        reached it);
      - every leg bends the way it bends standing: each elbow stands off the
        line from shoulder to forepaw toward the back, each knee off the
        line from hip to hind paw toward the front.
    Returns the problems and a dict of figures for the record."""
    rig = b.rig
    keys = keys_of(b, "Walk")
    n = len(keys) - 1
    fwd = Matrix.Rotation(STAND["face"], 3, Z) @ FWD
    stand_keys = keys_of(b, "StandUp")
    b.apply(stand_keys[-1])
    stand_planted = b.legs_planted()
    stand_contact = {paw: stand_planted[paw][0] @ b.walk_contact[paw] for paw in WALK_FOOTFALL}
    problems = []
    figs = {"speed": {}, "track": 0.0, "bend": {}, "ahead": {}, "slip": {}}

    def contacts():
        return {paw: rig.pose.bones[paw].matrix @ b.walk_contact[paw] for paw in WALK_FOOTFALL}

    def bends():
        out = {}
        for leg in LEGS:
            upper, lower, paw = (rig.pose.bones[x] for x in leg)
            line = (paw.head - upper.head).normalized()
            off = lower.head - upper.head
            off -= line * off.dot(line)
            pole = upper.parent.matrix.to_3x3() @ stand_planted[leg[2]][1]
            pole -= line * pole.dot(line)
            # How far the joint stands off the line the standing way (m), and
            # how far it is ahead of the paw joint along the heading (m).
            out[leg[2]] = (off.dot(pole.normalized()), (lower.head - paw.head).dot(fwd))
        return out

    # Keys: on the track while planted.
    for k in range(n + 1):
        b.apply(keys[k])
        got = contacts()
        g = (k / n + WALK_START) % 1.0
        for paw in WALK_FOOTFALL:
            x, h, th = walk_track(paw, g)
            if (g - WALK_FOOTFALL[paw]) % 1.0 <= WALK_DUTY + 1e-9:
                err = (got[paw] - (stand_contact[paw] + fwd * x + Z * h)).length
                figs["track"] = max(figs["track"], err)
                if err > WALK_TRACK_TOL:
                    problems.append(f"Walk key {k}: {paw} {err * 1000:.1f} mm off its track")
    # Sampled as the engine plays it.
    steps = int(round(n / FPS * rate))
    prev = None
    speeds = {paw: [] for paw in WALK_FOOTFALL}
    for i in range(steps + 1):
        t = i / rate
        b.apply(between(keys, t * FPS))
        now = contacts()
        for paw, (off, ahead) in bends().items():
            lo = figs["bend"].get(paw, 1.0)
            figs["bend"][paw] = min(lo, off)
            span = figs["ahead"].get(paw, (1.0, -1.0))
            figs["ahead"][paw] = (min(span[0], ahead), max(span[1], ahead))
            if off <= 0.0:
                problems.append(f"Walk {t:.4f} s: {paw}'s leg bends the wrong way")
        g = (i / steps + WALK_START) % 1.0
        for paw in WALK_FOOTFALL:
            phi = (g - WALK_FOOTFALL[paw]) % 1.0
            if phi <= WALK_DUTY:
                x, h, th = walk_track(paw, g)
                slip = (now[paw] - (stand_contact[paw] + fwd * x)).length
                figs["slip"][paw] = max(figs["slip"].get(paw, 0.0), slip)
                if slip > WALK_SLIP:
                    problems.append(f"Walk {t:.4f} s: {paw} {slip * 1000:.1f} mm off the ground's point")
        if prev is not None:
            g0 = ((i - 1) / steps + WALK_START) % 1.0
            g1 = (i / steps + WALK_START) % 1.0
            for paw in WALK_FOOTFALL:
                p0 = (g0 - WALK_FOOTFALL[paw]) % 1.0
                p1 = (g1 - WALK_FOOTFALL[paw]) % 1.0
                if p0 < WALK_DUTY and 0 < p1 <= WALK_DUTY and p1 > p0:  # on the ground throughout
                    d = (now[paw] - prev[paw]) * rate
                    speeds[paw].append((-d.dot(fwd), (d - fwd * d.dot(fwd)).length))
        prev = now
    for paw, sp in speeds.items():
        back = [a for a, _ in sp]
        cross = max(c for _, c in sp)
        figs["speed"][paw] = (min(back), max(back), cross, len(sp))
        if min(back) < WALK_SPEED * (1 - WALK_SPEED_TOL) or max(back) > WALK_SPEED * (1 + WALK_SPEED_TOL):
            problems.append(f"Walk: {paw} planted moves back at {min(back):.4f}..{max(back):.4f} m/s, "
                            f"not {WALK_SPEED:.4f}")
    rig.animation_data.action = bpy.data.actions[PREFIX + "Walk"]
    return problems, figs


# --------------------------------------------------------------------------
# Clips: (name, pose_at(u) for u in [0, 1), length in frames, key step).

# Curled asleep, as a red fox sleeps: a tight round ball. It lies on its
# right side with the body bent round to the left in a ring, so its belly and
# folded legs face the middle. The head lies on the ground at the end of the
# ring, chin and cheek down, the nose against the root of the brush. The brush
# leaves the rump the other way and wraps round the outside, across the face
# and round the front of the chest.
#
# The ring is laid out by curl_aims: each bone of the back, neck and head is
# aimed along a chord of a circle (radius `ring`) that starts at the rump,
# with the back tilted outward by `tilt`; the neck and head dip toward the
# ground; the brush follows a wider circle (radius `brush`) the other way
# round from the rump.

REAR = 0.115  # how far the rump reaches behind the hips joint


def curl_aims(ring, tilt, neck_dip, head_dip, head_turn, head_tilt, brush, widen, tail_dips):
    """Aims (see set_pose) for the curled pose, in the fox's own frame."""
    def length(name):
        h, t, _ = BONES[name]
        return (t - h).length

    def flat(a):  # the heading after turning `a` to the left from straight ahead (-y)
        return v(math.sin(a), -math.cos(a), 0.0)

    def tipped(d, pitch):
        return (d * math.cos(pitch) + Z * math.sin(pitch)).normalized()

    aims = {}
    phi = 0.0  # round the ring from the rump's end, which sits at its west, heading -y
    chain = [("hips", 0.0), ("spine.1", 0.0), ("spine.2", 0.0), ("chest", 0.0), ("neck", -neck_dip), ("head", -head_dip)]
    for name, pitch in chain:
        # The rump is rigid with the hips, so the hips span the rump too.
        # Each bone is a true chord, so its ends lie on the ring.
        ln = length(name) + (REAR if name == "hips" else 0.0)
        step = 2 * math.asin(min(1.0, ln / (2 * ring)))
        mid = phi + step / 2
        if name == "hips":
            hips_dir, out_rump = flat(mid), v(-1.0, 0.0, 0.0)
        if name == "head":
            mid += head_turn
        d = tipped(flat(mid), pitch)
        out = v(-math.cos(mid), -math.sin(mid), 0.0)  # away from the middle of the ring
        lean = head_tilt if name == "head" else tilt
        up = Z * math.cos(lean) + out * math.sin(lean)
        aims[name] = (tuple(round(c, 5) for c in d), tuple(round(c, 5) for c in up))
        phi += step
    # The brush: from the rump round the outside of the ring the other way
    # (across the face, then the front of the chest), on a spiral about the
    # ring's middle that widens from the brush's root to `brush` within
    # `widen` radians. Each bone is a chord of it.
    # The brush's root: 0.04 m along the hips from the rump's end, on the back
    # (0.025 m up the tilted spine). The ring's middle is `ring` east of the
    # rump's end.
    root = hips_dir * (REAR - 0.075) + (Z * math.cos(tilt) + out_rump * math.sin(tilt)) * 0.025
    centre = v(ring, 0.0, 0.0) - root
    centre.z = 0.0
    r0 = centre.length
    start_ang = math.atan2(-centre.y, -centre.x)

    def spiral(a):  # radians travelled round from the rump
        r = r0 + (brush - r0) * min(1.0, a / widen)
        ang = start_ang - a  # clockwise, seen from above
        return centre + v(math.cos(ang) * r, math.sin(ang) * r, 0.0)

    here, a = spiral(0.0), 0.0
    for i in range(5):
        ln = length(f"tail.{i + 1}")
        while (spiral(a) - here).length < ln and a < 2 * math.pi:
            a += 0.002
        there = spiral(a)
        d = tipped((there - here).normalized(), tail_dips[i])
        aims[f"tail.{i + 1}"] = tuple(round(c, 5) for c in d)
        here = there
    return aims


RING = dict(ring=0.125, tilt=1.3, neck_dip=0.15, head_dip=0.0, head_turn=0.1, head_tilt=0.75, brush=0.2, widen=1.0,
            tail_dips=[-0.45, -0.1, 0.0, 0.05, 0.1])

# face, cx and cy turn and move the ball so the head, with the brush curled
# round in front of it, is at the front (-y), and the ball is centred.
CURL = dict(
    face=1.2122, cx=0.2415, cy=-0.1084, drop=0.22, sway=0.0, shift=0.0,
    foreL=1.5, elbowL=2.0, wristL=-1.0, tuckL=0.7, foreR=1.5, elbowR=2.0, wristR=-1.0, tuckR=0.6,
    thighL=1.0, kneeL=1.9, hockL=1.0, hipTuckL=0.5, thighR=1.0, kneeR=1.9, hockR=1.0,
    earL=0.9, earR=0.9,
    aim=curl_aims(**RING),
)


def smooth(x):
    x = clamp01(x)
    return x * x * (3 - 2 * x)


def breath_at(u):
    """One slow breath per loop: in over 40%, out over 50%, a pause. Eased
    at every turn, so it has no corners, and back to 0 at the loop."""
    if u < 0.4:
        return smooth(u / 0.4)
    if u < 0.9:
        return 1.0 - smooth((u - 0.4) / 0.5)
    return 0.0


def pulse(u, at, width):
    """A smooth bump (0..1..0) centred on `at`, `width` wide, 0 elsewhere."""
    d = abs(u - at) / (width / 2)
    return 0.5 + 0.5 * math.cos(math.pi * d) if d < 1 else 0.0


def rest_at(u):
    b = breath_at(u)
    p = dict(CURL)
    p["breath"] = b
    # Two quick flicks of an ear, up off the head, about two-thirds through.
    p["earL"] = CURL["earL"] - 0.45 * pulse(u, 0.62, 0.05) - 0.3 * pulse(u, 0.69, 0.045)
    # The tip of the brush stirs, once round the loop.
    p["tailTip"] = 0.15 * math.sin(2 * math.pi * u)
    return p


def clips():
    """The looping clips: (name, pose_at(u), frames, key step)."""
    return [("Rest", rest_at, REST_FRAMES, 2)]


# --------------------------------------------------------------------------
# Waking: Fox_StandUp, Fox_LookAround, Fox_LieDown, strung together in the
# game as Rest -> StandUp -> LookAround -> LieDown -> Rest. StandUp starts on
# Fox_Rest's first pose and LieDown ends on it; LookAround starts and ends
# on StandUp's last pose (STAND).
#
# Standing, the fox has turned a quarter turn to its left from the curl's
# facing (root face = 90 degrees), so it stands side-on to whatever the curl
# faces (the fire, in the game), with its right side toward it: the way its
# spine and head already point in the curl, so it barely turns as it rises.
# Its tail is toward its old right. cx, cy centre the standing fox over the
# origin along its length and move it a little toward the old front.

STAND_FRAMES = 45  # Fox_StandUp: 1.5 s
LOOK_FRAMES = 120  # Fox_LookAround: 4 s
DOWN_FRAMES = 60   # Fox_LieDown: 2 s

STAND = dict(face=math.pi / 2, cx=0.08, cy=-0.05, earL=-0.12, earR=-0.12,
             tailPitch=[-0.12, -0.04, 0.0, 0.03, 0.05])

# Waking in the curl: the ring loosened into a gentle arc, still half on its
# side, the head lifted off the ground and the brush drawn back from the
# face. Laid out like the curl (curl_aims), so the curl eases into it
# without the head or chest passing through the ground.
UNCURL = dict(
    face=1.35, cx=0.2, cy=-0.13, drop=0.2,
    foreL=1.2, elbowL=2.0, wristL=-0.8, tuckL=0.4, foreR=1.2, elbowR=2.0, wristR=-0.8, tuckR=0.3,
    thighL=1.0, kneeL=1.9, hockL=1.0, hipTuckL=0.5, thighR=1.0, kneeR=1.9, hockR=1.0,
    earL=0.5, earR=0.5,
    aim=curl_aims(ring=0.4, tilt=0.9, neck_dip=-0.45, head_dip=0.05, head_turn=0.0, head_tilt=0.3, brush=0.36,
                  widen=1.0, tail_dips=[-0.5, -0.15, 0.0, 0.0, 0.05]),
)

# Lying on its belly, head up, legs folded under it: on the way between the
# curl and standing. The hocks rest on the ground about 0.1 m from the hips
# (not folded up against them), so the hind legs step out cleanly as the
# paws are planted. The brush lies on the ground round its right side.
LIE = dict(face=math.pi / 2, cx=0.07, cy=-0.05, drop=0.2,
           foreL=1.3, elbowL=2.2, wristL=-0.6, foreR=1.3, elbowR=2.2, wristR=-0.6,
           thighL=1.2, kneeL=1.3, hockL=1.2, thighR=1.2, kneeR=1.3, hockR=1.2,
           neckDip=0.1, earL=0.3, earR=0.3,
           aim={"tail.1": (0.0, 1.0, -0.6), "tail.2": (-0.2, 1.0, -0.35), "tail.3": (-0.5, 0.9, 0.0),
                "tail.4": (-0.9, 0.45, 0.0), "tail.5": (-1.0, -0.1, 0.0)})

# Front up on straight legs, the hind end still low: the paws are planted
# (Builder.plant), so the legs here only say which way the joints bend.
CROUCH = dict(face=math.pi / 2, cx=0.08, cy=-0.05, drop=0.12, pitch=0.2,
              thighL=0.6, kneeL=1.2, hockL=0.6, thighR=0.6, kneeR=1.2, hockR=0.6,
              earL=-0.05, earR=-0.05,
              aim={"tail.1": (0.0, 1.0, -0.45), "tail.2": (0.0, 1.0, -0.25), "tail.3": (-0.1, 1.0, -0.1),
                   "tail.4": (-0.2, 1.0, 0.0), "tail.5": (-0.3, 1.0, 0.05)})


def once_clips(b):
    """The clips built from key poses and planted paws: (name, pose(u)
    setting the rig, frames, loops?). The three waking clips play once;
    Fox_Walk loops (its last key is its first)."""
    curl = b.pose_of(rest_at(0.0))
    uncurl = b.pose_of(UNCURL)
    lie = b.pose_of(LIE)
    crouch = b.pose_of(CROUCH)
    stand = b.pose_of(STAND)
    planted = b.legs_planted()  # where the standing paws stand, and how the legs bend

    def wake(keys, times, fore, hind):
        def pose(u):
            b.apply(b.blend_keys(keys, times, u))
            for k, leg in enumerate(LEGS):
                b.plant(leg, planted[leg[2]], (fore if k < 2 else hind)(u))
            # Between the keys, a limb on the underside can pass a little
            # into the ground as the body rolls; lift the fox clear of it
            # (and plant the paws again), so it heaves up a touch instead.
            low = b.lowest()
            if low < -GROUND_SLACK:
                root = b.rig.pose.bones["root"]
                root.location = root.location + b.rest_rot("root").inverted() @ v(0, 0, -low)
                bpy.context.view_layer.update()
                for k, leg in enumerate(LEGS):
                    b.plant(leg, planted[leg[2]], (fore if k < 2 else hind)(u))
        return pose

    def ramp(a, c):  # 0 before a, easing to 1 at c
        return lambda u: smooth((u - a) / (c - a))

    # Standing up: the head lifts out from under the brush and the ring
    # loosens; it swings round onto its belly, turning; the front end rises
    # onto the front paws, then the hind end. Each paw is planted where it
    # stands once it reaches the ground, and stays there.
    up = wake([curl, uncurl, lie, crouch, stand],
              dict(root=[0.0, 0.2, 0.42, 0.44, 0.46], body=[0.0, 0.22, 0.42, 0.68, 0.92],
                   head=[0.0, 0.18, 0.4, 0.66, 0.9], tail=[0.03, 0.27, 0.5, 0.74, 0.96],
                   fore=[0.0, 0.2, 0.38, 0.6, 0.62], hind=[0.0, 0.2, 0.42, 0.66, 0.92]),
              ramp(0.36, 0.56), ramp(0.44, 0.66))

    def look(u):
        b.set_pose(look_at(u))
        bpy.context.view_layer.update()

    # Lying down: the hind end sinks with the paws still planted, then the
    # front folds and it lies on its belly; it rolls onto its side, curls
    # round and tucks its head under the brush.
    down = wake([stand, crouch, lie, uncurl, curl],
                dict(root=[0.0, 0.0, 0.45, 0.62, 0.9], body=[0.0, 0.22, 0.5, 0.72, 0.95],
                     head=[0.0, 0.25, 0.5, 0.75, 1.0], tail=[0.0, 0.25, 0.52, 0.76, 1.0],
                     fore=[0.0, 0.2, 0.47, 0.7, 0.92], hind=[0.0, 0.22, 0.47, 0.7, 0.92]),
                lambda u: 1.0 - ramp(0.26, 0.46)(u), lambda u: 1.0 - ramp(0.24, 0.45)(u))
    walk, b.walk_contact = walk_clip(b, planted)
    return [("StandUp", up, STAND_FRAMES, False), ("LookAround", look, LOOK_FRAMES, False),
            ("LieDown", down, DOWN_FRAMES, False), ("Walk", walk, WALK_FRAMES, True)]


def plateau(u, a, b, ramp):
    """0 outside [a, b], easing up to 1 over `ramp` inside each end."""
    return smooth((u - a) / ramp) * smooth((b - u) / ramp)


def look_at(u):
    """Looking round, standing: ears up, the head turned left (away from
    its right side, where the fire is in the game), then right toward the
    fire, then a sniff at the ground; the tail low, swaying gently. Every
    offset is 0 at both ends, so it starts and ends exactly on STAND."""
    p = dict(STAND)
    left = plateau(u, 0.06, 0.4, 0.1)
    right = plateau(u, 0.42, 0.74, 0.1)
    turn = 0.8 * left - 0.85 * right  # radians, neck and head together
    p["neckBend"] = 0.55 * turn
    p["headBend"] = 0.45 * turn
    up = plateau(u, 0.04, 0.76, 0.08)
    sniff = plateau(u, 0.78, 0.97, 0.06)
    nod = 0.06 * math.sin(2 * math.pi * (u - 0.78) / 0.05) * sniff
    p["neckDip"] = -0.12 * up + 0.35 * sniff
    p["headDip"] = 0.15 * sniff + nod
    p["headRoll"] = -0.12 * turn
    alert = plateau(u, 0.02, 0.98, 0.06)
    p["earL"] = STAND["earL"] - 0.1 * alert + 0.25 * right
    p["earR"] = STAND["earR"] - 0.1 * alert + 0.25 * left
    sway = math.sin(2 * math.pi * 2 * u) * plateau(u, 0.0, 1.0, 0.2)
    p["tailCurl"] = [0.04 * sway, 0.07 * sway, 0.09 * sway, 0.1 * sway, 0.1 * sway]
    return p


# --------------------------------------------------------------------------
# Walking: Fox_Walk, an unhurried four-beat lateral-sequence walk, in place,
# looping. One clip is one gait cycle: each paw steps once (left hind, left
# fore, right hind, right fore, a quarter cycle apart), so it holds two
# strides, a left and a right. It stands turned as STAND (the quarter turn
# left on root), so it walks toward the standing facing (model +x in
# Blender, +X in the engine), and the root doesn't move: the engine moves
# the fox. While a paw is on the ground its contact point (the front of the
# pad, WALK_CONTACT) moves straight back at WALK_SPEED, so with the fox
# moving forward at that speed the paw stays put on the ground. Late in
# each stance the heel lifts and the paw rolls over that point.

WALK_FRAMES = 36            # Fox_Walk: 1.2 s
WALK_DUTY = 24 / WALK_FRAMES  # the fraction of the cycle each paw is on the ground (lift-off on a key)
WALK_REACH = 0.27           # how far a planted paw travels back, touchdown to lift-off (m)
WALK_STRIDE = WALK_REACH / WALK_DUTY  # how far the fox goes per cycle (m)
WALK_SPEED = WALK_STRIDE / (WALK_FRAMES / FPS)  # m/s, real size
# When each paw touches down, as a fraction of the cycle.
WALK_FOOTFALL = {"hindpaw.L": 0.0, "forepaw.L": 0.25, "hindpaw.R": 0.5, "forepaw.R": 0.75}
WALK_START = 0.25           # the clip starts on the left forepaw's contact, its pose nearest standing
# Per leg kind: centre of the paw's reach ahead of where it stands (m); how
# high the pad's front lifts in the swing (m); the fraction of the stance
# after which the heel lifts, and how far (radians) by lift-off; how far
# the paw folds in the swing; how far the shoulder or hip slides forward
# with the paw (m, at the ends of the reach).
WALK_LEG = {
    "fore": dict(centre=-0.055, lift=0.035, heel_off=0.35, heel=0.95, fold=1.9, slide=0.012),
    "hind": dict(centre=-0.03, lift=0.03, heel_off=0.5, heel=0.55, fold=0.9, slide=0.0),
}
# The contact point: the front of the pad, from the paw bone's tail (m,
# forward and down, in the bind pose).
WALK_CONTACT = {"fore": (0.012, 0.008), "hind": (0.014, 0.008)}


def leg_kind(paw):
    return "fore" if paw.startswith("forepaw") else "hind"


def walk_track(paw, g):
    """Where a paw is at gait phase g: (forward offset of its contact point
    from where it stands, height of the contact point, heel lift in
    radians: positive turns the toes down about the contact point)."""
    P, E, beta = WALK_LEG[leg_kind(paw)], WALK_REACH, WALK_DUTY
    phi = (g - WALK_FOOTFALL[paw]) % 1.0
    if phi < beta:  # stance: straight back at constant speed
        f = phi / beta
        return P["centre"] + E / 2 - E * f, 0.0, P["heel"] * smooth((f - P["heel_off"]) / (1 - P["heel_off"]))
    s = (phi - beta) / (1 - beta)  # swing: forward, lifted and folded
    x = P["centre"] - E / 2 + E * smooth(s)
    h = P["lift"] * math.sin(math.pi * s)
    if s < 0.35:
        th = P["heel"] + (P["fold"] - P["heel"]) * smooth(s / 0.35)
    else:
        th = P["fold"] * (1 - smooth((s - 0.35) / 0.45))
    return x, h, th


def walk_body(g):
    """The body's pose at gait phase g: STAND, a little lower, with a small
    bob twice a cycle, the hips swaying over the hind paw on the ground,
    the back flexing a touch side to side, the head carried low and level
    with a small nod, ears up, the brush held out low behind and swaying."""
    p = dict(STAND)
    two = 4 * math.pi
    p["drop"] = 0.015 - 0.004 * math.cos(two * (g - WALK_DUTY / 2))
    p["pitch"] = 0.01 * math.cos(two * (g - 0.25 - WALK_DUTY / 2))
    p["sway"] = 0.006 * math.cos(2 * math.pi * (g - WALK_DUTY / 2))
    bend = 0.02 * math.sin(2 * math.pi * (g - WALK_DUTY / 2))
    p["bend"] = bend
    p["neckBend"] = -2.0 * bend
    p["headBend"] = -1.0 * bend
    nod = math.cos(two * (g - 0.25 - WALK_DUTY / 2))
    p["neckDip"] = 0.2 + 0.03 * nod
    p["headDip"] = -0.15 - 0.02 * nod
    p["earL"] = p["earR"] = -0.15
    sway = math.sin(2 * math.pi * (g - WALK_DUTY / 2 - 0.15))
    p["tailCurl"] = [0.03 * sway, 0.05 * sway, 0.07 * sway, 0.08 * sway, 0.08 * sway]
    lift = 0.02 * math.cos(two * (g - WALK_DUTY / 2 - 0.1))
    p["tailPitch"] = [-0.08 + lift, -0.02 + lift, 0.0, 0.03, 0.05]
    return p


def walk_clip(b, planted):
    """Fox_Walk's pose(u), from the standing paws (legs_planted): the body
    from walk_body, each paw planted (Builder.plant) on its track
    (walk_track). Also returns the paws' contact points in their bones'
    rest frames."""
    rig = b.rig
    fwd = Matrix.Rotation(STAND["face"], 3, Z) @ FWD  # the standing facing
    side = Z.cross(fwd)  # the fox's left: heel lift turns about it
    stand, contact = {}, {}
    for leg in LEGS:
        paw = leg[2]
        h, t, _ = BONES[paw]
        ahead, down = WALK_CONTACT[leg_kind(paw)]
        local = rig.data.bones[paw].matrix_local.inverted() @ (t + FWD * ahead - Z * down)
        contact[paw] = local
        stand[paw] = planted[paw][0] @ local  # where it touches the ground, standing

    def pose(u):
        g = (u + WALK_START) % 1.0
        b.set_pose(walk_body(g))
        tracks = {leg[2]: walk_track(leg[2], g) for leg in LEGS}
        for leg in LEGS:
            P = WALK_LEG[leg_kind(leg[2])]
            if P["slide"]:
                x = (tracks[leg[2]][0] - P["centre"]) / (WALK_REACH / 2)
                pb = rig.pose.bones[leg[0]]
                pb.location = b.rest_rot(leg[0]).inverted() @ (FWD * (P["slide"] * x))
        bpy.context.view_layer.update()
        for leg in LEGS:
            paw = leg[2]
            x, hgt, th = tracks[paw]
            S, T0 = planted[paw][0], stand[paw]
            T = T0 + fwd * x + Z * hgt
            m = Matrix.Translation(T) @ Matrix.Rotation(th, 4, side) @ Matrix.Translation(-T0) @ S
            b.plant(leg, (m, planted[paw][1]), 1.0)
    return pose, contact


# --------------------------------------------------------------------------
# Headless: build, preview, export


# The ruins' default view of the fox at its spot (game::kRuinsFoxSpot, facing
# the campfire), as a preview yaw (degrees from the fox's front, toward its
# right): the camera looks toward the world's -z, which is from the fox's
# back-right once it stands.
GAME_YAW = -60
# Key poses of the waking clips for fox_wake.png: (clip, frame). Then
# half-key frames where revision 4's leg IK flipped (fox-wake-look REQ-001),
# so the planted legs can be checked between keys too.
WAKE_SHOTS = [("StandUp", 15), ("StandUp", 27), ("StandUp", 45), ("LookAround", 30), ("LookAround", 70),
              ("LookAround", 106), ("LieDown", 24), ("LieDown", 42),
              ("StandUp", 22.5), ("StandUp", 26.5), ("StandUp", 34.5), ("LieDown", 9.5), ("LieDown", 13.5),
              ("LieDown", 22.5)]
# fox_walk.png: the standing pose (StandUp's end, where the walk starts
# from), then Fox_Walk's contacts and passing positions in order: (frame,
# what happens). The clip starts on the left forepaw's contact; each paw
# passes its planted partner halfway through its swing.
WALK_SHOTS = [(0, "LF contact"), (3, "RH passing"), (9, "RH contact"), (12, "RF passing"), (18, "RF contact"),
              (21, "LH passing"), (27, "LH contact"), (30, "LF passing")]
# The ruins' default view of the fox standing at its bed (game::kRuinsFoxSpot,
# 30 degrees down, the standing fox facing 19.4 degrees off straight at the
# camera), as a preview yaw: degrees from the fox's original front toward its
# right.
WALK_GAME_YAW, WALK_GAME_PITCH = 70.6, 30


def render_previews(b):
    """Preview renders into PREVIEW. fox.png puts five side by side: the curl
    as the game sees it (from the front, 50 degrees down), from above, low
    from the face side (the brush across the face, the head on the ground),
    low from the side (the legs folded in, the ball on the ground), and the
    standing bind pose. fox_wake.png shows key poses of the waking clips
    (WAKE_SHOTS) in two rows: as the game's default view first sees them
    (GAME_YAW), and from the side away from the fire. fox_walk.png shows the
    standing pose and the walk's contacts and passing positions
    (WALK_SHOTS), from the side and from the game's view. Each is also saved
    on its own."""
    os.makedirs(PREVIEW, exist_ok=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = 400, 400
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("w")
    world.color = srgb(110, 128, 96)
    scene.world = world
    cam_data = bpy.data.cameras.new("cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    rest = bpy.data.actions[PREFIX + "Rest"]
    shots = []

    def shoot(name, act, frame, yaw_deg, pitch_deg, target, scale):
        b.rig.animation_data.action = act
        if act is None:
            b.set_pose({})
        scene.frame_set(int(frame), subframe=frame - int(frame))
        yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
        d = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), math.sin(pitch)))
        cam_data.ortho_scale = scale
        cam.location = Vector(target) + d * 6
        cam.rotation_euler = (-d).to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(PREVIEW, name + ".png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        shots.append(path)

    def sheet(paths, cols, name):
        """Images (row by row, top first) tiled into PREVIEW/name."""
        w, h = scene.render.resolution_x, scene.render.resolution_y
        rows = (len(paths) + cols - 1) // cols
        img_out = bpy.data.images.new(name, cols * w, rows * h)
        out = [0.0] * (cols * w * rows * h * 4)
        for k, path in enumerate(paths):
            img = bpy.data.images.load(path)
            px = list(img.pixels)
            col, row_img = k % cols, rows - 1 - k // cols  # pixel rows run bottom-up
            for row in range(h):
                src = row * w * 4
                dst = ((row_img * h + row) * cols * w + col * w) * 4
                out[dst:dst + w * 4] = px[src:src + w * 4]
            bpy.data.images.remove(img)
        img_out.pixels = out
        img_out.filepath_raw = os.path.join(PREVIEW, name)
        img_out.file_format = "PNG"
        img_out.save()

    curl = v(0, 0, 0.1)
    shoot("rest_game", rest, 0, 0, 50, curl, 0.6)
    shoot("rest_top", rest, 0, 0, 89.9, curl, 0.6)
    shoot("rest_face_low", rest, 0, -35, 12, curl, 0.6)
    shoot("rest_side_low", rest, 0, 90, 3, curl, 0.6)
    shoot("bind_side", None, 0, 70, 12, v(0, 0.06, 0.3), 1.4)
    for name, frame, yaw, pitch in (("rest_back_low", 0, 180, 5), ("rest_other_side_low", 0, -90, 3),
                                    ("rest_inhale", 48, 0, 50), ("rest_ear_flick", 74, -35, 30)):
        shoot(name, rest, frame, yaw, pitch, curl, 0.6)
    shoot("bind_front", None, 0, 0, 10, v(0, 0, 0.3), 1.4)
    shoot("bind_34", None, 0, 35, 30, v(0, 0.06, 0.3), 1.4)

    # fox.png: the first five side by side.
    sheet(shots[:5], 5, "fox.png")

    # fox_wake.png: key poses of the waking clips, as the game first sees
    # them (top row: the default view, from the fox's back-right, 50 degrees
    # down) and from the side away from the fire (bottom row).
    wake = []
    for name, frame in WAKE_SHOTS:
        act = bpy.data.actions[PREFIX + name]
        n0 = len(shots)
        shoot(f"wake_{name}_{frame}_game", act, frame, GAME_YAW, 50, v(0, 0, 0.25), 1.5)
        shoot(f"wake_{name}_{frame}_side", act, frame, 180, 4, v(0, 0, 0.25), 1.5)
        wake.append(shots[n0:])
    sheet([w[0] for w in wake] + [w[1] for w in wake], len(wake), "fox_wake.png")

    # fox_walk.png: the standing pose, then the walk's key poses (WALK_SHOTS),
    # from the fox's left side (top row) and as the game's default view sees
    # the fox at its bed (bottom row).
    walk = []
    stand_up = bpy.data.actions[PREFIX + "StandUp"]
    walking = bpy.data.actions[PREFIX + "Walk"]
    for act, frame in [(stand_up, STAND_FRAMES)] + [(walking, f) for f, _ in WALK_SHOTS]:
        n0 = len(shots)
        tag = "stand" if act is stand_up else f"{frame}"
        shoot(f"walk_{tag}_side", act, frame, 180, 3, v(0.08, 0, 0.27), 1.35)
        shoot(f"walk_{tag}_game", act, frame, WALK_GAME_YAW, WALK_GAME_PITCH, v(0.08, 0, 0.25), 1.35)
        walk.append(shots[n0:])
    sheet([w[0] for w in walk] + [w[1] for w in walk], len(walk), "fox_walk.png")
    print("wrote previews to", PREVIEW)


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
    print("fox:", len(b.obj.data.vertices), "verts,", tris, "tris,", len(ORDER), "bones")
    problems = check(b, bind=True)[0]
    for clip in check_frames():
        more, ground = check(b, clip=clip)
        problems += more
        low = min(h for h, _ in ground.values()), max(h for h, _ in ground.values())
        lowest = min(f for _, f in ground.values())
        print(f"fox: {clip} checked at {len(ground)} frames; head's underside {low[0]:.3f}..{low[1]:.3f} m "
              f"above the ground, lowest point {lowest:.3f} m")
        more, turn, sink = check_motion(b, clip)
        problems += more
        print(f"fox: {clip} between keys: largest turn {turn[0]:.1f} degrees ({turn[1]}, key {turn[2]}), "
              f"lowest half-key point {sink[0]:.3f} m (key {sink[1]})")
    more, figs = check_walk(b)
    problems += more
    print(f"fox: Walk {WALK_FRAMES / FPS:.3f} s, stride {WALK_STRIDE:.4f} m per cycle, planted paws back at "
          f"{WALK_SPEED:.4f} m/s (real size)")
    for paw, (lo, hi, cross, n) in figs["speed"].items():
        print(f"fox: Walk {paw} planted: back at {lo:.4f}..{hi:.4f} m/s over {n} samples, slip at most "
              f"{figs['slip'][paw] * 1000:.2f} mm; bends the standing way by at least {figs['bend'][paw]:.3f} m; "
              f"elbow/knee {figs['ahead'][paw][0]:+.3f}..{figs['ahead'][paw][1]:+.3f} m ahead of the paw joint")
    if problems:
        raise RuntimeError("the fox comes apart:\n  " + "\n  ".join(problems))
    if PREVIEW:
        render_previews(b)
    b.rig.animation_data.action = None
    b.set_pose({})
    export()


if __name__ == "__main__" and bpy.app.background:
    main()
