# Art style: desert

The standing style for every asset in the desert game (`desert/`, the `pxe_desert` executable). It starts from the existing game's style ([game/docs/art-style.md](../../game/docs/art-style.md)) and changes what a first-person view at real scale needs. Tasks may override it, and say so when they do. What the engine requires of every file, whichever game makes it (format, axes, material names, skinned models), is the [asset contract](../../docs/engine-guide.md#asset-contract) in the engine guide; this doc doesn't repeat it.

## Look
- Low-poly and faceted: every face is flat-shaded, so models read as cut paper.
- One solid base colour per material. No textures.
- The engine lights everything with a hard three-tone toon ramp. Choose base colours as the mid tone; the engine makes the light and shadow tones.
- Glowing parts use an emissive material, which the engine draws at full colour and unlit.
- Keep palettes small and natural. The desert reads as a few warm blocks of colour: sand, palm bark, frond green.
- Assets are seen both close up and far off. From the 1.7 m eye, a 10 m palm 14 m away fills much of the view, and at 100 m it is a few dozen pixels across. Prefer strong silhouettes and large facets over fine geometry, so a shape reads at both distances.
- **View models are the exception.** A view model (the player's arms and what they carry) is seen from 0.15–0.6 m, where one pixel is under 1 mm, so it carries fine detail down to about 3 px on screen (about 2.5 mm at the hands). It is still flat-shaded, with one solid colour per material and no textures. Small round parts (screws, wires, dials) take as few sides as still read round at their size on screen, and a round part in a material with crease ink takes at least 10 sides (36° between them, under the 40° ink turn), so no ink runs along it. Stacked parts read through the edge pass's view-model outlines and through colour, so give neighbouring parts different materials.

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
Start from the existing game's Characters, Materials, Rigs and Animation sections, at real scale (no 1.6× character scale). Built by `desert/tools/blender/build_<name>.py` and exported to `desert/assets/characters/<name>.glb`, with previews beside it. The first character is the player's arms (`build_arms.py`, `arms.glb`, tasks `desert-arms` and `desert-arms-detail`): a rigged model drawn as the view model, in front of the whole scene, with the left bracer's hygrometer and the right wrist's sun compass. About 12,000–20,000 triangles.
- **A view model is modelled round the eye.** Its origin is the eye and its `root` joint sits there. It faces -Y in Blender (engine +Z, the view's forward), +Z up, Blender +X (engine +X) the player's left. The game places the origin at the eye with the view's axes, so the file's frame is the view's frame. The rest (bind) pose is the held pose.
- **Actor slots.** The desert's ranges in `pxe::kActorMaterials`: the arms take 0–14 (`Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Brass`, `Arms_Glow`, `Arms_Steel`, `Arms_Copper`, `Arms_Rubber`, `Arms_Thread`, `Arms_Worn`, `Arms_Grime`, `Arms_Dial`, `Arms_Paint`, `Arms_Amber`, `Arms_Screen`, in that glTF order). The game sets crease ink per slot (on the wrap, glove, brass, worn and grime). A new character takes the next free range, from 15.
- **Joints** follow the game's naming: lowercase, dotted chains, `.L`/`.R`, never renamed once shipped. The arms' 39: `root`; per side `shoulder`, `upper_arm`, `forearm`, `hand`, and `thumb`, `index`, `middle`, `ring`, `pinky`, each `.1`–`.3`.
- **Framing**, checked by the build through the eye camera (Blender: at the origin looking along -Y, +Z up, 60° vertical field of view, 16:9) and binding:
  - nothing in view above the lower 35% (NDC y ≤ -0.30);
  - every vertex in view at least 0.15 m ahead of the eye (the near plane is 0.1 m);
  - limbs leave through the bottom edge;
  - anything the player must read (the arms' gauge and compass face) wholly in view, facing the eye, and at least 3 px wide.
  At real proportions a forearm rises steeply, so only a few centimetres behind the hand show. Put readable details at the wrist end.
- **Test poses.** The attachment check also runs in poses beyond the rest pose, so later animation stays safe: wrists turned 30° each way about all three axes, fingers curled 60°, elbows opened 30°.
- **View-model clips.** A view model's clips move only its limbs: its `root` and every joint it doesn't animate are keyed at exactly their rest transform, and every translation and scale key equals rest. The framing rules (lower 35%, near clearance, limbs leaving through the bottom edge) hold at every sampled frame; readability (the gauge, the compass) is checked at rest only. A locomotion clip's phase is driven by the distance walked: assets.md records each clip's cycle length in metres. The arms' `Arms_Walk` and `Arms_Sprint` are 1.0 s each, 2.0 m and 3.0 m per cycle, the left hand furthest forward at phase 0.

## Delivery
- **assets.md** in the task folder documents each asset: its source and how to rebuild it, triangle count, materials, origin, collision footprint, and every point the code uses.
- **Preview renders** go beside the export: a three-quarter view (`<name>.png`), and a view from eye height (1.7 m) at the distance the player first sees the asset.
