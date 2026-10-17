#!/usr/bin/env python3
"""Procedural meadow props in the style t3ssel8r models in Blender.

Low-poly primitives, flat materials, no textures. Rocks and trunks are
displaced, voxel-remeshed, and decimated so the facets stay chunky.
Stairs are one tread with an Array modifier. Tree canopies are joined
icospheres whose loop normals are transferred from a smooth enclosing
sphere, so the whole crown lights as one blob.

Run from the repo root:

    blender --background --python tools/blender/generate_props.py

On macOS the binary is usually:

    /Applications/Blender.app/Contents/MacOS/Blender --background --python tools/blender/generate_props.py

Writes assets/props/*.glb and assets/props/stairs.meta.
Tested on Blender 4.0.2. Blender 4.1+ dropped mesh.use_auto_smooth; the
script skips that flag when it is missing and still applies Data Transfer.
"""

import math
import os
import sys

import bpy
from mathutils import Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "assets", "props")

# Display colours (sRGB). Blender stores the Principled socket in linear,
# and the glTF exporter writes that linear value as baseColorFactor. The
# engine converts it back, so these are the colours the meadow draws.
PILLAR = (0.67, 0.62, 0.54)
STEP = (0.72, 0.63, 0.48)
SLAB = (0.60, 0.56, 0.50)
ROCK = (0.56, 0.53, 0.49)
BARK = (0.50, 0.37, 0.28)
LEAF = (0.50, 0.66, 0.27)
LEAF_DEEP = (0.42, 0.56, 0.22)
BUSH = (0.44, 0.60, 0.24)

# Stair flight in glTF/engine space: origin on the top tread, +Z downhill.
# Blender is Z-up, so engine +Z is Blender -Y and engine +Y is Blender +Z.
STEP_COUNT = 7
STEP_WIDTH = 1.40
STEP_DEPTH = 0.64
STEP_RISE = 0.18
STEP_THICK = 0.24


def srgb_to_linear(c):
    if c <= 0.04045:
        return c / 12.92
    return ((c + 0.055) / 1.055) ** 2.4


def fresh():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.unit_settings.length_unit = "METERS"


def material(name, srgb):
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    mat.use_backface_culling = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    linear = tuple(srgb_to_linear(c) for c in srgb)
    bsdf.inputs["Base Color"].default_value = (linear[0], linear[1], linear[2], 1.0)
    bsdf.inputs["Roughness"].default_value = 1.0
    bsdf.inputs["Metallic"].default_value = 0.0
    # Specular was renamed to Specular IOR Level in later 4.x.
    for key in ("Specular", "Specular IOR Level"):
        if key in bsdf.inputs:
            bsdf.inputs[key].default_value = 0.0
            break
    return mat


def activate(obj):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    if obj.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def apply_modifiers(obj):
    activate(obj)
    bpy.context.view_layer.update()
    while obj.modifiers:
        name = obj.modifiers[0].name
        bpy.ops.object.modifier_apply(modifier=name)


def apply_transform(obj, location=True, rotation=True, scale=True):
    activate(obj)
    bpy.ops.object.transform_apply(location=location, rotation=rotation, scale=scale)


def shade_flat(obj):
    activate(obj)
    bpy.ops.object.shade_flat()


def enable_auto_smooth(mesh):
    # Blender 4.0. Removed in 4.1, where custom normals no longer need the flag.
    if hasattr(mesh, "use_auto_smooth"):
        mesh.use_auto_smooth = True
    if hasattr(mesh, "auto_smooth_angle"):
        mesh.auto_smooth_angle = math.pi


def mesh_stats(obj):
    mesh = obj.data
    mesh.calc_loop_triangles()
    return len(mesh.loop_triangles), len(mesh.vertices)


def add_cube(name, center, size, mat):
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=center)
    obj = bpy.context.active_object
    obj.name = name
    obj.scale = size
    apply_transform(obj)
    obj.data.materials.append(mat)
    return obj


