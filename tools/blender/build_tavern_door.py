"""Build the Sleeping Fox's door - one plank door, two iron strap hinges and a
ring handle, skinned to a one-joint rig - and export assets/scenes/tavern_door.glb.

Run headless (builds, checks, exports, then renders the preview beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_door.py
    PXE_TAVERN_OUT=/some/dir ... writes the glb and preview there instead.
    PXE_SKIP_PREVIEW=1 ... skips the previews.
    tavern_door_grain.png beside it: the grain at the game's 24 px/m, front
    (from the exterior's default camera) and back (from the room's design
    camera), toon-shaded with the crease ink the task asks for.

Or step by step in an open Blender, to watch it go together:
    import runpy; td = runpy.run_path(".../build_tavern_door.py", run_name="door")
    b = td["Builder"](); b.start(); b.leaf(); b.ironwork(); b.grain(); b.rig_up()
Each step adds its pieces to the Tavern_Door collection, leaving the rest of
the scene alone.

One model serves both doors, the tavern's front door in the ruins and the
inside door in the tavern. It fills the 2.2 x 3.4 m doorway (hero scale,
1.6x real): the leaf is 2.18 m wide, 3.38 m tall and 0.12 m thick, so it
clears the latch jamb and the head as it swings.

Axes (Blender): at rest (closed) the door's front looks along -Y (+Z in the
engine) and the leaf runs from the hinge along +X. The origin is the bottom
of the hinge edge on the back face: the back face is the plane y = 0 and the
leaf's thickness lies in front of it (y from -0.12 to 0). Doors swing into
the building, toward the back face, so the pivot on the back face lets the
leaf turn a full 100 degrees without cutting into its own frame. In the
engine that is a positive turn of `hinge` about +Y: the leaf's free edge goes
from +X toward -Z.

The front (outside) has the plank grooves (revision 2: 0.03 m deep, so
each face turns 45 degrees and takes crease ink) and the ring handle; the two strap
hinges wrap through the leaf and show on both faces, so the inside door seen
from the room (its back) still shows its iron.

Rig: one joint, `hinge`, at the origin, pointing up (+Z in Blender, +Y in
the engine), so its rest rotation is the identity. Every vertex is weighted
fully to it. No clips: code turns the joint.

Revision 2: grain. Each plank carries 2-3 thin raised grain streaks and the
door 2 knots, on both faces (the inside door shows its back), in a third,
darker material, Door_Grain. A streak is a low ridge 0.07 m across its foot
and 0.03 m across its top (0.05 m at mid-height, so it takes a pixel at
24 px/m), raised GRAIN_H off the plank, its two sides turned 45 degrees from
the plank face so the engine's crease ink (normals more than 40 degrees
apart) catches them; it tapers to a point at each end. A knot is a raised
oval boss with the same 45-degree sides. The leaf, ironwork, hinge, origin,
axes and swing are revision 1's; the build checks the grain stays on the
planks, clear of the iron, within GRAIN_H of each face, and clear of the
frame through the whole 100-degree swing.

Material order is the contract with the engine: Door_Wood (0), Door_Iron
(1), then Door_Grain (2), loaded into actor slots 12-14.
"""

import math
import os

import bmesh
import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
OUT_DIR = os.environ.get("PXE_TAVERN_OUT") or os.path.normpath(os.path.join(HERE, "..", "..", "assets", "scenes"))
OUT = os.path.join(OUT_DIR, "tavern_door.glb")
PREVIEW = os.path.join(OUT_DIR, "tavern_door.png")
GRAIN_PREVIEW = os.path.join(OUT_DIR, "tavern_door_grain.png")
COLLECTION = "Tavern_Door"


def srgb(r, g, b):
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return (lin(r), lin(g), lin(b))


# Materials, in the engine's order: (name, linear base colour). Base colours
# are the mid tone; the engine's toon ramp makes light and shade.
MATERIALS = [
    ("Door_Wood", srgb(128, 82, 46)),  # warm brown planks
    ("Door_Iron", srgb(46, 44, 48)),   # dark iron straps and ring
    ("Door_Grain", srgb(92, 58, 34)),  # revision 2: raised grain streaks and knots, darker than the planks
]
M = {name: i for i, (name, _) in enumerate(MATERIALS)}

