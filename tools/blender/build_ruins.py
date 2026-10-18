"""Build the ruins-and-pond scene and export it to assets/scenes/ruins.glb.

Run headless:
    /Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_ruins.py

Recreates the scene from t3ssel8r's "Creating a Scene for my 3D Pixel Art
Game" with the same modelling tricks: a terraced ground, stone pieces
chipped by a subdivide -> displace -> decimate modifier stack, rocks made
the same way from cubes, and tree canopies built from merged spheres whose
normals are bent toward one big ellipsoid so they shade as a single soft
mass.

Material names are the contract with the engine: Grass, Dirt, Stone, Bark,
Leaf, Water. Ground faces under paving are Dirt so no grass grows
through stone.

Layout coordinates are metres in a plan where +a runs up-right on screen and
+b up-left once the engine's camera looks at it. The whole scene sits under
one empty turned 45 degrees (and scaled 1.5x to match the video's framing),
so the axis-aligned plaza reads as diamonds from the camera.
"""

import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, "..", "..", "assets", "scenes", "ruins.glb"))

rng = random.Random(11)

# --------------------------------------------------------------------------
# Scene reset and materials


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


MATERIAL_COLOURS = {
    "Grass": (0.42, 0.50, 0.15),
    "Dirt": (0.25, 0.22, 0.19),
    "Stone": (0.50, 0.42, 0.30),
    "Bark": (0.37, 0.33, 0.29),
    "Leaf": (0.23, 0.37, 0.09),
    "Water": (0.27, 0.42, 0.55),
}


def material(name):
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    r, g, b = MATERIAL_COLOURS[name]
    mat.diffuse_color = (r, g, b, 1.0)
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (r, g, b, 1.0)
        bsdf.inputs["Roughness"].default_value = 1.0
    return mat


ROOT = None


def link(obj, mat_name=None):
    bpy.context.scene.collection.objects.link(obj)
    obj.parent = ROOT
    if mat_name:
        obj.data.materials.clear()
        obj.data.materials.append(material(mat_name))
    return obj


def bake_modifiers(obj):
    """Apply the modifier stack by swapping in the evaluated mesh."""
    deps = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(deps)
    mesh = bpy.data.meshes.new_from_object(evaluated)
    old = obj.data
    obj.modifiers.clear()
    obj.data = mesh
    bpy.data.meshes.remove(old)


def set_smooth(mesh, smooth):
    for poly in mesh.polygons:
        poly.use_smooth = smooth


# --------------------------------------------------------------------------
# Primitives


def box_mesh(name, size, centre=(0, 0, 0)):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    bmesh.ops.translate(bm, vec=Vector(centre), verts=bm.verts)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    return mesh


def noise_texture(name, kind="CLOUDS", scale=0.6, depth=2):
    tex = bpy.data.textures.get(name)
    if tex:
        return tex
    tex = bpy.data.textures.new(name, kind)
    if kind == "CLOUDS":
        tex.noise_scale = scale
        tex.noise_depth = depth
    elif kind == "VORONOI":
        tex.noise_scale = scale
    return tex


def box_normals(obj, size):
    """Give every face the normal of the box side it came from.

    The chipped silhouette stays, but each side of a block shades as one
    flat face, the way t3ssel8r transfers normals from the undisplaced
    shape. Without this, every small decimated facet flips between toon bands
    on its own as the sun moves and the blocks sparkle with triangles.
    """
    mesh = obj.data
    half = Vector(size) * 0.5
    normals = []
    for poly in mesh.polygons:
        c = poly.center
        r = Vector((c.x / half.x, c.y / half.y, c.z / half.z))
        axis = max(range(3), key=lambda i: abs(r[i]))
        n = Vector((0.0, 0.0, 0.0))
        n[axis] = 1.0 if r[axis] >= 0 else -1.0
        normals.extend([n] * poly.loop_total)
    set_smooth(mesh, True)
    mesh.normals_split_custom_set(normals)


