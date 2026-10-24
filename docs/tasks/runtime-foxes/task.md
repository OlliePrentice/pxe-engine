# Add foxes at runtime
Type: feature
Status: complete
Cycle: 1

## Goal
Foxes can only be created at startup (`main.cpp`, `foxes.resize(beds.size())` before the scene is built). Let the player add foxes while the game runs. A fox tool, used like the spell brushes, places a fox at the nearest valid bed to a click. The new fox clears the grass under it and keeps out of every other fox's way, like the startup foxes. It is scriptable for captures, so the behaviour can be checked without playing. Builds on `many-foxes` (shared model, a seed for each fox, `findBeds`, route reservation, hero pushout).

## Decisions
- **The tool.** Key 4 toggles a fox tool held in `BrushSettings::kind` (a new kind), exclusive with the 1/2/3 spell brushes, and shown in the Tab menu alongside them. With the tool active, a left-button press adds one fox; holding does not add more. A press over the menu (`menu.wantsMouse()`) is ignored, as for brushes. Keyboard and mouse only, like every other debug and world-editing tool: the gamepad only plays (gamepad.hpp).
- **Cursor preview.** While the tool is active, a ring of `Fox::kFootprint` (0.68 m) radius is drawn on the ground at the cursor, through the same pick and outline path as the brush ring. It does not show whether the spot is valid. The bed check runs only on a click.
- **Finding the bed.** On a click, candidate spots on a 0.25 m grid (as `findBeds`) are tried outward from the picked point, up to 2 m away, and the nearest that passes is taken.
- **Bed rules.** The same as startup beds (`findBeds`):
  - open lawn under the whole curl;
  - no part of the fox more than 3 cm below the ground;
  - the whole fox, through every frame of all five clips at `kSize` and tilted to the slope, at least `kBedClearance` (0.2 m) from everything standing in the scene (every campfire's dirt patch, stones and logs among it) and from the fox in every other bed.

  It must also clear every route another fox has committed to at that moment. Checked with the same body boxes `routeFree` uses, both the curl's `kFootprint` circle and the standing box at the bed must miss every box of every route still `walking_`. The hero is ignored, because the pushout moves it off the fox.
- **Facing.** The curl faces the nearest campfire within `kFaceFire` (6 m). Otherwise it is turned so the standing fox faces the current camera (as `kRuinsFoxSpot3`'s yaw does for the default view).
- **No room.** If no spot within 2 m passes, no fox is added. The game logs a line with the picked point, and shows "No room for a fox here" near the cursor for about 2 s, drawn through the ImGui layer the settings menu already draws every frame.
- **The new fox** goes through the same `build`/`spawn`/`setFires` path as a startup fox. It appears curled asleep with a fresh rest (20–40 s) before its first wake. It takes the next index (`foxes.size()`), so its seeds are `kSeed`/`kWalkSeed + index`, and the same sequence of adds always gives the same foxes. It is pushed to the end of `foxes` between frames, never during an update or pose.
- **Logging.** Every add logs one line: the fox's index, its bed (xz), its yaw, and how far the bed is from the picked point; or "no room" with the picked point.
- **Grass.** Grass blades and tufts (kinds 0 and 1, not leaf clusters) rooted within `kFootprint` of the new bed are erased from `scene.grass`, and only the grass instance buffer is re-uploaded (a renderer call for just that; not a full `Renderer::upload`). The fire grid's fuel is not updated, so a grass fire may burn under a fox. That is accepted.
- **When the model loads.** In ruins play the fox model always loads, even with `--no-fox` or when no foxes are shown at startup, so foxes can be added. It doesn't load in the hills or the bench. In captures it loads only when foxes are shown or `--add-fox` is given. The layout (`laidOut`) needed for the bed check is kept for the whole run whenever the model is loaded.
- **Bed check cost.** The fox's swept volume (every vertex of every clip frame, as `findBeds` builds it) is computed once when the model loads and reused for every click and `--add-fox`, and by `findBeds`. A worst-case click (no spot within 2 m passes, so every candidate is tried) must take ≤ 50 ms in a Release build, measured and recorded in the handoff. If it can't, stop and report rather than adding threads.
- **`--add-fox X,Z,SECS`** (repeatable) adds a fox as if clicked at world point X,Z, through the same search, rules, facing, grass, seeds and logging path:
  - In play, at SECS seconds after the start. It faces as a click would, using the camera at that moment.
  - In captures without `--fox-wake`, every `--add-fox` is added before the shot, in SECS order. They are then held asleep at `--time` like the other foxes.
  - In captures with `--fox-wake`, each is added at its SECS on the timeline `runFoxes` runs the foxes along (stopping there), so a bed can be refused for crossing a route committed at that moment.
- **Fox #0 and startup.** With no adds, startup behaviour is exactly as now. After an add, other foxes' walks (fox #0's included) may be blocked by the new bed. That is accepted.
- **Captures.** Every capture without `--add-fox` is byte-identical to before this task.

## Acceptance criteria
- [x] Key 4 toggles a fox tool in `BrushSettings::kind`, exclusive with the spell brushes and shown in the Tab menu; a left-button press (not hold, not over the menu) adds one fox — `main.cpp` `pollInput` key 4 → `BrushSettings::kFox`; `settings.cpp` "Fox (4)" radio; play loop adds only on `leftDown && !leftHeld && !menu.wantsMouse()`
- [x] While the tool is active, a `kFootprint` ring is drawn at the cursor on the picked ground; no bed check runs per frame — `main.cpp` fox-tool branch sets `cursorRadius = Fox::kFootprint` through the brush outline path; `addFox` runs only for queued asks
- [x] A click searches a 0.25 m grid outward from the picked point up to 2 m and takes the nearest passing spot — `fox.cpp` `Fox::findBed`: `kBedStep` grid within `kAddReach`, `stable_sort` by distance, first passing spot
- [x] The bed check applies every `findBeds` rule and also rejects a bed whose curl circle or standing box overlaps any box of a route another fox is walking at that moment — `fox.cpp` `findBed` uses the shared `Beds` rules (`onLawn`, `clearOfFoxes`, `clearOfScene`, `onGround`) plus `bedFree` against every walking `route_.body`
- [x] An added fox faces the nearest campfire within 6 m, otherwise the standing fox faces the current camera — `fox.cpp` `findBed`: `Beds::faceFire`, else `atan2(toCamera) - standTurn_`; `main.cpp` passes the camera's forward
- [x] With no valid spot, no fox is added, a log line names the picked point, and "No room for a fox here" shows near the cursor for about 2 s through ImGui — `main.cpp` `addFox` logs `no room ... within 2 m of X, Z`; `noRoomFor` 2 s drawn by `SettingsMenu::draw` note
- [x] An added fox takes index `foxes.size()`, is built, spawned and given fires through the same path as startup foxes, starts asleep with a fresh rest, and is appended between frames — `main.cpp` `addFox`: `build(model, foxes.size())`, `spawn`, `setFires`; queued asks join at the start of the next frame
- [x] Every add logs index, bed, yaw and distance from the picked point — `main.cpp` `addFox` log line (index, bed, yaw, distance, picked point)
- [x] Blades and tufts (not leaf clusters) under the new curl are erased and only the grass instance buffer is re-uploaded — `main.cpp` `addFox` erases `kind < 1.5` within `kFootprint`, then `Renderer::uploadGrass`
- [x] The fox model (and the layout) loads in ruins play even with `--no-fox`; not in the hills or the bench; in captures only with foxes shown or `--add-fox` — `main.cpp` `foxModelWanted`: ruins play always; not hills/self-test/bench; captures only with foxes shown or `--add-fox`
- [x] The fox's swept volume is built once at model load and shared by the click check, `--add-fox` and `findBeds`; a worst-case click measures ≤ 50 ms in Release (recorded in the handoff) — `fox.hpp` `Fox::Sweep`/`sweep()` built once, shared by `findBeds` and `findBed`; worst click 28.2–29.4 ms Release (handoff cycle 1)
- [x] `--add-fox X,Z,SECS` is repeatable and goes through the same path: at SECS in play; all before the shot in captures without `--fox-wake`; at SECS on the `runFoxes` timeline with `--fox-wake` — `main.cpp` `--add-fox` parsing, `addedFoxes` ordering; play queue by `played`; captures before the shot or at SECS within the `runFoxes` timeline
- [x] Captures without `--add-fox` are byte-identical to before this task (comparison recorded in the handoff) — Handoff cycle 1: 24 captures without `--add-fox` byte-identical to 2a64757 in Release and Debug
- [x] `--add-fox` and key 4 are in `--help` and the README (usage line, controls and fox paragraph), and the window-title help text lists 4 — `printUsage`, README controls/usage/fox paragraph, window title "4 add a fox"
- [x] Project builds cleanly from a fresh build directory with CMake — Orchestrator fresh Debug build, cycle 1: exit 0, only the existing stb/glfw warnings

## Assets
None.

## Out of scope
- Removing foxes
- Gamepad support for the fox tool
- Foxes added by gameplay (over time, at dusk, etc.)
- Background threads for the bed check
- Showing on the preview ring whether a spot is valid
- Updating the fire grid's fuel under a new fox

## Cycle log
- Cycle 1: 2026-10-23 started
- Cycle 1: complete. Fresh build passes; review found no issues.
