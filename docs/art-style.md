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
- The engine has 64 shared actor-material slots per frame (`pxe::kActorMaterials`). Each character (or moving prop) owns its own range, so they never share a colour: the hero uses 0–7, the fox 8–11, the tavern door 12–14 (`Door_Wood`, `Door_Iron`, `Door_Grain`). A new character takes the next free range and loads with `SkinnedModel::load(path, firstMaterial)`.

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

## Buildings and props
Static assets: buildings, rooms, furniture and scenery. The reference assets are the ruins (`tools/blender/build_ruins.py`) and the Sleeping Fox tavern (`tools/blender/build_tavern_exterior.py`, `build_tavern_interior.py`).
- Built by a reproducible Blender Python script in `tools/blender/build_<name>.py` and exported to `assets/scenes/<name>.glb`, with preview renders beside it (`assets/scenes/<name>.png`). As for characters, the script is the source of truth: build step by step in the live session through the Blender MCP so the user can watch, then rebuild and export headless. Never hand-edit the glb.
- glTF 2.0 binary, exported +Y up, 1 unit = 1 metre. Blender -Y is the engine's +Z. Each asset documents its origin; for a building, put it somewhere the code places it by, such as the middle of the doorway's threshold at floor level.
- **Material names are the contract with the engine's surfaces.** The engine colours static meshes from its own day, golden, twilight and night palettes by material name, not from the glb's colours. Use only `Wood` (warm brown: beams, floors, frames, exterior timber), `Oak` (light oak: interior furniture), `Plaster` (cream walls), `Roof` (blue-grey slate), `Stone` and `Dirt`, plus the ruins' `Grass`, `Bark`, `Leaf` and `Water` where they belong. Any other name loads as Stone. Colours in the .blend are for previews only.
- **Hero scale.** Anything the hero walks through, sits by or reaches is built at 1.6× real, the hero's size: a doorway about 2.2 × 3.4 m, tables about 1.2 m high, chair seats about 0.74 m, a bar about 1.7 m.
- **Look.** The same as characters: low-poly, faceted, every face flat-shaded, one solid colour per material. Details have to survive 24 px per metre. Prefer shapes that read as blocks of colour, such as timber against plaster or dark panes in pale walls, over fine geometry. A plank floor reads through V-grooves (bevelled edges) between planks, not through gaps.
- **Shadow-only geometry.** End an object's name with `_ShadowOnly` and it is drawn only into the sun and moon shadow map. It never appears in the main, reflection or edge passes, never in `scene.ground`, but it does count toward `scene.shelter`. The engine reads the glTF node name, which is the Blender object name, so every object name must be unique. Blender's `.001` duplicate suffix would hide `_ShadowOnly`, and each build script checks for it. Use shadow-only pieces for the roof and the near walls of a cutaway room, so the room is lit as if enclosed but the camera sees in.
- **Cutaway rooms.** Design each interior for one camera yaw (the engine looks 30° down, yaw on 45° steps) and document it. The two walls nearest that camera and the roof are shadow-only. Low drawn stubs (≤ 0.5 m) along the near walls' base keep the outline. Nothing crosses over the room (no tie beams), so the floor is never hidden. Windows are real holes, so daylight falls in.
- **Name what code uses.** Give each piece the code may look up its own object (`Chair_A1`, `Barrel_2`, `Wall_PosX`). Everything the code needs, such as hinge points, fire and candle bases, spawn points and blocker footprints, is printed by the build and recorded in the task's assets.md, in engine coordinates.
- **Roofs.** Pitch the roof to the building's size: on a small house, lower the ridge rather than let the roof rise higher than the house is deep. The Sleeping Fox's 9 × 7 m exterior uses a 45° hipped slate roof, modelled as stepped, overlapping courses cut into staggered 0.5 m tiles: each course lip and each tile joint (a V notch) turns more than 40° from the tile, so the engine's crease ink lines every course and joint at 24 px/m. It has a cross-gable over the door and a small gabled dormer on each long slope, with carved low-poly bargeboards and finials.
- **Room layouts are checked.** When a task sets walkway or path rules for a room, the build script measures them and fails if they break. It measures furniture in groups (a table with its seats, the bar with its stools) by the convex hulls of their plan outlines. A group either stands against a wall or leaves a full walkway to it. Paths are checked by sweeping a disc of the path's width from the hero's spawn. A room's script also checks that the room fits the game's 40 × 22.5 m view at its design yaw.
- **Masonry: individual stones in light mortar.** Stonework on a building (a plinth, threshold, chimney or hearth) is laid as separate `Stone` blocks, each 0.03–0.05 m proud of a recessed `Plaster` mortar bed. The bed sits 0.05 m inside the structure's outline, and no stone passes that outline, so footprints and figures don't move.
  - Joints are about 0.06 m (at least 0.05, so each takes a pixel at 24 px/m). Pale joints read against grey stones with no engine change and no new material.
  - Courses are coursed and staggered: each course's vertical joints keep at least 0.12 m from the course below's, and the faces take turns at each corner, like quoins.
  - Walkable tops (a threshold, a hearth slab) stay flush, with only their joints recessed.
  - Build scripts check joint width, proudness, stagger and the outline. The ruins' Stone and its palette are separate and unchanged.
- **Plaster render: faceted, with spalled patches.**
  - Every plaster panel between the timbers is cut into a few low facets: slanted strips about 0.5 m wide, each fanned round a point recessed 0.012–0.03 m into the wall. The toon ramp then breaks it into a few tones.
  - Facets go into the wall, never proud of the face, so bounds, doorways and walkways can't move.
  - A few camera-facing panels have a spalled patch: an irregular hole, its broken edge recessed up to 0.02 m, down to a mortar bed 0.08 m back, with stones in light mortar beneath. Patches keep clear of windows, doorways, signs, sconces, shelves and hearths. The Sleeping Fox has 3 outside and 2 inside.
  - No pargetting and no limewash variation.
- **Door grain.**
  - The tavern door's planks carry raised grain streaks and knots in a third, darker door material, `Door_Grain` (sRGB about (92, 58, 34), against `Door_Wood` (128, 82, 46)).
  - Streaks are low ridges 0.02 m high and 0.05 m wide at mid-height, with 45° sides, on both faces. Plank V-grooves also turn 45°.
  - The edge pass inks the door's wood with the Oak and Roof crease test (normals more than 40° apart), so joints and grain read. The door loads into actor slots 12–14.
- **Budgets.** A building exterior is up to 14,000 triangles, a furnished room about 20,000, and a small moving prop such as a door up to 800. The Sleeping Fox's exterior is 9,117 triangles, its 22.6 × 17 m room 11,927, and its door 506.
- **Moving props** (a door) are skinned like characters, with one skinned mesh, joints named for what they do (`hinge`), materials tagged with the prop's name (`Door_Wood`) and their own actor-material slot range, recorded in the task. Code turns the joints. They need no clips.

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