def join(parts, name, active_index=0):
    bpy.ops.object.select_all(action="DESELECT")
    for part in parts:
        part.select_set(True)
    bpy.context.view_layer.objects.active = parts[active_index]
    bpy.ops.object.join()
    obj = bpy.context.active_object
    obj.name = name
    return obj


def seat_on_ground(obj):
    """Bake the object transform, center X/Y, and put the lowest vertex on Z = 0.

    The origin stays at the bottom center, which is the pivot the engine places on the ground.
    """
    apply_transform(obj)
    verts = obj.data.vertices
    zs = [v.co.z for v in verts]
    xs = [v.co.x for v in verts]
    ys = [v.co.y for v in verts]
    zmin = min(zs)
    cx = (min(xs) + max(xs)) * 0.5
    cy = (min(ys) + max(ys)) * 0.5
    for v in verts:
        v.co.x -= cx
        v.co.y -= cy
        v.co.z -= zmin
    obj.data.update()


def shift_origin_to_base(obj):
    """Move the object origin to the bottom center without shifting the mesh in world space."""
    verts = obj.data.vertices
    zmin = min(v.co.z for v in verts)
    xs = [v.co.x for v in verts]
    ys = [v.co.y for v in verts]
    cx = (min(xs) + max(xs)) * 0.5
    cy = (min(ys) + max(ys)) * 0.5
    delta = Vector((cx, cy, zmin))
    for v in verts:
        v.co -= delta
    obj.location = obj.location + delta
    apply_transform(obj, rotation=False, scale=False)


def displace_remesh(obj, strength, noise_scale, voxel, ratio, seed):
    """Cloud displace, voxel remesh, then decimate. The chunky-rock stack."""
    # Global texture coordinates, so a temporary move changes the noise field.
    obj.location = (seed * 1.7, seed * 0.9, seed * 1.3)
    tex = bpy.data.textures.new(obj.name + "_disp", type="CLOUDS")
    tex.noise_scale = noise_scale
    tex.noise_depth = 2
    disp = obj.modifiers.new("Displace", "DISPLACE")
    disp.texture = tex
    disp.texture_coords = "GLOBAL"
    disp.direction = "NORMAL"
    disp.strength = strength
    disp.mid_level = 0.5
    apply_modifiers(obj)
    obj.location = (0.0, 0.0, 0.0)

    remesh = obj.modifiers.new("Remesh", "REMESH")
    remesh.mode = "VOXEL"
    remesh.voxel_size = voxel
    remesh.use_smooth_shade = False
    if hasattr(remesh, "adaptivity"):
        remesh.adaptivity = 0.0

    dec = obj.modifiers.new("Decimate", "DECIMATE")
    dec.decimate_type = "COLLAPSE"
    dec.ratio = ratio
    apply_modifiers(obj)
    shade_flat(obj)


def export_selected(path):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_yup=True,
        export_normals=True,
        export_tangents=False,
        export_materials="EXPORT",
        export_texcoords=False,
        export_colors=False,
        export_cameras=False,
        export_lights=False,
        export_animations=False,
        export_skins=False,
        export_morph=False,
        export_extras=False,
    )


def export_objects(objects, path):
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    export_selected(path)
    tris = 0
    verts = 0
    names = []
    for obj in objects:
        t, v = mesh_stats(obj)
        tris += t
        verts += v
        slots = [slot.name for slot in obj.data.materials if slot]
        names.append(f"{obj.name}[{','.join(slots)}]")
    print(f"wrote {os.path.relpath(path, ROOT)}  tris={tris} verts={verts}  {', '.join(names)}")


def chip_corner(obj, drop):
    """Knock one top corner down so a capital reads as broken, not milled."""
    verts = obj.data.vertices
    zmax = max(v.co.z for v in verts)
    xmax = max(v.co.x for v in verts)
    ymax = max(v.co.y for v in verts)
    for v in verts:
        if v.co.z > zmax - 0.08 and v.co.x > xmax * 0.2 and v.co.y > ymax * 0.2:
            v.co.z -= drop
            v.co.x -= drop * 0.35
    obj.data.update()


