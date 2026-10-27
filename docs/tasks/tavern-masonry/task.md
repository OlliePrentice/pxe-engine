# Tavern masonry, render and door grain
Type: feature
Status: open
Cycle: 1

## Goal
Give the Sleeping Fox more surface detail, inside and out. The door gets bold, stylised wood grain and visible plank joints. The stonework (exterior plinth and chimney, interior hearth) is laid as individual stones in pale mortar. The plaster render gets gently faceted panels and a few spalled patches where it has fallen away to show the stones beneath. Today the door reads as one brown slab, the plinth as a grey strip and the plaster as flat cream.

## Decisions
- **Scope.** "Rendering/brickwork" means the plaster render on the wall panels and the stonework. Both buildings get it: the exterior's plinth, chimney and plaster panels, and the interior's hearth and plaster walls. The door model serves both doors, so both get the grain.
- **Door grain (geometry plus a third material).** Each plank gets 2–3 thin raised grain streaks and the door gets 2 knots, all in a new third door material `Door_Grain`: a darker brown than `Door_Wood`, in actor slot 14. Streaks are about 0.05 m wide so each takes at least one pixel at 24 px/m. Material order: `Door_Wood` (0), `Door_Iron` (1), `Door_Grain` (2). Slot 14 is free (hero 0–7, fox 8–11, door 12–13). Grain appears on both faces, since the inside door shows its back.
- **Door crease ink.** In the edge pass, the door's wood (`Door_Wood` and `Door_Grain`) takes crease ink, indoors and outdoors, with the same test as Oak and Roof (normals more than ~40° apart, no silhouette step, one pixel wide), so plank joints and grain ridges read. The hero, fox and every other actor are unchanged, keeping their existing outline and rim behaviour. The developer chooses the mechanism, for example a distinct surface code for those actor slots in the normal target.
- **Stonework: individual stones in light mortar.** Every stone is modelled on its own, in `Stone`, set slightly proud (about 0.03–0.05 m) of a recessed mortar bed in `Plaster`, so pale joints show between grey stones. No engine change and no new material. The ruins' Stone is untouched. Joints are at least about 0.05 m wide so they take a pixel. This applies to:
  - the exterior plinth (all four sides, the threshold included);
  - the exterior chimney;
  - the interior hearth.
- **Render detail.**
  - **Uneven render:** every plaster panel is gently faceted (a few low-relief facets, ≤ 0.03 m deep), so the toon shading breaks it into a few tones instead of one flat cream.
  - **Spalled patches:** 3–4 panels on the exterior and 2 on the interior's visible walls have a patch where the render has fallen away. Each patch is an irregular outline showing stones in light mortar beneath, built as Q3 describes. Patches are on panels the default camera sees (exterior) or on the visible walls (interior), clear of windows, the doorway, the sign and the sconces.
  - No pargetting and no limewash variation.
- **Existing content stays put.** No wall, timber, window, door frame, roof, chimney position, sign, furniture, fire, sconce or candle moves or resizes. The exterior's plan extents, footprint, doorway, vestibule and placement figures are unchanged. The interior's walkway, path and view-fit checks still pass, and its bounds and every recorded figure are unchanged. Proud stones and facets must not intrude into the doorway, vestibule or any walkway. The door's leaf size, hinge, joint and swing are unchanged, and the grain stays within its 0.12 m thickness plus at most 0.02 m, clear of the frame through the 100° swing.
- **Branch.** Work on `add-tavern`, as the previous tavern tasks did.

