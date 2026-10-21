# Art style

The standing style for every asset in the project. Tasks may override it, and say so when they do. The reference asset is the hooded hero (`tools/blender/build_hooded_hero.py`).

## Look
- Low-poly and faceted: every face is flat-shaded, so models read as cut paper.
- One solid base colour per material. No textures.
- The engine lights everything with a hard three-tone toon ramp. Choose base colours as the mid tone; the engine makes the light and shadow tones.
- Glowing parts use an emissive material, which the engine draws at full colour and unlit.
- Exaggerate silhouette features slightly (ears, tails, hoods, capes) so they read at pixel-art resolution.

## Scale and units
- 1 Blender unit = 1 metre, modelled at real-world size.
- Characters are drawn at 1.6× real size in the world (the hero's `size`). Model at real size; the engine applies the scale.

## Characters
- Built by a reproducible Blender Python script in `tools/blender/build_<name>.py`, exported to `assets/characters/<name>.glb`, with a preview render.
- glTF 2.0 binary, exported +Y up. The figure faces -Y in Blender, which is +Z in the engine. Origin at ground level, centred under the body.
- One skinned mesh with its armature. glTF material order is the contract with the engine: material i is the character's actor material i. Name the materials meaningfully.
- Rig for smooth, blended animation. Clips loop seamlessly and are prefixed with a short character tag (`HH_` for the hero).
- Budget: the hero is about 5,000 triangles. Smaller characters should come in proportionally lower.
