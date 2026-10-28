# Art style: desert

The standing style for every asset in the desert game (`desert/`, the `pxe_desert` executable). It starts from the existing game's style ([game/docs/art-style.md](../../game/docs/art-style.md)) and changes what a first-person view at real scale needs. Tasks may override it, and say so when they do. What the engine requires of every file, whichever game makes it (format, axes, material names, skinned models), is the [asset contract](../../docs/engine-guide.md#asset-contract) in the engine guide; this doc doesn't repeat it.

## Look
- Low-poly and faceted: every face is flat-shaded, so models read as cut paper.
- One solid base colour per material. No textures.
- The engine lights everything with a hard three-tone toon ramp. Choose base colours as the mid tone; the engine makes the light and shadow tones.
- Glowing parts use an emissive material, which the engine draws at full colour and unlit.
- Keep palettes small and natural. The desert reads as a few warm blocks of colour: sand, palm bark, frond green.
- Assets are seen both close up and far off. From the 1.7 m eye, a 10 m palm 14 m away fills much of the view, and at 100 m it is a few dozen pixels across. Prefer strong silhouettes and large facets over fine geometry, so a shape reads at both distances.

## Scale and units
- Real-world scale, 1 Blender unit = 1 metre. Nothing is drawn larger than life: there is no visible hero, so the existing game's 1.6× hero scale does not apply.
- The player's eye, 1.7 m above the ground, is the scale reference. Judge every asset's proportions from there.
- In first person the player sees assets from every side and from below (standing under a palm crown). Meshes are drawn with back faces culled, so thin parts such as fronds, leaves and cloth are two-sided (a reversed duplicate) or thin closed volumes.

## Scenery and props
Static assets: plants, rocks, buildings and props. The reference asset is the date palm (`desert/tools/blender/build_date_palm.py`).
- Built by a reproducible Blender Python script in `desert/tools/blender/build_<name>.py` and exported to `desert/assets/scenes/<name>.glb`. The script is the source of truth: build step by step in the user's live Blender session through the Blender MCP so the user can watch, then rebuild and export headless. Never hand-edit the glb.
- Each asset documents its origin. For scenery, that is the base centre at ground level.
- **Materials** are the engine's surfaces, named as the asset contract lists. The desert uses `Sand` (ground), `Bark` (palm trunks, frond stubs, dates) and `Leaf` (palm fronds). Colours in the .blend are for previews only.
- **Name what code uses.** Every point and size the code needs, such as a trunk's collision radius, an asset's height and its extent, is printed by the build and recorded in the task's assets.md, in engine coordinates.
- **Budgets.** A palm is about 3,000 triangles.

## Terrain
- The dunes are generated in the game's code, not in Blender: long, smooth, wind-aligned ridges with a sharper crest on the downwind face, built on a 1 m grid with smooth normals, so the toon ramp cuts each slope into a few clean bands that follow its curve.
- No ripples, rocks or scrub until a task adds them.

## Palette
- **Sand:** a warm ochre mid tone, about sRGB (214, 176, 118), by day. Golden hour pushes it orange, twilight pink-mauve, and night cool blue-grey. The engine holds these as `Sand`'s default palettes.
- **Palm:** the trunk is the engine's `Bark` brown and the fronds its `Leaf` green. If the green reads wrong against the sand, a grey-green `Frond` surface is the fix, as its own task.

## Characters
None yet. When the desert gains characters, start from the existing game's Characters, Rigs and Animation sections, at real scale.

## Delivery
- **assets.md** in the task folder documents each asset: its source and how to rebuild it, triangle count, materials, origin, collision footprint, and every point the code uses.
- **Preview renders** go beside the export: a three-quarter view (`<name>.png`), and a view from eye height (1.7 m) at the distance the player first sees the asset.
