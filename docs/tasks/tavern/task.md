# The Sleeping Fox tavern
Type: feature
Status: open
Cycle: 3

## Goal
Add an enterable tavern, the Sleeping Fox, to the ruins. Its door swings open on its own when the hero comes near and closes again when the hero walks away. Walking through the open doorway shows a short loading screen with a random lore passage, then loads a new interior scene: one roomy hall with a bar, tables, chairs, barrels, shelves, a burning hearth, a brazier and candles. Walking back out through the inside door (which also opens on its own) shows the loading screen again and returns the hero to the ruins outside the door. The ruins stay in memory, so returning doesn't rebuild them. The tavern is bigger inside than out.

## Decisions
- **Placement (revised in cycle 3).** The user found the cycle-2 spot too cramped. The tavern can stand anywhere on the map: it doesn't need to be in the default view or near the campfire ("we want the player to explore"). The exterior stays 9 × 7 m. The developer finds the spot by search under these rules:
  - **Hard rules:**
    - the door faces the camera at the default yaw, within ±15°, so a player walking up in the default view sees the door and doorway straight on;
    - it is on the map, on open lawn, with lawn variation under the footprint ≤ 0.6 m;
    - the hero can walk to the doorway from where it starts in play, over ground it can climb (no step over the hero's limit);
    - nothing overlaps: the footprint and the overhangs (eaves, chimney, sign) are ≥ 0.3 m from everything in `ruins.glb`, crowns included, from the campfire's patch, stones and logs, and from fox #0's bed (curl and standing body through all five clips).
  - **Aim (not hard rules):** ≥ 4 m of open lawn all round, measured from the outer edge of the eaves, chimney and sign, with no tree, crown, rock, paving or bush inside that ring, and ≥ 10 m from the campfire's patch. Among the spots that meet the hard rules, pick the one with the most open lawn round it (capped at 4 m), then the most level lawn.

  The spot, yaw, clearances, open-ring distance and walking route length from the hero's start go in the handoff and the code. If no spot meets the hard rules, stop and report.
- **One fox in play (revised in cycle 2).** Play in the ruins shows only fox #0, at `kRuinsFoxSpot`, unchanged. Hand-placed foxes #2 and #3 (`kRuinsFoxSpot2`, `kRuinsFoxSpot3`, `kRuinsFoxYaw3`) and their placement code are removed. `--foxes N`, `--fox`, `--add-fox` and the fox brush work as before. (User: "we only need 1 fox in the scene, it's just a demo".)
- **Foxes and the tavern.** The tavern only has to clear fox #0's bed, not its possible walks. Fox walks route around the tavern automatically, because `routeClear` rejects routes onto unwalkable ground. Fox #0's walk choices may differ from before.
- **Ruins integration.** The exterior is part of `SceneLayout::solids`, so `--foxes N` beds and fox walks avoid it. No grass grows under its footprint. It casts shadows and is in `scene.ground`, so it blocks the hero. Its Stone and Roof faces feed `scene.shelter`. The roof, the door head and anything over the doorway and vestibule above 3.3 m stay out of `scene.ground`, as in cycle 1.
- **Scale.** Everything the hero uses is at the hero's scale, 1.6× real: a doorway 2.2 m wide × 3.4 m tall in both scenes, tables about 1.2 m high, chairs and stools to match.
- **Interior size (revised in cycle 2).** The room has double the cycle-1 floor area: about **22.6 × 17 m** inside (the user's choice). The exterior's size is independent of it.
- **Interior contents (revised in cycle 2).** The room should be easy to move around in, so contents scale up but the room gets less crowded, not more (user: "right now the room is a bit crowded, it should still be easy to move around in"). It holds:
  - **Tables:** 6 tables with chairs, one of them a long trestle table with benches.
  - **Bar:** a bar with 6 stools.
  - **Barrels:** 5.
  - **Fires:** the hearth plus one brazier.
  - **Windows:** 4, on the walls the camera sees.
  - **Stage:** a small, empty, raised corner stage.
  - **Candles:** one on each table and on the bar.

  Walkways and paths:
  - every walkway between furniture, walls, stage and hearth is ≥ 2.0 m wide (at hero scale);
  - a ≥ 2.5 m clear path runs from the door to the bar, to the hearth and to every table.
- **Shadow-only geometry.** `pxe::Scene::shadowOnly` meshes are drawn into the sun, moon and lightning shadow maps only, never in the main, reflection or edge passes, and never in `scene.ground`. They count toward `scene.shelter`. They are marked in the glb by the node name suffix `_ShadowOnly`. The interior's roof and the two walls nearest the camera are shadow-only.
- **Indoor lighting follows the world clock.** The same `lightingAt` and `WorldClock` drive the tavern. Daylight falls through the windows as patches that move with the time of day. At night the room is lit only by:
  - the hearth: a campfire-size `FireSite`, sheltered;
  - the brazier: a fire of size 0.5, sheltered;
  - the candles: size 0.2.

  No grass, water, fireflies, god rays or cloud shadows indoors. Rain never lifts the room's shade (ISSUE-001).
- **Interior colours and edges (added in cycle 2).** Indoors you only ever see a surface's shadow tone, so:
  - Wood's shadow tone is a darker warm brown and Plaster's a muted warm beige, in all four phases. Mid and lit tones stay warm brown and cream. Stone, Bark and the other existing surfaces are unchanged.
  - Wood, Plaster and Roof get the dark ink outlines but no light rims, so plank grooves read as quiet planks.
- **Weather indoors.** The clock, rain and wind keep running inside. No rain streaks are drawn indoors, but overcast still dims the palette. Rain and wind audio play at 40% inside and 100% outside. Lightning still lights the room through the windows, and thunder still plays.
- **Camera indoors.** Yaw is locked to the room's design yaw: Q/E and the bumpers do nothing inside. The camera follows the hero, its target clamped so the room stays framed. On leaving, the previous outdoor yaw is restored.
- **Transition trigger.** No key. Walking into a trigger zone through the open doorway starts the transition. On entering, the hero appears just inside the inside door, facing into the room. On leaving, the hero appears just outside the exterior door, facing away from the tavern.
- **Ruins state while inside.** The ruins `pxe::Scene` (CPU side) and foxes persist; leaving re-uploads the scene without rebuilding it. Foxes, the fireball and the ruins' fires hold while the hero is inside. Any lawn grass fire is put out on entering, and burnt grass stays burnt.
- **Loading screen.** Drawn with ImGui:
  - a dark background, the title "The Sleeping Fox" and one lore passage picked at random each time;
  - one frame of the screen is drawn before loading starts;
  - it stays up at least 2.0 s, then fades into the scene over about 0.3 s;
  - it shows in both directions.
- **Lore.** Six passages, word for word, in a plain array:
  1. Long before the pillars fell, travellers sheltered here on the road east. The temple is gone, but the Sleeping Fox keeps its old hearth burning.
  2. The foxes of the ruins curl up by any fire left untended. The innkeeper swears they always know when the stew is on.
  3. No one remembers who laid the stones of the plaza. The old songs say they were raised in a single night, and broken in another.
  4. Rain cannot reach a fire beneath a roof. Hooded wanderers learned that long before they learned to call fire from their palms.
  5. When lightning walks the hills, the regulars count the seconds to the thunder, and drink one for each.
  6. The pond below the ruins is said to show the sky as it was, not as it is. Fishermen come back with no fish and strange stories.
- **New surfaces.** `kSurfWood`, `kSurfPlaster` and `kSurfRoof`, mapped from the material names `Wood`, `Plaster` and `Roof`, each with day, golden, twilight and night palettes.
- **Door.** One skinned model (`tavern_door.glb`, joint `hinge`) serves both doors, in actor slots 12–13:
  - it swings inward 100° over 0.4 s, eased, while the hero is within 3 m of the doorway's centre;
  - it closes over 0.4 s once the hero is farther away;
  - if it reverses mid-swing, it continues from its current angle;
  - its leaf blocks the hero until it is fully open;
  - foxes don't open it, and it makes no sound.
- **Collision indoors.** Walls, bar, tables, barrels, shelves, stage edge and hearth block the hero through `scene.ground`. Chairs, stools, benches, the brazier and the cutaway walls block through `Blocker`s.
- **Hero required.** Only the hero can enter. With `--no-hero` the doors never open.
- **Testing hooks.** `--scene tavern` (play, captures, bench) and `--tavern-door-open`, in `--help` and the README.
- **Art-style doc.** `docs/art-style.md` has the **Buildings and props** section (added in cycle 1). The designer updates its budgets and notes if cycle 2 changes them.

## Acceptance criteria
- [x] `pxe::Surface` has `kSurfWood`, `kSurfPlaster` and `kSurfRoof`; `loadGltf` maps `Wood`, `Plaster` and `Roof`; `setPhasePalettes` sets all four phases for each, with Wood's shadow tone a warm dark brown and Plaster's a warm muted beige; shaders and arrays sized by surface count are updated — `mesh.hpp` Surface 7–9; `gltf.cpp` `surfaceFor`; `scene.cpp` `setPhasePalettes` (Wood shadow `4c3626`, Plaster `9c8a70` by day); `common.glsl` `kSurfaces` 10
- [x] Wood, Plaster and Roof draw ink outlines but no light rims in the edge pass — `edge.frag` `timbered()` excludes them from `rim`
- [x] `pxe::Scene` has shadow-only meshes, drawn only into the shadow maps; `_ShadowOnly` nodes are routed there and excluded from `scene.ground`, grass and the main, reflection and edge passes, but included in `scene.shelter` — `scene.hpp` `Scene::shadowOnly`; `renderer.cpp` drawn in the sun/flash shadow passes only; `scene.cpp` `buildTavern` routes `_ShadowOnly`, ground from visible geometry, shelter includes them
- [x] Indoors, rain never fades the room's shade toward lit; window patches fade toward shade as rain rises; outdoor rendering is unchanged (ISSUE-001) — `common.glsl` `sunLit` with `uIndoors`; `renderer.cpp` `bindToon`; ISSUE-001 resolved (`tavern_1500_rain1.png`)
- [ ] `tools/blender/build_ruins.py` and `assets/scenes/ruins.glb` are byte-identical to before this task (`git diff HEAD` shows no change to either)
- [x] Only fox #0 is hand-placed in play; `kRuinsFoxSpot2`, `kRuinsFoxSpot3`, `kRuinsFoxYaw3` and their placement code are gone; `--foxes N`, `--fox`, `--add-fox` and the fox brush behave as before — `kRuinsFoxSpot2/3`, `kRuinsFoxYaw3` gone (grep); `main.cpp` hand-placed set is fox #0
- [ ] `buildScene` places the 9 × 7 m `tavern_exterior.glb` at a spot and yaw recorded in the code and handoff that meets every hard placement rule in Decisions (door facing the default camera within ±15°; on open lawn; reachable on foot from the hero's start; no overlap: ≥ 0.3 m from ruins, crowns, campfire and fox #0's bed); the handoff reports the open-ring distance and campfire distance it reached against the 4 m and 10 m aims; it is in `SceneLayout::solids`, `scene.ground` (minus roof and doorway overheads) and the shadow casters; no grass grows under it
- [x] A `"tavern"` scene builds from the ~22.6 × 17 m `tavern_interior.glb`: visible geometry, shadow-only roof and near walls, a sheltered hearth (size 1), a brazier (size 0.5) and candles (size 0.2) at the positions in `assets.md`; no grass, water or fireflies; the camera clamp uses the new room bounds — `scene.cpp` `buildTavern`: hearth size 1, brazier 0.5, 7 candles 0.2 at assets.md points; `tavern.hpp` `kRoomMin/Max` ±11.7 × ±8.9
- [x] `assets.md` documents that the interior layout meets the walkway rules (≥ 2.0 m walkways; ≥ 2.5 m paths from the door to the bar, the hearth and every table), and the interior's blockers in code match its chair, stool, bench and brazier footprints — assets.md walkways table (narrowest 2.10 m) and path check; `tavern.cpp` `tavernBlockers` matches all 17 chairs, 6 stools, 2 bench capsules, brazier
- [x] The door model loads once and draws as an actor in both scenes, in slots 12–13; its hinge eases open 100° over 0.4 s within 3 m of the doorway centre and closes beyond 3 m, reversing from the current angle; a `Blocker` closes the doorway unless the door is fully open — `tavern.cpp` `TavernDoor::loadModel` (slot 12), `update` smoothstep 100°/0.4 s within `kReach` 3 m, `block`
- [x] Walking into the trigger zone through the open exterior door goes to the tavern, and through the open inside door goes to the ruins; spawn points and facings are as in Decisions — `main.cpp` passage on `Doorway::inZone` entry; `ruinsDoorway`/`tavernDoorway` arrive points and yaws
- [x] The loading screen draws with ImGui before loading begins, shows "The Sleeping Fox" and one of the six lore passages word for word, stays ≥ 2.0 s and fades over ~0.3 s, in both directions — `main.cpp` `Passage` (framesUp before load, `kLoadingShown` 2.0, `kLoadingFade` 0.3); `settings.cpp` `LoadingScreen`; `tavern.hpp` `kTavernLore` word for word
- [x] The ruins scene and fox persist while inside (no rebuild on return); foxes and the fireball don't update while inside; the lawn grass fire is put out on entering and burnt grass persists — `main.cpp` `Outdoors` kept, `goOutside` re-uploads; fox/fireball updates gated on `!inside`; `Renderer::quenchGrassFire` keeps the burn texture
- [x] Indoors: yaw locked, camera target clamped to the room, previous yaw restored on return — `main.cpp` `input.rotate = 0` inside, `framed()`, `outdoorYaw` restored in `goOutside`
- [x] Indoors: no rain streaks, rain and wind audio at 40%; clock, weather, lightning and thunder carry on — `renderer.cpp` edge `uRain` 0 indoors; `main.cpp` `kIndoorWeather` 0.4 on wind/rain volumes
- [ ] `--scene tavern` works in play, captures and `--bench`; `--tavern-door-open` opens every tavern door; both are in `--help`, and the README describes the tavern, where it stands, and the single fox in play (and no longer mentions removed ruins props)
- [x] Every asset loaded matches its `assets.md` entry — door slot 12, hinge joint; exterior/interior coordinates in `tavern.cpp`/`scene.hpp` match assets.md revision 2
- [x] Project builds cleanly from a fresh build directory with CMake — orchestrator fresh Debug build, cycle 2: pass, only the existing stb/glfw warnings

## Assets
The standing style is `docs/art-style.md`, including its **Buildings and props** section. Every asset is built by its reproducible Blender script, watched live through the MCP and rebuilt headless. Never hand-edit a glb. The doorway is 2.2 × 3.4 m in both buildings, and both take the same door model.

- **ruins (revision 2, cycle 3)**: restore `tools/blender/build_ruins.py` and `assets/scenes/ruins.glb` to exactly what they were before this task (HEAD). The props removed in cycle 2 (`BushD`, `Rock`, `Rock.001`, `Flag.009/011/012`, `FlagInset.009/011/012`) come back, and their ground goes back to Dirt. User: "yeah definitely add those back".
- **tavern_exterior (revision 2)**: the same style and materials as revision 1, rebuilt with a **9 × 7 m plinth footprint**.
  - Keep: the timber frame and plaster, the stone foundation reaching 0.6 m below the origin, the hipped thatch roof at a pitch that looks right at this size (lower the ridge rather than make the roof taller than the house is wide), a stone chimney on a side wall, dark recessed window panes, and the fox sign by the door.
  - The sign must not touch the door jamb or hang over the doorway.
  - The 2.2 × 3.4 m doorway and a vestibule deep enough for the trigger zone (≥ 2.5 m).
  - Origin: the middle of the threshold at floor level. Exported +Y up, door facing -Y in Blender (+Z in the engine).
  - Budget: ≤ 4,000 triangles.
  - assets.md must give the same placement figures as revision 1: hinge position and closed direction, footprint, overhang extents (eaves, chimney, sign), doorway and vestibule rectangles.
- **tavern_interior (revision 2)**: the same style, materials, design yaw (45°), cutaway approach (shadow-only roof and the two near walls, with visible stubs ≤ 0.5 m) and doorway, rebuilt as a room about **22.6 × 17 m** inside. Contents as listed in Decisions: 6 tables (one trestle with benches), a bar with 6 stools, back shelves, 5 barrels, the stone hearth, one brazier, 4 windows on the visible walls, a small raised corner stage, and a candle on each table and the bar.
  - Meet the walkway rules (≥ 2.0 m walkways; ≥ 2.5 m clear paths from the door to the bar, the hearth and every table), and have the build script check them and fail if they don't hold.
  - The whole room must still fit the 40 × 22.5 m view at the design yaw.
  - Budget: ≤ 20,000 triangles.
  - assets.md must give the same figures as revision 1 (design yaw, hearth fire base, every candle base, inside door hinge, closed direction and doorway centre, hero spawn and facing, room inner bounds) plus the brazier's fire base and footprint, every chair, stool and bench footprint, and the stage's extent.
- **tavern_door**: unchanged.

## Out of scope
- NPCs, a barkeep, and interacting with anything but the door
- Chimney smoke, glowing windows from outside, interior or door sounds, and any audio besides the rain/wind volume
- A second storey, other buildings or interiors
- Foxes entering the tavern, opening its door or reacting to it
- Burning the tavern; indoors the brushes, fox tool and fireball do nothing
- Camera rotation indoors
- Changes to fox #0's bed or the fox's clips

## Cycle log
- Cycle 1: 2026-10-24 started
- Cycle 1: 2026-10-25 complete. Build passes. Review found ISSUE-001. The user changed the spec: exterior placement and size, one fox, a 2× interior with roomier contents, warmer Wood/Plaster shadow tones, no light rims on the new surfaces.
- Cycle 2: 2026-10-25 started. Outstanding: ISSUE-001 plus the spec changes above.
- Cycle 2: 2026-10-25 orchestrator decisions: FlagInsets removed with their flags; the tavern spot moved to (−11.75, −0.25) because the chimney failed the campfire margin at (−11, −1); TreeF's crown hiding about 46% of the doorway from the default yaw is accepted (style norm, Q/E rotates outside, moving TreeF would change the reference composition).
- Cycle 2: complete. Fresh build passes, ISSUE-001 resolved, no new issues.
- Cycle 3: 2026-10-25 reopened by the user: the cycle-2 spot is too cramped. The tavern can go anywhere on the map with its door facing the default camera; the cleared ruins props are restored; the size stays 9 × 7 m.
