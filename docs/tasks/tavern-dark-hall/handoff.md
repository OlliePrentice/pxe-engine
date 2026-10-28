## Cycle 1
Build: pass (`cmake --preset check --fresh`, `cmake --build --preset check --clean-first`; the only warning is stb_image_write's `sprintf`)

### Changes
- engine/include/pxe/scene.hpp: new `BoxFace` enum and `DarkRegion` struct (centre, half extents, yaw about +y, open face, start and end strength, fade distance), documented in generic terms; `Scene::darkRegions`, empty by default.
- engine/src/renderer.cpp: `bindDarkRegions(prog, scene)` flattens up to `kDarkRegions` (8) regions into uniforms each frame: centre, the box's own +x and +z in the world, half extents, the open face's outward normal in the box's frame, and (start, end, fade). Called from `bindToon` (mesh, grass and water programs; only mesh.frag reads them) and for the edge pass. Nothing is kept between frames.
- engine/shaders/common.glsl: the region uniforms and `darkAt(wpos)`: -1 outside every region, otherwise start strength at the open face, linear to end strength at the fade distance (depth measured straight in along the face's normal), held beyond; the darker share where regions overlap.
- engine/shaders/mesh.frag: inside a region a scene surface is `bandColour(surface, 1) * dark` (its mid colour for the time of day, unlit), replacing sun, moon, fill, sky light, lightning, cloud shade and firelight. Actors likewise take `colour * uActorTint[1] * dark`. Both checks run only when `uDarkCount > 0`.
- engine/shaders/edge.frag: its own copy of the uniforms and `darkAt` (the edge pass doesn't include common.glsl, and already duplicates the cloud functions the same way). Light rims and bark strokes inside a region mix toward `uRim * dark` instead of `uRim`. Point lights (fireflies, candles, the grass fire's glow) skip pixels inside a region. Ink outlines and crease ink are unchanged. The original rim and stroke returns are untouched for pixels outside a region.
- game/src/scene.cpp: `kTavernVestibule` now runs to z -6.05. New constants `kHallDarkStart` (0.35), `kHallDarkEnd` (0.05), `kHallDarkFade` (4 m) and `kHallDarkMargin` (0.05 m). `buildRuins` adds one `pxe::DarkRegion` over the hall, placed with `tavernPoint`/`kTavernYaw`, open face +Z.
- game/src/tavern.cpp, tavern.hpp: `ruinsBlockers()` returns the invisible wall on the old back wall's line: a capsule from local (-1.9, -3.1) to (1.9, -3.1), radius 0.1, which covers z -3.0 to -3.2. Comments now say "hall" rather than "vestibule".
- game/src/main.cpp: `blockersIn` adds `ruinsBlockers()` to the outdoor blockers next to the door's leaf (outdoors only, and not in the hills).
- docs/engine-guide.md: a `darkRegions` bullet in "Building a Scene" and `DarkRegion`/`BoxFace` in the header reference.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::BoxFace`, `pxe::DarkRegion`, `pxe::Scene::darkRegions`; `bindDarkRegions` in renderer.cpp; `darkAt` in common.glsl and edge.frag. No game names. `git diff -U0 -- engine | grep '^+' | grep -iE "tavern|hall|vestibule|fox|hero|ruins"` finds nothing.
- Game: `buildRuins` creates the region in `built.scene.darkRegions`; `game::ruinsBlockers()` (tavern.cpp) supplies the blocker, and main.cpp's `blockersIn` hands it to the hero each frame.
- Misplaced code touched: none.

### Region placement
In the exterior's local frame the region is x -1.95..1.95, y -0.05..4.45, z -6.10..-0.25, so the centre is (0, 2.2, -3.175) and the half extents (1.95, 2.25, 2.925). The open face is exactly on the front wall's inside face (z -0.25), as the spec gives. The other five faces are pushed out by `kHallDarkMargin` (5 cm) past the floor top (y 0), the side walls' inner faces (x ±1.9), the walls' top (4.4) and the rear wall's inside face (-6.05). A surface lying exactly in a face's plane would otherwise fall inside or outside pixel by pixel through float rounding (the floor at y 0 against a face at y 0). The margin lies inside the floor slab, the side walls and the rear wall, so nothing outside the house is reached. The engine header and guide document this rule for any game.

### Verification
Every capture was compared with `cmp` against two builds of base 8edf83b (Debug, `git archive` into a scratch directory): the base as committed, and the base with HEAD's revised `tavern_exterior.glb` dropped in.
- `--capture` default (ruins): byte-identical to both.
- `--scene hills`: byte-identical to both.
- `--scene tavern`: byte-identical to both.
- `--hour 21 --foxes 3`: byte-identical to both.
- `--hour 21 --candles --shelter-test --extra-fires 6`: byte-identical to both.
- `--self-test`: PASS. Stdout is identical to both bases.
- Logs: identical to base-with-new-asset apart from the output file line. Against the plain base the only other line that differs is `tavern 9117 triangles` -> `9105`, which is the designer's revision 6 dropping the back wall's 12 triangles.
- The default ruins captures need no explanation: they match even though the ruins now have a region. The door is shut and every pixel inside the box is hidden. Hills and tavern have an empty list.
- Open door from outside (`--target -11.4,2.5,-25.4 --yaw 180 --tavern-door-open`, `--hour 14` and `--hour 21`): the doorway shows the plank floor running back. At 14:00 the floor is (47,36,27) at the threshold, which is 35% of day Wood mid 0x86664e, and fades evenly to (7,5,4), 5%, from about 4 m in to the back. At 21:00 it does the same from the night palette's Wood mid, so it is near-black throughout. No grey square. Against the base, the changed pixels are only inside the opening: a bounding box of (456,212)-(505,282), 3147 pixels, at both hours.

### How to playtest
- Walk up to the Sleeping Fox's door by day and by night. As it opens, the doorway should show a dark plank floor: dim just inside the threshold, fading to near-black further in, with no grey wall. It should look the same at every hour apart from the palette's tint. Campfire and candle light should not brighten it.
- Stand in the doorway and step in. The parts of the hero past the wall's inside face should go dark, and the parts still in the doorway should stay lit. There will be a hard line where the hero crosses the open face (see limitations).
- In the hall, walk along either side, outside the passage zone at x ±1.1, and try to go deeper. The hero should stop about where the old back wall stopped it, roughly 3 m in from the wall's inside face. The passage into the room should still trigger as before through the middle.
- Check that the open door leaf, lying against the hall's left wall, reads as dark, and that the roof over the hall still lets the hero walk in (the headroom).

### Known limitations
- Glowing actor materials (`ActorMaterial::glow`: eyes, a blade, the sword smear) are left glowing inside a region. I read the spec's "replaces sun, moon, fill, sky light and firelight" as covering light, not emissive colour. A fireball or glowing blade going black in the hall would look wrong. If the orchestrator wants glows darkened too, it is a one-line change in `actorColour`.
- The fade is a smooth per-pixel gradient (linear, as specified), not stepped into bands. It reads cleanly in the captures, but it is the one smooth ramp in a toon-shaded frame.
- Rain is not changed inside a region. The edge pass's murk, splashes and streaks still draw over the hall floor in rain, as they already did over any roofed floor outdoors (the edge pass has no shelter test). God rays and fire, smoke and firefly sprites are unchanged too.
- Grass and water shaders don't read the regions (the spec names the scene, actor and edge shaders). Grass or water inside a region would stay lit. The hall has neither.
- A character straddling the open face is split: the part inside is dark, the part outside lit, with a hard line at the face. The spec asks for this, but it is worth a look in play.
- The hall is open on the ground map back to the rear wall, so anything that follows the ground but not the hero's blockers (a rolling fireball) can go deeper into the hall than before. I have not checked this in play.
- README.md still mentions "the dark vestibule" in the tavern paragraph. I left the README for the orchestrator's doc pass.
