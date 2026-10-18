# Blender props

`generate_props.py` builds the meadow props the way the reference scenes are modeled, then writes glTF into `assets/props/`.

```bash
blender --background --python tools/blender/generate_props.py
```

macOS:

```bash
/Applications/Blender.app/Contents/MacOS/Blender --background --python tools/blender/generate_props.py
```

Tested on Blender 4.0.2. Blender 4.1 and later removed `mesh.use_auto_smooth`; the script skips that flag when it is missing and still runs Data Transfer.

## What each prop is

| File | How it is built |
|---|---|
| `rock_a.glb`, `rock_b.glb`, `rock_c.glb` | Icosphere, cloud Displace (global coordinates, so the seed offset changes the field), voxel Remesh, Decimate, flat shaded. Origin at the bottom center. |
| `pillar.glb`, `pillar_broken.glb` | Stacked shaft courses with a gap between them, a chipped top, a `Moss` cap, and rubble at the foot. The broken pillar has no capital. |
| `stairs.glb` | One tread, Array modifier with a constant offset. In Blender the flight goes along -Y and down -Z, which is +Z downhill and -Y down after the Y-up export. `stairs.meta` records count, width, depth, rise, and thickness for the height-field cut. One corner of the lowest tread is chipped. |
| `wall.glb`, `wall_low.glb` | Courses with seams, chipped tops, moss caps, and rubble. Heights are uneven so the run looks broken. Origin at the bottom center. |
| `rubble.glb`, `pebble.glb`, `boulder.glb`, `stump.glb` | A cluster of small stones, a single pebble, a small boulder, and a flared stump with a moss cap. |
| `slab.glb` | A flat pad. Origin at the bottom center. |
| `tree_a.glb`, `tree_b.glb`, `tree_c.glb` | 6-sided trunk, tapered, with a root flare, standing upright. Crown is joined icospheres. Loop normals are transferred from a smooth icosphere that encloses the crown. The engine hides that mesh and spawns leaf cards on the vertices. Trunk and crown stay separate objects so a flat shade on the trunk cannot wipe the crown normals. |
| `bush.glb` | Several lobes, material `Bush`, sat on the ground. Drawn as leaf cards, same as a crown. |

Materials are Principled BSDF, metallic 0, roughness 1, no textures. The script writes the display colour into the socket as linear so the exported `baseColorFactor` round-trips back to that colour in the engine.

## Authoring on a Mac

Export settings, units, axis, material names, and how to instance a new file are in the root [README](../../README.md#blender-props). Short version: metres, origin at the base, +Y Up on, normals on, material named `Stone` / `Step` / `Rock` / `Bark` / `Leaf` / `Bush` / `Moss`.