def chipped(obj, strength=0.07, ratio=0.18, levels=3, tex_scale=0.35, size=None):
    """t3ssel8r's stone stack: subdivide, displace with noise, decimate."""
    sub = obj.modifiers.new("Subdivide", "SUBSURF")
    sub.subdivision_type = "SIMPLE"
    sub.levels = levels
    sub.render_levels = levels
    disp = obj.modifiers.new("Displace", "DISPLACE")
    disp.texture = noise_texture("StoneNoise", "CLOUDS", tex_scale, 2)
    disp.texture_coords = "GLOBAL"
    disp.strength = strength
    disp.mid_level = 0.5
    dec = obj.modifiers.new("Decimate", "DECIMATE")
    dec.decimate_type = "COLLAPSE"
    dec.ratio = ratio
    bake_modifiers(obj)
    if size is not None:
        box_normals(obj, size)
    else:
        set_smooth(obj.data, False)


def stone_block(name, size, centre, rot_z=0.0, tilt=(0.0, 0.0), strength=0.07, ratio=0.2):
    mesh = box_mesh(name, size)
    obj = bpy.data.objects.new(name, mesh)
    link(obj, "Stone")
    obj.matrix_local = Matrix.Translation(Vector(centre)) @ Matrix.Rotation(rot_z, 4, "Z") \
        @ Matrix.Rotation(tilt[0], 4, "X") @ Matrix.Rotation(tilt[1], 4, "Y")
    chipped(obj, strength=strength, ratio=ratio, size=size)
    return obj


def rock(name, centre, size, flat=0.7, rot=None, levels=2, ratio=0.2):
    """Cube -> smooth subdivision -> cloud displacement -> heavy decimation."""
    mesh = box_mesh(name, (size, size * rng.uniform(0.8, 1.15), size * flat))
    obj = bpy.data.objects.new(name, mesh)
    link(obj, "Stone")
    rot = rot if rot is not None else rng.uniform(0, math.tau)
    obj.matrix_local = Matrix.Translation(Vector(centre)) @ Matrix.Rotation(rot, 4, "Z")
    sub = obj.modifiers.new("Subdivide", "SUBSURF")
    sub.levels = levels
    disp = obj.modifiers.new("Displace", "DISPLACE")
    disp.texture = noise_texture("RockNoise", "CLOUDS", 0.5, 2)
    disp.texture_coords = "GLOBAL"
    disp.strength = size * 0.22
    dec = obj.modifiers.new("Decimate", "DECIMATE")
    dec.decimate_type = "COLLAPSE"
    dec.ratio = ratio
    bake_modifiers(obj)
    set_smooth(obj.data, False)
    return obj


# --------------------------------------------------------------------------
# Ground

GRID = 0.5
HALF = 36.0
TERRACE_H = 0.9
LOWER_H = -1.7
POND_C = Vector((7.4, 7.6))
POND_R = (6.0, 3.3)
POND_ROT = math.radians(-45.0)  # long axis runs across the screen
WATER_Z = TERRACE_H - 0.35

# The flight drops toward -b (screen lower right) from the plaza's front edge.
STAIRS = {"b0": -2.6, "steps": 6, "depth": 0.55, "a0": 2.6, "a1": 7.0}
PLAZA = (-2.6, 9.4, -2.6, 2.6)  # a0, a1, b0, b1

footprints = []  # (a0, a1, b0, b1) rectangles that should be Dirt, not Grass


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def pond_radius(theta):
    theta -= POND_ROT
    wobble = 1.0 + 0.10 * math.sin(3 * theta + 1.1) + 0.06 * math.sin(5 * theta + 0.3)
    c, s = math.cos(theta), math.sin(theta)
    ellipse = 1.0 / math.sqrt((c / POND_R[0]) ** 2 + (s / POND_R[1]) ** 2)
    return ellipse * wobble


def pond_rel(a, b):
    """Distance from the pond centre divided by the shoreline radius."""
    d = Vector((a, b)) - POND_C
    if d.length < 1e-6:
        return 0.0
    return d.length / pond_radius(math.atan2(d.y, d.x))


