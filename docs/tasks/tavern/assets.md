# Assets: The Sleeping Fox tavern

**All coordinates in this file are in engine coordinates: +Y up, +Z = Blender -Y, 1 unit = 1 metre. Each one is relative to its own asset's origin, before the code places the asset.** Everything is at hero scale (1.6× real). Each build script prints these figures from the same constants it builds with, so a rebuild can be checked against this file.

Rebuild any asset headless (it builds, checks, exports and renders its previews):

```
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_<exterior|interior|door>.py
```

The script is the source of truth. There is no .blend source, following the project's layout (`build_ruins.py`, `build_fox.py`). Each build fails if a check fails: the material names, unique object names (so a Blender `.001` can never hide `_ShadowOnly`), the triangle budget, and asset-specific checks listed below.

---

## ruins (revision)
Revision: 2026-10-25 (tavern cycle 3): back at HEAD. Ran `git checkout HEAD -- tools/blender/build_ruins.py assets/scenes/ruins.glb` because the user asked for the cycle-2 removals to be undone ("yeah definitely add those back"). `BushD`, `Rock`, `Rock.001`, `Flag.009/011/012` and `FlagInset.009/011/012` are back, and their ground squares are Dirt again. `git diff HEAD` shows neither file. The glb has 225 mesh nodes under the `Ruins` root, including all nine. The cycle-2 notes below are kept as history only and no longer describe the files.

Source: tools/blender/build_ruins.py
Export: assets/scenes/ruins.glb
Preview: none (no preview existed before, and the scene is unchanged apart from the removals)
Format: glb, static
Units, axes, origin: unchanged
Revision: 2026-10-25 (tavern cycle 2):
- removed `BushD`, `Rock`, `Rock.001`, `Flag.009`, `Flag.011` and `Flag.012`, plus those flags' inset slabs `FlagInset.009`, `FlagInset.011` and `FlagInset.012` (orchestrator's decision);
- the three flags' ground squares are back to Grass.

**How:**
- The script still builds every prop, so every draw from the seeded `rng`, and every later object's auto-name (`Rock.002`, `Flag.010`, ...), is exactly as before. `remove_cleared_props()` deletes the nine `CLEARED_PROPS` just before export. It fails the build if any of them is missing, so a name shift can't go unnoticed.
- `build_flagstones` records each flag's footprint by object name (`flag_footprints`). `clear_flag_footprints()` drops the cleared flags' footprints before `build_ground`, so `in_footprint` no longer marks those cells Dirt. Cells that another footprint also covers stay Dirt.
- This script resets Blender's factory settings, so it is built headless only, never in the live session.

**Verified** against the previous glb (`git show HEAD:assets/scenes/ruins.glb`) with a standalone glb parser:
- 225 → 216 mesh nodes. Exactly the nine above are gone and nothing was added.
- 215 of the 216 kept nodes are **byte-identical**: POSITION, NORMAL and index buffers, material per primitive, accessor min/max and world transform.
- The one that differs is `Ground`, and only in its Grass/Dirt split:
  - The same 41,472 triangles, with identical positions, winding and normals, and an identical node transform.
  - 96 triangles (48 grid cells of 0.5 m) changed from Dirt to Grass, and nothing else changed material. All 96 lie under the removed flags: 32 under `Flag.009`, 44 under `Flag.011` and 20 under `Flag.012`. Part of `Flag.012`'s square is also covered by another footprint, so it stays Dirt.
  - The Grass and Dirt primitives' vertex and index counts change accordingly: Grass 20,364 → 20,394 vertices, Dirt 1,075 → 1,005. Vertices are duplicated along the material boundary.
- The material list is unchanged.

---

