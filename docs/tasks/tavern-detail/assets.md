# Assets: tavern detail pass

**All coordinates are in engine coordinates: +Y up, +Z = Blender -Y, 1 unit = 1 metre, relative to the asset's own origin, before the code places it.** Hero scale (1.6× real) throughout. Each build script prints these figures from the constants it builds with.

Rebuild headless. Each build constructs the asset, runs its checks, exports, and renders the previews. It fails if any check fails.

```
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_exterior.py
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_interior.py
```

The scripts are the source of truth. As in the tavern task, there is no `.blend` source.

Unchanged: `tavern_door` (`tools/blender/build_tavern_door.py`, `assets/scenes/tavern_door.glb`) and the ruins (`tools/blender/build_ruins.py`, `assets/scenes/ruins.glb`). Neither file was touched.

**How "unchanged" was verified.** Each new glb was parsed against the revision-2 glb at HEAD (`7749a4f`), node by node. Each node's triangles (positions to 1e-5, per material) and its transform were compared.
- Exterior: `Foundation`, `Walls`, `Timbers`, `Windows`, `Doorway`, `Vestibule`, `Chimney` and `Sign` are **byte-identical**: the same POSITION, NORMAL and index buffers, material and transform. Only `Roof` changed. `Gable_Front`, `Dormer_Front` and `Dormer_Back` are new.
- Interior:
  - 66 of the revision-2 nodes have **identical geometry and transforms**. 26 are fully identical. The other 40 changed only their material, Wood to Oak, on exactly the pieces in the material table below.
  - `Sconce_1` to `Sconce_4` are new.
  - So every figure below that repeats revision 2 is confirmed unchanged, not just reprinted.

---

## tavern_exterior (revision 3)
Source: tools/blender/build_tavern_exterior.py
Export: assets/scenes/tavern_exterior.glb
Preview: assets/scenes/tavern_exterior.png. Four views, ortho, 30° down, 48 px/m:
1. **The game's default camera** as the tavern is placed. The ruins' yaw is 180° and `kTavernYaw` is 12.5°, which is 167.5° in the tavern's own frame. This view shows the roof, cross-gable and front dormer as the player first sees them.
2. Front-right.
3. Front-left.
4. Back-left, which shows the back dormer.
Format: glb, static (no skin)
Units and scale: 1 unit = 1 metre, hero scale
Axes: +Y up. The door faces +Z (Blender -Y).
Origin: the middle of the doorway's threshold at floor level (y = 0), as in revision 2.
Triangles: **2,800** (budget 4,000). Revision 2 was 1,854.
- Foundation 252, Walls 288, Timbers 516, Windows 360, Doorway 36, Vestibule 48, Chimney 118, Sign 158: all as revision 2.
- Roof 206, Gable_Front 342, Dormer_Front 238, Dormer_Back 238.

Materials, in glTF order: Stone, Dirt, Wood, Plaster, Roof. No textures.
- The order changed from revision 2's (Stone, Dirt, Wood, Roof, Plaster) because Plaster is now used before Roof. Static meshes are coloured by material name, so this has no effect in the engine.
- Preview colours: Roof is slate, sRGB (90, 102, 118), the engine's day mid tone `5a6676`. The rest are unchanged.

Nodes and objects:
- `Foundation`, `Walls`, `Timbers`, `Windows`, `Doorway`, `Vestibule`, `Chimney`, `Sign`: unchanged.
- `Roof`: the main slate roof, all `Roof`.
- `Gable_Front`: the cross-gable over the door. Plaster body; Wood framing, bargeboards, pendant and finial; its slates in Roof.
- `Dormer_Front`, `Dormer_Back`: Plaster, Wood, a Dirt window pane, and Roof slates.

Animations: none
Revision: 3, 2026-10-27. A slate roof replaces the thatch, with a cross-gable and two dormers. Nothing else changed.

### The new roof
- **Main roof.** Hipped slate at 45°, over revision 2's eave line (x -4.55 to 5.35, z -7.1 to 0.8), with the eave underside still at y 4.35.
  - A thin eave edge (top y 4.6), then 10 stepped courses up every slope. Each course tilts out from its bed, so its lower edge stands 0.08 m proud of the course below. The slope reads as bands about 0.54 m apart on the slant, about 13 px at 24 px/m.
  - Ridge and hip rolls. The ridge runs x -0.6 to 1.4 at z -3.15.
