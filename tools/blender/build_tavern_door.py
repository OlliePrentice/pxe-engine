"""Build the Sleeping Fox's door - one plank door, two iron strap hinges and a
ring handle, skinned to a one-joint rig - and export assets/scenes/tavern_door.glb.

Run headless (builds, checks, exports, then renders the preview beside the glb):
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_door.py
    PXE_TAVERN_OUT=/some/dir ... writes the glb and preview there instead.
    PXE_SKIP_PREVIEW=1 ... skips the preview.

Or step by step in an open Blender, to watch it go together:
    import runpy; td = runpy.run_path(".../build_tavern_door.py", run_name="door")
    b = td["Builder"](); b.start(); b.leaf(); b.ironwork(); b.rig_up()
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

The front (outside) has the plank grooves and the ring handle; the two strap
hinges wrap through the leaf and show on both faces, so the inside door seen
from the room (its back) still shows its iron.

Rig: one joint, `hinge`, at the origin, pointing up (+Z in Blender, +Y in
the engine), so its rest rotation is the identity. Every vertex is weighted
fully to it. No clips: code turns the joint.

Material order is the contract with the engine: Door_Wood (0) then
Door_Iron (1), loaded into actor slots 12-13.
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
]
M = {name: i for i, (name, _) in enumerate(MATERIALS)}

WIDTH = 2.18     # leaf, hinge edge to latch edge (m)
HEIGHT = 3.38
THICK = 0.12
PLANKS = 3       # wide planks, 0.73 m each (0.45 m real)
GROOVE_HALF = 0.03   # V-groove between planks on the front: half its width
GROOVE_DEPTH = 0.025
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
TRI_BUDGET = 100  # strictly fewer


class Builder:
    STEPS = ["start", "leaf", "ironwork", "rig_up"]

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


def check(b):
    problems = []
    mesh = b.obj.data
    tris = triangles(mesh)
    if tris >= TRI_BUDGET:
        problems.append(f"{tris} triangles, budget is under {TRI_BUDGET}")
    names = [m.name for m in mesh.materials]
    if names != [n for n, _ in MATERIALS]:
        problems.append(f"materials {names}, want {[n for n, _ in MATERIALS]}")
    used = sorted({p.material_index for p in mesh.polygons})
    if used != [0, 1]:
        problems.append(f"material slots used {used}, want [0, 1]")
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
    # leaf reaches back past the hinge line (negative x).
    a = math.radians(100)
    reach = min(v.co.x * math.cos(a) - v.co.y * math.sin(a) for v in mesh.vertices)
    return problems, tris, lo, hi, reach


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
    problems, tris, lo, hi, reach = check(b)
    print(f"tavern_door: {len(b.obj.data.vertices)} verts, {tris} tris; bounds (Blender) "
          f"{tuple(round(c, 3) for c in lo)} .. {tuple(round(c, 3) for c in hi)}; "
          f"open 100 degrees it reaches {reach:.3f} m back past the hinge line")
    if problems:
        raise RuntimeError("the door fails its checks:\n  " + "\n  ".join(problems))
    export()
    if not os.environ.get("PXE_SKIP_PREVIEW"):
        render_preview(b)


if __name__ == "__main__" and bpy.app.background:
    main()