## tavern_exterior
Source: tools/blender/build_tavern_exterior.py
Export: assets/scenes/tavern_exterior.glb
Preview: assets/scenes/tavern_exterior.png (from the front-right, front-left and back-left; ortho, 30° down, at 48 px/m)
Format: glb, static (no skin)
Units and scale: 1 unit = 1 metre, hero scale (1.6× real)
Axes: +Y up. The door faces +Z (Blender -Y).
Origin: the middle of the doorway's threshold, centred across the doorway and through the wall's thickness, at floor level (y = 0). The foundation reaches y = -0.6.
Triangles: 1,854 (budget 4,000)
Materials (glTF order): Stone, Dirt, Wood, Roof, Plaster. No textures. Colours in the glb are preview-only.
Nodes and objects: `Foundation` (Stone plinth and threshold), `Walls` (Plaster), `Timbers` (Wood frame), `Windows` (Dirt panes, Wood mullions and sills), `Doorway` (Wood jambs and head), `Vestibule` (Wood floor, Dirt walls), `Roof` (Roof), `Chimney` (Stone, Dirt pot), `Sign` (Wood board and bracket, Plaster fox, Dirt hangers)
Animations: none
Revision: 1, 2026-10-24, first build
Revision: 2, 2026-10-25. Rebuilt with a 9 × 7 m plinth (it was 17.2 × 13.2):
- The roof is hipped thatch at 45° (it was 50°), with the ridge top at y 9.13 (it was 13.6). The roof rises 4.2 m over a house 6.8 m deep.
- The chimney is shorter, and stands 0.65 m past the plinth (it was 0.9).
- The sign hangs on its own post, 0.625 m clear of the jamb.
- Same style, materials, doorway, door placement and vestibule.

The build checks:
- the plinth is 9.0 × 7.0 m;
- the vestibule is at least 2.5 m deep;
- the sign is at least 0.3 m from the jamb's outer edge;
- the roof rises less than the house is deep;
- the chimney top is at least 1 m above the roof where it passes.

