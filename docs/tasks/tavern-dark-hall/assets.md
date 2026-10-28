# Assets: dark hall behind the tavern door

**All coordinates are in engine coordinates: +Y up, +Z = Blender -Y, 1 unit = 1 metre, relative to the asset's own origin, before the code places it.** Hero scale (1.6× real), as before. The build script prints these figures from the constants it builds with.

Rebuild headless. The build constructs the asset, runs its checks, exports, and renders the previews. It fails if any check fails.

```
/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_tavern_exterior.py
```

As in the earlier tavern tasks, the script is the source of truth and there is no `.blend` source. The build ran headless in a separate Blender process. The user's live Blender session was not touched.

Before the change, the HEAD script (`8edf83b`) was rebuilt into a scratch directory. Its glb was `cmp`-identical to the committed `tavern_exterior.glb`, so the new glb's differences come only from this revision.

**How "unchanged" was verified.** The new glb was parsed against HEAD's (`git show 8edf83b:assets/scenes/tavern_exterior.glb`) node by node. For every primitive, the raw POSITION, NORMAL and index accessor bytes, the index type, any other attributes and the material name were compared byte for byte. The node's translation, rotation, scale and matrix were compared too.
- `Foundation`, `Walls`, `Timbers`, `Windows`, `Doorway`, `Roof`, `Chimney`, `Sign`, `Gable_Front`, `Dormer_Front` and `Dormer_Back` are **byte-identical**: the same POSITION, NORMAL and index bytes, the same material on each primitive, and the same transform.
- Only `Vestibule` changed. Its transform is unchanged (identity).
- The node set (12 nodes) and the glTF material order (Plaster, Stone, Dirt, Wood, Roof) are unchanged. There are still no animations, skins or extra attributes.
- The glb was also re-imported into an empty Blender scene. All 12 objects load with the triangle counts and materials below. `Vestibule` spans x -2.1 to 2.1, y -0.1 to 4.4, z -6.05 to -0.25.

---

## tavern_exterior (revision 6)
Source: tools/blender/build_tavern_exterior.py
Export: assets/scenes/tavern_exterior.glb
Preview:
- `assets/scenes/tavern_exterior_hall.png` (new):
  - **Top row:** the doorway from the game's default camera (yaw 167.5° in the tavern's own frame, 30° down, ortho). Left is a 24 px/m toon render with the day palette and outline, scaled 3×. Right is a smooth render of the same 6 × 4.5 m crop. There is no door and no darkening, so the floor shows plainly: it fills the opening to the head, and a sliver of the right side wall shows at the top right. There is no grey square.
  - **Bottom:** the same yaw from 60° down with `Roof`, `Gable_Front` and the dormers hidden. It shows the whole hall running from the doorway to the rear wall.
- `assets/scenes/tavern_exterior.png` (the four views, as before). Only the doorway in the default-camera panel changed (pixels x 344–443, y 413–485): the floor now shows where the back wall did.
- `assets/scenes/tavern_exterior_masonry.png`. Only the top 35 rows of the plinth crop changed (the same doorway, both panels).
- `assets/scenes/tavern_exterior_roof.png` was regenerated and came out byte-identical, so it is not in the commit.

Format: glb, static (no skin)
Units and scale: 1 unit = 1 metre, hero scale
Axes: +Y up. The door faces +Z (Blender -Y). The frame is the same as revision 5.
Origin: unchanged, at the middle of the doorway's threshold at floor level (y = 0).
Triangles: **9,105** (budget 14,000). Revision 5 was 9,117.
- `Vestibule` has **36** (under the 100 budget). Revision 5 had 48; the back wall's 12 are gone.
- The rest are unchanged: Foundation 1,064, Walls 1,526, Timbers 516, Windows 360, Doorway 36, Roof 2,870, Gable_Front 679, Dormer_Front 403, Dormer_Back 403, Chimney 1,054, Sign 158.

Materials, in glTF order: Plaster, Stone, Dirt, Wood, Roof. These are unchanged and there are no textures. `Vestibule` uses Wood (the floor) and Dirt (the side walls), as before.
Nodes and objects: the same 12 nodes. `Vestibule` is the hall's Wood floor and two Dirt side walls. Its name is kept so the loader and the code need no change.
Animations: none
Revision: 6, 2026-10-28. The vestibule becomes a hall. Its Dirt back wall is deleted, and its Wood floor and both Dirt side walls run back to the rear wall's inside face. Nothing else changed.

