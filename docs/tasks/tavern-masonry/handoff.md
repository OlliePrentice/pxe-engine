## Cycle 1
Build: pass

Fresh Debug build in a new directory: no errors, and no warnings from the files touched. The only warnings are the existing ones from `third_party/stb` and the duplicate-library linker note.

### Changes
- `engine/include/pxe/skinned.hpp`: `ActorMaterial` gets `bool crease = false`. A material with it set is inked along its sharp creases in the edge pass. Defaults to off, so every existing actor material is unchanged.
- `engine/src/renderer.cpp`: the per-slot `uActorGlow` int array becomes `uActorFlags` (bit 1 glow, bit 2 crease). It is one array, as before, so the mesh shader's uniform count is unchanged.
- `engine/shaders/mesh.frag`:
  - Glow is now read from bit 1 of `uActorFlags`. The result is identical.
  - An actor pixel whose slot has the crease bit writes surface code 11 (`kSurfActorCrease`) to the normal target. Every other actor pixel still writes Stone (2).
  - Code 11 is free (scene surfaces are 0–10, and the leaf sprite is 15), and it encodes as (11 + 1) / 16 = 0.75. That survives the RGBA8 target exactly, and it is below the 0.99 sprite cut-off.
- `engine/shaders/edge.frag`: `main` decodes code 11 into a flag, `actorCrease`, and turns the surface into Stone. So rims, outlines, point lights, rain haze and splashes treat it exactly as they treated every actor before. `edges()` adds crease ink for flagged pixels:
  - The test is the Oak/Roof one: a 4-neighbour more than 40° apart (`kCreaseCos`) with no silhouette step.
  - The one-pixel tie-break is the same, with the partner being another code-11 pixel. So wood against grain falls on the side turned further from the camera. Against anything else (the iron, the frame, the threshold, the wall), the ink falls on the door side.
  - The strength is the Roof treatment indoors and outdoors: `min(mix(col, ink, 0.65), col * 0.8)`.
  - For neighbouring Oak and Roof pixels, code 11 is "not creased", just as Stone was, so their ink placement is unchanged.
- `engine/shaders/common.glsl`: a comment now documents code 11 in the surface encoding.
- `game/src/tavern.hpp` / `tavern.cpp`:
  - New material constants `kWood`, `kIron`, `kGrain` and `kMaterials = 3`, with comments for slots 12–14.
  - `loadModel` throws if the door glb doesn't have exactly three materials.
  - `pose` sets `crease` on the Door_Wood and Door_Grain slots (12 and 14) and leaves it off on Door_Iron (13).
- `README.md`: door slots are now 12 to 14, and the door's crease ink is described. Added the masonry, faceted render and spalled patches to the tavern paragraph.

### Criteria checked
- **Slots.** The door loads with `SkinnedModel::load(path, 12)`. `tavern_door.glb` materials are `Door_Wood`, `Door_Iron`, `Door_Grain`, so they land in 12, 13 and 14.
  - Hero: 6 materials in 0–5, plus the smear core in 7.
  - Fox: 4 materials in 8–11.
  - Nothing else writes slot 14.
- **Captures, this build against HEAD's (00f00a5) build.** HEAD's code is unchanged, with the same new assets. Pixels that differ:
  - Exterior at `--target -11.4,3,-28`, hours 10 and 22: 562, all on the door (plank joints, grain, door edge against frame and threshold).
  - Interior (`--scene tavern`) at 10 and 22: 470, all on the inside door.
  - Default ruins at 10 and 22: 123, all in the door at the top-left corner.
  - Hills: 0.
  - Hero in the tavern, hero + fox in the ruins, and hero mid-attack with the smear: differences only in the door region. Hero, fox and smear pixels are identical.
- **New glbs integrate with no code change.** Checked against a build of 799b846, which has the revision-4/3 assets and the same code:
  - **Walking in.** The hero ran into the doorway from 2.5 m in front, along four parallel lines. Its height at every sampled moment (0.25–2.5 s) and its end position in the vestibule are identical to the old assets.
  - **Ground heights**, sampled from the glbs with the engine's HeightMap rule (max height at the cell centre, Roof excluded outside, shadow-only excluded inside, door headroom rule):
    - Doorway and vestibule cells: unchanged.
    - Threshold front lip (local z 0.42–0.47): the 0.05 m strip outside the mortar bed now reads the face course's top (about -0.28 local) instead of 0. In the game the lawn there (about -0.04) is the higher surface, so the walk is unchanged.
    - Plinth tops: within -0.05 to +0.04.
    - Hearth slab and the floor round it: 0 to -0.05 at 0.2 m cells.
    - No cell became climbable that wasn't.
    - Room doorway: unchanged.
  - **Scene build logs.** Fires, candles, sconces and the brazier are at the same positions, and all are sheltered as before: the hearth is still sheltered by the breast stones. The `--shelter-test` output is identical. The only log changes are the triangle and primitive counts, and grass dropped under the tavern (2,088 → 2,089 in total, split differently between "under stone" and "under the tavern").
  - `kTavernSpot`, `kTavernYaw`, blockers, doorway rects, fire, candle, sconce and window constants are not touched.

### Issues addressed
- None (cycle 1).

### How to playtest
- Walk to the Sleeping Fox's door by day and at night.
  - The plank joints and every grain streak and knot now have dark ink lines, clearly visible on the dark wood.
  - The iron straps and ring are outlined as before, with no crease ink on the iron itself.
  - The door's bottom edge on the threshold and its sides against the frame take a one-pixel line on the door side.
- Let the door swing open and closed. Ink should stay one pixel wide on the leaf through the swing, with no flicker on the plank faces.
- Go inside. The inside door shows its back face. Each grain streak and knot is outlined (ink on the plank beside the side turned away from the camera, as in the designer's back-view preview), at the same heavy strength as outside.
- Check the hero and a fox next to the door: their outlines and rims look exactly as before, including the sword smear and glowing eyes.
- Look at the masonry: plinth, threshold, chimney, the three front-wall patches, the interior hearth and the two interior patches. Walk over the threshold and along the hearth's slab. There should be no hitches or new bumps.
- Try rain outside near the door: splashes and the wet haze on the door are as on stone before.

### Known limitations
- On the inside door's back face, the one-pixel rule inks both sides of each streak: the far side on the streak and the near side on the plank. At 24 px/m each streak therefore reads as a dark outlined band, not a single line. This is the placement the spec's test gives, and it matches `tavern_door_grain.png` (back view, solid ink). The heavier 0.65 mix makes it bolder than that preview's 0.35 panel.
- The back face has no plank joints to ink, because the V-grooves are on the front only in the model. That is an asset matter if it is wanted, and is not in this task's criteria.
- The door edge lines against the frame and threshold are a side effect of the "against anything else, on this side" rule, the same as oak meeting the floor.