**Doorway** (unchanged). The clear opening is 2.2 m wide × 3.4 m tall: x -1.1 to 1.1, y 0 to 3.4. It runs through the wall from z = +0.27 (the jamb faces, outside) to z = -0.25 (the wall's inside face). The Wood jambs are at x ±1.1 to ±1.3, and the head at y 3.4 to 3.65. The threshold is Stone, flush with y = 0, and steps out to z = +0.47.

**Door hinge and closed direction** (unchanged). Place `tavern_door.glb` with its origin at **(-1.1, 0, -0.25)** and **yaw 0** (no rotation).
- Closed, the leaf runs from the hinge along **+X** to x = 1.08, and its front faces +Z (outside). Its thickness occupies z -0.25 to -0.13.
- Opening is a **positive** turn of `hinge` about +Y, which swings the free edge toward -Z, into the building. At 100° the leaf lies along -Z, reaching z ≈ -2.4 and back to x ≈ -1.49.
- Doorway blocker for the closed door: the segment (-1.1, z -0.19) to (1.1, z -0.19).

**Vestibule** (unchanged). A Wood floor at y = 0, spanning **x -1.9 to 1.9 and z -0.25 to -3.0**: 2.75 m deep behind the wall's inside face, and 3.0 m from the origin. Its Dirt walls stand x ±1.9 to ±2.1 and z -3.0 to -3.2, up to y 4.4. It is open above into the shell under the roof, and holds the door's full 100° swing.

**Footprint polygon** (xz, ground level, the plinth outline in order):
(-4.1, 0.35), (-1.3, 0.35), (-1.3, 0.47), (1.3, 0.47), (1.3, 0.35), (4.9, 0.35), (4.9, -3.6), (5.55, -3.6), (5.55, -5.2), (4.9, -5.2), (4.9, -6.65), (-4.1, -6.65).
That is the **9.0 × 7.0 m** plinth (x -4.1 to 4.9, z 0.35 to -6.65), plus the threshold step at the front (to z 0.47) and the chimney base on the +X end (to x 5.55, z -3.6 to -5.2). Nothing else touches the ground.

**Overhead outlines beyond the footprint** (for clearance and framing checks):
- Roof eaves: x -4.55 to 5.35, z -7.1 to 0.8, underside y = 4.35. The roof's highest point is the ridge roll, at **y 9.13**, running x -0.6 to 1.4 at z -3.15. The thatch slopes meet the ridge at y ≈ 8.84.
- Chimney: the base is x 4.5 to 5.55 and z -3.6 to -5.2, from the foundation to y 4.9. Above it, the flue is x 4.65 to 5.47 and z -3.88 to -4.92, up to y 8.2. The cap is x 4.53 to 5.59, z -3.76 to -5.04, y 8.2 to 8.45. The Dirt pot is x 4.86 to 5.26, z -4.2 to -4.6, and its top is at **y 8.9**.
- Sign: board, hangers and bracket at **x 1.925 to 2.125**, reaching out to **z = 1.77**, between y 2.75 and 4.35. The board itself is y 2.75 to 3.75 and z 0.45 to 1.65.
  - It is 0.625 m from the jamb's outer edge (x 1.3) and 0.825 m from the opening (x 1.1).
  - Nothing of it is over the doorway or the threshold.
  - Its post is x 1.9 to 2.15 on the front wall.

So the exterior's whole plan outline, eaves and sign included, is x -4.55 to 5.59 and z -7.1 to 1.77. Its height is y -0.6 to 9.13.

**Other notes**
- Windows are dark recessed panes (Dirt, 0.2 m back), not holes, at 1.2 m wide and y 1.5 to 3.2. There are two on the front (x -3.1 to -1.9 and 2.7 to 3.9), two on the back, one on the -X end and one on the +X end (z -0.9 to -2.1). No glow.
- Plaster panels sit 0.08 m behind the rails and braces, and 0.10 m behind the posts.

---

## tavern_interior
Source: tools/blender/build_tavern_interior.py
Export: assets/scenes/tavern_interior.glb
Preview: assets/scenes/tavern_interior.png and assets/scenes/tavern_interior_overview.png.
- `tavern_interior.png` is the design view (yaw 45°, 30° down, ortho), framed exactly as the game's 960×540 view at 24 px/m with the target at the room centre, and rendered at 2× pixels. The shadow-only pieces are hidden.
- `tavern_interior_overview.png` has two views:
  - left: the design view with the shadow-only roof and walls shown;
  - right: a plan from straight above with them hidden. In the plan, right is +X and up is -Z, so the hearth wall is at the bottom and the door on the right.
Format: glb, static (no skin)
Units and scale: 1 unit = 1 metre, hero scale (1.6× real)
Axes: +Y up, Blender -Y = engine +Z
Origin: the room's floor centre, with the floor top at y = 0. The plank grooves dip to y = -0.048.
Triangles: 8,843 (budget 20,000)
Materials (glTF order): Wood, Dirt, Stone, Plaster, Roof. Roof is used only on `Roof_ShadowOnly`. The brazier's iron is Dirt and its bowl Stone.
Nodes and objects (66):
- Shell: `Floor`, `Threshold`, `DoorStep`, `Wall_PosX` (door wall), `Wall_PosZ` (hearth wall), `Timbers`, `Doorway`, `WallStub_NegX`, `WallStub_NegZ`
- Shadow-only (exact node names): `Wall_NegX_ShadowOnly`, `Wall_NegZ_ShadowOnly`, `Roof_ShadowOnly`
- `Hearth`, `Stage`, `Brazier`, `Bar`, `Bar_Mugs`, `Shelves`, `Bottles`, `Shelf_Mugs`, `Barrel_1`–`Barrel_5`
- `Table_A`–`Table_F`, `Table_A_Mugs`, `Table_C_Mugs`, `Table_E_Mugs`
- Seats: `Chair_A1`–`A4`, `Chair_B1`–`B2`, `Chair_C1`–`C4`, `Chair_D1`–`D4`, `Chair_F1`–`F3`, `Bench_E1`, `Bench_E2`, `Stool_1`–`Stool_6`
- `Candle_1`–`Candle_7`
Animations: none
Revision: 1, 2026-10-24, first build
Revision: 2, 2026-10-25. Rebuilt at 22.6 × 17 m inside (it was 16 × 12) with roomier contents:
- 6 tables (one a trestle with benches);
- the bar along the door wall, with 6 stools;
- 5 barrels;
- a brazier and a small corner stage, both new;
- 4 windows, all on the drawn walls. The shadow-only walls no longer have openings;
- 7 candles.

The build now checks the walkway and path rules and the view fit. It keeps the design yaw, the cutaway approach, the doorway and the door model.

**Design yaw: 45°**, the engine's default (`Camera::yaw_`). The camera looks toward +X and +Z, 30° down.
- The cutaway walls nearest it are **-X** (`Wall_NegX_ShadowOnly`) and **-Z** (`Wall_NegZ_ShadowOnly`). Their drawn stubs are 0.45 m tall (`WallStub_*`).
- The drawn walls are +X (doorway, bar and one window) and +Z (hearth and three windows).
- Nothing crosses over the room.

**Room inner bounds (camera clamp): x -11.3 to 11.3, z -8.5 to 8.5.**
- The outer faces of the walls are at x ±11.7 and z ±8.9.
- The shadow-only roof is y 4.55 to 4.95.
- Posts stand 0.12 m into the room from the +X and +Z walls. The stubs' skirting stands 0.06 m in from -X and -Z.
- At the design yaw, the visible room projects to **29.1 × 18.6 m** on screen, inside the 40 × 22.5 m view, and the build checks this. A camera target clamped to the inner bounds keeps it framed.

**Hearth fire base point: (-4.2, 0.41, 7.75)**, 0.08 m above the ash bed.
- The firebox opening is x -5.3 to -3.1, from the hearth slab at y 0.3 up to the lintel at y 2.3.
- The firebox runs from z 7.2 (the opening) to 8.3 (the sooty back).
- The hearth slab is x -6.3 to -2.1, z 6.6 to 8.5.
- Above, the breast reaches the wall plate.

**Brazier fire base point: (8.6, 1.16, -4.4)**, 0.08 m above the coal bed (y 1.08), size 0.5.
- **Footprint:** a circle at (8.6, -4.4) of radius **0.456** (legs and bowl). Its top is y 1.08.
- It blocks through a `Blocker`, so leave `Brazier` out of `scene.ground` as for the seats. Its nearest neighbours are the +X wall at 2.13 m and Table_F's chairs at 2.39 m.

**Candle flame base points** (the top centre of each wax stub):
| Candle | On | Base point |
|---|---|---|
| Candle_1 | Table_A | (-7.3, 1.45, 1.9) |
| Candle_2 | Table_B | (-2.2, 1.45, 1.9) |
| Candle_3 | Table_C | (2.55, 1.45, 1.9) |
| Candle_4 | Table_D | (-7.3, 1.45, -4.1) |
| Candle_5 | Table_E (trestle) | (-1.6, 1.45, -4.1) |
| Candle_6 | Table_F (round) | (4.2, 1.45, -4.1) |
| Candle_7 | Bar | (8.0, 1.95, 5.55) |

**Inside door hinge and closed direction.** Place `tavern_door.glb` with its origin at **(11.3, 0, 0.0)** and **yaw +90° about +Y**, so model +Z maps to world +X and model +X to world -Z.
- Closed, the leaf runs from the hinge along **-Z** to z = -2.18. Its front (ring handle) faces +X, out of the room, and its back (straps) faces in. Its thickness occupies x 11.3 to 11.42.
- Opening is a **positive** turn of `hinge` about +Y, as outside, swinging the leaf into the room (-X). At 100° the leaf points along (-0.985, 0, +0.174) from the hinge.
- The swing is clear of all furniture, and the build checks this. The nearest piece is the bar top's end (x 7.4 to 8.6, z 2.5), about 2.2 m from the open leaf's tip at (9.15, 0.38).
- Doorway blocker for the closed door: the segment (11.36, z 0.0) to (11.36, z -2.2).

**Doorway centre: (11.5, 0, -1.1)**, at the wall's mid-thickness. The centre on the room-side face is (11.3, 0, -1.1).
- The clear opening is 2.2 × 3.4 m: z 0.0 to -2.2, y 0 to 3.4, through x 11.3 to 11.7. The jambs are at z 0 to 0.2 and -2.2 to -2.4, and the head at y 3.4 to 3.65.
- Outside it, `DoorStep` (Dirt, top y = -0.01) covers x 11.7 to 14.1, z -2.6 to 0.4. Past that there is nothing but the clear colour.

**Hero spawn: (8.7, 0, -1.1), facing -X** (into the room).
- It is 2.6 m inside the room-side face and 2.8 m from the doorway centre, so the door opens on arrival and closes as the hero walks away.
- It is 2.82 m from the hinge, outside the door's swing.
- It has 2.45 m clearance all round, and the build checks that nothing is within 0.6 m.

**Walkways and paths (checked by the build, which fails if either rule breaks).** Furniture is measured in groups by the convex hulls of each object's plan outline: each table with its chairs or benches, candle and mugs; the bar with its stools, mugs and candle; the shelves; the barrels; the hearth; the stage; the brazier. Walls are measured from their posts (+X, +Z) or skirting (-X, -Z).
- **Every walkway is ≥ 2.0 m.** Every gap between two groups is at least 2.0 m. A group either stands against a wall (≤ 0.1 m) or leaves at least 2.0 m to it.
  - Standing against walls: the bar (+Z), shelves (+X, +Z), barrels (+Z), hearth (+Z), stage (-X, +Z).
  - The narrowest gaps:
    | Gap | Width |
    |---|---|
    | Bar to shelves (behind the bar) | 2.10 m |
    | Brazier to +X wall | 2.13 m |
    | Table_D to Table_E | 2.18 m |
    | Barrels to hearth | 2.20 m |
    | Hearth to stage | 2.20 m |
    | Table_E to Table_F | 2.27 m |
    | Table_A to -X wall | 2.33 m |
    | Table_D to -X wall | 2.33 m |
    | Table_B to Table_C | 2.34 m |
    | Brazier to Table_F | 2.39 m |
    | Table_D to -Z wall | 2.74 m |
    | Bar to Table_C | 2.76 m |
  - Everything else is wider.
- **A ≥ 2.5 m clear path runs from the door to the bar, the hearth and every table.** A 2.5 m disc swept from the spawn over a 5 cm grid reaches the bar, the hearth and Table_A to Table_F. The main aisle runs straight in from the door between the two rows of tables (z -2.38 to 0.18 between the chairs, about 2.56 m wide). The hearth is reached down the 2.68 m gap between Table_A and Table_B.
  - As a sanity test, a 3.0 m disc fails to reach the hearth, so the check can fail.

**Seat blocker footprints** (chairs, stools, benches): centre (x, z), a bounding radius measured from the mesh, and the seat as a rectangle of the given half-sizes turned by yaw about +Y (the same angle as Blender's Z yaw). Chair backs are 1.52 m tall, seats 0.74 m. Bench seats are 0.74 m. Stool seats are 1.18 m.
| Name | Centre (x, z) | Radius | Half-size (along, across), yaw |
|---|---|---|---|
| Chair_A1 | (-6.05, 2.05) | 0.467 | 0.33 × 0.33, -83.1° |
| Chair_A2 | (-8.55, 1.95) | 0.467 | 0.33 × 0.33, 85.4° |
| Chair_A3 | (-7.24, 0.65) | 0.467 | 0.33 × 0.33, 2.9° |
| Chair_A4 | (-7.12, 3.15) | 0.467 | 0.33 × 0.33, 172.0° |
| Chair_B1 | (-2.38, 0.65) | 0.467 | 0.33 × 0.33, 6.9° |
| Chair_B2 | (-2.26, 3.15) | 0.467 | 0.33 × 0.33, 175.4° |
| Chair_C1 | (3.8, 2.05) | 0.467 | 0.33 × 0.33, -83.1° |
| Chair_C2 | (1.3, 1.95) | 0.467 | 0.33 × 0.33, 85.4° |
| Chair_C3 | (2.61, 0.65) | 0.467 | 0.33 × 0.33, 2.9° |
| Chair_C4 | (2.73, 3.15) | 0.467 | 0.33 × 0.33, 172.0° |
| Chair_D1 | (-6.05, -3.95) | 0.467 | 0.33 × 0.33, -83.1° |
| Chair_D2 | (-8.55, -4.05) | 0.467 | 0.33 × 0.33, 85.4° |
| Chair_D3 | (-7.24, -5.35) | 0.467 | 0.33 × 0.33, 2.9° |
| Chair_D4 | (-7.12, -2.85) | 0.467 | 0.33 × 0.33, 172.0° |
| Chair_F1 | (5.47, -3.95) | 0.467 | 0.33 × 0.33, -83.1° |
| Chair_F2 | (2.93, -4.05) | 0.467 | 0.33 × 0.33, 85.4° |
| Chair_F3 | (4.26, -5.37) | 0.467 | 0.33 × 0.33, 2.9° |
| Bench_E1 | (-1.6, -5.125) | 1.814 | 1.8 × 0.225, 0° (runs along x) |
| Bench_E2 | (-1.6, -3.075) | 1.814 | 1.8 × 0.225, 0° (runs along x) |
| Stool_1 | (7.05, 7.8) | 0.28 | 0.28 × 0.28, -8.6° |
| Stool_2 | (7.05, 6.9) | 0.28 | 0.28 × 0.28, 0.0° |
| Stool_3 | (7.05, 6.0) | 0.28 | 0.28 × 0.28, 8.6° |
| Stool_4 | (7.05, 5.1) | 0.28 | 0.28 × 0.28, -8.6° |
| Stool_5 | (7.05, 4.2) | 0.28 | 0.28 × 0.28, 0.0° |
| Stool_6 | (7.05, 3.3) | 0.28 | 0.28 × 0.28, 8.6° |

The benches are long and thin, so a single circle would block far too much. Use the rectangle, or two to four circles along it.

**Stage:** a raised Wood platform in the corner between the -X and +Z walls, its front corner cut off.
- Plan outline: (-11.3, 8.5), (-8.5, 8.5), (-8.5, 6.9), (-9.5, 5.9), (-11.3, 5.9).
- Top at **y 0.72**, above the 0.55 m step, so its edge blocks through `scene.ground`. It is empty.

**Other furniture** (blocks through `scene.ground`; all taller than the 0.55 m step):
- Tables are 1.2 m high:
  - Square, 1.6 × 1.6 m: Table_A at (-7.3, 1.9), Table_B at (-2.2, 1.9), Table_C at (2.55, 1.9), Table_D at (-7.3, -4.1).
  - Table_E, the trestle: top 3.8 × 1.1 m, x -3.5 to 0.3, z -4.65 to -3.55.
  - Table_F, round: radius 0.85 at (4.2, -4.1).
- Bar: the body is x 7.6 to 8.5 and z 2.6 to 8.5, its far end against the +Z wall. The top is y 1.7, overhanging to x 7.4 to 8.6 and z 2.5. The foot rail reaches x 7.24.
- Shelves: x 10.7 to 11.3, z 2.6 to 8.35, up to y 3.7. The 2.1 m behind the bar is clear.
- Barrels (radius 0.5, height 1.45), at (0.6, 7.85), (1.65, 7.85), (2.7, 7.85), (1.1, 6.95), (2.2, 6.95).
- Hearth slab: x -6.3 to -2.1, z 6.6 to 8.5.

**Windows** are real holes, 1.4 × 1.7 m (sill y 1.5, head y 3.2), each crossed by a Wood mullion and transom so the light falls in four panes. All four are on the drawn walls, as the task asks:
- +Z wall: x -10.6 to -9.2 (over the stage), x 0.95 to 2.35 (over the barrels), and x 4.0 to 5.4.
- +X wall: z -7.3 to -5.9.

So daylight patches fall only while the sun is on the +X/+Z side of the room. The shadow-only walls have no openings.

---

## tavern_door
Source: tools/blender/build_tavern_door.py
Export: assets/scenes/tavern_door.glb
Preview: assets/scenes/tavern_door.png (front from the front-left, back from the back-right, and swung open 100°; ortho, 30° down)
Format: glb, one skinned mesh (`Tavern_Door`) with a one-joint armature
Units and scale: 1 unit = 1 metre, hero scale. The leaf is 2.18 m wide × 3.38 m tall × 0.12 m thick, sized to clear the 2.2 × 3.4 m doorway as it swings.
Axes: +Y up. At rest (closed) the front faces +Z (Blender -Y). The leaf runs from the hinge along +X.
Origin: the bottom of the hinge edge, on the back face. The leaf spans x 0 to 2.18, y 0 to 3.38, and z 0 to 0.12 (front). Iron stands 0.015 off each face and to x = -0.012 at the hinge edge. The ring stands to z = 0.15.
Triangles: 94 (budget < 100)
Materials, in glTF order: 0 `Door_Wood`, warm brown, sRGB (128, 82, 46); 1 `Door_Iron`, dark iron, sRGB (46, 44, 48). Load with `SkinnedModel::load(path, 12)` for actor slots 12–13. Neither material is emissive.
Nodes and objects: joint `hinge` (the only joint) at the origin, with identity rest rotation and translation. Its parent is the armature node `Tavern_Door_Rig`, also identity. The mesh node `Tavern_Door` has every vertex weighted 1.0 to `hinge`.
Animations: none. Code turns `hinge` about +Y with `SkinnedModel::turn`. A positive angle opens the door toward its back face, which is into the building in both placements above. Turned 100°, the leaf reaches 0.38 m back past the hinge line.
Revision: 1, 2026-10-24, first build

**Skinned loader check** (`engine/src/skinned.cpp`, verified by parsing the exported glb with the loader's rules):
- One node has both a mesh and a skin (`Tavern_Door`).
- There is 1 joint, well under `kMaxJoints` (64).
- Both primitives carry POSITION, NORMAL, JOINTS_0 and WEIGHTS_0.
- Materials are in the order Door_Wood, Door_Iron.
- `animations` is empty. The loader's clip loop then adds nothing, which is fine: `clip()` returns -1 and nothing samples clips.
- `joint("hinge")` finds index 0.
- `base_` is identity because the armature node is identity.

**Detail:** the front has three wide planks with V-grooves between them and a ring handle (plate and hexagonal ring) by the latch edge, 1.5 m up. The two tapered strap hinges pass through the leaf, so they show on both faces. The inside door, seen from the room, shows its back, so it still shows iron.

---

## Notes for the developer
- **`_ShadowOnly` reaches the loader intact.** `loadGltf` sets `GltfPrimitive::node` from `cgltf_node::name`, which is the Blender object name. The three interior nodes are named exactly `Roof_ShadowOnly`, `Wall_NegX_ShadowOnly` and `Wall_NegZ_ShadowOnly`, confirmed in the exported glb. A suffix test (`ends_with("_ShadowOnly")`) is enough. Each node holds two primitives (Plaster, Wood) or one (Roof), and all of them carry the node name.
- **Overhead geometry and `scene.ground`.** `HeightMap` keeps the highest surface over each cell. Over both doorways and the exterior's vestibule there is geometry above head height:
  - The door heads, at y 3.4 to 3.65.
  - On the exterior, the roof eaves, which reach z = +0.8 in front of the door at y ≥ 4.35.
  - The roof over the vestibule.
  
  Fed into the ground as-is, the hero cannot step into either doorway. On the exterior it cannot even reach the front wall under the eaves. The likely fix is to keep surfaces above the hero's head out of `scene.ground` (for example, the exterior's Roof faces, and faces above y ≈ 3.3 over the doorway rectangles and the vestibule). They would still be in shadows and `scene.shelter`. The interior's own roof is shadow-only, so only its door head matters there.
- Static materials `Wood`, `Plaster` and `Roof` load as Stone until `surfaceFor` maps them.
- The interior's candles are modelled in the glb as a Stone dish and Plaster wax, unlike `addCandle`'s Foam box. Use the base points above for the flames.
- **Cycle 2 changes for code:** every interior figure moved. That covers the room bounds, the hinge (11.3, 0, 0), the doorway centre (11.5, 0, -1.1), the spawn (8.7, 0, -1.1), the hearth, the 7 candles (there were 6) and all the seats.
- **New interior pieces for code:**
  - `Brazier`: a `Blocker` and a size-0.5 fire;
  - `Bench_E1` and `Bench_E2`: rectangular blockers;
  - `Stool_6`;
  - `Barrel_5`;
  - `Stage`: in `scene.ground`.
- The inside door's head clearance region is now x 11.3 to 14.1, z -2.4 to 0.2 (the jambs included).
- The exterior's footprint, overhangs and height all changed. Re-run the placement search with the polygon and outlines above.