WIDTH = 2.18     # leaf, hinge edge to latch edge (m)
HEIGHT = 3.38
THICK = 0.12
PLANKS = 3       # wide planks, 0.73 m each (0.45 m real)
GROOVE_HALF = 0.03   # V-groove between planks on the front: half its width
GROOVE_DEPTH = 0.03   # revision 2 (revision 1: 0.025, whose faces turned 39.8 degrees, under the crease test); now 45
# Strap hinges: centre heights, length from the hinge edge, height at the
# hinge and at the tip, and how far they stand off each face.
STRAP_Z = (0.55, 2.83)
STRAP_LEN = 1.55
STRAP_H0, STRAP_H1 = 0.17, 0.10
STRAP_PROUD = 0.015
# Ring handle on the front, by the latch edge, 1.5 m up (0.95 m real).
PLATE = (1.80, 1.96, 1.62, 1.80)  # x0, x1, z0, z1
RING_R_OUT, RING_R_IN = 0.11, 0.08
RING_PROUD = 0.03
TRI_BUDGET = 800  # revision 2 (revision 1: under 100)
# Revision 2: grain. Plank faces (x ranges) between the front's V-grooves;
# the back is flat, but its grain keeps to the same planks.
PLANK_X = [(WIDTH * k / PLANKS + (GROOVE_HALF if k else 0.0),
            WIDTH * (k + 1) / PLANKS - (GROOVE_HALF if k < PLANKS - 1 else 0.0)) for k in range(PLANKS)]
GRAIN_H = 0.02            # raised off the plank face (the task: at most 0.02)
GRAIN_HALF_BASE = 0.035   # a streak is 0.07 m across its foot,
GRAIN_HALF_TOP = 0.015    # 0.03 m across its top: 0.05 m at mid-height; each side turns 45 degrees
GRAIN_TAPER = 0.25        # each end tapers to a point over this length
KNOT_R = (0.10, 0.15)     # a knot's half-axes (x, z) at its foot; its top is GRAIN_H in all round
CREASE_DEG = 40.0         # the engine's crease test: normals more than this apart
TOP_DEG = 10.0            # a grain face within this of the plank face is a top; every other face is a side
# Streaks: (face, plank, x at the foot, z from, z to, sway at the three inner
# stations). Each plank gets 2-3 on each face, kept off the strap hinges
# (z 0.465-0.635 and 2.745-2.915, x < 1.55), the ring handle and the knots.
STREAKS = [
    ("front", 0, 0.15, 0.75, 2.62, (0.0, 0.03, -0.01)),
    ("front", 0, 0.40, 1.45, 2.62, (0.0, -0.02, 0.02)),     # from just above the knot
    ("front", 0, 0.58, 0.72, 1.95, (-0.02, 0.0, 0.02)),     # right of the knot
    ("front", 1, 0.88, 0.72, 2.66, (0.02, -0.02, 0.0)),     # left of the knot
    ("front", 1, 1.12, 0.80, 1.95, (0.0, 0.02, -0.01)),     # up to the knot
    ("front", 1, 1.30, 1.10, 2.66, (-0.02, 0.0, 0.0)),
    ("front", 2, 1.64, 0.72, 2.62, (0.0, -0.02, 0.01)),     # left of the ring handle
    ("front", 2, 1.90, 1.95, 2.66, (0.01, -0.01, 0.0)),     # above it
    ("front", 2, 2.08, 0.10, 1.30, (0.0, -0.01, 0.01)),     # below it
    ("back", 0, 0.18, 0.75, 2.55, (0.0, 0.02, -0.02)),
    ("back", 0, 0.38, 1.00, 2.66, (0.02, -0.01, 0.0)),
    ("back", 0, 0.58, 0.72, 2.20, (-0.01, 0.02, 0.0)),
    ("back", 1, 0.86, 0.72, 2.62, (0.0, 0.02, -0.01)),      # left of the knot
    ("back", 1, 1.08, 1.30, 2.66, (0.0, -0.02, 0.01)),      # from above the knot
    ("back", 1, 1.28, 0.72, 2.10, (0.01, 0.0, -0.02)),
    ("back", 2, 1.58, 0.75, 2.66, (0.0, -0.01, 0.01)),      # left of the knot
    ("back", 2, 1.80, 0.72, 2.00, (0.0, 0.02, 0.0)),        # up to the knot
    ("back", 2, 1.98, 0.90, 2.66, (-0.01, 0.0, 0.01)),
]
KNOTS = [("front", 0, 0.38, 1.20), ("front", 1, 1.10, 2.20), ("back", 1, 1.05, 1.05), ("back", 2, 1.80, 2.25)]
GRAIN_CLEAR = 0.03        # checked: grain keeps this clear of the iron, the plank edges and the leaf's ends
# Revision 1's figures, which revision 2 keeps (checked).
REV1_REACH = -0.3786      # turned 100 degrees, how far the leaf reaches back past the hinge line
SWING_DEG = 100.0
FRAME_CLEAR_X = (0.0, 2.2)  # in the hinge's frame: the opening between the jambs (engine x, from the hinge)