## Acceptance criteria
- [ ] `tavern_door.glb` has three materials in the order `Door_Wood`, `Door_Iron`, `Door_Grain`, with 2–3 raised grain streaks per plank and 2 knots on both faces. Leaf size, hinge joint and rest pose are unchanged. ≤ 800 triangles.
- [ ] The door code loads the model into slots 12–14, and nothing else uses slot 14.
- [ ] The edge pass draws crease ink (normals > ~40° apart, no silhouette step, one pixel wide) on the door's `Door_Wood` and `Door_Grain` pixels, indoors and outdoors. Hero, fox and other actors draw exactly as before.
- [ ] `tavern_exterior.glb` revision 5: plinth and chimney built of individual `Stone` stones proud of a recessed `Plaster` mortar bed; every plaster panel faceted (≤ 0.03 m relief); 3–4 spalled patches on camera-facing panels showing stone and mortar. All revision-4 checks pass (plan extents, height, doorway and vestibule, roof tiles), plus new checks for joint width, stone proudness and patch placement. ≤ 14,000 triangles.
- [ ] `tavern_interior.glb` revision 4: the hearth built of individual stones in light mortar; visible plaster walls faceted; 2 spalled patches on the visible walls. All revision-3 checks pass (walkways, paths, view fit, Oak, sconces). Every recorded figure is unchanged. ≤ 20,000 triangles.
- [ ] Placement constants (`kTavernSpot`, `kTavernYaw`), doorway, vestibule, hinge, footprint, room bounds, fire, candle and sconce positions and blockers are unchanged in code. If an asset's recorded figures change, it is a defect.
- [ ] `tools/blender/build_ruins.py` and `assets/scenes/ruins.glb` are unchanged. The ruins' Stone palette and edges are unchanged.
- [ ] `docs/art-style.md` records the light-mortar masonry, the faceted render with spalled patches, and the door's grain material and slot range 12–14.
- [ ] Every asset loaded matches its `assets.md` entry.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
The standing style is `docs/art-style.md` (Buildings and props). Rebuild from the scripts, watched live through the Blender MCP in a fresh empty file (never modify a file the user has open), then rebuild headless and export. Never hand-edit a glb. Previews go beside each glb, including a close view at game scale (24 px/m) of each new detail.

- **tavern_door (revision 2)**: revision 1 plus grain.
  - Grain: 2–3 raised streaks per plank and 2 knots, in `Door_Grain`: a darker brown (preview about sRGB (92, 58, 34) against `Door_Wood`). Streaks about 0.05 m wide, raised ≤ 0.02 m, on both faces. Their sides turn more than 40° from the plank face, so crease ink catches them.
  - Material order: `Door_Wood`, `Door_Iron`, `Door_Grain`.
  - Unchanged: leaf 2.18 × 3.38 × 0.12 m, the `hinge` joint, its origin and axes, the ironwork, and swing clearance (the build checks it).
  - Format: glb, +Y up, 1 unit = 1 m, as revision 1.
  - Budget: ≤ 800 triangles.
  - Animations: none.
- **tavern_exterior (revision 5)**: revision 4 plus masonry and render detail.
  - Plinth and chimney: individual stones (`Stone`) 0.03–0.05 m proud of a recessed `Plaster` mortar bed, with joints ≥ ~0.05 m and coursed, staggered joints.
  - Plaster panels: faceted, ≤ 0.03 m relief.
  - Spalled patches: 3–4, on camera-facing panels, each an irregular outline showing stone and mortar, clear of windows, the doorway and the sign.
  - Everything else is unchanged, and all revision-4 checks pass.
  - Format, units, axes and origin: as revision 4.
  - Budget: ≤ 14,000 triangles.
  - Animations: none.
- **tavern_interior (revision 4)**: revision 3 plus masonry and render detail.
  - The hearth (slab, cheeks, breast and jutting course) is built of individual stones in light mortar. The mantel stays Oak, and the sooty back and ash bed are unchanged.
  - Visible plaster walls are faceted.
  - 2 spalled patches on the visible walls, clear of windows, shelves, sconces and the hearth.
  - Nothing moves, and all revision-3 checks pass.
  - Format, units, axes and origin: as revision 3.
  - Budget: ≤ 20,000 triangles.
  - Animations: none.

## Out of scope
- Pargetting, limewash colour variation and any new static material or engine surface
- Changing the ruins or the Stone palette
- Moving or resizing anything existing
- Outdoor view culling (planned as the next task after the tavern merges)

## Cycle log
- Cycle 1: 2026-10-27 started