- **Cross-gable** (`Gable_Front`), facing +Z, over the door.
  - The face is 2.8 m wide (x -1.4 to 1.4) in the front wall's plaster plane (z 0.17). It rises from where it meets the main slates (y ≈ 5.13).
  - Its eaves are at y 5.58, and its slated roof is pitched at 50° to a ridge roll. The underside of its ridge is at y 7.25.
  - The face is timber-framed plaster: corner posts, a tie beam, a king post and two raking braces, all Wood.
  - Carved bargeboards (Wood) run under both rakes. Their lower edges are stepped (tooth and notch), with a tail at each eave and a pendant under the apex. A finial (shaft, diamond bulb and spike) stands on the apex, its top at **y 8.10**.
  - Its roof runs back 2.45 m into the main slope and is buried there.
- **Dormers.**
  - `Dormer_Front` is on the front slope at x 2.6, right of the cross-gable. `Dormer_Back` is on the back slope at x 0.4.
  - Each face is 1.0 m wide, with a dark Dirt pane split by a Wood mullion between Wood posts, sill and lintel. Its tympanum is plaster with a king post.
  - Each has the same slated 50° gable roof, carved bargeboards, pendant and finial. The finial tops are at **y 7.62**.
  - The front face is at z -0.15. The back face is at z -6.15.

### Rule checks (build_tavern_exterior.py `roof_checks`; the build fails if any breaks)
| Rule | Result |
|---|---|
| Everything new (`Roof`, `Gable_Front`, `Dormer_*`) inside revision 2's eaves in plan (x -4.55 to 5.35, z -7.1 to 0.8) | Pass. Roof: x -4.55 to 5.35, z -7.1 to 0.8. Gable_Front: x -1.6 to 1.6, z -2.28 to 0.42. Dormer_Front: x 1.96 to 3.24, z -1.9 to 0.03. Dormer_Back: x -0.24 to 1.04, z -6.33 to -4.4. |
| Whole exterior's plan outline still revision 2's (x -4.55 to 5.59, z -7.1 to 1.77) | Pass: identical |
| Top at most 0.5 m above revision 2's ridge (9.13), so ≤ 9.63 | Pass. **The roof's highest point is now y 8.66** (the ridge roll), 0.47 m *lower* than revision 2. |
| Nothing new over the doorway (x -1.1 to 1.1, z -0.25 to 0.85) or the vestibule (x -1.9 to 1.9, z -3.0 to -0.25) below y 3.3 | Pass. The lowest new face is the eave underside at 4.35; the new gables start at 4.98. |
| Every new Wood, Plaster or Dirt face is over the plinth (x -4.1 to 4.9, z -6.65 to 0.35) | Pass. Only Roof overhangs the walls. See the developer notes. |
| Each gable's and dormer's roof is buried under the main slates at its back end | Pass |

The old checks still run and pass: plinth 9.0 × 7.0 m, vestibule ≥ 2.5 m, sign ≥ 0.3 m from the jamb, roof rise less than the house's depth, chimney top ≥ 1 m above the roof.

**Roof top height: y 8.66** (the `Roof` ridge roll). The new gables' highest point is the cross-gable's finial, at y 8.10. The asset's highest point is now the chimney pot at **y 8.9**, which is unchanged. So the exterior's height runs y -0.6 to 8.9 (revision 2: -0.6 to 9.13).

### Revision-2 placement figures, restated and confirmed unchanged
The parts are byte-identical, and the build prints the same values.
- **Doorway:** clear opening 2.2 × 3.4 m: x -1.1 to 1.1, y 0 to 3.4, through the wall from z +0.27 to -0.25. Wood jambs at x ±1.1 to ±1.3; head at y 3.4 to 3.65. Stone threshold flush with y 0, stepping out to z +0.47.
- **Door hinge:** place `tavern_door.glb` at **(-1.1, 0, -0.25)**, yaw 0.
  - Closed, it runs along +X to x 1.08, with its front facing +Z.
  - A positive turn opens it inward.
  - Doorway blocker: the segment (-1.1, z -0.19) to (1.1, z -0.19).