def height(a, b):
    # Terrace behind the plaza that holds the pond, with a soft bank.
    s = a + b
    edge = 9.0 + 1.0 * math.sin((a - b) * 0.17 + 0.7) + 0.4 * math.sin((a - b) * 0.5)
    h = TERRACE_H * smoothstep(edge - 1.6, edge + 1.6, s)
    # Lower meadow in front of the stairs.
    drop = STAIRS["b0"] - STAIRS["steps"] * STAIRS["depth"] + 0.3 * math.sin(a * 0.31 + 1.9)
    low = smoothstep(drop + 1.4, drop - 0.6, b)
    h += (LOWER_H - h) * low
    # Gentle undulation so the grass has some light to play with.
    h += 0.16 * math.sin(a * 0.37 + b * 0.11) * math.sin(b * 0.29 - 0.4)
    h += 0.07 * math.sin(a * 0.9 + 1.3) * math.cos(b * 0.8)
    # Pond basin.
    r = pond_rel(a, b)
    if r < 1.15:
        floor = WATER_Z - 0.7
        h = floor + (h - floor) * smoothstep(0.75, 1.12, r)
    # Flatten the plaza and the stair run.
    a0, a1, b0, b1 = PLAZA
    inside = smoothstep(a0 - 1.2, a0, a) * (1 - smoothstep(a1 + 0.2, a1 + 0.8, a)) \
        * smoothstep(b0 - 1.2, b0, b) * (1 - smoothstep(b1, b1 + 1.0, b))
    h = h * (1 - inside)
    # Cut the ground away under the stair flight so the treads show.
    st = STAIRS
    run = st["steps"] * st["depth"]
    cut = smoothstep(st["a0"] - 1.3, st["a0"] - 0.85, a) * (1 - smoothstep(st["a1"] + 0.85, st["a1"] + 1.3, a)) \
        * smoothstep(st["b0"] + 0.3, st["b0"] - 0.2, b) * smoothstep(st["b0"] - run - 1.0, st["b0"] - run, b)
    h += (LOWER_H - 0.5 - h) * cut
    return h


def in_footprint(a, b):
    for a0, a1, b0, b1 in footprints:
        if a0 <= a <= a1 and b0 <= b <= b1:
            return True
    return pond_rel(a, b) < 1.02


