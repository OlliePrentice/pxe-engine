# Assets: tavern masonry, render and door grain

**All coordinates are in engine coordinates: +Y up, +Z = Blender -Y, 1 unit = 1 metre, relative to the asset's own origin, before the code places it.** Hero scale (1.6× real) throughout. Each build script prints these figures from the constants it builds with.

Rebuild headless. Each build constructs the asset, runs its checks, exports, and renders its previews. It fails if any check fails.

```
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_door.py
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_exterior.py
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_interior.py
```

The scripts are the source of truth. As in the earlier tavern tasks, there is no `.blend` source. Each asset was built step by step in a fresh empty file in the live Blender session through the MCP, where every check passed. It was then rebuilt and exported headless with the commands above.

The ruins (`tools/blender/build_ruins.py`, `assets/scenes/ruins.glb`) are untouched. Neither file is in `git diff`.

**How "unchanged" was verified.** Each new glb was parsed against its HEAD version (`799b846`), node by node. For each (node, material), the triangles (positions to 1e-5) and the node transform were compared.
- **Door:** `Door_Iron` is identical. `Door_Wood` keeps its 36 triangles, and only the V-groove bottoms moved (see the groove note below). `Door_Grain` is new.
- **Exterior:**
  - `Timbers`, `Windows`, `Doorway`, `Vestibule`, `Roof` and `Sign` are identical.
  - In `Gable_Front`, `Dormer_Front` and `Dormer_Back`, every Wood, Dirt and Roof triangle is identical. Only Plaster changed (the faceted face and cheeks).
  - `Foundation`, `Walls` and `Chimney` are rebuilt.
  - No node transform changed.
- **Interior:**
  - 95 of the 101 (node, material) pairs are identical. That covers every furniture piece, `Floor`, `Timbers`, `Doorway`, the stubs, the shadow-only walls and roof, the sconces and candles.
  - Only `Hearth` (Stone, plus a new Plaster mortar bed) and `Wall_PosZ` and `Wall_PosX` (Plaster, plus new Stone in the patches) changed.
  - In `Hearth`, the Oak mantel, the Wood logs and the Dirt back and ash are identical. In both walls, the Wood (mullions, sills) is identical.
  - No node transform changed.
- **Printed figures:** every figure each build prints was diffed against the HEAD build's output, and they match line for line. The one difference is the interior's `materials_by_object` line, which now lists Plaster on `Hearth` and Stone on `Wall_PosZ` and `Wall_PosX`.

---

## tavern_door (revision 2)
Source: tools/blender/build_tavern_door.py
Export: assets/scenes/tavern_door.glb
Preview:
- `assets/scenes/tavern_door.png`: as revision 1. Front from the front-left, back from the back-right, and swung open 100°.
- `assets/scenes/tavern_door_grain.png`, new: the grain at the game's 24 px/m (scaled 4×). The front is as the exterior's default camera sees it (yaw 167.5 in the tavern frame). The back is as the room's design camera sees the inside door (yaw -45 in the door's frame). Each has three panels:
  - toon-shaded, with crease ink on `Door_Wood` and `Door_Grain` mixed at 0.35 of the day ink;
  - the same with the ink solid;
  - a smooth render at 96 px/m.
Format: glb, one skinned mesh (`Tavern_Door`) with a one-joint armature (`Tavern_Door_Rig`, joint `hinge`), as revision 1
Units and scale: 1 unit = 1 m, hero scale. Leaf 2.18 × 3.38 × 0.12 m (unchanged).
Axes: +Y up. At rest the front faces +Z, and the leaf runs from the hinge along +X (unchanged).
Origin: the bottom of the hinge edge on the back face (unchanged).
Triangles: **506** (budget 800). Revision 1 was 94. Grain adds 412: 18 streaks × 18, and 4 knots × 22.
Materials, in glTF order (the contract):
| # | Name | Preview sRGB |
|---|---|---|
| 0 | `Door_Wood` | (128, 82, 46) |
| 1 | `Door_Iron` | (46, 44, 48) |
| 2 | `Door_Grain` | (92, 58, 34), new |