class Builder:
    STEPS = ["start", "leaf", "ironwork", "grain", "rig_up"]

    def __init__(self):
        self.bm = bmesh.new()
        self.coll = None
        self.obj = None
        self.rig = None

    # ---- scene plumbing ---------------------------------------------------

    def start(self):
        """Fresh Tavern_Door collection and materials; the rest of the scene is left alone."""
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

        mesh = bpy.data.meshes.new("Tavern_Door")
        for name, colour in MATERIALS:
            mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
            mat.diffuse_color = (*colour, 1.0)
            if bpy.app.version < (5, 0, 0):
                mat.use_nodes = True
            bsdf = mat.node_tree.nodes.get("Principled BSDF") if mat.node_tree else None
            if bsdf:
                bsdf.inputs["Base Color"].default_value = (*colour, 1.0)
                bsdf.inputs["Roughness"].default_value = 1.0
                bsdf.inputs["Metallic"].default_value = 0.0
                bsdf.inputs["Emission Strength"].default_value = 0.0
            mesh.materials.append(mat)
        self.obj = bpy.data.objects.new("Tavern_Door", mesh)
        self.coll.objects.link(self.obj)
        self.obj.vertex_groups.new(name="hinge")
        self.refresh()

    def refresh(self):
        self.bm.to_mesh(self.obj.data)
        for p in self.obj.data.polygons:
            p.use_smooth = False
        self.obj.data.update()

    # ---- geometry helpers -------------------------------------------------

    def face(self, pts, mat, facing):
        """One flat face; flipped if needed so its normal points along `facing`."""
        vs = [self.bm.verts.new(p) for p in pts]
        f = self.bm.faces.new(vs)
        f.normal_update()
        if f.normal.dot(Vector(facing)) < 0:
            f.normal_flip()
        f.material_index = M[mat]
        f.smooth = False
        return f

    def hexa(self, c, mat, skip=()):
        """Six faces over eight corners: 0-3 the bottom (x0y0, x1y0, x1y1, x0y1),
        4-7 the top in the same order. Faces in `skip` are left out."""
        vs = [self.bm.verts.new(p) for p in c]
        sides = {"-z": (0, 3, 2, 1), "+z": (4, 5, 6, 7), "-y": (0, 1, 5, 4), "+x": (1, 2, 6, 5),
                 "+y": (2, 3, 7, 6), "-x": (3, 0, 4, 7)}
        for key, idx in sides.items():
            if key in skip:
                continue
            f = self.bm.faces.new([vs[i] for i in idx])
            f.material_index = M[mat]
            f.smooth = False

    # ---- the door ---------------------------------------------------------

    def leaf(self):
        """The plank leaf: one slab whose top outline has a V-groove between
        each pair of planks on the front, extruded the door's full height."""
        outline = [(0.0, 0.0), (0.0, -THICK)]
        for k in range(1, PLANKS):
            x = WIDTH * k / PLANKS
            outline += [(x - GROOVE_HALF, -THICK), (x, -THICK + GROOVE_DEPTH), (x + GROOVE_HALF, -THICK)]
        outline += [(WIDTH, -THICK), (WIDTH, 0.0)]  # counter-clockwise seen from above
        bottom = [self.bm.verts.new((x, y, 0.0)) for x, y in outline]
        top = [self.bm.verts.new((x, y, HEIGHT)) for x, y in outline]
        n = len(outline)
        for i in range(n):
            j = (i + 1) % n
            f = self.bm.faces.new((bottom[i], bottom[j], top[j], top[i]))
            f.material_index = M["Door_Wood"]
            f.smooth = False
        for ring, rev in ((top, False), (bottom, True)):
            f = self.bm.faces.new(list(reversed(ring)) if rev else ring)
            f.material_index = M["Door_Wood"]
            f.smooth = False
        self.refresh()

    def ironwork(self):
        """Two tapered strap hinges, each one box through the leaf so it shows
        on both faces, then the ring handle's plate and a hexagonal ring on
        the front."""
        y0, y1 = -THICK - STRAP_PROUD, STRAP_PROUD
        x0, x1 = -0.012, STRAP_LEN  # just past the hinge edge, so the end covers it
        for zc in STRAP_Z:
            a, b = STRAP_H0 / 2, STRAP_H1 / 2
            self.hexa([(x0, y0, zc - a), (x1, y0, zc - b), (x1, y1, zc - b), (x0, y1, zc - a),
                       (x0, y0, zc + a), (x1, y0, zc + b), (x1, y1, zc + b), (x0, y1, zc + a)], "Door_Iron")
        px0, px1, pz0, pz1 = PLATE
        py0, py1 = -THICK - 0.012, -THICK + 0.005
        self.hexa([(px0, py0, pz0), (px1, py0, pz0), (px1, py1, pz0), (px0, py1, pz0),
                   (px0, py0, pz1), (px1, py0, pz1), (px1, py1, pz1), (px0, py1, pz1)], "Door_Iron", skip=("+y",))
        # Ring: flat top against the plate's underside; front face and outer rim.
        cx = (px0 + px1) / 2
        cz = pz0 - RING_R_OUT * math.sin(math.radians(60))
        yf, yb = -THICK - RING_PROUD, -THICK + 0.002
        outer = [(cx + RING_R_OUT * math.cos(math.radians(60 * k)), cz + RING_R_OUT * math.sin(math.radians(60 * k)))
                 for k in range(6)]
        inner = [(cx + RING_R_IN * math.cos(math.radians(60 * k)), cz + RING_R_IN * math.sin(math.radians(60 * k)))
                 for k in range(6)]
        for k in range(6):
            j = (k + 1) % 6
            self.face([(outer[k][0], yf, outer[k][1]), (outer[j][0], yf, outer[j][1]),
                       (inner[j][0], yf, inner[j][1]), (inner[k][0], yf, inner[k][1])], "Door_Iron", (0, -1, 0))
            mid = math.radians(60 * k + 30)
            self.face([(outer[k][0], yf, outer[k][1]), (outer[j][0], yf, outer[j][1]),
                       (outer[j][0], yb, outer[j][1]), (outer[k][0], yb, outer[k][1])], "Door_Iron",
                      (math.cos(mid), 0, math.sin(mid)))
        self.refresh()

    def grain(self):
        """Revision 2: raised grain streaks and knots in Door_Grain on both
        faces. Each streak runs up its plank on a gently swaying centre line,
        a low ridge (GRAIN_H high, 45-degree sides, a narrow flat top) that
        tapers to a point at each end. Each knot is an oval boss with the
        same sides. Records every piece in self.grain_items for the checks."""
        self.grain_items = []
        h = GRAIN_H
        for face, plank, x, z0, z1, sway in STREAKS:
            y0, out = (-THICK, -1.0) if face == "front" else (0.0, 1.0)

            def P(px, pz, ph):
                return (px, y0 + out * ph, pz)
            inner = [z0 + GRAIN_TAPER, (z0 + z1) / 2, z1 - GRAIN_TAPER]
            xs = [x + d for d in sway]
            tips = [(xs[0], z0), (xs[-1], z1)]
            rows = []  # (base-left, top-left, top-right, base-right) at each inner station
            for xc, zc in zip(xs, inner):
                rows.append([P(xc - GRAIN_HALF_BASE, zc, 0.0), P(xc - GRAIN_HALF_TOP, zc, h),
                             P(xc + GRAIN_HALF_TOP, zc, h), P(xc + GRAIN_HALF_BASE, zc, 0.0)])
            outward = (0.0, out, 0.0)
            for ra, rb in zip(rows, rows[1:]):
                for i in range(3):  # left side, top, right side
                    facing = (float(i - 1), out, 0.0)
                    self.face([ra[i], ra[i + 1], rb[i + 1], rb[i]], "Door_Grain", facing)
            for row, (tx, tz), down in ((rows[0], tips[0], True), (rows[-1], tips[1], False)):
                tip = P(tx, tz, 0.0)
                for i in range(3):
                    side = 0.0 if i == 1 else (-1.0 if i == 0 else 1.0)
                    facing = Vector(outward) + Vector((side * 2.0, 0.0, -1.0 if down else 1.0)) * 0.5
                    self.face([row[i], row[i + 1], tip], "Door_Grain", facing)
            self.grain_items.append({"kind": "streak", "face": face, "plank": plank,
                                     "x": (min(xs) - GRAIN_HALF_BASE, max(xs) + GRAIN_HALF_BASE), "z": (z0, z1)})
        for face, plank, x, z in KNOTS:
            y0, out = (-THICK, -1.0) if face == "front" else (0.0, 1.0)
            a, c = KNOT_R
            base = [(x + a * math.cos(math.tau * k / 8), z + c * math.sin(math.tau * k / 8)) for k in range(8)]
            top = [(x + (a - h) * math.cos(math.tau * k / 8), z + (c - h) * math.sin(math.tau * k / 8)) for k in range(8)]
            B = [(px, y0, pz) for px, pz in base]
            T = [(px, y0 + out * h, pz) for px, pz in top]
            for k in range(8):
                j = (k + 1) % 8
                ang = math.tau * (k + 0.5) / 8
                self.face([B[k], B[j], T[j], T[k]], "Door_Grain", (math.cos(ang), out, math.sin(ang)))
            self.face(T, "Door_Grain", (0.0, out, 0.0))
            self.grain_items.append({"kind": "knot", "face": face, "plank": plank,
                                     "x": (x - a, x + a), "z": (z - c, z + c)})
        self.refresh()

    def rig_up(self):
        """One joint, `hinge`, at the origin pointing up; every vertex on it."""
        arm = bpy.data.armatures.new("Tavern_Door_Rig")
        rig = bpy.data.objects.new("Tavern_Door_Rig", arm)
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
        eb = arm.edit_bones.new("hinge")
        eb.head, eb.tail = Vector((0, 0, 0)), Vector((0, 0, 1.0))
        eb.roll = 0.0
        bpy.ops.object.mode_set(mode="OBJECT")
        rig.select_set(False)
        if prev_active and prev_active.name in view_layer.objects:
            view_layer.objects.active = prev_active
        self.obj.parent = rig
        mod = self.obj.modifiers.new("Armature", "ARMATURE")
        mod.object = rig
        group = self.obj.vertex_groups["hinge"]
        group.add(list(range(len(self.obj.data.vertices))), 1.0, "REPLACE")
        self.rig = rig

    def build_all(self):
        for step in self.STEPS:
            getattr(self, step)()