### The hall (the `Vestibule` node)
- **Back depth: local z = -6.05** (exact; float32 in the glb is -6.0500002). This is the inside face of the exterior's rear wall.
  - It was measured from `Walls`: the nearest rear-wall vertex behind the hall (x -2.1 to 2.1) is at z -6.05, and the wall's outer face is at z -6.47.
  - In the script it is `PY1 - WALL_T` = 6.47 - 0.42.
  - The `hall_checks` step in the build fails if the hall's back and the measured face ever differ.
- **The hall is 5.8 m deep** behind the front wall's inside face, from z -0.25 to -6.05. Revision 5's vestibule was 2.75 m deep, z -0.25 to -3.0.
- **Floor:** Wood, top at y 0 (slab y -0.1 to 0), x -1.9 to 1.9, z -0.25 to **-6.05**. It is still a plain slab, the same style as revision 5's floor (the task's "keep the existing floor's style"). It is not grooved: the region darkens it to 35% and less.
- **Side walls:** Dirt, x -2.1 to -1.9 and 1.9 to 2.1, y 0 to **4.4**, z -0.25 to **-6.05**. Their inner faces are at x ±1.9, and their back ends butt against the rear wall's inside face.
- **Back wall:** none. Nothing of `Vestibule` stands over the floor between x -1.9 and 1.9 (checked). The hall is closed at the back by `Walls`' inside face, which is Plaster, from y 0.45 up. Below that is `Foundation`'s Plaster mortar bed face, also at z -6.05, up to y 0.42.
- **Headroom:** the build's headroom check now covers the whole hall (x -1.9 to 1.9, z -0.25 to -6.05). Nothing of `Foundation`, `Walls`, `Chimney`, the gables or the dormers is in it below y 3.3, and nothing of `Roof`, `Gable_Front` or the dormers comes down over it below y 3.3. Both pass. Nothing new stands over the doorway or the hall below y 3.3 except the floor and the side walls.
- **The other figures are unchanged:** the doorway, the hinge at (-1.1, 0, -0.25), the footprint, eaves, chimney, sign and roof heights. The build prints them identically.

### Build checks added (`hall_checks`)
| Rule | Result |
|---|---|
| The hall's back (`VESTIBULE[2]`) equals the rear wall's inside face measured from `Walls` | Pass: z -6.05 |
| The Wood floor and both Dirt side walls reach that face | Pass |
| No `Vestibule` vertex over the floor inside x -1.9..1.9 (no back wall) | Pass |
| `Vestibule` under 100 triangles | Pass: 36 |

All revision 5 checks (masonry, render, roof, tiles, plinth, sign, chimney) still pass with the same figures.

## Notes for the developer
- **Dark region** (in the exterior's local frame, before `tavernPoint`/`kTavernYaw`):
  - x -1.9 to 1.9, y 0 to 4.4.
  - z from **-0.25** (the open face, the front wall's inside face) to **-6.05** (the back face, the rear wall's inside face).
  - So the centre is (0, 2.2, -3.15), the half extents are (1.9, 2.2, 2.9), and the open face is +Z.
- **`kTavernVestibule`:** extend it to x -1.9 to 1.9, z -6.05 to -0.25. No piece other than the floor and the hall's side walls is below y 3.3 over that rectangle (checked).
- **Old back-wall line:** the deleted wall stood at x -1.9 to 1.9, z -3.0 to -3.2. Your invisible blocker goes there. The asset has nothing there now.
- **Only `Vestibule`'s geometry changed.** It has the same node name and materials, so loading needs no change. `scene.ground` gets a longer Wood floor at y 0 and two longer Dirt walls up to 4.4 at x ±1.9 to ±2.1. Both lie inside the house's walls. The side walls are taller than headroom, so they stay in the ground map as walls along the hall, as before.
- **What the doorway shows:** at the 30° pitch, the line from the door head (y 3.4) meets the rear wall at about y 0.05. So through the opening you see floor almost to the rear wall, with at most a pixel of its foot. The back windows (y 1.5 and up) and the side walls' tops can't be seen through the doorway.
