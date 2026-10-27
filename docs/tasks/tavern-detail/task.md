# Tavern detail pass
Type: feature
Status: open
Cycle: 2

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

## Acceptance criteria
- [ ] `pxe::Surface` has `kSurfOak`. `loadGltf`/`surfaceFor` maps `Oak` to it. Every array, shader constant and helper sized by or ranged over surfaces (`kSurfCount`, `kSurfaces` in `common.glsl` and `edge.frag`, `flatFaced`, `built`, `timbered`, bands, normal-alpha encoding) includes it, and no existing surface's index changes meaning.
- [ ] `setPhasePalettes` sets Oak in all four phases, with the day ramp `8a6640 / b48c5a / d4b07a`. Wood's ramps are byte-identical to before.
- [ ] `setPhasePalettes` sets Roof to the slate ramp, with the day ramp `3c4454 / 5a6676 / 7e8a98`, and blue-grey slate ramps in golden, twilight and night.
- [ ] `tavern_interior.glb` uses `Oak` on exactly the pieces listed in Decisions, and `Wood` stays on the floor, wall timbers, frames, sill and skirting (checked in `build_tavern_interior.py` and in assets.md's material table).
- [ ] The interior has four wall sconces on the visible walls at about 2.6 m. Their flame bases are recorded in assets.md, and `buildTavern` adds a sheltered fire of size 0.35 at each. The candle size is 0.3. The hearth and brazier are unchanged.
- [ ] The interior build still passes its walkway, path and view-fit checks, and every existing piece's position is unchanged from revision 2 (assets.md figures for hearth, brazier, candles, door, spawn, bounds, chairs, stools, benches and stage match the revision-2 values).
- [ ] An indoors-only fill light in the toon shading gives faces of different orientation different tones. Its strength is full in day and golden, fades through twilight and is zero at night. Outdoor shading takes the old code path unchanged.
- [ ] The edge pass draws indoors-only crease ink on Oak (not on Wood) where neighbouring normals differ by more than about 40° without a silhouette step. Outdoors, `edges()` produces the same result as before.
- [ ] `tavern_exterior.glb` has a stepped slate-course roof in `Roof`, a front cross-gable over the door, a gabled dormer on each long slope, and decorative bargeboards and finials. The build script checks that everything stays inside the revision-2 plan extents, rises at most 0.5 m above the old ridge, and leaves the doorway and vestibule clear below 3.3 m.
- [ ] `kTavernSpot`, `kTavernYaw`, the exterior's door hinge, doorway, vestibule and footprint figures are unchanged. The tavern's `solids`, ground and overhead exclusion still work with the new roof (`scene.ground` still excludes the roof).
- [ ] Indoors, wind volume is 0 and rain volume is 40%. Outdoors both are 100%.
- [ ] `tools/blender/build_ruins.py`, `assets/scenes/ruins.glb` and `assets/scenes/tavern_door.glb` are unchanged.
- [ ] `docs/art-style.md` names `Oak` as an allowed material (light oak: interior furniture) and describes the Roof as slate.
- [ ] Every asset loaded matches its `assets.md` entry.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
The standing style is `docs/art-style.md` (Buildings and props). Rebuild from the scripts, watched live through the Blender MCP, then rebuild headless and export. Never hand-edit a glb. Previews go beside each glb.

- **tavern_exterior (revision 3)**: revision 2 with a new roof.
  - Slate roof of stepped overlapping courses in `Roof`. Cross-gable over the door, facing +Z in the engine. One gabled dormer on each long slope. Gable faces timber-framed plaster (`Wood`, `Plaster`). Low-poly carved bargeboards and a finial on each gable (`Wood`).
  - Stays inside the revision-2 plan extents (eaves, chimney, sign). Top at most 0.5 m above the revision-2 ridge. Nothing over the doorway or vestibule below 3.3 m. The script checks all three and fails if they break.
  - Unchanged: footprint, plinth, walls, chimney, windows, sign, doorway, vestibule and origin.
  - Format: glb, +Y up, 1 unit = 1 m, door facing -Y in Blender (+Z in the engine). Origin: the middle of the threshold at floor level, as revision 2.
  - Budget: ≤ 4,000 triangles.
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
- **tavern_door**: unchanged.

## Out of scope
- Moving, resizing or replacing any furniture, wall, window, fire or the tavern itself
- Changing the floor's colour, Wood's palette, or any exterior timber colour
- Converting the roof to a full gable or half-hip shape
- New sounds, and any change to rain or thunder volumes
- Outdoor lighting or outdoor edge changes
- Chimney smoke, glowing windows, NPCs

## Cycle log
- Cycle 1: 2026-10-27 started, base 7749a4f (spec commit; edf89bc before it)
- Cycle 1: designer c41bd80, developer 388cee3. Fresh build passes. Review found ISSUE-001 (crease ink on the Wood floor's plank grooves).
- Cycle 2: 2026-10-27 started, base HEAD after the cycle-1 review. Outstanding: ISSUE-001.
