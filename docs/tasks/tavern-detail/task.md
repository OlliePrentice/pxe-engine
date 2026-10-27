# Tavern detail pass
Type: feature
Status: open
Cycle: 3

## Goal
Make the Sleeping Fox easier to read. Inside, the bar, tables and other furniture blend into the floor because everything wooden is one `Wood` material seen only in its dark shadow tone, and nothing lines the creases. Outside, the roof is a plain grey shape. This task gives the exterior a slate tile roof with decorative gables, gives the interior furniture a lighter oak colour (the floor stays dark), adds an indoor fill light, crease lines and more lights so edges read, and silences the wind indoors.

## Decisions
- **Roof covering: slate tiles.** The exterior's thatch becomes a slate roof modelled as stepped, overlapping courses. Each course is a strip with a visible step at its lower edge, so the slope reads as bands at 24 px/m. Individual tiles aren't modelled. No textures (art style). The material stays `Roof`.
- **Roof palette.** `kSurfRoof` gets a blue-grey slate ramp. Day: shadow `3c4454`, mid `5a6676`, lit `7e8a98`. Golden, twilight and night are scaled from the day ramp the way Stone's phases are from its day ramp. `Roof` is used only by the tavern (the interior's roof is shadow-only), so nothing else changes colour. The developer confirms no other glb uses `Roof`.
- **Gables (option a).** The roof stays hipped. It gains:
  - a front cross-gable over the door, facing the default camera;
  - one small gabled dormer on each long slope;
  - carved (stepped or notched, low-poly) bargeboards and a finial on each gable.
  The gable faces are timber-framed plaster (`Wood` and `Plaster`), slated as the roof.
- **Stay inside today's extents.** Everything new on the exterior stays inside the revision-2 eave and overhang extents in plan (eaves, chimney, sign), so the placement, its clearances, the 4 m ring, `SceneLayout::solids` and the footprint are unchanged. The highest point may rise by at most 0.5 m above today's ridge. Nothing new hangs over the doorway or vestibule below 3.3 m. The build script checks all of this and fails if it breaks. `kTavernSpot` and `kTavernYaw` don't change.
- **Oak surface.** A new engine surface, `kSurfOak`, mapped from the material name `Oak`, with its own four-phase palette. Day: shadow `8a6640`, mid `b48c5a`, lit `d4b07a`. Golden, twilight and night are scaled from the day ramp the way Wood's phases are from its day ramp. Like Wood, Plaster and Roof, Oak is flat-faced, built and timbered: outlines, no light rims. `Wood`'s palette is unchanged.
- **What turns oak.** In `tavern_interior` only:
  - **Oak:** the bar (body, top, posts, foot rail), every table, the trestle table and its benches, every chair and stool, the back shelves, the barrels' wood, the stage lip and the hearth mantel.
  - **Stays Wood:** the floor, wall posts, rails and braces, window and door frames, the sill, and the cutaway stubs' skirting.
  - **Unchanged:** the door model (`tavern_door.glb`, actor slots 12–13) and every exterior timber.
- **Indoor fill light.** Indoors only (`uIndoors`), a fixed directional fill from above and to one side, chosen for the design yaw (45°), lifts faces by orientation through the toon ramp, so tops, fronts and sides of the same object take different tones instead of one flat shadow tone.
  - Strength follows the clock: full by day and in golden hour, fading through twilight, **off at night**. At night the room stays lit only by its fires, as the tavern task decided.
  - Window daylight patches stay clearly brighter than anything the fill alone lights.
  - Outdoor rendering is unchanged.