def build_ground():
    n = int(2 * HALF / GRID)
    bm = bmesh.new()
    verts = []
    for j in range(n + 1):
        row = []
        for i in range(n + 1):
            a = -HALF + i * GRID
            b = -HALF + j * GRID
            row.append(bm.verts.new((a, b, height(a, b))))
        verts.append(row)
    faces = []
    for j in range(n):
        for i in range(n):
            f = bm.faces.new((verts[j][i], verts[j][i + 1], verts[j + 1][i + 1], verts[j + 1][i]))
            c = f.calc_center_median()
            f.material_index = 1 if in_footprint(c.x, c.y) else 0
            faces.append(f)
    bmesh.ops.triangulate(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new("Ground")
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new("Ground", mesh)
    link(obj)
    mesh.materials.append(material("Grass"))
    mesh.materials.append(material("Dirt"))
    set_smooth(mesh, True)


# --------------------------------------------------------------------------
# Water


def build_pond():
    """A flat water sheet that runs a little past the shore under the bank,
    so the waterline is where the sheet meets the ground. The engine draws
    the light contact line there, and around anything standing in the
    water, from depth."""
    segs = 72
    bm = bmesh.new()
    centre = bm.verts.new((POND_C.x, POND_C.y, WATER_Z))
    ring = []
    for k in range(segs):
        t = k / segs * math.tau
        r = pond_radius(t) * 1.12
        d = Vector((math.cos(t), math.sin(t)))
        p = POND_C + d * r
        ring.append(bm.verts.new((p.x, p.y, WATER_Z)))
    for k in range(segs):
        bm.faces.new((centre, ring[k], ring[(k + 1) % segs]))
    mesh = bpy.data.meshes.new("Water")
    bm.to_mesh(mesh)
    bm.free()
    water = link(bpy.data.objects.new("Water", mesh), "Water")
    set_smooth(water.data, False)

    # Stepping stones and a rim of rocks on the far bank.
    for (da, db, s) in [(-2.2, 0.6, 1.1), (1.9, -0.7, 0.9), (0.4, 1.4, 0.7)]:
        rock("PondStone", (POND_C.x + da, POND_C.y + db, WATER_Z - 0.05), s, flat=0.45)
    for k in range(9):
        t = math.radians(35 + k * 13 + rng.uniform(-4, 4))
        r = pond_radius(t) * 1.06
        p = POND_C + Vector((math.cos(t), math.sin(t))) * r
        rock("RimStone", (p.x, p.y, height(p.x, p.y) - 0.1), rng.uniform(0.7, 1.3), flat=0.55)
    # A big boulder sitting half in the water on the far right of the pond,
    # as in the video. Added last so earlier random draws, and so the rest
    # of the scene, stay as they were.
    # More facets than the small rocks, so it reads as a rounded, cut boulder.
    rock("PondBoulder", (POND_C.x + 4.2, POND_C.y - 3.4, WATER_Z - 0.15), 2.7, flat=0.85, rot=0.6, levels=3, ratio=0.12)


# --------------------------------------------------------------------------
# Stonework


def build_plaza():
    a0, a1, b0, b1 = PLAZA
    footprints.append((a0, a1, b0, b1))
    tile = 1.0
    gap = 0.07
    a = a0
    while a < a1 - 0.01:
        b = b0
        while b < b1 - 0.01:
            if rng.random() > 0.08:
                lift = rng.uniform(-0.03, 0.05)
                stone_block("Paving", (tile - gap, tile - gap, 0.3),
                            (a + tile / 2, b + tile / 2, -0.1 + lift),
                            rot_z=rng.uniform(-0.03, 0.03),
                            tilt=(rng.uniform(-0.03, 0.03), rng.uniform(-0.03, 0.03)),
                            strength=0.05, ratio=0.3)
            b += tile
        a += tile


def build_stairs():
    b = STAIRS["b0"]
    a0, a1 = STAIRS["a0"], STAIRS["a1"]
    steps = STAIRS["steps"]
    depth = STAIRS["depth"]
    rise = -LOWER_H / steps
    footprints.append((a0 - 0.9, a1 + 0.9, b - steps * depth - 0.3, b + 0.2))
    bottom = LOWER_H - 0.4
    for k in range(steps):
        top = -rise * (k + 1) + rise * 0.5
        # Each tread is a row of three blocks so the chipping breaks it up.
        width = a1 - a0
        for piece in range(3):
            pa0 = a0 + width * piece / 3
            pa1 = a0 + width * (piece + 1) / 3
            stone_block("Step", (pa1 - pa0 - 0.04, depth - 0.03, top - bottom),
                        ((pa0 + pa1) / 2, b - depth * (k + 0.5), (top + bottom) / 2),
                        rot_z=rng.uniform(-0.02, 0.02), strength=0.06, ratio=0.25)
    # Cheek walls on both sides of the flight.
    for side, a in (("L", a0 - 0.45), ("R", a1 + 0.45)):
        for k in range(steps):
            top = -rise * k + 0.25
            stone_block("Cheek" + side, (0.8, depth, top - bottom),
                        (a, b - depth * (k + 0.5), (top + bottom) / 2),
                        strength=0.06, ratio=0.25)


def build_pillar(name, a, b, drums, broken=False):
    """Stacked, uneven drums with blocks jutting out, so the capital and the
    ledges cast their own shadows down the shaft as the sun moves."""
    footprints.append((a - 1.1, a + 1.1, b - 1.1, b + 1.1))
    z = 0.0
    stone_block(name + "Plinth", (2.0, 2.0, 0.5), (a, b, z + 0.25), strength=0.06)
    z += 0.5
    stone_block(name + "Base", (1.6, 1.6, 0.35), (a, b, z + 0.175), strength=0.05)
    z += 0.35
    for k in range(drums):
        h = rng.uniform(0.85, 1.15)
        w = rng.uniform(1.12, 1.38)
        off = (rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1))
        # Drums share one orientation so a whole face of the pillar turns to
        # the sun at once; the chipping and offsets still break it up.
        stone_block(name + "Drum", (w, w * rng.uniform(0.92, 1.08), h - 0.04), (a + off[0], b + off[1], z + h / 2),
                    strength=0.08, ratio=0.2)
        # Now and then a block juts out of the drum: a ledge that throws a
        # short shadow down the face below it.
        if rng.random() < 0.55:
            side = rng.choice([(1, 0), (-1, 0), (0, 1), (0, -1)])
            jut = rng.uniform(0.12, 0.22)
            bw = rng.uniform(0.45, 0.8)
            bh = rng.uniform(0.25, 0.45)
            cx = a + off[0] + side[0] * (w / 2 + jut / 2 - 0.08) + (0 if side[0] else rng.uniform(-0.2, 0.2))
            cy = b + off[1] + side[1] * (w / 2 + jut / 2 - 0.08) + (0 if side[1] else rng.uniform(-0.2, 0.2))
            size = (jut + 0.16, bw, bh) if side[0] else (bw, jut + 0.16, bh)
            stone_block(name + "Jut", size, (cx, cy, z + rng.uniform(0.2, h - 0.2)), strength=0.06, ratio=0.25)
        z += h
    if broken:
        stone_block(name + "Broken", (1.2, 1.2, 0.5), (a + 0.1, b, z + 0.2),
                    rot_z=0.2, tilt=(0.18, 0.1), strength=0.12, ratio=0.15)
        return
    stone_block(name + "Capital", (1.85, 1.85, 0.45), (a, b, z + 0.225), strength=0.06)
    z += 0.45
    stone_block(name + "Abacus", (1.5, 1.5, 0.35), (a, b, z + 0.175), rot_z=0.04, strength=0.06)