def make_pillar(path, shaft, chip):
    fresh()
    stone = material("Stone", PILLAR)
    base_h = 0.22
    cap_h = 0.28
    base = add_cube("Base", (0.0, 0.0, base_h * 0.5), (1.05, 1.05, base_h), stone)
    shaft_obj = add_cube(
        "Shaft",
        (0.0, 0.0, base_h + shaft * 0.5),
        (0.72, 0.72, shaft),
        stone,
    )
    cap = add_cube(
        "Capital",
        (0.0, 0.0, base_h + shaft + cap_h * 0.5),
        (0.96, 0.90, cap_h),
        stone,
    )
    chip_corner(cap, chip)
    obj = join([base, shaft_obj, cap], "Pillar", active_index=0)
    shade_flat(obj)
    export_objects([obj], path)


def make_stairs(path):
    fresh()
    step = material("Step", STEP)
    obj = add_cube(
        "Stairs",
        (0.0, 0.0, -STEP_THICK * 0.5),
        (STEP_WIDTH, STEP_DEPTH, STEP_THICK),
        step,
    )
    array = obj.modifiers.new("Array", "ARRAY")
    array.count = STEP_COUNT
    array.use_relative_offset = False
    array.use_constant_offset = True
    # Blender -Y becomes glTF/engine +Z. Blender -Z becomes engine -Y.
    array.constant_offset_displace = (0.0, -STEP_DEPTH, -STEP_RISE)
    apply_modifiers(obj)
    shade_flat(obj)
    export_objects([obj], path)
    meta = os.path.join(OUT, "stairs.meta")
    with open(meta, "w", encoding="utf-8") as fh:
        fh.write(
            f"count {STEP_COUNT}\n"
            f"width {STEP_WIDTH:.4f}\n"
            f"depth {STEP_DEPTH:.4f}\n"
            f"rise {STEP_RISE:.4f}\n"
            f"thick {STEP_THICK:.4f}\n"
        )
    print(f"wrote {os.path.relpath(meta, ROOT)}")


def make_wall(path, blocks):
    """blocks: (x center, height, length) along Blender X. Thickness is 0.34."""
    fresh()
    stone = material("Stone", PILLAR)
    parts = []
    for i, (x, height, length) in enumerate(blocks):
        parts.append(add_cube(f"Block{i}", (x, 0.0, height * 0.5), (length, 0.34, height), stone))
    obj = join(parts, "Wall", active_index=0)
    shade_flat(obj)
    shift_origin_to_base(obj)
    export_objects([obj], path)


def make_slab(path):
    fresh()
    stone = material("Slab", SLAB)
    obj = add_cube("Slab", (0.0, 0.0, 0.07), (1.6, 1.15, 0.14), stone)
    shade_flat(obj)
    shift_origin_to_base(obj)
    export_objects([obj], path)


def make_rock(path, seed, radius, strength, voxel, ratio, squash):
    fresh()
    rock = material("Rock", ROCK)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=radius, location=(0.0, 0.0, radius * 0.85))
    obj = bpy.context.active_object
    obj.name = "Rock"
    obj.scale = (1.0, 1.0, squash)
    apply_transform(obj)
    obj.data.materials.append(rock)
    displace_remesh(obj, strength, noise_scale=0.45 + (seed % 5) * 0.05, voxel=voxel, ratio=ratio, seed=seed)
    seat_on_ground(obj)
    # seat_on_ground leaves the origin at the previous object origin. Pull it to the base.
    shift_origin_to_base(obj)
    export_objects([obj], path)