- **Vestibule:** Wood floor at y 0, **x -1.9 to 1.9, z -0.25 to -3.0** (2.75 m deep). Its Dirt walls are x ±1.9 to ±2.1 and z -3.0 to -3.2, up to y 4.4.
- **Footprint polygon** (xz): (-4.1, 0.35), (-1.3, 0.35), (-1.3, 0.47), (1.3, 0.47), (1.3, 0.35), (4.9, 0.35), (4.9, -3.6), (5.55, -3.6), (5.55, -5.2), (4.9, -5.2), (4.9, -6.65), (-4.1, -6.65). That is the 9.0 × 7.0 m plinth, plus the threshold step and the chimney base.
- **Eaves:** x -4.55 to 5.35, z -7.1 to 0.8, underside y 4.35.
- **Chimney:**
  - Base x 4.5 to 5.55, z -3.6 to -5.2.
  - Flue x 4.65 to 5.47, z -3.88 to -4.92, up to y 8.2.
  - Cap x 4.53 to 5.59, z -3.76 to -5.04, y 8.2 to 8.45.
  - Pot top y 8.9.
- **Sign:** x 1.925 to 2.125, out to z 1.77, between y 2.75 and 4.35. The board is y 2.75 to 3.75, z 0.45 to 1.65. It is 0.625 m from the jamb's outer edge.
- **Plan outline overall:** x -4.55 to 5.59, z -7.1 to 1.77.
- `kTavernSpot` and `kTavernYaw` need no change. Nothing in the placement's inputs (footprint, eaves, chimney, sign) moved.

---

## tavern_interior (revision 3)
Source: tools/blender/build_tavern_interior.py
Export: assets/scenes/tavern_interior.glb
Preview:
- `assets/scenes/tavern_interior.png`: the design view at yaw 45°, 30° down, ortho, framed as the game's 960×540 view at 24 px/m and rendered at 2×. The shadow-only pieces are hidden.
- `assets/scenes/tavern_interior_overview.png`: the design view with the shadow-only pieces shown, and a plan view.
Format: glb, static (no skin)
Units, axes, origin, design yaw, cutaway and shadow-only pieces: as revision 2.
- 1 unit = 1 m, hero scale, +Y up.
- The origin is the floor's centre, with the floor top at y 0.
- The design yaw is 45°.
- The shadow-only nodes are `Roof_ShadowOnly`, `Wall_NegX_ShadowOnly` and `Wall_NegZ_ShadowOnly`.

Triangles: **9,251** (budget 20,000). Revision 2 was 8,843; the four sconces add 102 each.
Materials, in glTF order: Oak, Dirt, Wood, Stone, Plaster, Roof. Roof is only on `Roof_ShadowOnly`.
- Oak's preview colour is sRGB (180, 140, 90).
- Roof's preview colour is now slate. It is never drawn.
- Static meshes are coloured by name, so the order doesn't matter.

Nodes and objects (70): revision 2's 66 plus `Sconce_1`, `Sconce_2`, `Sconce_3` and `Sconce_4`.
Animations: none
Revision: 3, 2026-10-27. The listed furniture moves from Wood to Oak, and four wall sconces are added. Nothing moved.

### Oak vs Wood (checked by `oak_checks` in the build)
| Object(s) | Material(s) |
|---|---|
| `Bar` (body, top, front posts, foot rail and its brackets) | **Oak** (the kick plinth stays Dirt) |
| `Table_A` – `Table_F` (every table, including the trestle `Table_E` and its frame) | **Oak** |
| `Bench_E1`, `Bench_E2` | **Oak** |
| `Chair_A1`–`A4`, `B1`–`B2`, `C1`–`C4`, `D1`–`D4`, `F1`–`F3` | **Oak** |
| `Stool_1` – `Stool_6` | **Oak** |
| `Shelves` (cupboard, uprights, boards) | **Oak** |
| `Barrel_1` – `Barrel_5` | **Oak** staves (the hoops stay Dirt) |
| `Stage` | **Oak** lip (the sides of the platform, from the dark kick up to the top). The planked top stays **Wood**, like the floor, and the kick stays Dirt. |
| `Hearth` | **Oak** mantel beam. The logs stay **Wood**. Stone and the sooty Dirt are unchanged. |
| `Floor` | **Wood** |
| `Timbers` (wall posts, rails, wall plate) | **Wood** |
| `Wall_PosX`, `Wall_PosZ` (window mullions, transoms and sill boards) | **Wood** (and Plaster) |
| `Doorway` (door frame), `Threshold` (the sill) | **Wood** |
| `WallStub_NegX`, `WallStub_NegZ` (skirting) | **Wood** (and Plaster) |
| `Bar_Mugs`, `Shelf_Mugs`, `Table_A_Mugs`, `Table_C_Mugs`, `Table_E_Mugs` | **Wood**. Mugs aren't in the task's list, so they stay unchanged and read as dark spots on the oak. |