def build_planter(name, a, b, size, height_m=0.6):
    footprints.append((a - size / 2, a + size / 2, b - size / 2, b + size / 2))
    t = 0.38
    s = size
    z = height(a, b) + height_m / 2 - 0.1
    stone_block(name + "N", (s, t, height_m), (a, b + s / 2 - t / 2, z), strength=0.06)
    stone_block(name + "S", (s, t, height_m), (a, b - s / 2 + t / 2, z), strength=0.06)
    stone_block(name + "E", (t, s - 2 * t, height_m), (a + s / 2 - t / 2, b, z), strength=0.06)
    stone_block(name + "W", (t, s - 2 * t, height_m), (a - s / 2 + t / 2, b, z), strength=0.06)
    stone_block(name + "Floor", (s - 2 * t, s - 2 * t, 0.2), (a, b, z - height_m / 2 + 0.15),
                strength=0.04)


def build_flagstones():
    """Old paving half swallowed by the lawn, west of the plaza."""
    size = 1.9
    pitch = 2.35
    for i in range(5):
        for j in range(4):
            if rng.random() < 0.35:
                continue
            a = -15.0 + i * pitch + rng.uniform(-0.15, 0.15)
            b = -0.5 + j * pitch + rng.uniform(-0.15, 0.15)
            footprints.append((a - size / 2, a + size / 2, b - size / 2, b + size / 2))
            z = height(a, b) - 0.06
            stone_block("Flag", (size, size, 0.3), (a, b, z), rot_z=rng.uniform(-0.06, 0.06),
                        strength=0.07, ratio=0.22)
            # A smaller inset slab gives the carved border the video's tiles have.
            stone_block("FlagInset", (size * 0.62, size * 0.36, 0.1),
                        (a + rng.uniform(-0.2, 0.2), b + rng.uniform(-0.2, 0.2), z + 0.17),
                        strength=0.03, ratio=0.3)