def transfer_canopy_normals(canopy):
    """Normals from a smooth sphere that encloses the joined lobes."""
    verts = [canopy.matrix_world @ v.co for v in canopy.data.vertices]
    center = Vector((0.0, 0.0, 0.0))
    for v in verts:
        center += v
    center /= float(len(verts))
    radius = max((v - center).length for v in verts) * 1.04

    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=radius, location=center)
    source = bpy.context.active_object
    source.name = "SmoothEnclosing"
    activate(source)
    bpy.ops.object.shade_smooth()

    enable_auto_smooth(canopy.data)
    activate(canopy)
    mod = canopy.modifiers.new("TransferNormals", "DATA_TRANSFER")
    mod.object = source
    mod.use_loop_data = True
    mod.data_types_loops = {"CUSTOM_NORMAL"}
    mod.loop_mapping = "POLYINTERP_NEAREST"
    apply_modifiers(canopy)
    bpy.data.objects.remove(source, do_unlink=True)

    mesh = canopy.data
    enable_auto_smooth(mesh)
    mesh.calc_normals_split()
    worst = 0.0
    changed = 0
    for poly in mesh.polygons:
        for li in poly.loop_indices:
            delta = (mesh.loops[li].normal - poly.normal).length
            worst = max(worst, delta)
            if delta > 0.08:
                changed += 1
    print(f"  canopy normals: custom={mesh.has_custom_normals} changed_loops={changed} max_delta={worst:.3f}")
    if changed < 10:
        # Data Transfer did not take (usually auto-smooth was unavailable).
        # Fall back to the same enclosing sphere, written as custom loop normals.
        print("  data transfer was weak; writing enclosing-sphere normals directly")
        if hasattr(mesh, "normals_split_custom_set"):
            normals = []
            for loop in mesh.loops:
                co = mesh.vertices[loop.vertex_index].co
                n = (canopy.matrix_world @ co) - center
                if n.length < 1e-6:
                    n = Vector((0.0, 0.0, 1.0))
                else:
                    n.normalize()
                # normals_split_custom_set wants local-space normals.
                n = canopy.matrix_world.to_3x3().inverted() @ n
                n.normalize()
                normals.append(n)
            mesh.normals_split_custom_set(normals)
            enable_auto_smooth(mesh)


def bake_around_base(objects, lean):
    """Rotate every part around the world origin, then bake so glTF nodes sit at identity.

    Joining a flat-shaded trunk onto the canopy clears the transferred normals,
    so the trunk and the crown stay separate objects in the file.
    """
    root = bpy.data.objects.new("Root", None)
    bpy.context.scene.collection.objects.link(root)
    for obj in objects:
        obj.parent = root
        obj.matrix_parent_inverse = root.matrix_world.inverted()
    root.rotation_euler = (math.radians(lean[0]), math.radians(lean[1]), 0.0)
    bpy.context.view_layer.update()
    for obj in list(objects):
        activate(obj)
        bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
        apply_transform(obj)
    bpy.data.objects.remove(root, do_unlink=True)


def lift_above_ground(objects):
    zmin = min(v.co.z + obj.location.z for obj in objects for v in obj.data.vertices)
    if zmin >= -0.01:
        return
    for obj in objects:
        for v in obj.data.vertices:
            v.co.z -= zmin
        obj.data.update()


def make_tree(path, trunk_h, trunk_r, lobes, lean, leaf_color, name, bush=False):
    """lobes: list of (local xyz, radius). lean: (rx, ry) degrees, Blender axes."""
    fresh()
    bark = material("Bark", BARK)
    leaf = material("Leaf" if not bush else "Bush", leaf_color)

    objects = []
    if not bush:
        bpy.ops.mesh.primitive_cylinder_add(
            vertices=8, radius=trunk_r, depth=trunk_h, location=(0.0, 0.0, 0.0)
        )
        trunk = bpy.context.active_object
        trunk.name = "Trunk"
        for v in trunk.data.vertices:
            v.co.z += trunk_h * 0.5
        trunk.data.materials.append(bark)
        displace_remesh(
            trunk,
            strength=trunk_r * 0.55,
            noise_scale=0.35,
            voxel=max(0.10, trunk_r * 0.55),
            ratio=0.55,
            seed=int(trunk_h * 10),
        )
        # Remesh recenters; put the base back on Z = 0 and the origin at the base.
        seat_on_ground(trunk)
        shift_origin_to_base(trunk)
        objects.append(trunk)

    lobe_objs = []
    for i, (loc, radius) in enumerate(lobes):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=radius, location=loc)
        lobe = bpy.context.active_object
        lobe.name = f"Lobe{i}"
        lobe.data.materials.append(leaf)
        lobe_objs.append(lobe)
    canopy = join(lobe_objs, "Canopy", active_index=0) if len(lobe_objs) > 1 else lobe_objs[0]
    canopy.name = "Canopy"
    objects.append(canopy)

    if bush:
        # Lobes are created away from the origin. Bake that, then sit the lowest vertex on Z = 0.
        seat_on_ground(canopy)
    else:
        bake_around_base(objects, lean)
        lift_above_ground(objects)

    # Transfer after the lean is baked, so the enclosing sphere matches the crown.
    transfer_canopy_normals(canopy)
    export_objects(objects, path)


