## Cycle 1
Build: pass (fresh build directory, Debug, no new warnings)

### Placement (decision needed, see Known limitations)
- Spot: `kTavernSpot = (-11.5, 1.65, -20.5)` (world), yaw `kTavernYaw` = 45° (door faces +x+z, toward the pond and the default camera). In `game/src/scene.hpp`.
- It stands on open lawn beyond the far left of the pond, behind TreeC. Floor y = 1.65 is the highest lawn under the footprint (lawn there is 1.08–1.65). The plinth reaches 1.05, so it meets the lawn all round. The lawn in front of the door is about 1.30, a 0.35 m step up onto the threshold.
- Margins, footprint to footprint, measured from above with a scratch tool against ruins.glb as the engine loads it:
  - nearest ruins.glb object: TreeCCrown, 1.03 m. Nearest object standing on the ground: RimStone.006 (pond rim), 2.08 m. Eaves, chimney and sign to TreeCCrown: 0.53 m.
  - campfire: 14.50 m from the fire's centre, 13.54 m from its dirt patch rim.
  - hand-placed fox beds: nearest 15.74 m. No fox route reaches more than about 6.6 m from its bed (`Fox::kMaxReach` 5 m plus the body box's 1.53 m corner plus a heightmap cell), so the tavern is at least 9.1 m from every route.
- The scene bounds are unchanged (-76.37, -3.40, -76.37 .. 76.37, 16.84, 76.37), so the sun's shadow map is fitted exactly as before.
- In the default view (yaw 180), the tavern's door and the front wall round it show in the top-left corner (screen x ≈ 0–200, y ≈ 0–110 at 960×540). The rest of the building is out of frame above.

### Changes
- engine/include/pxe/mesh.hpp: adds `kSurfWood` (7), `kSurfPlaster` (8) and `kSurfRoof` (9). `kSurfCount` is now 10.
- engine/src/gltf.cpp, gltf.hpp: `surfaceFor` maps the material names `wood*`, `plaster*` and `roof*`.
- engine/shaders/common.glsl: `kSurfaces` is 10, with the new surface constants. The surface code in the normal target's alpha is now `(surface + 1) / 16` (`kSurfaceScale`), because `/8` could not hold 10 surfaces plus the leaf marker. Adds `flatFaced()`.
- engine/shaders/mesh.frag, water.frag: use the /16 surface code. Wood, plaster and roof take `toonToneFlat`, like stone.
- engine/shaders/edge.frag: decodes /16, with the leaf sprite read as 15. The fire and firefly band arrays are sized by `kSurfaces`. Wood, plaster and roof take stone's rims and outlines and its hard rain splashes (`built()`).
- engine/shaders/grass.frag: comment only.
- engine/include/pxe/scene.hpp: adds `Scene::shadowOnly` (meshes drawn only into the sun/moon shadow map and lightning's) and `Scene::indoors` (no rain drawn by the edge pass, no cloud shadows, no god rays).
- engine/src/renderer.cpp, renderer.hpp:
  - Shadow-only meshes are drawn into the sun and flash shadow maps only. They are left out of the main, reflection and edge passes and the fire cubes.
  - `indoors` zeroes cloud strength and rays for the frame, and gives the edge pass `uRain` 0. The toon shaders still get the rain's overcast.
  - `upload()` no longer resets the fire grid, so a grass fire stays with the scene it burns in. `FireGrid::advance` already rebuilds it for a different scene.
  - New `quenchGrassFire()`.
- engine/src/firegrid.cpp, firegrid.hpp: new `FireGrid::quench()`. It clears fire, embers, crown fire and warmth, life, glow and patch readbacks with masked clears on the GPU. Fuel, wetness and the burn texture are kept, so burnt grass stays burnt.
- engine/src/gl.hpp: loads `glColorMaski`.
- engine/src/heightmap.cpp, heightmap.hpp: `HeightMap::build` takes an optional `overhead(xz, y)` predicate. A surface over a cell is left out of that cell where the predicate holds. This keeps door heads, walls and rails out of the ground only over the doorway cells, so the walls either side stay solid. No step-height fudging.
- engine/include/pxe/camera.hpp: `restingYawRadians()`, the yaw a turn is heading to, used to restore the yaw on leaving.
- game/src/scene.hpp, scene.cpp:
  - Day, golden, twilight and night palettes for Wood, Plaster and Roof.
  - Tavern placement constants with the margins documented. `kTavernFootprint`, `tavernPoint`, `tavernLocal` and `inTavernFootprint`.
  - The exterior is added in `layOutRuins` after the candles, so it is in `SceneLayout::solids`, the meshes and the shadows.
  - Shelter is built from Stone plus Roof.
  - Ground is built from every solid surface except Roof, with the overhead filter: faces higher than 3.3 m over the floor are dropped over the doorway (x ±1.1, z -0.25..0.85) and the vestibule (x ±1.9, z -3..-0.25).
  - Grass blades and tufts inside the footprint are dropped after scattering. Firefly homes inside it are dropped too.
  - Candles on the lawn, extra fires and the shelter test's crown fire skip the footprint (none of their search areas reach it at this spot).
  - New `buildTavern()` ("tavern" scene):
    - routes nodes ending `_ShadowOnly` to `scene.shadowOnly`;
    - shelter = Stone + all shadow-only geometry;
    - ground = visible geometry only, with the inside door's head cleared over x 8..10.8, z -4.1..-1.9;
    - hearth `FireSite` at (-3.6, 0.41, 5.25), size 1, and six candle fires (size 0.2) at the assets.md points; all report sheltered;
    - yaw 45, `indoors`, the ruins' sun turned into the room's frame, no horizon ridge.
  - Bounds and tavern triangle count are logged.
- game/src/tavern.hpp, tavern.cpp (new): the six lore passages (`kTavernLore`) and the room's camera box. Also:
  - `Doorway` for each scene: hinge, door yaw, doorway centre, closed leaf, trigger zone, arrival point and facing.
  - `tavernBlockers()`: 13 chairs and 5 stools as circles at their assets.md radius, plus the two near walls (-x, -z) as capsules, because those walls are shadow-only and their stubs are below a step.
  - `TavernDoor`: one shared model loaded with `SkinnedModel::load(path, 12)`, which throws if there is no `hinge` joint. The hinge turns about +Y by 100° with smoothstep easing over 0.4 s, toward open while the hero is within 3 m of the doorway centre. Turning back continues from the current angle. The leaf blocks the hero (`Blocker`, radius 0.06) until the door is fully open. `holdOpen()` serves `--tavern-door-open`.
- game/src/settings.hpp, settings.cpp: `LoadingScreen` overlay drawn with ImGui on the foreground draw list (over the menu too): a near-black plate, the title, and the passage wrapped under it, all faded by `opacity`.
- game/src/main.cpp:
  - `--scene tavern` and `--tavern-door-open` (help and parsing).
  - Ruins/hills creation is factored into `Outdoors` + `buildOutdoors()`: fox model, layout, beds, scene and fox spawn. It is built in place, at start or lazily on the first exit when starting inside.
  - `framed()` clamps the indoor camera target in screen space so the whole room box stays in view. `heroStart()` puts the hero at the inside door in the tavern.
  - Doors are posed and updated, and their leaf blocks the hero, in the bench, captures and play.
  - The interactive loop now has `inside`, and a `Passage` for the loading screen:
    - entering the trigger zone from outside it starts the passage;
    - one frame of the screen is drawn alone, then the other scene is loaded on the next frame;
    - the screen stays up until both 2.0 s (wall clock) and the load are done, then fades over 0.3 s;
    - the hero gets no input while the screen is up.
  - Going in: saves the camera's resting yaw and the ruins' clock fires and next id, quenches the grass fire, uploads the tavern, places its fires, spawns the hero at (5.4, -3.3) facing -x, and locks yaw 45.
  - Going out: restores the fires and yaw, re-uploads the ruins (or builds them the first time), and spawns the hero 2.3 m in front of the door facing away.
  - Indoors: Q/E and bumpers ignored; brushes, fox tool, G and fireball do nothing. Foxes and the fireball are not updated or drawn, the camera follows the hero through `framed()`, and wind and rain volumes are ×0.4.
- game/CMakeLists.txt: adds tavern.cpp.
- README.md: describes the tavern, the new flags, the tavern build scripts, and the Q/E note.

### Issues addressed
- None (cycle 1).

### Burnt grass
Burnt grass persists. On entering, `Renderer::quenchGrassFire()` puts out every flame, ember and burning crown but keeps the fire grid's fuel and burn texture. The grid stays bound to the ruins scene, which never moves, so coming back shows the burnt patch with no flames. Checked with a temporary scripted run: light the lawn, go in, come out, capture. The burnt patch was there and nothing was burning.

### Captures
- With the tavern moved far out of view (temporary env override, since removed), the default ruins capture was pixel-identical to a baseline built from HEAD. So the surface count change, the /16 surface encoding, the edge changes and the always-present door actor do not alter existing content.
- Compared against HEAD with a 30-capture suite: every ruins capture from the default view or a rotated or shifted view changed. In each, the tavern is visible: its door and front wall at the top left, its shadow over the lawn there, and its reflection in the pond. The diff boxes are confined to that region (default: x 0..541, y 0..198 of 960×540).
  - The suite covered default, dawn, dusk, night, rain, drizzle, flash, candles + shelter test, extra fires, fireflies, fox, fox-wake + fox-walk, foxes 8, hero, hero-move, grass fire, fireball, brush, no-edges, test-colours, --target, --slide, and yaw 45/90/135/225/270/315.
  - `--foxes 20` also changed elsewhere: bed search now avoids the tavern (as the spec asks), so one bed near it moved and the beds after it shifted (364 grass sprites dropped instead of 366). `--foxes 8` placed the same beds.
  - `--scene hills`: unchanged.
- So every docs/compare image of the ruins taken from the default framing would now show the tavern in the top-left corner.

### How to playtest
- Default play: the door is at the top left. Walk round the left side of the pond to it. The door swings in as you come within 3 m and shuts once you are farther. Turn back mid-swing and it should reverse smoothly. You can't pass it until it is fully open.
- Walk into the dark vestibule. The loading screen should show "The Sleeping Fox" and a random passage for about 2 s, then fade into the room. The hero is just inside the right-hand door, facing into the room, and the camera yaw is 45. Q/E should do nothing.
- In the room, walk into chairs, stools, tables, the bar, barrels, shelves, the hearth and the near (cut-away) walls. All should stop you. Watch window light patches move with time (try 9:00, 15:00, 17:30). At night (22:30) only the hearth and candles light it.
- Indoors, set rain to a downpour (R): no streaks or splashes, the colours dim, the audio drops to 40%, and lightning (L) lights the room through the windows with thunder. 1/2/3 brushes, 4, G and F should do nothing.
- Walk out through the inside door onto the step. After the loading screen you should be on the lawn in front of the tavern, facing away, with the camera yaw back to what it was. The foxes and any fireball should continue from where they were.
- Light the lawn (G) near the tavern, go in, come back out: the fire should be out and the burnt patch still there.
- `--scene tavern` starts inside at the door. Walking out builds the ruins behind the loading screen (slow in Debug, about 15 s; the screen stays up until done).
- Captures: `--scene tavern --hour 9 --capture a.png`, `--tavern-door-open --target -10,2,-22 --capture b.png`, `--scene tavern --hero-move 90,1.5,1 --capture c.png` (walks out of the open inside door), `--bench 600 --scene tavern --hero`.
- `--no-hero`: the doors never open.

### Known limitations
- **Placement visibility (needs a decision).** A search over the lawn (0.5 m steps, yaw in 15° steps) checked footprint clearance ≥ 0.3 m from ruins.glb, the campfire, and the fox beds plus their route reach, and that it sits on open lawn with no more than 0.6 m of lawn variation under it. No spot puts most of the 17.2 × 13.2 m building inside the default view. The best spot whose door faces the camera, the one chosen, shows only the door and part of the front wall in the top-left corner. The task says to stop rather than move a fox. I read "visible" as "can be seen from the default camera" and went ahead, but if the user wants the tavern prominent in the default view, that needs a change to the constraints, the default framing, or the asset's size.
- The hanging sign (x ≈ 3 local, out to z 1.77, y 2.75–4.35) is in `scene.ground`, so it blocks a 0.2 m strip of the façade at head height. The hero can't walk along the front wall under it; it has to go round.
- The open door leaf has no collision, so the hero can overlap it (the spec only asks for blocking while it is closed).
- Indoors, the camera clamp keeps the whole room in view, so with the hero near the door the room sits on the right of the screen with black on the left. With `--no-hero` indoors the camera pans freely, unclamped.
- While the hero is inside, all the ruins' clock fires (the campfire too, not only the fireball) are held as they were. Rain does not burn the campfire down while you are away. The tavern's hearth and candles are re-placed, fully lit, on each entry.
- Under heavy rain, the existing toon rule fades every cast shadow, the room's roof shadow included. Indoors in a downpour the room is lit evenly, dimmed only by the overcast, as outside.
- Wood, plaster and roof take stone's rims, so the interior's V-grooved plank floor shows many thin light lines. It reads as planks, but the user may find it busy.
- The loading screen uses ImGui's default bitmap font, scaled.
- I could not drive real keyboard input. The interactive passages were exercised with a temporary scripted trigger (since removed): ruins → tavern and tavern → ruins, including the lazy ruins build. Walking through both doorways was checked with `--hero-move` captures.
- No asset requests raised.

## Cycle 2
Build: pass (fresh build directory, Debug; the only warning is the existing one from third_party/stb_image_write.h)

### Placement
- **Spot: `kTavernSpot = (-11.75, 0.235, -0.25)`, yaw `kTavernYaw` = 5°**, door toward the default camera. In `game/src/scene.hpp`, with the figures below in its comment.
- How it was settled: a new search tool (scratchpad `fit/fit4.cpp`) with the real rebuilt assets. It loads the rebuilt `ruins.glb` and `tavern_exterior.glb`, uses the assets.md footprint polygon and overhang outlines (eaves, chimney cap, sign), and frames the exterior by projecting every one of its vertices, with the foundation clipped where it meets the lawn. It scanned x -16..-8, z -5..3 at 0.25 m and yaw -20..20° at 2.5°: 532 spots pass every rule.
- It checks every placement rule:
  - **Frame box** (default 960×540 view, yaw 180): x 26–280, y 110–372 px. **Margins: left 26, top 110, right 680, bottom 168 px** (≥ 20 required).
    - Checked in-engine against a no-tavern render (noon): the tavern's pixels start at exactly y 110 and end at x 280. The left eave (x 26–45) is behind the crowns. Below y 372 the diff is its shadow and the cleared grass.
  - **Lawn:** all Grass under the footprint, no Dirt, -0.24 to 0.23 m (**0.47 m variation**, ≤ 0.6 required). Floor is 0.235, just above the highest lawn.
  - **Clearances, footprint to footprint from above:**
    - ruins.glb: **1.70 m**, the nearest being Paving.003.
    - Eaves, chimney and sign to crowns above floor + 4 m: **1.52 m**, to TreeCCrown.
    - Campfire: **0.72 m** beyond its 0.96 m patch reach, from the chimney base corner.
    - Fox #0: **3.27 m** beyond a 1.53 m circle round its bed, which is the far corner of `Fox::kBody*` and covers its curl and standing body through all clips.
  - **Tree crowns:** about **25% of the exterior's pixels are hidden** (TreeFCrown 16%, TreeECrown 10%), mostly the roof's left half and the front's left end. **The doorway, with a hero standing 1 m in front of it, is 46% hidden by TreeF's crown.**
- **Why not (-11, -1) exactly.** In the asset, the chimney is on the +X end. At (-11, -1, 5°) it comes within 0.3 m of the campfire's patch, so the spot fails.
  - The passing spots close to it, such as (-11.5, -1), sit on 0.55 m of lawn variation. That leaves a 0.55 m step onto the threshold, equal to the hero's 0.55 m step height.
  - Spots further toward the camera are flatter, but TreeF's crown hides the doorway more: 65% at (-11.25, 1.0), my first pick.
  - The chosen spot balances the two: a **0.47 m step** from the lawn (-0.24) onto the threshold, and 46% door occlusion. No passing spot near yaw 5° hides less than about 30%.
- Scene bounds are unchanged (-76.37..76.37), so the shadow map fit is as before.

### Changes
- game/src/scene.hpp:
  - Removed `kRuinsFoxSpot2`, `kRuinsFoxSpot3`, `kRuinsFoxYaw3` and their comments.
  - New `kTavernSpot` and `kTavernYaw` with the placement figures above.
  - `kTavernFootprint` is the revision-2 9 × 7 m polygon from assets.md.
- game/src/main.cpp: `FoxSet::kHandPlaced` is gone. Play now shows `kFirst` (fox #0 only), and the second and third beds are no longer pushed. Updated the comments and the `--fox` help line. `--foxes N`, `--fox`, `--add-fox` and the fox tool paths are untouched.
- game/src/fox.cpp: comment only ("the hand-placed bed").
- game/src/tavern.hpp:
  - `kRoomMin`/`kRoomMax` are now the new room: inner bounds ±11.3 × ±8.5, out to the walls' outer faces ±11.7 × ±8.9, up to the roof underside at 4.55. The camera clamp (`framed()`) uses them. At yaw 45 the room projects to 29.1 × 18.6 m and fits the view, so the clamp keeps it all in frame.
  - Blocker comment.
- game/src/tavern.cpp:
  - `tavernDoorway()`: hinge (11.3, 0, 0) at yaw +90°, centre (11.5, -1.1), closed leaf x 11.36 from z 0 to -2.2. The trigger zone is x 12.05..14.1 × z -2.2..0 (0.35 m out from the wall's outer face, across the opening). The hero arrives at (8.7, -1.1) facing -X.
  - `tavernBlockers()`:
    - the 17 chairs (r 0.467) and 6 stools (r 0.28) at the assets.md centres;
    - the brazier (8.6, -4.4, r 0.456);
    - the two benches as capsules along x from end to end (centre ± 1.8 m, radius 0.225), which covers each 3.6 × 0.45 m rectangle;
    - the -X and -Z walls as capsules on their mid-lines (x -11.5, z -8.7), corner to corner, r 0.2.
- game/src/scene.cpp:
  - Palettes: Wood and Plaster shadow tones are warm in all four phases, dimmed with the phase. Mid and lit tones and all other surfaces are unchanged.

    | Phase | Wood | Plaster |
    |---|---|---|
    | day | `4c3626` | `9c8a70` |
    | golden | `463022` | `947c66` |
    | twilight | `34261f` | `5c4e46` |
    | night | `221a1a` | `3e3432` |
  - `kRoomDoorway` is the new inside-door head region: x 11.3..14.1, z -2.4..0.2, jambs included.
  - `buildTavern`:
    - hearth fire at (-4.2, 0.41, 7.75), size 1;
    - **brazier fire** at (8.6, 1.16, -4.4), size 0.5;
    - 7 candle flames at the assets.md points. All report sheltered in the log.
    - The ground now leaves out `Chair_*`, `Stool_*`, `Bench_*`, `Brazier` and `WallStub_*` (`blockedNotGround`), which block through Blockers as the spec says. The stage, tables, bar, barrels, shelves, hearth and walls stay in the ground.
    - Why the stubs are out: in the ground, the 0.45 m stubs lifted the hero 0.45 m into the air whenever it stood against a near wall. Its four foot-corner ground samples reach the stub's cell. I found this in a capture: running into the -Z wall left the hero at y 0.45. It now stays at floor height.
- engine/shaders/edge.frag:
  - `timbered()` (Wood, Plaster, Roof): never rimmed. Every silhouette, upward-facing ones included, takes the ink outline. Creases such as plank grooves draw nothing.
  - `uSunRain` in point-light dimming (ISSUE-001).
- engine/shaders/common.glsl, engine/src/renderer.cpp: ISSUE-001, see below.
- README.md:
  - One fox in play (fox #0), with the three-fox text removed.
  - The tavern's new place, size and look.
  - The six props removed from the ruins (BushD, Rock, Rock.001, Flag.009/.011/.012 with their inset slabs, ground back to grass).
  - The new room (22.6 × 17 m, 2 m walkways, six tables with the trestle and benches, six stools, five barrels, brazier, stage, candles on every table and the bar).
  - The rain keeps the room in shade.
- docs/tasks/tavern/issues.md: ISSUE-001 set to fixed, with notes.

### Issues addressed
- ISSUE-001: indoors (`uIndoors`), rain no longer fades the room's shade. Window sun patches fade toward shade as rain rises and are gone in a downpour. Outdoor rendering is byte-identical (checked on 6 captures with the tavern out of view). The edge pass's fire and firefly dimming now uses the real rain level indoors too.

### Captures
Headless captures from the final build, in `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/031add42-8fdc-4ccb-87c2-6c47c358642c/scratchpad/c2/final/`:
- `default.png`: the default view, with the tavern on the left.
- `tavern_1000.png`, `tavern_2230.png`, `tavern_1000_rain1.png`: `--scene tavern` at 10:00, at 22:30, and at 10:00 with `--rain 1`.
  - At 10:00 no sun falls through the windows (the sun is on the far side of this orientation).
- `tavern_1500.png`, `tavern_1500_rain05.png`, `tavern_1500_rain1.png`: 15:00 dry, at rain 0.5 and at rain 1. These show ISSUE-001: the patches are full, then faint, then gone, while the room stays in shade.
- `exterior_walk_in.png`: the hero 1 s into walking from the arrival point into the open exterior door. Its head shows in the doorway, and TreeF's crown hides the rest.

Which captures changed (30-capture suite, current build against the same build with the tavern moved out of view, so the ruins.glb prop removals are excluded):
- **Every ruins capture that shows the tavern changed. Unchanged: `--scene hills`.**
- From the default framing, the diff boxes are confined to the tavern, its shadow and its cleared grass: x 45–456, y 110–408 (default, fox, foxes8, foxwake, grassfire, fireball, brush, drizzle, noedges, testcol); dawn, dusk, rain and night are narrower (x ≤ 279).
- Knock-ons:
  - `--extra-fires`, `--candles` and `--fireflies` skip the new footprint, so later fires, candles and fireflies shift (boxes up to x 891).
  - `--foxes 20` beds avoid the tavern, so later beds shift.
  - The rotated (`--yaw`), `--target`, `--slide` and hero captures show the tavern elsewhere in frame.
- Against HEAD, every ruins capture also differs by the removed props.
- Every `--scene tavern` capture changed: new room, new palette, no rims.

### How to playtest
- Default play: the tavern is on the left, beside the campfire.
  - Walk to its door, which faces the camera. It is partly behind the tree crown in front of it.
  - The door swings in within 3 m and you step up 0.47 m onto the threshold. Walk into the vestibule to trigger the loading screen.
- Only one fox (by the campfire) is in play. `--foxes 8`, `--add-fox` and key 4 still add foxes.
- Inside, you arrive at (8.7, -1.1) by the inside door, facing in, and the door opens on arrival then closes as you walk in. Try these:
  - Walk the main aisle west to the far wall.
  - Walk into the brazier (just left of the door, toward the near wall) and slide round it.
  - Walk into the trestle benches (you slide along them), chairs, stools, the stage edge, the hearth and the barrels.
  - Hug the near (cut-away) walls: the hero should stay at floor height.
- Check the walkways read as roomy, and that the plank floor and plaster read as quiet warm browns and beiges with dark outlines and no light lines.
- Weather and light indoors:
  - At 15:00, set heavy rain (R): the window patches fade out and nothing else brightens.
  - At night (22:30): the hearth, the brazier and 7 candles.
  - Lightning (L) still lights the room through the windows.
- Walk out through the inside door: you return in front of the tavern facing away, with the camera yaw restored.
- `--scene tavern`, `--tavern-door-open` and `--bench 600 --scene tavern --hero` (about 1.35 ms a frame here).

### Known limitations
- **Door visibility from the default yaw.** TreeF's crown hides about half of the doorway, and a hero walking into it, from yaw 180 (46% at this spot). Every passing spot near yaw 5° hides at least about 30%, so the placement rules can't avoid this. The hero is fully visible from other yaws (Q/E). If the user wants the door clear, it needs a decision: thin or remove TreeF (another ruins change), allow a larger threshold step, or accept a spot farther back.
- **Margins are modest:** 26 px on the left (rule ≥ 20) and 0.47 m of lawn variation (rule ≤ 0.6).
- **Twilight and night shadow tones** for Wood and Plaster are warm but dimmed to stay in key with those phases. Outdoors at night, the tavern's shaded faces read warm near-black among the navy scene. Worth a look in play.
- **Light rims removed.** Wood, Plaster and Roof lose light rims outdoors too, so the exterior's thatch ridge and timber tops now take only ink. The spec asks for this for these surfaces in general.
- **Wet rain highlights stay.** The rain's wet light line along "hard" surfaces' upper outlines still applies to the exterior's wood, plaster and thatch outdoors (it is a rain effect, not an edge rim). Indoors no rain is drawn.
- **The hanging sign is in `scene.ground`** (carried over from cycle 1). The hero can't walk under the sign board; it has to go round its post, which is 0.6 m clear of the door jamb.
- **Fox #0's walk choices may differ** from before. Some walk targets toward the tavern are now rejected. Not measured.
- Interactive passages (keyboard) were not driven. Walking through both doorways, the blockers and the door were checked with `--hero-move` captures.
- No asset requests raised.

## Cycle 3
Build: pass (fresh build directory, Debug; the only warnings are the existing ones: `third_party/stb/stb_image_write.h` sprintf and ld's duplicate `libglfw3.a`, both also in cycle 2's fresh build)

### Placement
- **Spot: `kTavernSpot = (-28.625, 0.06, 29.5)`, yaw `kTavernYaw` = 12.5° (0.21816616 rad).** In `game/src/scene.hpp`, with these figures in its comment.
- Where: alone on open lawn far out to the front left of the ruins (toward the camera and left in the default view), out of the default view. It is outside the 40 × 22.5 m default frame, so the default view is the original composition again.
- How it was settled: a new search tool, scratchpad `fit/fit5.cpp`, run against the restored `ruins.glb` (HEAD, 226 primitives) and the real `tavern_exterior.glb` (revision 2). It uses the assets.md footprint polygon and the overhang outlines (eaves -4.55..5.35 × -7.1..0.8, chimney cap, sign with its post).
  - Whole map (lawn bounds -76.4..76.6 in x and z) at 0.5 m and yaw -15..15° at 2.5°: 270,601 spots pass every hard rule, 233,308 of them with the full 4 m ring. Then 0.125 m steps round the best.
  - Ranking as asked: open ring (capped at 4 m) first, then lawn variation. Ties on variation (0.125 m at several spots) went to the smaller yaw, then the shorter route.
- **Hard rules (all pass):**
  - Door facing: 12.5° off straight at the default camera (rule ±15°).
  - Lawn: all Grass under the footprint, no Dirt, no gaps, -0.068 to 0.057 m (**0.125 m variation**, rule ≤ 0.6). Floor at 0.06, just over the highest lawn. The lawn 0.8 m before the door is at -0.04, so the step onto the threshold is **0.10 m**.
  - Reachable: a Dijkstra flood fill over `scene.ground`-equivalent cells (0.2 m; ruins.glb minus water and leaves) from the hero's play start (2, -6) (the default focus, which `heroStart` uses) to the cell 0.86 m in front of the doorway. It uses the hero's rules from `hero.cpp`: a 0.55 m step up, the four foot corners at ±0.24 m, the 0.32 m look-ahead probe, the deep-water rule, and diagonal moves only where both axis moves pass. The campfire and fox #0's bed are blocked out, and the tavern's footprint is dilated by the foot.
  - Clearances, seen from above, footprint, eaves, chimney and sign to:
    - everything in ruins.glb, crowns included: **6.83 m** (TreeECrown);
    - the campfire's patch, stones and logs (0.96 m reach): **32.9 m**;
    - fox #0's bed (1.534 m body circle): **35.0 m**.
- **Open ring: 4 m reached** (nothing of ruins.glb within 6.83 m of the eaves, chimney or sign). The nearest ground that isn't lawn is the map's edge, 8.6 m from the eaves at the left rear.
- **Campfire: ≥ 10 m reached** (32.9 m from its patch).
- **Walking route from the hero's start: 51.1 m** with the tavern itself in the way, 49.0 m without it, 47.4 m straight line. The start is behind and right of the tavern, so the route goes round its back and side to the door.
- Scene bounds are unchanged (-76.37, -3.40, -76.37 .. 76.37, 16.84, 76.37, from the build log). The tavern is inside them (top at y 9.19). `fitLight` fits the sun's shadow map to the bounds' sphere, so the new spot is covered at the same texel size. The noon captures show its shadow cast on the lawn.

### Changes
- game/src/scene.hpp: `kTavernSpot` (-28.625, 0.06, 29.5) and `kTavernYaw` 12.5°, with a rewritten comment (the rules, figures and route above).
- README.md: the intro and the Sleeping Fox paragraph now say where it stands (alone, far out front left, out of the default view, door to the camera), how the spot was found, and the ring, campfire and route figures. The text about the six removed ruins props is gone, and so is the sentence that fox #0's walks may differ (the tavern is now 35 m from its bed, beyond any walk).
- Nothing else needed changing. Everything else that depended on the spot goes through `kTavernSpot`/`kTavernYaw`/`inTavernFootprint`/`tavernPoint`: the doorway, door, trigger zone and arrival point, grass clearing, ground overheads and solids. The skips are still in place for `--extra-fires`, `--candles`, the shelter test and fireflies. They no longer trigger, because those scatter within about 15 m of the focus and the tavern is farther out. The same goes for `--foxes N` beds (focus ± 20 m).

### Issues addressed
- None named this cycle (no open issues). This cycle covers the "Placement (revised in cycle 3)" decision and the README criterion.

### Captures
In `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/031add42-8fdc-4ccb-87c2-6c47c358642c/scratchpad/c3/`:
- `default.png`: the default view (15:00). The tavern is out of frame. Byte-identical to HEAD's.
- `tavern_view.png` (15:00, the capture default) and `tavern_view_noon.png` (12:00): centred on the tavern at the default yaw (`--target -28.9,3,26.3`).
- `walk_in_0.6.png`, `walk_in_1.0.png`, `walk_in_1.4.png`: `--target -28.13,0.5,31.74 --hero-move -12.5,T --hour 12`. The hero starts at the exterior's arrival point, 2.3 m out, and runs straight at the door. At 0.6 s it is in the open doorway. At 1.0 s it is in the vestibule at local z -2.39 (log: hero at -29.29 0.06 27.20), just short of the vestibule's back wall. At 1.4 s it has slid along that wall. The door opened as it came.

Which captures changed: the 30-capture suite (`runcaps.sh`: default, six yaws, dawn, dusk, night, rain, drizzle, flash, candles + shelter test, extra fires, fireflies, fox, fox wake/walk, foxes 8 and 20, hero, hero move, grass fire, fireball, brush, hills, no edges, test colours, target, slide) from this build against a HEAD (pre-task) build, at `c3/suite_new` and `c3/suite_head`: **all 30 are byte-identical to HEAD.** With the props back and the tavern out of every one of those framings, the ruins look exactly as before the task. The `--scene tavern` captures are unaffected this cycle (no change to the interior or its code).

### How to playtest
- Start play. From the campfire area, head toward the bottom-left of the screen (toward the camera and left), about 50 m past TreeE and the paving. The tavern stands alone on the lawn with its door toward the camera.
- You come at it from behind. Walk round its right side (the chimney end) to the front. The door opens within 3 m, and the step onto the threshold is small (0.1 m). Walk into the vestibule to trigger the loading screen.
- Walk back out: you arrive 2.3 m in front of the door, facing away, with the camera yaw restored.
- Check its shadow and lighting at a few times of day (see Known limitations about the afternoon).

### Known limitations
- **It stands at the edge of the scene's lighting model and near the map's edge.** The search rules pick the most level 4 m-ring spot, and the flattest lawn is out at the edge.
  - The spot is about 47 m from the ruins' horizon centre (2, -6), just outside the 46 m ridge ring (`horizonRadius`) that casts the hills' shadow. From about 13:30 the whole area is in that ridge shadow, so at the capture default of 15:00 the tavern is in shade (`tavern_view.png`). It is fully lit in the morning and at noon (`tavern_view_noon.png`).
  - The map is a diamond, and its black edge (no ground) shows in the lower left of any view centred on the tavern, about 8.6 m from its eaves.
  - Neither breaks a hard rule. If the user wants it in the lit, well-framed part of the map, the search can add "inside the horizon ring" and "no map edge in the centred view" rules. A strong alternative that passes every hard rule with a 4 m ring, ≥ 10 m from the campfire, inside the ring and closer: **(-12.4, -25.9) at 12.5°**. That is behind the ruins (far side of the pond), with 0.235 m lawn variation, an 18.3 m campfire distance and a 27.6 m route (from the 0.5 m scan; not refined or checked in-engine).
- **Approach from behind.** The hero starts behind and right of it, so the player first sees its back and has to walk round to the door. This follows from the door-to-camera rule.
- Interactive passages (keyboard) weren't driven. Walking to and through the doorway was checked by the flood fill and the `--hero-move` captures.
- No asset requests raised.

## Cycle 4
Build: pass (fresh build directory, Debug, at `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/031add42-8fdc-4ccb-87c2-6c47c358642c/scratchpad/c4/build`; the only warnings are the existing ones: `third_party/stb/stb_image_write.h` sprintf and ld's duplicate `libglfw3.a`)

### Placement (trial of the cycle-3 alternative)
- **Spot: `kTavernSpot = (-11.4, 1.645, -25.9)`, yaw `kTavernYaw` = 12.5° (unchanged, 0.21816616 rad).** Behind the ruins, on the lawn beyond the pond on the left of the default view, about 24 m from the hero's start.
- Search: `fit5` (the cycle-3 tool, unchanged rules and the full 4 m ring) over x -15.4..-9.4, z -28.9..-22.9 (±3 m of (-12.4, -25.9)) at 0.25 m, yaw -15..15° at 2.5°: 7,973 spots pass every hard rule (152 fail clearance), 2,067 with the full 4 m ring. A second scratch tool, `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/031add42-8fdc-4ccb-87c2-6c47c358642c/scratchpad/c4/fit6.cpp` (fit5 plus a `prefs` mode), scored all 2,067 ring-4 spots on the three preferences:
  - map edge: rays for every 6th pixel of the 960 × 540 view at the default yaw (180°), pitch 30°, centred on the footprint's middle 3 m above the floor, marched onto the ruins' ground and water; a ray that hits nothing is the void past the map's edge;
  - hills' shadow: `horizonLight` from `common.glsl` reproduced on the CPU with the ruins' ridge (centre (2, -6), radius 46, height 12, relief 9) and `sunDirection` (peak 55°), over the footprint plus the 4 m ring at lawn level and over the walls 4 m above the floor;
  - approach: the start in front of the door (cosine of the angle between the door's normal and the direction to the start) and the walk with the tavern masked out compared with the walk without it.
  - The Pareto set over (least void, most lawn lit at 15:00, least variation) is 7 spots along one line; (-11.4, -25.9, 12.5°) is best on both void and 15:00 light. I picked it over (-12.4, -25.9), the most level spot, which has more void (779 against 674 samples) and less lit lawn at 15:00 (60% against 66%), for 0.056 m more lawn variation.
- **Hard rules (all pass):**
  - Door facing: 12.5° off straight at the default camera (rule ±15°).
  - Lawn: all Grass under the footprint, no Dirt, no gaps, 1.354 to 1.644 m (**0.291 m variation**, rule ≤ 0.6). Floor at 1.645. The lawn 0.8 m before the door is at 1.599, so the step onto the threshold is **0.04 m**.
  - Reachable: the cycle-3 flood fill (hero's step, feet, look-ahead and water rules) reaches the door front from (2, -6) in **27.2 m**, the same with the tavern masked out (straight line 23.2 m; the extra is the way round the pond).
  - Clearances, seen from above (footprint, eaves, chimney, sign): ruins.glb **4.01 m** (TreeGCrown; then PondCrown 4.10, Water 4.42, RimStone.005 4.37); campfire patch **18.1 m**; fox #0's bed **17.4 m** (fox walks reach 5 m from the bed, so they don't come near).
- **Open ring: 4 m reached. Campfire: ≥ 10 m reached (18.1 m).**
- Scene bounds unchanged (log: -76.37, -3.40, -76.37 .. 76.37, 16.84, 76.37); the roof top at about y 10.8 is inside.

### The three preferences: one met, two not
None of the 2,067 ring-4 spots in the ±3 m window meets all three. As asked, I didn't widen the search.
1. **No map edge in the centred default-yaw view: not met.** No spot in the window has none: the fewest void samples is 674 of 14,400 (4.7%), at this spot. The void is a black triangle at the top left of `tavern_view_*.png`, where the diamond-shaped map ends behind and left of the tavern, about 25 m or more back from it. (The cycle-3 spot scored 336 by the same measure.)
2. **Out of the hills' shadow at 06, 09, 12, 15 and 18: partly met.**
   - 09:00 and 12:00: all of it lit, lawn and walls.
   - 15:00: the walls are all lit, and so is the lawn before the door. The ridge's shadow covers 34% of the 4 m ring's lawn, a wedge at the back left that just takes in the footprint's back left corner. This is the most lit lawn at 15:00 of any spot in the window (66%).
   - 06:00 and 18:00: in shade, but no spot anywhere can meet these. At those hours the sun is exactly on the horizon (`sunDirection` gives y = 0), so `horizonLight` shades the whole map, the campfire and the focus included.
   - The hills' shadow reaches the door at about 15:33, against 16:20 at the campfire and 13:36 at the cycle-3 door. In the morning the door is lit from 06:43, the campfire from 07:13.
   - Tree shadow is separate from the hills' shadow and isn't covered by this preference. At 15:00 the lawn before the door is in the cast shadow of the big tree to its left (`tavern_view_1500.png`). At 12:00 that tree and TreeG shade the lawn in front of the door.
3. **Approach to the door side without walking round the building: met.** The hero's start is in front of the door (cosine 0.93), and the walking route is the same 27.2 m with or without the tavern in the way. The hero goes round the pond and straight up to the door.

### Changes
- game/src/scene.hpp: `kTavernSpot` (-11.4, 1.645, -25.9); `kTavernYaw` unchanged at 12.5°. The comment is rewritten with the figures above.
- README.md: the intro sentence and the Sleeping Fox paragraph give the new place, figures and walk.
- Nothing else changed. Everything placed relative to the tavern (doorway, door, trigger zone, arrival point, grass clearing, ground overheads, solids, and the scatter skips) follows `kTavernSpot`/`kTavernYaw`. The log shows 567 grass sprites dropped under the tavern.

### Issues addressed
- None named this cycle (no open issues). This cycle is the placement trial the orchestrator asked for.

### Captures
In `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/031add42-8fdc-4ccb-87c2-6c47c358642c/scratchpad/c4/`:
- `tavern_view_1200.png`, `tavern_view_1500.png`: centred on the tavern at the default yaw (`--target -11.69,4.6,-29.06`).
- `default.png`: the default view at 15:00. **The foot of the tavern's front wall now shows at the top left edge** (about x 130–270, y 0–20 px), so the default view is no longer byte-identical to HEAD. I didn't re-run the 30-capture suite. Any framing that reaches about 20 m behind and 13 m left of the focus will also show it.
- `walk_in_0.6.png`, `walk_in_1.0.png`, `walk_in_1.4.png`: `--target -10.90,2.08,-23.655 --hero-move -12.5,T --hour 12`. The hero starts at the arrival point, 2.3 m out, and runs at the door. At 0.6 s it is in the open doorway (-11.55, 1.64, -26.57). At 1.0 s it is in the vestibule at local z -2.39 (-12.06, 1.64, -28.20). At 1.4 s it has slid along the vestibule's back wall (-12.58, 1.64, -28.20).

### How to playtest
- Start play. From the campfire, go up and left past the pond (away from the camera). The tavern is just beyond the top left of the default view, its door toward you. The walk is about 27 m and doesn't go round the building.
- Walk in. The door opens within 3 m, and the step onto the threshold is tiny (0.04 m). Walk out again: you arrive 2.3 m in front of the door, facing the pond.
- Compare it with the cycle-3 spot (-28.625, 0.06, 29.5): one constant change to go back.
- Look at it around 15:00–16:00 to see the hills' shadow line crossing it, and from the default view to see the strip of it at the top left.

### Known limitations
- The centred view shows the map's edge at the top left. No spot in the ±3 m window avoids that.
- **Crowns hide much of the building from the camera.** In plan they clear it by 4 m, but at 30° pitch the big tree to its left (in front, toward the camera) covers the left half of the front wall and roof, and TreeG's crown covers the right end and chimney. The door itself stays visible in every capture (`tavern_view_*.png`). The 4 m ring rule is measured in plan, so it doesn't catch this.
- The default view changed: a sliver of the tavern now shows at its top left. The user may or may not want that. The task says the tavern needn't be in the default view, but doesn't say it must stay out of it.
- The lawn under it varies by 0.291 m (the cycle-3 spot: 0.125 m). It is well inside the 0.6 m rule.
- Interactive play (keyboard) wasn't driven. The walk and the doorway were checked by the flood fill and the `--hero-move` captures.
- No asset requests raised.