def build_ruin_wall():
    """Broken low wall leading off the plaza toward the lower meadow."""
    a, b = 12.0, -2.0
    for k in range(5):
        h = 1.2 - k * 0.2 + rng.uniform(-0.1, 0.1)
        if h < 0.3:
            break
        stone_block("Wall", (1.05, 0.7, h), (a + k * 1.08, b, height(a + k * 1.08, b) + h / 2 - 0.1),
                    rot_z=rng.uniform(-0.04, 0.04), strength=0.08, ratio=0.2)


# --------------------------------------------------------------------------
# Trees


def canopy(name, centre, radii, blobs=12):
    """Merged spheres, remeshed into one skin, with normals bent toward a
    single ellipsoid so the crown shades as one soft mass. Lobes of mixed
    sizes sit out on the ellipsoid around a smaller core, so the outline is
    lumpy, like a real crown, rather than a ball on a stick."""
    bm = bmesh.new()
    cx, cy, cz = centre
    for k in range(blobs):
        # Spread lobes over the upper two thirds; crowns hang, they do not
        # bulge underneath.
        u = rng.uniform(-0.45, 1.0)
        th = rng.uniform(0, math.tau)
        rr = math.sqrt(1 - u * u)
        d = Vector((rr * math.cos(th) * radii[0], rr * math.sin(th) * radii[1], u * radii[2]))
        d *= rng.uniform(0.55, 0.85)
        r = min(radii) * rng.uniform(0.28, 0.58)
        geom = bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=r)
        bmesh.ops.translate(bm, vec=Vector((cx, cy, cz)) + d, verts=geom["verts"])
    geom = bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=12, radius=1.0)
    bmesh.ops.scale(bm, vec=Vector(radii) * 0.55, verts=geom["verts"])
    bmesh.ops.translate(bm, vec=Vector(centre), verts=geom["verts"])
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    link(obj, "Leaf")
    rem = obj.modifiers.new("Remesh", "REMESH")
    rem.mode = "VOXEL"
    rem.voxel_size = 0.22
    dec = obj.modifiers.new("Decimate", "DECIMATE")
    dec.ratio = 0.25
    bake_modifiers(obj)

    mesh = obj.data
    set_smooth(mesh, True)
    c = Vector(centre)
    normals = []
    for v in mesh.vertices:
        d = v.co - c
        e = Vector((d.x / radii[0] ** 2, d.y / radii[1] ** 2, d.z / radii[2] ** 2))
        n = (v.normal * 0.45 + e.normalized() * 0.55).normalized()
        normals.append(n)
    mesh.normals_split_custom_set_from_vertices(normals)
    return obj


def catmull_rom(points, step=0.3):
    """Resample a poly-line into a smooth curve, roughly `step` metres apart."""
    pts = [Vector(p) for p in points]
    ext = [pts[0] * 2 - pts[1]] + pts + [pts[-1] * 2 - pts[-2]]
    out = []
    for i in range(1, len(ext) - 2):
        p0, p1, p2, p3 = ext[i - 1], ext[i], ext[i + 1], ext[i + 2]
        n = max(2, int((p2 - p1).length / step))
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(pts[-1])
    return out