# --------------------------------------------------------------------------
# Checks, preview and export


def triangles(mesh):
    return sum(len(p.vertices) - 2 for p in mesh.polygons)


def swing_point(co, deg):
    """A model point (Blender) turned `deg` open about the hinge, in the
    hinge's engine frame: (x along the closed leaf, z out of the front)."""
    a = math.radians(deg)
    x, z = co.x, -co.y
    return x * math.cos(a) + z * math.sin(a), -x * math.sin(a) + z * math.cos(a)


def check(b):
    problems = []
    mesh = b.obj.data
    tris = triangles(mesh)
    if tris > TRI_BUDGET:
        problems.append(f"{tris} triangles, budget is {TRI_BUDGET}")
    names = [m.name for m in mesh.materials]
    if names != [n for n, _ in MATERIALS]:
        problems.append(f"materials {names}, want {[n for n, _ in MATERIALS]}")
    used = sorted({p.material_index for p in mesh.polygons})
    if used != [0, 1, 2]:
        problems.append(f"material slots used {used}, want [0, 1, 2]")
    if [bone.name for bone in b.rig.data.bones] != ["hinge"]:
        problems.append("rig must have exactly one bone, hinge")
    group = b.obj.vertex_groups["hinge"].index
    for v in mesh.vertices:
        ws = [(g.group, g.weight) for g in v.groups]
        if ws != [(group, 1.0)]:
            problems.append(f"vertex {v.index} weights {ws}")
            break
    lo = Vector([min(v.co[i] for v in mesh.vertices) for i in range(3)])
    hi = Vector([max(v.co[i] for v in mesh.vertices) for i in range(3)])
    if lo.z < -1e-6 or hi.z > HEIGHT + 1e-6 or hi.x > WIDTH + 1e-6:
        problems.append(f"bounds {tuple(lo)} .. {tuple(hi)} outside the leaf")
    # Swing clearance, for assets.md: turned 100 degrees open, how far the
    # leaf reaches back past the hinge line (negative x). Revision 1's.
    reach = min(swing_point(v.co, SWING_DEG)[0] for v in mesh.vertices)
    if abs(reach - REV1_REACH) > 1e-4:
        problems.append(f"open 100 degrees the leaf reaches {reach:.4f}, revision 1 reached {REV1_REACH}")

    # ---- revision 2: grain ----
    g = M["Door_Grain"]
    grain_vs = sorted({i for p in mesh.polygons if p.material_index == g for i in p.vertices})
    other_vs = {i for p in mesh.polygons if p.material_index != g for i in p.vertices}
    if set(grain_vs) & other_vs:
        problems.append("grain shares vertices with the leaf or the iron")
    # Raised at most GRAIN_H off its face, never past the leaf's ends.
    for i in grain_vs:
        co = mesh.vertices[i].co
        if not (-THICK - GRAIN_H - 1e-6 <= co.y <= GRAIN_H + 1e-6) or -THICK + 1e-6 < co.y < -1e-6:
            problems.append(f"grain vertex at y {co.y:.4f} is not within {GRAIN_H} m of a face")
            break
    # Every face either a top (within TOP_DEG of the plank) or a side
    # turned more than CREASE_DEG from it.
    side_min, top_max, n_side = 180.0, 0.0, 0
    for p in mesh.polygons:
        if p.material_index != g:
            continue
        front = p.center.y < -THICK / 2
        plank_n = Vector((0, -1, 0)) if front else Vector((0, 1, 0))
        ang = math.degrees(p.normal.angle(plank_n))
        if ang <= TOP_DEG:
            top_max = max(top_max, ang)
        else:
            side_min = min(side_min, ang)
            n_side += 1
            if ang <= CREASE_DEG:
                problems.append(f"a grain side at {tuple(round(c, 2) for c in p.center)} turns only {ang:.1f} degrees")
                break
    # The plank joints: both faces of each V-groove turn more than CREASE_DEG
    # from the plank, so crease ink lines every joint (revision 2).
    groove_deg = math.degrees(math.atan2(GROOVE_DEPTH, GROOVE_HALF))
    if groove_deg <= CREASE_DEG:
        problems.append(f"the plank grooves turn only {groove_deg:.1f} degrees")
    # Counts: 2-3 streaks per plank on each face, 2 knots on each face.
    counts = {}
    for it in b.grain_items:
        key = (it["kind"], it["face"], it["plank"] if it["kind"] == "streak" else None)
        counts[key] = counts.get(key, 0) + 1
    for face in ("front", "back"):
        for k in range(PLANKS):
            n = counts.get(("streak", face, k), 0)
            if not 2 <= n <= 3:
                problems.append(f"{face} plank {k}: {n} streaks, want 2-3")
        n = sum(v for (kind, f, _), v in counts.items() if kind == "knot" and f == face)
        if n != 2:
            problems.append(f"{face}: {n} knots, want 2")
    if abs((GRAIN_HALF_BASE + GRAIN_HALF_TOP) - 0.05) > 1e-9:
        problems.append("a streak is not 0.05 m wide at mid-height")
    # On its plank, clear of the iron and the leaf's ends, and of each other.
    iron = []
    for zc in STRAP_Z:
        iron.append(("both", (-0.012, STRAP_LEN), (zc - STRAP_H0 / 2, zc + STRAP_H0 / 2)))
    px0, px1, pz0, pz1 = PLATE
    ring_lo = pz0 - RING_R_OUT * math.sin(math.radians(60)) - RING_R_OUT
    iron.append(("front", (px0 - 0.03, px1 + 0.03), (ring_lo, pz1)))
    items = b.grain_items
    for it in items:
        (x0, x1), (z0, z1) = it["x"], it["z"]
        e0, e1 = PLANK_X[it["plank"]]
        tag = f"{it['face']} {it['kind']} on plank {it['plank']} at x {x0:.2f}-{x1:.2f}"
        if x0 < e0 + GRAIN_CLEAR - 1e-6 and it["plank"] > 0 or x1 > e1 - GRAIN_CLEAR + 1e-6 and it["plank"] < PLANKS - 1:
            problems.append(f"{tag}: off its plank ({e0:.3f}-{e1:.3f})")
        if x0 < GRAIN_CLEAR or x1 > WIDTH - GRAIN_CLEAR or z0 < GRAIN_CLEAR or z1 > HEIGHT - GRAIN_CLEAR:
            problems.append(f"{tag}: too near the leaf's edge")
        for face, (ix0, ix1), (iz0, iz1) in iron:
            if face not in ("both", it["face"]):
                continue
            if x0 < ix1 + GRAIN_CLEAR and x1 > ix0 - GRAIN_CLEAR and z0 < iz1 + GRAIN_CLEAR and z1 > iz0 - GRAIN_CLEAR:
                problems.append(f"{tag}, z {z0:.2f}-{z1:.2f}: over the iron")
    for i, a in enumerate(items):
        for c in items[i + 1:]:
            if a["face"] != c["face"]:
                continue
            if (a["x"][0] < c["x"][1] + GRAIN_CLEAR and c["x"][0] < a["x"][1] + GRAIN_CLEAR
                    and a["z"][0] < c["z"][1] + GRAIN_CLEAR and c["z"][0] < a["z"][1] + GRAIN_CLEAR):
                problems.append(f"{a['face']} grain at x {a['x'][0]:.2f} and x {c['x'][0]:.2f} overlap")
    # Swing: through the whole 100 degrees, no grain enters the frame. In
    # the hinge's frame the wall (and its jambs) lies at z > 0 outside the
    # opening x 0..2.2, and the head above y 3.4.
    worst = 0.0
    for step in range(0, int(SWING_DEG * 2) + 1):
        deg = step / 2
        for i in grain_vs:
            co = mesh.vertices[i].co
            x, z = swing_point(co, deg)
            if z > 1e-6 and not (FRAME_CLEAR_X[0] - 1e-6 <= x <= FRAME_CLEAR_X[1] + 1e-6):
                worst = max(worst, z)
    if worst > 0:
        problems.append(f"grain enters the frame during the swing (by {worst:.3f} m)")
    stats = {"grain_tris": sum(len(p.vertices) - 2 for p in mesh.polygons if p.material_index == g),
             "min_side_deg": round(side_min, 1), "max_top_deg": round(top_max, 1), "sides": n_side,
             "streaks": sum(1 for it in items if it["kind"] == "streak"),
             "knots": sum(1 for it in items if it["kind"] == "knot"), "groove_deg": round(groove_deg, 1)}
    return problems, tris, lo, hi, reach, stats