**Load with `SkinnedModel::load(path, 12)` for actor slots 12–14.** None of the materials is emissive.
Nodes and objects: `Tavern_Door` (the mesh plus skin node), `Tavern_Door_Rig`, joint `hinge` (index 0, identity rest). Every vertex is weighted 1.0 to `hinge`. The loader check passes: one mesh plus skin node, every primitive has POSITION, NORMAL, JOINTS_0 and WEIGHTS_0, and there are no animations.
Animations: none
Revision: 2, 2026-10-27. Raised grain streaks and knots in a new third material, `Door_Grain`. Plank V-grooves are deepened from 0.025 to 0.03 m (see below).

### Grain
- **Streaks.** 3 per plank on each face: 9 on the front and 9 on the back.
  - Each streak is a low ridge raised **0.02 m** off the plank. It is 0.07 m across its foot and 0.03 m across its top, so **0.05 m at mid-height**.
  - Both sides turn **45°** from the plank face.
  - It sways gently along 0.72–2.66 m of the plank (a few run 0.10–1.30), and tapers to a point at each end over 0.25 m.
- **Knots.** 2 on each face: front on plank 1 at (x 0.38, y 1.20) and plank 2 at (1.10, 2.20); back on plank 2 at (1.05, 1.05) and plank 3 at (1.80, 2.25). Model x and y.
  - Each is an oval boss, 0.20 × 0.30 m at its foot, with the same 0.02 m rise and 45° sides.
  - Nearby streaks start, stop or pass beside them.
- **Crease test, checked.** Every `Door_Grain` face is either a top (within 10° of the plank: the widest is 4.6°, the tapering tips) or a side turned more than 40°. The smallest side angle is 45.0°.
- **Placement, checked.**
  - All grain stays on its plank.
  - It keeps 0.03 m clear of the strap hinges, the ring plate and ring, the leaf's edges, and the other grain.
  - It is within 0.02 m of its face. The leaf with grain is z -0.02 to 0.14, against 0 to 0.12 for the leaf alone. The iron already stood 0.015 off each face, and the ring stands to z 0.15.
- **Plank joints (V-grooves): deepened from 0.025 to 0.03 m.** Revision 1's groove faces turned only 39.8°, just under the crease test, so plank joints could never take crease ink. At 0.03 m they turn 45.0°, and the build checks it. The groove stays 0.06 m wide, inside the leaf's 0.12 m. Leaf size, bounds and every recorded figure are unchanged.
- **24 px/m preview.** In the front view, 206 of the 389 grain pixels take crease ink. In the back view, the ink falls mostly on the plank pixels beside each streak's side turned away from the camera, as the one-pixel rule says.

### Revision-1 figures, restated and confirmed unchanged
- Leaf 2.18 × 3.38 × 0.12 m, spanning x 0 to 2.18 and y 0 to 3.38 in the model.
- Iron to x -0.012 at the hinge edge.
- Joint `hinge` at the origin, identity.
- Turned 100° open, the leaf reaches **0.379 m** back past the hinge line (-0.3786, checked against revision 1). Back-face grain stops at x 2.035, so it does not add reach.
- **Swing check (new):** through every 0.5° step from 0 to 100°, no grain vertex enters the frame. Wall side, in the hinge's frame: z > 0 only while 0 ≤ x ≤ 2.2.
- Exterior placement: hinge **(-1.1, 0, -0.25)**, yaw 0. Interior placement: hinge **(11.3, 0, 0.0)**, yaw +90°. Both as in the tavern assets.md.

---

## tavern_exterior (revision 5)
Source: tools/blender/build_tavern_exterior.py
Export: assets/scenes/tavern_exterior.glb
Preview:
- `assets/scenes/tavern_exterior.png` (the four views, as revision 4) and `tavern_exterior_roof.png` (as revision 4).
- `assets/scenes/tavern_exterior_masonry.png`, new. Each panel is a 24 px/m toon render with the engine's day palette and outline, scaled up, beside a smooth render of the same crop:
  - top row: the plinth, threshold and the patch under the left window, from the default camera (8 × 3 m, 3×);
  - bottom row: the chimney from the front-right (yaw 225; 3.2 × 9.6 m, 3×), and the patch over the right window from the default camera (2 × 1.2 m, 6×).