- **Crease lines indoors (revised in cycle 1).** Indoors only, Oak surfaces draw an ink line where the surface turns sharply between neighbouring pixels (normals more than about 40° apart, no silhouette depth step), such as the bar top meeting its front, including where Oak meets the Wood floor. Wood pixels draw no crease ink: inking Wood put a line on every floor plank groove, restyling the floor the user excluded (orchestrator decision, ISSUE-001). Outdoors the edge pass is unchanged.
- **Slate at golden hour (cycle 1).** The Roof's golden lit tone comes out a warm grey (`938d73`) when scaled the way Stone's is. Accepted: every surface's golden lit tone is warm in this game, and the shadow and mid tones stay blue-grey.
- **More lights.** The interior gains four wall sconces. Each is an iron bracket (`Stone`) with an open flame dish, and its flame is drawn by the engine as a fire of size 0.35, sheltered, like the candles. They hang at about 2.6 m (hero scale) on the wall posts of the two visible (far) walls, clear of windows, shelves and the hearth. The candles go from size 0.2 to 0.3. The hearth and brazier are unchanged.
- **Wind indoors.** Wind audio is 0% indoors and back to 100% outside. Rain stays at 40% indoors, and thunder is unchanged.
- **Existing content.** No furniture, wall, window, fire, candle, blocker, spawn point or the door moves. The interior's walkway and path rules still hold, and the build script still checks them. The room's bounds, design yaw and camera clamp are unchanged. The exterior keeps its footprint, plinth, chimney, sign, windows, doorway and vestibule.
- **Branch.** Work on `add-tavern` (user's choice).

- **Cycle 3 revisions (user playtest, 2026-10-27).** The user found that the roof tiles still don't read, the oak looks grey, and the room goes dark too early.
  - **Tiles that read.** Each slate course is cut into staggered tiles: a notch or step joint about every 0.5 m, offset by half a tile from the course below. Outdoors, Roof pixels take crease ink, using the same test as indoor Oak (neighbouring normals more than ~40° apart, no silhouette step, one pixel wide), so every course lip and tile joint draws a dark line. Outdoors, no other surface gains crease ink: Stone, Wood, Plaster and the rest are unchanged. The exterior's triangle budget rises from 4,000 to 8,000.
  - **Honey oak, warm at every hour.** This replaces the cycle-1 Oak ramps. Day: shadow `8c5a30`, mid `b47a42`, lit `d49c5c`. Golden is warmer. Twilight and night are darker warm browns. No tone in any phase is blue or grey: each tone's red channel stays above its blue channel, and above green.
  - **Hearth stone warm at night.** In the tavern scene only, Stone's twilight and night ramps are warm greys instead of the outdoor blue (no tone with blue above red). The outdoor Stone palette is unchanged.
  - **Brazier lit only at night.** The brazier lights at dusk (sun at the horizon, elevation 0°) and goes out at dawn (elevation back above 0°). Its flame and light fade in or out over about 2 s. The hearth, candles and wall sconces stay lit at all hours.
  - **Sky light through the windows.** The existing sun patches stay. Each of the four windows also lets in a soft sky light: a lit area on the floor and walls in front of it, whose colour and strength follow the sky outside. It is warm white by day, amber at golden hour, a fading violet-blue through twilight, and a faint cool moonlight at night. The room is lit from its windows until the sky is fully dark (sun 14° below the horizon), then only faintly by the moon.
  - **Fill light tinted.** The cycle-1 fill light's strength curve is unchanged, but it takes the same time-of-day tint as the sky light.
  - **Window openings show the sky.** Seen from inside, each window opening shows the sky's current colour instead of black or the background. The exterior's panes are unchanged.

## Acceptance criteria
- [x] `pxe::Surface` has `kSurfOak`. `loadGltf`/`surfaceFor` maps `Oak` to it. Every array, shader constant and helper sized by or ranged over surfaces (`kSurfCount`, `kSurfaces` in `common.glsl` and `edge.frag`, `flatFaced`, `built`, `timbered`, bands, normal-alpha encoding) includes it, and no existing surface's index changes meaning. — `mesh.hpp` kSurfOak 10, kSurfCount 11; `gltf.cpp` `surfaceFor`; `common.glsl`/`edge.frag` kSurfaces 11, `flatFaced`/`built`/`timbered` include Oak
- [x] `setPhasePalettes` sets Oak in all four phases, with the day ramp `8a6640 / b48c5a / d4b07a`. Wood's ramps are byte-identical to before. — `scene.cpp` `setPhasePalettes` Oak in four phases, day 8a6640/b48c5a/d4b07a; Wood lines unchanged in diff
- [x] `setPhasePalettes` sets Roof to the slate ramp, with the day ramp `3c4454 / 5a6676 / 7e8a98`, and blue-grey slate ramps in golden, twilight and night. — `scene.cpp` Roof day 3c4454/5a6676/7e8a98; golden lit warm grey accepted (Decisions)
- [x] `tavern_interior.glb` uses `Oak` on exactly the pieces listed in Decisions, and `Wood` stays on the floor, wall timbers, frames, sill and skirting (checked in `build_tavern_interior.py` and in assets.md's material table). — `build_tavern_interior.py` `oak_checks`; assets.md Oak/Wood table; glb materials list Oak
- [x] The interior has four wall sconces on the visible walls at about 2.6 m. Their flame bases are recorded in assets.md, and `buildTavern` adds a sheltered fire of size 0.35 at each. The candle size is 0.3. The hearth and brazier are unchanged. — `scene.cpp` `kRoomSconces` (4, size 0.35, sheltered), `kRoomCandleSize` 0.3; ruins `kCandleSize` kept 0.2
- [x] The interior build still passes its walkway, path and view-fit checks, and every existing piece's position is unchanged from revision 2 (assets.md figures for hearth, brazier, candles, door, spawn, bounds, chairs, stools, benches and stage match the revision-2 values). — designer: 66 rev-2 nodes identical geometry/transforms; walkway/path/view-fit pass (assets.md)
- [x] An indoors-only fill light in the toon shading gives faces of different orientation different tones. Its strength is full in day and golden, fades through twilight and is zero at night. Outdoor shading takes the old code path unchanged. — `common.glsl` `indoorTone`, `mesh.frag` under `uIndoors`; `lighting.cpp` fill = fade(elev, night, golden); `renderer.cpp` uFill 0 outdoors
- [x] The edge pass draws indoors-only crease ink on Oak (not on Wood) where neighbouring normals differ by more than about 40° without a silhouette step. Outdoors, `edges()` produces the same result as before. — `edge.frag` `edges()` `creased()` Oak only, gated on `uIndoors` (ISSUE-001)
- [x] `tavern_exterior.glb` has a stepped slate-course roof in `Roof`, a front cross-gable over the door, a gabled dormer on each long slope, and decorative bargeboards and finials. The build script checks that everything stays inside the revision-2 plan extents, rises at most 0.5 m above the old ridge, and leaves the doorway and vestibule clear below 3.3 m. — `build_tavern_exterior.py` roof/gable/dormer steps and extent checks; top 8.66 (below old ridge 9.13); preview
- [x] `kTavernSpot`, `kTavernYaw`, the exterior's door hinge, doorway, vestibule and footprint figures are unchanged. The tavern's `solids`, ground and overhead exclusion still work with the new roof (`scene.ground` still excludes the roof). — unchanged in diff; non-roof exterior nodes byte-identical (designer); `layOutRuins` ground list excludes Roof
- [x] Indoors, wind volume is 0 and rain volume is 40%. Outdoors both are 100%. — `main.cpp` `heard.wind = 0` inside, rain × kIndoorWeather 0.4
- [x] `tools/blender/build_ruins.py`, `assets/scenes/ruins.glb` and `assets/scenes/tavern_door.glb` are unchanged. — `git diff 7749a4f..HEAD` touches none of them
- [x] `docs/art-style.md` names `Oak` as an allowed material (light oak: interior furniture) and describes the Roof as slate. — art-style.md materials list and Roof wording (c41bd80)
- [x] Every asset loaded matches its `assets.md` entry. — interior Oak/sconce bases and exterior figures match assets.md
- [ ] (cycle 3) `tavern_exterior.glb` revision 4: every slate course is cut into staggered tiles, with joints about every 0.5 m offset by half a tile between courses. All revision-3 checks still pass (extents, height, doorway). ≤ 8,000 triangles.
- [ ] (cycle 3) Outdoors, the edge pass draws crease ink on Roof pixels only (normals > ~40° apart, no silhouette step, one pixel wide). Every other outdoor surface's `edges()` result is unchanged.
- [ ] (cycle 3) Oak's four-phase ramps: day `8c5a30 / b47a42 / d49c5c`. In every phase and tone, red > green > blue.
- [ ] (cycle 3) In the tavern scene only, Stone's twilight and night ramps have no tone with blue above red. The ruins' Stone palette is byte-identical.
- [ ] (cycle 3) The brazier's fire is lit only while the sun's elevation is below 0°, fading in and out over about 2 s. The hearth, candles and sconces stay lit at all hours.
- [ ] (cycle 3) Indoors, each window casts a sky light whose colour and strength follow the time of day: warm white by day, amber in golden hour, violet-blue fading through twilight, faint cool moonlight at night. It reaches zero only for the moonlight floor at night, never earlier than sun elevation −14°. The sun patches still draw.
- [ ] (cycle 3) The fill light takes the sky light's time-of-day tint, with its strength curve unchanged.
- [ ] (cycle 3) Seen from inside, the window openings show the sky's current colour.
- [ ] (cycle 3) Outdoor rendering is unchanged except the Roof (palette unchanged, crease ink and tile geometry new).
- [ ] Project builds cleanly from a fresh build directory with CMake. — orchestrator fresh Debug build, cycle 2: pass, only existing stb/glfw warnings

## Assets
The standing style is `docs/art-style.md` (Buildings and props). Rebuild from the scripts, watched live through the Blender MCP, then rebuild headless and export. Never hand-edit a glb. Previews go beside each glb.

- **tavern_exterior (revision 3)**: revision 2 with a new roof.
  - Slate roof of stepped overlapping courses in `Roof`. Cross-gable over the door, facing +Z in the engine. One gabled dormer on each long slope. Gable faces timber-framed plaster (`Wood`, `Plaster`). Low-poly carved bargeboards and a finial on each gable (`Wood`).
  - Stays inside the revision-2 plan extents (eaves, chimney, sign). Top at most 0.5 m above the revision-2 ridge. Nothing over the doorway or vestibule below 3.3 m. The script checks all three and fails if they break.
  - Unchanged: footprint, plinth, walls, chimney, windows, sign, doorway, vestibule and origin.
  - Format: glb, +Y up, 1 unit = 1 m, door facing -Y in Blender (+Z in the engine). Origin: the middle of the threshold at floor level, as revision 2.
  - Budget: ≤ 4,000 triangles (revision 3); ≤ 8,000 from revision 4 (cycle 3).
  - Animations: none.
  - assets.md: the revision-2 placement figures, restated and confirmed unchanged, plus the new roof's top height.
- **tavern_interior (revision 3)**: revision 2 with:
  - the pieces listed in Decisions moved from `Wood` to a new `Oak` material (preview colour a light oak, about sRGB (180, 140, 90));
  - four wall sconces: an iron bracket (`Stone`) and open flame dish at about 2.6 m on wall posts of the two visible walls, clear of windows, shelves and the hearth, each named (`Sconce_1`..`Sconce_4`), with each flame base printed and recorded in engine coordinates.
  - Nothing else moves. Walkway, path and view-fit checks still pass.
  - Format, units, axes, origin, design yaw, cutaway and shadow-only pieces: as revision 2.
  - Budget: ≤ 20,000 triangles.
  - Animations: none.
  - assets.md: all revision-2 figures, confirmed unchanged, plus the sconce flame bases and the material table showing which pieces are Oak.
- **tavern_exterior (revision 4, cycle 3)**: revision 3 with each slate course cut into staggered tiles. Each tile is about 0.5 m wide along the course. The joints are a notch or step deep enough to give a normal change of more than 40° at pixel scale, and are offset by half a tile from the course below. Hips, ridge and gable and dormer slopes are tiled the same way. Everything else is unchanged, and all revision-3 checks still pass. Budget ≤ 8,000 triangles. Preview: a close default-camera view of the roof.
- **tavern_door**: unchanged.

## Out of scope
- Moving, resizing or replacing any furniture, wall, window, fire or the tavern itself
- Changing the floor's colour, Wood's palette, or any exterior timber colour
- Converting the roof to a full gable or half-hip shape
- Crease ink outdoors on anything but Roof; changing the outdoor Stone palette
- New sounds, and any change to rain or thunder volumes
- Outdoor lighting or outdoor edge changes
- Chimney smoke, glowing windows, NPCs

## Cycle log
- Cycle 1: 2026-10-27 started, base 7749a4f (spec commit; edf89bc before it)
- Cycle 1: designer c41bd80, developer 388cee3. Fresh build passes. Review found ISSUE-001 (crease ink on the Wood floor's plank grooves).
- Cycle 2: 2026-10-27 started, base HEAD after the cycle-1 review. Outstanding: ISSUE-001.
- Cycle 2: developer 847d22b fixed ISSUE-001. Fresh build passes; ISSUE-001 resolved, no new issues. Task complete.
- Cycle 3: 2026-10-27 reopened by the user after a playtest: tiles don't read, oak looks grey, the room goes dark too early. Base 4b479b7. Decisions under "Cycle 3 revisions".