def render_preview(b):
    """tavern_door.png: three views side by side at 30 degrees down, as the
    engine's camera sees things - the front from the front-left, the back
    from the back-right, and the door swung open 100 degrees seen from the
    front-left."""
    import numpy as np

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_WORKBENCH"
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.display.shading.show_object_outline = True
    scene.render.resolution_x, scene.render.resolution_y = 400, 500
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("preview")
    world.color = srgb(110, 128, 96)
    scene.world = world
    cam_data = bpy.data.cameras.new("preview")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 4.6
    cam = bpy.data.objects.new("preview", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    paths = []

    def shoot(name, yaw_deg, target):
        # yaw as the engine's camera: it looks along (sin yaw, -cos yaw) in
        # Blender's ground plane; 180 looks straight at the door's front.
        yaw, pitch = math.radians(yaw_deg), math.radians(30)
        fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
        cam.location = Vector(target) - fwd * 20
        cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(os.path.dirname(PREVIEW), f"_tavern_door_{name}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)

    pb = b.rig.pose.bones["hinge"]
    pb.rotation_mode = "XYZ"
    shoot("front", 180 + 35, (WIDTH / 2, 0, HEIGHT / 2))
    shoot("back", 35, (WIDTH / 2, 0, HEIGHT / 2))
    pb.rotation_euler = (0, math.radians(100), 0)  # the bone's local y is Blender +Z (engine +Y)
    bpy.context.view_layer.update()
    shoot("open", 180 + 35, (0.2, 1.0, HEIGHT / 2))
    pb.rotation_euler = (0, 0, 0)
    bpy.context.view_layer.update()

    w, h = scene.render.resolution_x, scene.render.resolution_y
    sheet = np.zeros((h, w * len(paths), 4), dtype=np.float32)
    for k, path in enumerate(paths):
        img = bpy.data.images.load(path)
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        sheet[:, k * w:(k + 1) * w, :] = px.reshape(h, w, 4)
        bpy.data.images.remove(img)
        os.remove(path)
    out = bpy.data.images.new("tavern_door_sheet", w * len(paths), h)
    out.pixels.foreach_set(sheet.ravel())
    out.filepath_raw = PREVIEW
    out.file_format = "PNG"
    out.save()
    print("wrote", PREVIEW)


def view_axes(yaw_deg, pitch_deg=30.0):
    """The game camera's forward, right and up in Blender, looking along yaw
    (as render_preview's shoot), pitch degrees down."""
    yaw, pitch = math.radians(yaw_deg), math.radians(pitch_deg)
    fwd = Vector((math.sin(yaw) * math.cos(pitch), -math.cos(yaw) * math.cos(pitch), -math.sin(pitch)))
    right = fwd.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(fwd).normalized()
    return fwd, right, up


PX_PER_M = 24.0
DAY_INK = (0x4c, 0x4c, 0x66)
INK_MIX = 0.35            # as indoor Oak's crease ink


def toon_crop(centre, width_m, height_m, yaw_deg, crease):
    """Ray cast the scene pixel by pixel at the game's 24 px/m, as the
    game's camera at yaw_deg sees it, and toon-shade it in three tones of
    each material's base colour. Ink: the outline (a depth step over 0.35 m)
    and crease ink on the `crease` materials' pixels (a neighbour's normal
    more than 40 degrees apart, no depth step, one pixel on the side turned
    from the camera). Returns (mixed, solid, stats): the ink mixed at
    INK_MIX of the day ink, and drawn solid."""
    import numpy as np

    scene = bpy.context.scene
    dg = bpy.context.evaluated_depsgraph_get()
    fwd, right, up = view_axes(yaw_deg)
    sun = (-fwd + up * 0.6 - right * 0.5).normalized()
    W, H = int(round(width_m * PX_PER_M)), int(round(height_m * PX_PER_M))
    names = [n for n, _ in MATERIALS]
    tones = {}
    for name, col in MATERIALS:
        c = [((v ** (1 / 2.2)) * 255) for v in col]
        tones[name] = (tuple(v * 0.62 for v in c), tuple(c), tuple(min(255, v * 1.25) for v in c))
    N = np.zeros((H, W, 3), dtype=np.float32)
    D = np.full((H, W), 1e9, dtype=np.float32)
    S = np.full((H, W), -1, dtype=np.int32)
    C = np.zeros((H, W, 3), dtype=np.float32)
    C[:] = (110, 128, 96)
    for row in range(H):
        sy = (0.5 - (row + 0.5) / H) * height_m
        for col in range(W):
            sx = ((col + 0.5) / W - 0.5) * width_m
            origin = Vector(centre) + right * sx + up * sy - fwd * 30.0
            hit, loc, nrm, idx, obj, _ = scene.ray_cast(dg, origin, fwd)
            if not hit:
                continue
            ev = obj.evaluated_get(dg)
            mat = ev.data.materials[ev.data.polygons[idx].material_index].name
            n = nrm.normalized()
            if n.dot(fwd) > 0:
                n = -n
            N[row, col] = n
            D[row, col] = (loc - origin).dot(fwd)
            S[row, col] = names.index(mat)
            ndl = n.dot(sun)
            C[row, col] = tones[mat][2 if ndl > 0.6 else (1 if ndl > 0.2 else 0)]
    creased = {names.index(m) for m in crease}
    ink = np.zeros((H, W), dtype=bool)
    crease_px = 0
    f = np.array(fwd, dtype=np.float32)
    cos_t = math.cos(math.radians(CREASE_DEG))
    for row in range(H):
        for col in range(W):
            sp = S[row, col]
            if sp < 0:
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
                if sp in creased and sq >= 0 and abs(dq - d) < 0.35 and float(n @ nq) < cos_t:
                    to_cam = float(nq @ f) - float(n @ f)
                    if sq not in creased or to_cam < -1e-3 or (abs(to_cam) <= 1e-3 and n[0] > nq[0]):
                        ink[row, col] = True
                        crease_px += 1
                        break
    mixed = C.copy()
    mixed[ink] = mixed[ink] * (1 - INK_MIX) + np.array(DAY_INK, dtype=np.float32) * INK_MIX
    solid = C.copy()
    solid[ink] = (20, 18, 30)
    grain_i = names.index("Door_Grain")
    stats = {"px": (W, H), "grain_px": int((S == grain_i).sum()),
             "grain_px_inked": int(ink[S == grain_i].sum()), "crease_px": crease_px}
    return mixed, solid, stats


def render_grain_preview(b):
    """tavern_door_grain.png: the grain at the game's 24 px/m, scaled 4x
    nearest neighbour. Front as the exterior's default camera sees it (yaw
    167.5 in the tavern's frame, which is the door's), then the back as the
    room's design camera sees the inside door (yaw 45 in the room, the door
    turned 90: yaw -45 in the door's frame). For each: toon-shaded with the
    crease ink mixed at 0.35, the same with the ink solid, and a smooth
    Workbench render at 96 px/m."""
    import numpy as np

    scene = bpy.context.scene
    scale = 4
    width_m, height_m = 2.8, 3.5
    centre = (WIDTH / 2, -THICK / 2, HEIGHT / 2)
    panels = []
    report = {}
    for label, yaw in (("front", 180.0 - 12.5), ("back", -45.0)):
        mixed, solid, stats = toon_crop(centre, width_m, height_m, yaw, ("Door_Wood", "Door_Grain"))
        report[label] = stats
        scene.render.engine = "BLENDER_WORKBENCH"
        scene.display.shading.light = "STUDIO"
        scene.display.shading.color_type = "MATERIAL"
        scene.display.shading.show_object_outline = True
        H, W = mixed.shape[:2]
        scene.render.resolution_x, scene.render.resolution_y = W * scale, H * scale
        scene.render.resolution_percentage = 100
        cam_data = bpy.data.cameras.new("grain_preview")
        cam_data.type = "ORTHO"
        cam_data.ortho_scale = max(W, H) * scale / (PX_PER_M * scale)
        cam = bpy.data.objects.new("grain_preview", cam_data)
        scene.collection.objects.link(cam)
        scene.camera = cam
        fwd, right, up = view_axes(yaw)
        cam.location = Vector(centre) - fwd * 30.0
        cam.rotation_euler = fwd.to_track_quat("-Z", "Y").to_euler()
        path = os.path.join(os.path.dirname(GRAIN_PREVIEW), "_tavern_door_grain_smooth.png")
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

        def to_rgba(c):
            big = np.repeat(np.repeat(c[::-1] / 255.0, scale, axis=0), scale, axis=1)  # rows top-down -> image bottom-up
            return np.concatenate([big, np.ones(big.shape[:2] + (1,), dtype=np.float32)], axis=2)
        gap = np.ones((H * scale, 8, 4), dtype=np.float32)
        panels += [to_rgba(mixed), gap, to_rgba(solid), gap, smooth, gap, gap]
    sheet = np.concatenate(panels[:-2], axis=1)
    out = bpy.data.images.new("tavern_door_grain", sheet.shape[1], sheet.shape[0])
    out.pixels.foreach_set(sheet.astype(np.float32).ravel())
    out.filepath_raw = GRAIN_PREVIEW
    out.file_format = "PNG"
    out.save()
    bpy.data.images.remove(out)
    print("wrote", GRAIN_PREVIEW)
    return report


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
        export_skins=True,
        export_animations=False,
        export_def_bones=False,
        export_reset_pose_bones=True,
        export_apply=False,
    )
    print("wrote", OUT)


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    b = Builder()
    b.build_all()
    problems, tris, lo, hi, reach, stats = check(b)
    print(f"tavern_door: {len(b.obj.data.vertices)} verts, {tris} tris; bounds (Blender) "
          f"{tuple(round(c, 3) for c in lo)} .. {tuple(round(c, 3) for c in hi)}; "
          f"open 100 degrees it reaches {reach:.4f} m back past the hinge line")
    print(f"tavern_door: grain: {stats}")
    if problems:
        raise RuntimeError("the door fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_preview(b)
        print(f"tavern_door: grain preview: {render_grain_preview(b)}")


if __name__ == "__main__" and bpy.app.background:
    main()