No other object has Oak. The build fails if any listed piece keeps Wood, or if Oak appears anywhere else.

### Wall sconces (new)
Each sconce is a Stone iron bracket on a wall post: a back plate, an arm, and a diagonal strut. It holds an open Stone flame dish with a dark Dirt fuel bed. The dish centre is 0.38 m out from the post's face, and the flame base is at **y 2.6**. The engine draws each flame as a sheltered fire of size 0.35.

**Sconce flame base points:**
| Sconce | Wall | Where | Flame base |
|---|---|---|---|
| Sconce_1 | +Z (hearth wall) | the corner post by the -X cutaway, over the stage | **(-11.175, 2.6, 8.0)** |
| Sconce_2 | +Z | the post over the bar's far end | **(7.408, 2.6, 8.0)** |
| Sconce_3 | +X (door wall) | the post between the door and the +X window | **(10.8, 2.6, -4.15)** |
| Sconce_4 | +X | the corner post by the -Z cutaway | **(10.8, 2.6, -8.375)** |

Each sconce is checked by `sconce_checks`:
- it is on a post centre of a drawn wall;
- its flame base is within 0.1 m of 2.6;
- every part is at least 0.3 m from every window opening, from `Shelves` and from `Hearth`.

Each also passes the existing door-swing and spawn checks. Sconce_3's nearest fire is the brazier, about 2.6 m away. The sconces aren't in any furniture group, so the walkway and path figures are unchanged.

### Revision-2 figures, restated and confirmed unchanged
- **Design yaw 45°.** The cutaway walls are -X and -Z (stubs 0.45 m). The drawn walls are +X and +Z.
- **Room inner bounds (camera clamp):** x -11.3 to 11.3, z -8.5 to 8.5. Outer faces at x ±11.7, z ±8.9. The shadow-only roof is y 4.55 to 4.95.
- **View fit:** 29.13 × 18.64 m on screen at the design yaw, inside 40 × 22.5 m (checked).
- **Hearth fire base:** **(-4.2, 0.41, 7.75)**.
  - The firebox opening is x -5.3 to -3.1, y 0.3 to 2.3, running z 7.2 to 8.3.
  - The slab is x -6.3 to -2.1, z 6.6 to 8.5.
- **Brazier fire base:** **(8.6, 1.16, -4.4)**, size 0.5. Its footprint is a circle at (8.6, -4.4) of radius 0.456, with its top at y 1.08.
- **Candle flame bases** (the candles' geometry is unchanged; the engine's size change to 0.3 is in code):
  | Candle | On | Base point |
  |---|---|---|
  | Candle_1 | Table_A | (-7.3, 1.45, 1.9) |
  | Candle_2 | Table_B | (-2.2, 1.45, 1.9) |
  | Candle_3 | Table_C | (2.55, 1.45, 1.9) |
  | Candle_4 | Table_D | (-7.3, 1.45, -4.1) |
  | Candle_5 | Table_E | (-1.6, 1.45, -4.1) |
  | Candle_6 | Table_F | (4.2, 1.45, -4.1) |
  | Candle_7 | Bar | (8.0, 1.95, 5.55) |
- **Inside door:** place `tavern_door.glb` at **(11.3, 0, 0.0)**, yaw +90° about +Y.
  - Closed, it runs along -Z to z -2.18.
  - A positive turn opens it into the room.
  - Doorway blocker: the segment (11.36, z 0.0) to (11.36, z -2.2).
- **Doorway centre:** **(11.5, 0, -1.1)**. The room-side face centre is (11.3, 0, -1.1).
  - The opening is z 0.0 to -2.2, y 0 to 3.4.
  - `DoorStep` covers x 11.7 to 14.1, z -2.6 to 0.4.