Format: glb, static. Units, axes and origin as revision 4: the middle of the doorway's threshold at floor level.
Triangles: **9,117** (budget 14,000). Revision 4 was 5,990.
- Foundation 1,064 (was 252), Walls 1,526 (288), Chimney 1,054 (118).
- Gable_Front 679 (616), Dormer_Front 403 (364), Dormer_Back 403 (364).
- Timbers 516, Windows 360, Doorway 36, Vestibule 48, Roof 2,870 and Sign 158 are unchanged.

Materials, in glTF order: Plaster, Stone, Dirt, Wood, Roof (revision 4: Stone, Dirt, Wood, Plaster, Roof). The order follows first use. Static meshes are coloured by name, so it has no effect. No new material.
Nodes and objects: as revision 4. `Foundation` and `Chimney` now hold Stone and Plaster (the mortar bed). `Walls` holds Plaster and Stone (the patches' stones).
Animations: none
Revision: 5, 2026-10-27. The plinth, threshold and chimney are individual stones in light mortar. The plaster panels are faceted, with three spalled patches. Nothing moved or resized.

### Masonry (plinth, threshold, chimney)
- **Every stone is its own block of `Stone`**, standing **0.035–0.05 m proud** of a recessed `Plaster` mortar bed.
  - The bed sits 0.05 m inside revision 4's outline, and no stone passes that outline.
  - Joints are **0.06 m** (the narrowest measured is 0.055, on the chimney shoulder's slanted courses), 1.4 px at 24 px/m.
- **Courses are coursed and staggered.**
  - Each course's vertical joints are at least **0.12 m** from the course below's. The measured minimum is exactly 0.12.
  - Stones run 0.55–0.85 m long.
  - At every corner the faces take turns course by course, like quoins.
- **Plinth.**
  - Three courses (y -0.6 to -0.25, -0.25 to 0.10, 0.10 to about 0.47) on all four sides.
  - The stones run 0.22 m in, under the timber face. Top-course stones' tops are 0.45–0.47, 0.03–0.05 over the bed's top (0.42).
  - The right end is split either side of the chimney's base.
- **Threshold.**
  - Three slabs, their tops flush at **y 0** (walkable, unchanged), over a course of stones on its face.
  - The joints between slabs are recessed to y -0.05.
- **Chimney.**
  - Stones on the base, the two stack stages (6 courses each), the shoulder's four slopes (2 courses each, a mortar line along each hip) and the four jogged flue courses (2 courses each).
  - The cap stays one stone, and the pot stays Dirt.
- **Totals:** 233 stones in 105 courses and rows.
- **Ruins:** untouched. Its Stone palette and edges are the engine's and unchanged.

### Render
- **Faceted panels.** Every plaster panel between the timbers is cut into gentle facets: 70 panels.
  - That covers all four walls, the cross-gable's face (rectangle and apex) and cheeks, and both dormers' faces and cheeks.
  - Each panel is split into slanted strips about 0.5 m wide, and each strip is fanned into four facets round a point recessed **0.012–0.03 m into the wall**.
  - Panel edges stay on revision 4's plaster face. Nothing stands proud of it (checked), so no plan, doorway or vestibule figure can move.
  - Each panel has at least three facet directions (checked).
- **Spalled patches: 3, all on the front wall**, which faces the default camera (facing 0.845 against the 0.5 minimum).
  | Patch | Panel | Outline: x, y | Stones inside / seen from the default camera |
  |---|---|---|---|
  | 1 | under the left window | -2.85 to -2.11, 0.83 to 1.18 | 4 / 4 |
  | 2 | under the right window | 3.13 to 3.75, 0.80 to 1.18 | 3 / 3 |
  | 3 | over the right window | 2.87 to 3.62, 3.53 to 3.92 | 3 / 3 |
  - Each is an irregular 11-point outline. The broken render edge is recessed up to 0.02 m.
  - The hole's sides go down to a Plaster mortar bed **0.08 m** behind the face.
  - Stones (Stone, 0.24–0.34 × 0.13 m, joints 0.06 m, 0.035–0.05 m proud of the bed) still sit behind the render's face.
  - All are checked to be at least 0.06 m inside their panel, ≥ 0.15 m from every window opening and sill board, and ≥ 0.3 m from the doorway (jambs included) and the sign.

### New checks (`masonry_checks`, `render_checks`; the build fails if any breaks)
| Rule | Result |
|---|---|
| Every joint, between stones and between courses, ≥ 0.05 m | Pass: minimum 0.055 |
| Every stone 0.03–0.05 m proud of its bed; exposed tops 0.03–0.05 over the bed's top | Pass: 0.035–0.05 |
| Each course's joints ≥ 0.12 m from the course below's | Pass: minimum 0.12 |
| No Foundation or Chimney vertex outside revision 4's boxes | Pass |
| Every plaster panel faceted, ≥ 3 facet directions, recessed 0.012–0.03, none proud of the face | Pass: 70 panels |
| 3–4 patches, inside their panels, clear of windows, doorway and sign, on a camera-facing wall, ≥ 3 stones inside and seen by the default camera, joints ≥ 0.05, stones 0.03–0.05 proud and behind the render | Pass: 3 patches |
| Nothing of Foundation, Walls, Chimney or the gables inside the doorway (x -1.1 to 1.1, z -0.25 to 0.85) or vestibule rectangles between the floor and 3.3 m | Pass |
| ≤ 14,000 triangles | Pass: 9,117 |

### Revision-4 checks: all still pass, with the same figures
- Plinth 9.0 × 7.0 m. Vestibule 2.75 m deep (≥ 2.5). Sign 0.625 m from the jamb (≥ 0.3). Roof rise less than the house's depth. Chimney top ≥ 1 m above the roof.
- Roof rules: everything inside revision 2's eaves, top y 8.66, nothing over the doorway or vestibule below 3.3, the gables' Wood, Plaster and Dirt over the plinth, gable roofs buried, plan outline x -4.55 to 5.59 and z -7.1 to 1.77.
- Tile checks:
  - 340 main-roof joints; Gable_Front 32; each dormer 12.
  - Notches ≥ 49.9°, lips ≥ 48.7°.
  - Every one of the 78 front joints and 83 front lip points is inked in the roof preview.
  - Roof ink pixels went from 8,047 of 25,582 to 8,054 of 25,610, only because the chimney's new joints let a few more slate pixels show past it.

### Revision-4 placement figures, restated and confirmed unchanged
The build prints them identically:
- **Doorway:** clear opening 2.2 × 3.4 m, x -1.1 to 1.1, y 0 to 3.4, through the wall from z +0.27 to -0.25. Threshold flush with y 0, stepping out to z +0.47.
- **Door hinge:** **(-1.1, 0, -0.25)**, yaw 0. Doorway blocker: the segment (-1.1, z -0.19) to (1.1, z -0.19).
- **Vestibule:** x -1.9 to 1.9, z -0.25 to -3.0 (2.75 m deep). Dirt walls up to y 4.4.
- **Footprint polygon** (xz): (-4.1, 0.35), (-1.3, 0.35), (-1.3, 0.47), (1.3, 0.47), (1.3, 0.35), (4.9, 0.35), (4.9, -3.6), (5.55, -3.6), (5.55, -5.2), (4.9, -5.2), (4.9, -6.65), (-4.1, -6.65).
- **Eaves:** x -4.55 to 5.35, z -7.1 to 0.8, underside y 4.35.
- **Chimney:** base x 4.5 to 5.55, z -3.6 to -5.2; flue x 4.65 to 5.47, z -3.88 to -4.92, up to y 8.2; cap x 4.53 to 5.59, z -3.76 to -5.04, y 8.2 to 8.45; pot top **y 8.9**. The build's measured extents print exactly as before: x 4.5 to 5.59, z -5.2 to -3.6, top 8.9.
- **Sign:** x 1.925 to 2.125, out to z 1.77, y 2.75 to 4.35. 0.625 m from the jamb.
- **Heights:** roof top y 8.66. Gable finial 8.10, dormer finials 7.62. Overall y -0.6 to 8.9.
- **Plan outline:** x -4.55 to 5.59, z -7.1 to 1.77.
- `kTavernSpot` and `kTavernYaw` need no change.

---

## tavern_interior (revision 4)
Source: tools/blender/build_tavern_interior.py
Export: assets/scenes/tavern_interior.glb
Preview:
- `assets/scenes/tavern_interior.png` and `tavern_interior_overview.png`, as revision 3.
- `assets/scenes/tavern_interior_masonry.png`, new: from the design camera, 24 px/m toon crops with the day palette, each beside a smooth render. The hearth (6 × 5.5 m, 3×) and the patch on the hearth wall (2.4 × 1.6 m, 6×). The shadow-only pieces are hidden.
Format: glb, static. Units, axes, origin, design yaw (45°), cutaway and shadow-only nodes: as revision 3.
Triangles: **11,927** (budget 20,000). Revision 3 was 9,251: Hearth 1,142 (144), Wall_PosZ 1,210 (264), Wall_PosX 852 (120). Everything else is unchanged.
Materials, in glTF order: Oak, Dirt, Wood, Stone, Plaster, Roof (unchanged). Roof is only on `Roof_ShadowOnly`.
Nodes and objects (70): unchanged names. `Hearth` adds Plaster (mortar). `Wall_PosZ` and `Wall_PosX` add Stone (the patches).
Animations: none
Revision: 4, 2026-10-27. The hearth is individual stones in light mortar. The drawn walls are faceted, with two spalled patches. Nothing moved.

### Hearth
- **Slab.** Flagstones in three rows, full thickness (y -0.1 to 0.3).
  - Tops flush at **y 0.3**. The ash bed and fire base are unchanged.
  - Front and side faces sit exactly on revision 3's outline (proud 0.05 of a bed 0.05 in), so the Hearth hull and every walkway figure hold.
- **Cheeks.** Six courses each, on front, outer and firebox faces, taking turns at the corners.
  - Firebox faces at x -5.3 and -3.1 (at most, as before).
  - A mortar joint over the slab and under the lintel.
- **Lintel.** A long lintel stone over the firebox between two springers. Its underside stays at **y 2.3**, so the firebox opening is unchanged.
- **Breast.** One course behind the mantel, the jutting course, then four courses to the wall plate.
  - The jutting course's tops sit 0.03–0.05 over its bed, so its joints show from above.
- **Mantel and the rest.** The mantel stays `Oak`. The sooty back, ash bed and logs are unchanged and byte-identical.
- **Totals:** 120 stones in 58 courses and rows. Joints 0.06. Proud 0.035–0.05. Stagger ≥ 0.12.
- **Envelope, checked.** No Stone or Plaster vertex of `Hearth` is outside revision 3's slab, cheek, lintel, breast and jutting-course boxes.

### Render
- **Faceted walls.** All 57 plaster panels of `Wall_PosZ` and `Wall_PosX` are faceted as on the exterior, recessed 0.012–0.03 m **into the wall**.
  - No Plaster or Stone of either wall stands into the room (checked).
  - So the inner bounds (±11.3, ±8.5), walkways (measured from the posts) and view fit cannot move.
  - The shadow-only walls and stubs are unchanged.
- **Spalled patches: 2**, both facing the design camera (0.612 against the 0.5 minimum):
  | Wall | Outline | Stones inside / seen from the design camera |
  |---|---|---|
  | +Z (hearth wall), panel between the stage's window and the hearth | x -8.40 to -7.15, y 2.00 to 2.69, on z 8.5 | 10 / 8 |
  | +X (door wall), panel between the shelves' end and the door | z 0.80 to 1.79, y 1.92 to 2.61, on x 11.3 | 10 / 10 |
  - Each was checked at ≥ 0.06 m inside its panel, and ≥ 0.3 m from every window, `Shelves`, `Hearth` and every sconce.
  - The patches are built as on the exterior: mortar bed 0.08 m back, stones 0.035–0.05 proud, joints 0.06.

### Revision-3 checks: all still pass, and every recorded figure is unchanged
The build output was diffed line for line against HEAD's.
- **Design yaw 45°.** Cutaway walls -X and -Z.
- **Room inner bounds (camera clamp):** x -11.3 to 11.3, z -8.5 to 8.5. Outer faces x ±11.7, z ±8.9.
- **View fit:** 29.13 × 18.64 m, inside 40 × 22.5.
- **Hearth fire base:** **(-4.2, 0.41, 7.75)**.
  - Firebox opening x -5.3 to -3.1, y 0.3 to 2.3, z 7.2 to 8.3.
  - Slab x -6.3 to -2.1, z 6.6 to 8.5.
  - Hearth block bounds x -6.3 to -2.1, z 6.6 to 8.5, top 4.6.
- **Brazier fire base:** **(8.6, 1.16, -4.4)**, size 0.5. Footprint radius 0.456 at (8.6, -4.4), top 1.08.
- **Candles:** (-7.3, 1.45, 1.9), (-2.2, 1.45, 1.9), (2.55, 1.45, 1.9), (-7.3, 1.45, -4.1), (-1.6, 1.45, -4.1), (4.2, 1.45, -4.1), (8.0, 1.95, 5.55).
- **Sconce flame bases:** Sconce_1 (-11.175, 2.6, 8.0), Sconce_2 (7.408, 2.6, 8.0), Sconce_3 (10.8, 2.6, -4.15), Sconce_4 (10.8, 2.6, -8.375). The sconce checks pass.
- **Inside door:** hinge **(11.3, 0, 0.0)**, yaw +90°. Blocker (11.36, z 0.0) to (11.36, z -2.2).
- **Doorway centre:** (11.5, 0, -1.1). Room-side face centre (11.3, 0, -1.1).
- **Hero spawn:** **(8.7, 0, -1.1)**, clearance 2.45 m.
- **Walkways:**
  - Bar–Shelves 2.10, Brazier–+X wall 2.13, Table_D–Table_E 2.18, Barrels–Hearth 2.20, Hearth–Stage 2.20, Table_E–Table_F 2.27, Table_A/Table_D–-X wall 2.33, Table_B–Table_C 2.34, Brazier–Table_F 2.39. All other gaps are as before.
  - The 2.5 m path reaches the bar, the hearth and Table_A to Table_F.
- **Seats:** all 25 match the table in `docs/tasks/tavern-detail/assets.md`.
- **Stage outline and top 0.72.** Table, bar, shelf and barrel positions. Window openings. All unchanged.
- **Oak:** exactly as revision 3. `oak_checks` passes, and the mantel is Oak.

---

## Notes for the developer
- **Door slots 12–14.** The door now has three materials. Load it with `SkinnedModel::load(path, 12)` so `Door_Grain` lands in slot 14. Nothing else may use slot 14.
- **Door crease ink.** Plank grooves and grain sides all turn 45° from the plank face, with no depth step (≤ 0.03 m). So the Oak/Roof test (`dot < 0.766`, `|dq - d| < kSilhouette`) fires on them once `Door_Wood` and `Door_Grain` pixels are flagged for crease ink.
  - At a 0.35 mix the lines are faint on dark wood. `tavern_door_grain.png` panel 1 shows that mix, and panel 2 shows where the ink falls.
- **Masonry needs no engine change.** Joints read as `Plaster` (day mid `c8bca2`) between `Stone` (`a49c8c`). The patches' stones sit 0.04–0.05 m behind the render, and the mortar beds 0.05–0.08 m back.
- **`scene.ground`.**
  - The plinth tops now have 0.06 m slots, recessed 0.03–0.07 m.
  - The threshold's slab tops are flush at y 0, with joints dropping to y -0.05.
  - The hearth slab's top is flush at y 0.3, with joints to 0.25.
  - All of these are far below the 0.55 m step.
- **Nothing placement-related changed.** `kTavernSpot`, `kTavernYaw`, doorway, vestibule, hinge, footprint, room bounds, fire, candle and sconce positions and blockers need no change.
