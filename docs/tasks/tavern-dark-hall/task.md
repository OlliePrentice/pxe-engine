# Dark hall behind the tavern door
Type: feature
Status: open
Cycle: 1

## Goal
Outside the Sleeping Fox, when the door opens, the doorway shows a flat grey square: the vestibule's Dirt back wall (tavern_exterior.glb, 2.75 m behind the wall's inside face) and a short strip of floor. Through the open door the player should see the inside floor instead, heavily dimmed: a dark hall whose plank floor runs back into the building and fades to near-black. That needs a revised exterior asset (the hall) and a new general engine capability (dark regions) that the game places over the hall.

## Decisions
- **Hall, not a room slice.** The vestibule's Dirt back wall is removed. Its side walls and Wood floor run back to the inside face of the exterior's rear wall, which makes a 3.8 m wide hall (x -1.9 to 1.9). The orthographic camera's 30° pitch needs about 5.9 m of floor to fill the 3.4 m opening, and the hall gives about 6 m. No furniture or room detail.
- **Fade with depth.** At the threshold the floor shows at 35% of its normal colour. It falls linearly to 5% by 4 m in and holds at 5% to the back. Both strengths and the fade distance are game tuning constants, set on the region.
- **Every hour looks the same.** The hall darkens the same way by day and by night and follows the day/night palette. The room's fires add no warm glow.
- **Everything inside the region darkens.** That covers the floor and walls, actors (the hero stepping into the doorway, the open door leaf against the hall's side wall) and the edge pass's light rims. Dark ink lines are unchanged.
- **Walking is unchanged.** The hero can go no deeper than now. An invisible blocker takes the old back wall's place (local z -3.0 to -3.2, x -1.9 to 1.9). The doorway passage zone is unchanged. The headroom cut-out (`kTavernVestibule`) is extended over the whole hall, so the roof over the hall stays out of the ground map.
- **Approved change to an existing asset:** tavern_exterior.glb's `Vestibule` node changes as described under Assets. Every other node stays byte-identical.

## Layering
- Engine: **dark regions** on `pxe::Scene`, a list of oriented boxes, each with one open face. Each region holds:
  - a centre, half extents and a yaw about +Y;
  - which face is open;
  - a start strength, an end strength and a fade distance.

  Any surface, actor or edge-pass light rim inside a box is scaled toward black by depth from the open face. The scale is the start strength at the open face, moves linearly to the end strength at the fade distance, and holds there beyond it. It replaces sun, moon, fill, sky light and firelight for those pixels. The engine names nothing game-specific. The list is empty by default, and an empty list changes no pixel. The region data reaches the shaders with the rest of the scene upload or each frame, whichever the developer finds simpler. Either way the engine keeps no pointer to game data.
- Game: `buildRuins` (game/src/scene.cpp) adds one dark region over the hall in the exterior's local frame, placed with `tavernPoint`/`kTavernYaw`. It also extends `kTavernVestibule` to the hall's new depth. The blocker for the old back wall's line goes into the outdoor scene's blockers, next to the door's own blocker.

## Acceptance criteria
- [ ] `pxe::Scene` has a dark-region list as described in Layering, defaulting to empty, and is documented in its header with generic wording.
- [ ] The scene, actor and edge shaders apply the darkening inside a region: start strength at the open face, a linear fade to the end strength at the fade distance, then held. Sun, moon, fill, sky light and fire light don't brighten pixels inside a region. Light rims darken. Ink lines are unchanged.
- [ ] With no dark regions the output is byte-identical to the cycle base. Captures for `--scene hills` and `--scene tavern` (both have no region) and `--self-test` match the base with `cmp`. The default ruins captures and `--hour 21 --foxes 3` should match too, because the door is shut and the hall is hidden. The handoff explains any difference.
- [ ] The ruins scene places one region over the hall: open face on the wall's inside face (local z -0.25), back face at the rear wall's inside face, sides at x ±1.9, from the floor to the walls' top. Start 0.35, end 0.05, fade 4 m, each as a named constant in game/src/scene.cpp.
- [ ] `kTavernVestibule` covers the hall to its new depth.
- [ ] A blocker on the old back wall's line (local x -1.9 to 1.9, z about -3.1) stops the hero going deeper than before. `Doorway::zoneLo`/`zoneHi` are unchanged.
- [ ] The loaded tavern_exterior.glb matches assets.md: no Dirt back wall, and the hall floor and side walls reach the rear wall's inside face.
- [ ] `--capture` of the open door from outside (`--target -11.4,2.5,-25.4 --yaw 180 --tavern-door-open`, at `--hour 14` and `--hour 21`) shows plank floor through the doorway: dim at the threshold, near-black further in, with no grey square.
- [ ] The engine has no game names (`grep -rniE "tavern|hall|vestibule|fox|hero|ruins" engine/` finds nothing new).
- [ ] `docs/engine-guide.md` documents dark regions.
- [ ] Project builds cleanly from a fresh build directory with CMake (`check` preset; only stb's `sprintf` warning).

## Assets
- **tavern_exterior.glb (revision)**: a change to the `Vestibule` node only.
  - Delete the Dirt back wall (x -1.9 to 1.9, z -3.0 to -3.2).
  - Extend both Dirt side walls (x ±1.9 to ±2.1, up to y 4.4) back from z -3.0 to the inside face of the exterior's rear wall. Measure that face from `Walls`.
  - Extend the Wood floor (y 0, x -1.9 to 1.9) back to the same face.
  - Keep the plank material and style of the existing floor (docs/art-style.md).
  - `Foundation`, `Walls`, `Timbers`, `Windows`, `Doorway`, `Roof`, `Chimney`, `Sign`, `Gable_Front`, `Dormer_Front` and `Dormer_Back` stay byte-identical (POSITION, NORMAL, index buffers, material, transform). Verify this the way tavern-detail's assets.md did.
  - Nothing new below y 3.3 over the doorway or the hall except the floor and side walls.
  - Format: glb, as now.
  - Units and axes: 1 unit = 1 metre, +Y up, the same frame as the current file.
  - Origin: unchanged (the middle of the doorway's threshold, floor level).
  - Budget: the Vestibule node stays under 100 triangles.
  - Animations: none.
  - Record the hall's exact new back depth in assets.md. The game uses it for the region and the headroom.

## Out of scope
- A warm glow from the room's fires through the doorway at night (a possible later task).
- Any visible room detail in the hall (furniture, back wall art).
- Changes to the interior scene (tavern_interior.glb), the doorway passages, or the door's animation.
- Dark regions in other scenes.

## Cycle log
- Cycle 1: 2026-10-28 started, base 31fae82