def trunk(name, points, r0, r1, flare=0.0, roots=0.0, sides=11):
    """A bark tube along a curve: the rings twist a little, knots and bulges
    along the length make the outline crooked, and the base can flare into
    root buttresses. Shaded smooth; the shader paints the bark."""
    path = catmull_rom(points)
    bm = bmesh.new()
    rings = []
    # Parallel-transport a frame along the path so the facets do not flip.
    tangent = (path[1] - path[0]).normalized()
    side = tangent.cross(Vector((0.0, 0.0, 1.0)))
    if side.length < 1e-3:
        side = Vector((1.0, 0.0, 0.0))
    side.normalize()
    length = sum((path[i + 1] - path[i]).length for i in range(len(path) - 1))
    travelled = 0.0
    phase = rng.uniform(0, math.tau)
    root_phase = rng.uniform(0, math.tau)
    seed = Vector((rng.uniform(0, 100), rng.uniform(0, 100), rng.uniform(0, 100)))
    base_z = path[0].z
    for i, c in enumerate(path):
        if i > 0:
            travelled += (path[i] - path[i - 1]).length
            new_t = (path[min(i + 1, len(path) - 1)] - path[i - 1]).normalized()
            axis = tangent.cross(new_t)
            if axis.length > 1e-6:
                side.rotate(Matrix.Rotation(tangent.angle(new_t), 3, axis.normalized()))
            tangent = new_t
        up = tangent.cross(side).normalized()
        f = travelled / max(length, 1e-6)
        h = c.z - base_z
        radius = r0 + (r1 - r0) * f
        ring = []
        for k in range(sides):
            th = k / sides * math.tau + travelled * 0.18 + phase
            r = radius * (1.0 + flare * math.exp(-h / 0.7))
            r *= 1.0 + roots * max(0.0, math.cos(4.0 * th + root_phase)) ** 2 * math.exp(-h / 0.9)
            r *= 1.0 + 0.06 * math.sin(3.0 * th + 1.7)
            out = side * math.cos(th) + up * math.sin(th)
            # Knots and bulges: lumpy noise over the bark so neighbouring
            # facets turn slightly different ways and shade as patches.
            r *= 1.0 + 0.16 * noise.noise(seed + (c + out * radius) * 1.7)
            ring.append(bm.verts.new(c + out * r))
        rings.append(ring)
    for i in range(len(rings) - 1):
        for k in range(sides):
            bm.faces.new((rings[i][k], rings[i][(k + 1) % sides], rings[i + 1][(k + 1) % sides], rings[i + 1][k]))
    # Two or three long seams marked sharp, each running part of the way up.
    # The trunk stays smooth elsewhere; along a seam the normals split, and
    # the edge pass draws one thin light line there, like the few bark
    # strokes on the video's trunks.
    bm.edges.ensure_lookup_table()
    for _ in range(rng.choice([2, 3])):
        k = rng.randrange(sides)
        i0 = rng.randrange(0, max(1, len(rings) // 3))
        i1 = min(len(rings) - 1, i0 + max(2, int(len(rings) * rng.uniform(0.35, 0.7))))
        for i in range(i0, i1):
            edge = bm.edges.get((rings[i][k], rings[i + 1][k]))
            if edge:
                edge.smooth = False
    bm.faces.new(list(reversed(rings[0])))
    bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    link(obj, "Bark")
    # Smooth normals: the bark's colour comes from painted noise in the
    # shader, not from facets, so it reads soft rather than carved.
    set_smooth(mesh, True)
    return obj


def tree(name, a, b, height_m, crown, lean=(0.0, 0.0)):
    """A trunk that forks into two or three branches reaching up into a
    lumpy crown, so some limbs show through and under the foliage."""
    z = height(a, b) - 0.2
    top = (a + lean[0], b + lean[1], z + height_m)
    mid = (a + lean[0] * 0.4 + rng.uniform(-0.2, 0.2), b + lean[1] * 0.4 + rng.uniform(-0.2, 0.2), z + height_m * 0.5)
    r0 = crown[0] * 0.13 + 0.14
    trunk(name + "Trunk", [(a, b, z - 0.3), mid, top], r0, r0 * 0.6, flare=0.45, roots=0.35)
    centre = (top[0], top[1], top[2] + crown[2] * 0.6)
    for k in range(rng.choice([2, 3])):
        ang = rng.uniform(0, math.tau)
        reach = (math.cos(ang) * crown[0] * 0.5, math.sin(ang) * crown[1] * 0.5)
        tip = (centre[0] + reach[0], centre[1] + reach[1], centre[2] + rng.uniform(-0.2, 0.5) * crown[2])
        bend = (top[0] + reach[0] * 0.35, top[1] + reach[1] * 0.35, top[2] + crown[2] * 0.25)
        trunk(name + "Branch", [top, bend, tip], r0 * 0.55, r0 * 0.22)
    canopy(name + "Crown", centre, crown)
    footprints.append((a - 0.4, a + 0.4, b - 0.4, b + 0.4))


def pond_tree():
    """The big tree on the pond's front-left bank, as in the video: a thick,
    crooked trunk with a flared, rooted base and a broken stub, leaning out
    over the water and opening into one big limb, under a broad crown that
    hangs low over the pond."""
    a, b = 2.6, 7.4
    z = height(a, b) - 0.3
    trunk("PondTrunk", [(a, b, z - 0.5), (a + 0.35, b + 0.05, z + 1.3), (a + 0.6, b + 0.45, z + 2.6),
                        (a + 1.3, b + 0.9, z + 3.9), (a + 1.7, b + 1.6, z + 5.4), (a + 2.1, b + 2.2, z + 6.8)],
          0.85, 0.42, flare=0.4, roots=0.25, sides=12)
    # One big limb leaning out over the water toward screen right.
    trunk("PondLimb", [(a + 1.0, b + 0.7, z + 3.3), (a + 2.1, b + 0.6, z + 4.6), (a + 3.4, b + 0.9, z + 5.9),
                       (a + 4.4, b + 1.4, z + 7.0)], 0.42, 0.2, sides=9)
    # A short broken stub low on the trunk, angled up and out.
    trunk("PondStub", [(a + 0.35, b + 0.15, z + 1.9), (a - 0.15, b + 0.55, z + 2.6), (a - 0.45, b + 0.8, z + 3.0)],
          0.27, 0.16, sides=7)
    canopy("PondCrown", (a + 2.6, b + 2.0, z + 8.2), (5.2, 4.6, 2.6), blobs=13)
    footprints.append((a - 1.0, a + 1.0, b - 1.0, b + 1.0))


def bush(name, a, b, r):
    z = height(a, b) + r * 0.45
    canopy(name, (a, b, z), (r, r * 0.9, r * 0.75), blobs=7)


# --------------------------------------------------------------------------


def main():
    global ROOT
    reset()
    ROOT = bpy.data.objects.new("Ruins", None)
    bpy.context.scene.collection.objects.link(ROOT)
    ROOT.rotation_euler = (0.0, 0.0, math.radians(45.0))
    ROOT.scale = (1.5, 1.5, 1.5)

    # Stonework first: each piece registers a footprint so the ground under
    # it becomes Dirt.
    build_plaza()
    build_stairs()
    build_pillar("PillarA", 0.6, 0.6, drums=5)
    build_pillar("PillarB", 6.6, 0.6, drums=5)
    build_planter("PlanterA", 10.6, -1.0, 2.4)
    build_planter("PlanterB", -4.2, -1.8, 1.8, height_m=0.5)
    build_flagstones()
    build_ruin_wall()

    for (a, b, s) in [(-1.0, 7.0, 1.3), (-2.0, 8.0, 0.8), (-3.6, -4.6, 0.8), (10.5, -4.4, 1.0),
                      (-9.0, 9.0, 1.4), (14.5, 3.0, 0.9), (0.5, -6.5, 0.7), (16.0, -7.0, 1.6),
                      (-6.5, -0.8, 0.6)]:
        rock("Rock", (a, b, height(a, b) - 0.1 * s), s)

    pond_tree()
    tree("TreeA", 0.5, -8.6, 1.6, (2.2, 2.0, 3.0))
    tree("TreeB", 7.5, -9.2, 1.4, (2.0, 1.9, 2.8))
    tree("TreeC", -6.0, 15.0, 4.5, (5.5, 5.0, 3.5))
    tree("TreeD", 19.0, 2.0, 3.5, (4.6, 4.2, 3.2))
    tree("TreeE", -15.0, -3.0, 3.0, (4.0, 3.8, 3.0))
    tree("TreeF", -13.0, 6.0, 5.5, (4.5, 4.2, 3.0))
    tree("TreeG", 17.0, 15.0, 5.0, (4.8, 4.4, 3.2))
    bush("BushA", -8.5, -5.5, 1.6)
    bush("BushD", -3.2, 3.8, 1.0)
    bush("BushE", 9.8, 2.6, 0.9)
    bush("BushB", 14.0, -3.0, 1.2)
    bush("BushC", 16.0, 9.0, 1.4)

    build_pond()
    build_ground()

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
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


main()