def main():
    os.makedirs(OUT, exist_ok=True)
    make_pillar(os.path.join(OUT, "pillar.glb"), shaft=2.35, chip=0.16)
    make_pillar(os.path.join(OUT, "pillar_broken.glb"), shaft=1.15, chip=0.34)
    make_stairs(os.path.join(OUT, "stairs.glb"))
    make_wall(
        os.path.join(OUT, "wall.glb"),
        [(-1.15, 0.72, 0.95), (-0.15, 0.95, 0.90), (0.85, 0.55, 0.80), (1.70, 0.78, 0.70)],
    )
    make_wall(
        os.path.join(OUT, "wall_low.glb"),
        [(-0.85, 0.38, 0.80), (0.05, 0.28, 0.70), (0.85, 0.46, 0.75)],
    )
    make_slab(os.path.join(OUT, "slab.glb"))
    make_rock(os.path.join(OUT, "rock_a.glb"), seed=3, radius=0.72, strength=0.28, voxel=0.16, ratio=0.22, squash=0.72)
    make_rock(os.path.join(OUT, "rock_b.glb"), seed=11, radius=0.48, strength=0.22, voxel=0.13, ratio=0.28, squash=0.85)
    make_rock(os.path.join(OUT, "rock_c.glb"), seed=19, radius=0.58, strength=0.30, voxel=0.15, ratio=0.20, squash=1.05)

    # Canopy local positions are Blender Z-up, origin at the trunk base.
    make_tree(
        os.path.join(OUT, "tree_a.glb"),
        trunk_h=2.35,
        trunk_r=0.22,
        lobes=[((0.15, 0.05, 3.15), 1.45), ((0.85, -0.15, 3.05), 0.95), ((-0.55, 0.25, 2.85), 0.85)],
        lean=(0.0, 14.0),
        leaf_color=LEAF,
        name="TreeA",
    )
    make_tree(
        os.path.join(OUT, "tree_b.glb"),
        trunk_h=2.15,
        trunk_r=0.20,
        lobes=[((0.0, 0.0, 2.95), 1.35), ((0.7, 0.15, 2.75), 0.8)],
        lean=(6.0, 0.0),
        leaf_color=LEAF,
        name="TreeB",
    )
    make_tree(
        os.path.join(OUT, "tree_c.glb"),
        trunk_h=1.85,
        trunk_r=0.18,
        lobes=[((-0.1, 0.05, 2.55), 1.15), ((0.55, -0.2, 2.45), 0.75), ((-0.45, 0.2, 2.35), 0.7)],
        lean=(-4.0, 8.0),
        leaf_color=LEAF_DEEP,
        name="TreeC",
    )
    make_tree(
        os.path.join(OUT, "bush.glb"),
        trunk_h=0.0,
        trunk_r=0.0,
        lobes=[((0.0, 0.0, 0.55), 0.62), ((0.38, 0.08, 0.48), 0.42)],
        lean=(0.0, 0.0),
        leaf_color=BUSH,
        name="Bush",
        bush=True,
    )
    print("done")


if __name__ == "__main__":
    main()
