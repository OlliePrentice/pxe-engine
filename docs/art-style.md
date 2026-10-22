# Art style

The standing style for every asset in the project. Tasks may override it, and say so when they do. The reference assets are the hooded hero (`tools/blender/build_hooded_hero.py`) and the fox (`tools/blender/build_fox.py`).

## Look
- Low-poly and faceted: every face is flat-shaded, so models read as cut paper.
- One solid base colour per material. No textures.
- The engine lights everything with a hard three-tone toon ramp. Choose base colours as the mid tone; the engine makes the light and shadow tones.
- Glowing parts use an emissive material, which the engine draws at full colour and unlit.
- Exaggerate silhouette features slightly (ears, tails, hoods, capes) so they read at pixel-art resolution. The fox's ears and brush are oversized for this.
- Keep palettes small and natural: a few materials per character, each a clear block of colour. The fox uses four: rust fur (208, 98, 40), cream (238, 220, 188), dark brown-black (54, 38, 33) and black (20, 17, 17), all sRGB.

## Scale and units
- 1 Blender unit = 1 metre, modelled at real-world size. The engine applies each character's in-world scale; the asset never does.
- Characters are drawn at 1.6× real size in the world (the hero's `size`).
- Small characters may be drawn larger so they read at pixel scale. At 1.6× the fox was only 15–20 px across in the default view, so it is drawn at 2.4× (`Fox::kSize`). Choose a character's scale per task and record it in the task and the character's code.

## Characters
- Built by a reproducible Blender Python script in `tools/blender/build_<name>.py`, exported to `assets/characters/<name>.glb`, with preview renders beside it.
- The script is the source of truth. Assets are built step by step in the user's live Blender session through the Blender MCP, so the user can watch, and are then rebuilt and exported headless from the script. Never hand-edit the glb.
- glTF 2.0 binary, exported +Y up. The figure faces -Y in Blender, which is +Z in the engine. Origin at ground level, centred under the body (for a character that rests lying down, under the resting pose).
- Budget: the hero is about 5,000 triangles. Smaller characters come in proportionally lower: the fox is 968.

## Materials
- One skinned mesh with its armature. glTF material order is the contract with the engine: material i is the character's actor material i. Name materials with the character's tag (`Fox_Fur`, `Fox_Cream`).
- The engine has 16 shared actor-material slots per frame (`pxe::kActorMaterials`). Each character owns its own range, so characters never share a colour: the hero uses 0–7, the fox 8–11. A new character takes the next free range and loads with `SkinnedModel::load(path, firstMaterial)`.

## Rigs
- Rig the full skeleton the creature will need, even if the task only animates one clip, so later clips never mean re-rigging. A quadruped has root, hips, a spine chain, chest, neck, head, ears, four legs with upper/lower/paw, and a tail chain of at least four bones.
- Joint names are lowercase, dotted for chains (`spine.1`, `tail.3`), with `.L`/`.R` for sides. Never rename joints once a character ships; code looks some up by name.
- Weight so the mesh stays attached in every pose: no open ends, gaps or floating pieces, even in extreme poses like a tight curl.

## Animation
- Clips are prefixed with a short character tag (`HH_` for the hero, `Fox_` for the fox), at 30 fps, every joint keyed on every frame, linear interpolation. Code looks clips up by name, not index.
- **Loops** end exactly on their first frame, so the seam is invisible.
- **Clips that chain** (rest → stand up → look around → lie down) meet pose for pose: each one's first frame equals the previous one's last frame exactly. The engine crossfades only as a safety net. Keep clips separate rather than one long clip, so they can be reused when the character gets behaviour.
- **Turns** made within a clip go on the `root` joint. If a clip leaves the character facing a new way, document its new forward in assets.md.
- **Locomotion is in place**, with no forward root motion; code moves the character. Each locomotion clip documents its stride length per cycle at real size. Code derives ground speed as stride × scale / clip duration, so planted feet don't slide. A planted foot moves backward at exactly that speed while it is down.
- Feet stay planted on the ground while standing, with no sliding or floating. Leg IK bends in a stable plane taken from the standing pose: elbows and knees never flip to the other side between keys.
- **Every headless build checks every clip** and fails if a check fails:
  - attachment, in the bind pose and at sampled frames;
  - no joint turning more than 45° between adjacent keys, interpolated as the engine does (slerped the short way);
  - no vertex more than 1 cm below the ground at any half-key;
  - for walks: each planted foot stays within about 1 mm of a fixed ground point, and every leg bends the way it does standing.
  The game samples between keys at 60 Hz or more, so a clip that only looks right at its keys is broken.

## Delivery
- **assets.md** in the task folder documents each asset:
  - source and how to rebuild it;
  - triangle count;
  - material order and colours;
  - joints;
  - each clip's name, length, loop or one-shot, and joins;
  - strides and speeds;
  - the character's footprint at in-game scale, through every clip, for the developer's clearance checks.
- **Preview renders** show the key poses of each clip in two views: from the side, and from the game's default camera angle as the character will be seen in the scene.