- **Hero spawn:** **(8.7, 0, -1.1)**, facing -X. It has 2.45 m clearance, checked ≥ 0.6 m.
- **Walkways (≥ 2.0 m) and paths (≥ 2.5 m), still checked and passing.**
  - The narrowest gaps are as revision 2: bar to shelves 2.10, brazier to +X wall 2.13, Table_D to Table_E 2.18, barrels to hearth 2.20, hearth to stage 2.20, Table_E to Table_F 2.27, Table_A and Table_D to the -X wall 2.33, Table_B to Table_C 2.34, brazier to Table_F 2.39.
  - A 2.5 m disc from the spawn reaches the bar, the hearth and Table_A to Table_F.
- **Seats:** all 25 match revision 2's table exactly: centre, radius, half-size and yaw. (Chair_A4, B2, C4 and D4 print yaw -188.0°/-184.6°, the same angles as the table's 172.0°/175.4°.)
  | Name | Centre (x, z) | Radius | Half-size, yaw |
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
  | Bench_E1 | (-1.6, -5.125) | 1.814 | 1.8 × 0.225, 0° |
  | Bench_E2 | (-1.6, -3.075) | 1.814 | 1.8 × 0.225, 0° |
  | Stool_1 | (7.05, 7.8) | 0.28 | 0.28 × 0.28, -8.6° |
  | Stool_2 | (7.05, 6.9) | 0.28 | 0.28 × 0.28, 0.0° |
  | Stool_3 | (7.05, 6.0) | 0.28 | 0.28 × 0.28, 8.6° |
  | Stool_4 | (7.05, 5.1) | 0.28 | 0.28 × 0.28, -8.6° |
  | Stool_5 | (7.05, 4.2) | 0.28 | 0.28 × 0.28, 0.0° |
  | Stool_6 | (7.05, 3.3) | 0.28 | 0.28 × 0.28, 8.6° |
- **Stage:** outline (-11.3, 8.5), (-8.5, 8.5), (-8.5, 6.9), (-9.5, 5.9), (-11.3, 5.9), top **y 0.72**.
- **Other furniture:**
  - Tables are 1.2 m high, at A (-7.3, 1.9), B (-2.2, 1.9), C (2.55, 1.9), D (-7.3, -4.1). Table_E's top is x -3.5 to 0.3, z -4.65 to -3.55. Table_F is round, r 0.85, at (4.2, -4.1).
  - The bar is x 7.24 to 8.6 (rail included), z 2.5 to 8.5, top y 1.7.
  - The shelves are x 10.7 to 11.3, z 2.6 to 8.35, up to y 3.7.
  - Barrels stand at (0.6, 7.85), (1.65, 7.85), (2.7, 7.85), (1.1, 6.95), (2.2, 6.95).
- **Windows:** +Z wall at x -10.6 to -9.2, 0.95 to 2.35 and 4.0 to 5.4; +X wall at z -7.3 to -5.9. Each is 1.4 × 1.7 m (y 1.5 to 3.2).

---

## Notes for the developer
- **`Oak` is a new material name** on 40 interior nodes. Until `surfaceFor` maps `Oak` to `kSurfOak`, those nodes load as Stone.
- **Exterior `scene.ground`.**
  - The new gables and dormers carry Wood, Plaster and Dirt high on the roof. Unlike Roof, those go into `scene.ground`.
  - The build checks that all of it stands over the plinth. So the ground's highest surface over those cells only rises where the walls already make them solid, and nothing new reaches past the walls or the doorway. Only Roof material overhangs, and the ground already leaves Roof out.
  - The cross-gable's Wood and Plaster over the doorway rectangle start at y 4.98, above the 3.3 m headroom cut-off. So `kTavernDoorway` and `kTavernVestibule` still clear the way in.
- **Shelter.** The new gable and dormer slates are `Roof`, so they join the thatch's role in `scene.shelter`. The roof's plan coverage is unchanged.
- **The exterior is now lower at the top:** 8.66 for the ridge (was 9.13), and 8.9 overall (the chimney pot). Nothing in the code reads the old height as far as these assets go.
- **Sconces:** add a sheltered fire of size 0.35 at each flame base above, transformed with the room as the candles are. The sconces need no blockers; they hang at 2.1 m and up on the walls.
