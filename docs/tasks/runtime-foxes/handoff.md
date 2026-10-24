# Handoff: runtime-foxes

## Cycle 1
Build: pass

Fresh Debug build in a new, empty directory: `cmake -S . -B <new dir> -DCMAKE_BUILD_TYPE=Debug && cmake --build <new dir> --parallel`. Both steps exit 0. The only warnings are the two that were already there: stb's `sprintf` deprecation and the linker's duplicate-libglfw note. The repo's `build/` (Release) also builds with no new warnings.

### Changes
- game/src/fox.hpp, game/src/fox.cpp:
  - New `Fox::Sweep` and `static Fox::sweep(model)`. This is findBeds' swept-volume build, moved out unchanged: every vertex of every 1/60 s frame of all five clips, sorted into 1 cm voxels, with the 12-voxel blocks. It is built once when the model loads and passed to every search.
  - `findBeds(const Sweep&, layout, count)` now takes the sweep instead of the model.
  - New private `Fox::Beds` (defined in fox.cpp) holds findBeds' rules, moved out of its lambdas: the solids' metre grid, `onLawn`, `faceFire`, `place`, `clearOfScene`, `onGround`, `clearOfFoxes` and `take`. findBeds and the new findBed both use it, so the two searches apply the same rules.
  - `clearOfFoxes` gains an exact block-level skip (see "Bed check cost").
  - New `findBed(sweep, layout, at, toCamera, foxes, bed) const`, the click search. It runs on a built, not yet spawned fox.
  - New `bedFree(xz, yaw, foxes)`, the committed-route rule.
  - routeFree's box lambdas became the members `bodyBox`/`overlaps`/`touches`. Their grown half sizes (`boxHalf_`, `boxCorner_`) are now computed once in `build()` instead of on every routeFree call. The arithmetic is the same, and so are the captures.
  - `kAddReach` = 2 m. New `bedPos()`/`bedYaw()` accessors.
- game/src/main.cpp:
  - `--add-fox X,Z,SECS` (`Options::addFoxes`) with parsing and `--help`.
  - Key 4 → `BrushSettings::kFox`.
  - `foxModelWanted()` decides when the model loads. `addedFoxes()` orders the adds by SECS.
  - `addFox()` is the one path for clicks and `--add-fox`.
  - The camera is now set up before the foxes spawn, so capture adds can face it.
  - The `--fox-wake` run stops at each add's SECS.
  - In the play loop: the click edge, the ring, the queue of asks that join at the start of the next frame, the scripted adds, and the "No room" note timer.
  - The window title lists "4 add a fox". Its buffer grew from 256 to 320 so the line isn't cut off.
- game/src/settings.hpp, game/src/settings.cpp:
  - `BrushSettings::kFox` = 3.
  - A "Fox (4)" radio button among the spell brushes, with a line of help.
  - A cream ring colour for the fox tool.
  - `SettingsMenu::draw` takes `note`/`notePoint` and draws the note on a dark plate through ImGui's foreground draw list.
- engine/include/pxe/renderer.hpp, engine/src/renderer.cpp: new `Renderer::uploadGrass(scene)` re-uploads only the grass instance buffer. `upload()` now calls it for its grass step, with the same calls in the same order.
- README.md:
  - Controls table: rows for 1/2/3 (missing before) and 4.
  - `--add-fox` in the usage block.
  - The fox paragraph now covers the tool, the bed rules, the route rule, facing, "no room", the grass, logging, model loading and `--add-fox`.

### What changed, per acceptance criterion
1. **Key 4, the tool.**
   - `pollInput`: key 4 sets `in.brush = BrushSettings::kFox`. The existing toggle line makes it exclusive with 1/2/3, and pressing 4 again turns it off.
   - The Tab menu shows "Fox (4)" as a fifth radio button on the same `brush.kind`.
   - A fox is asked for only on the press edge (`leftDown && !leftHeld`) and only if `!menu.wantsMouse()` at that moment. `leftHeld` is tracked every frame whatever the tool, so holding the button, or dragging off the menu while holding, never adds a fox.
   - No gamepad mapping.
2. **Ring.**
   - With the fox tool, `cursorRadius = Fox::kFootprint` and the ring is drawn through the same `pick` and 40-point outline code as the brush ring, in a cream colour.
   - The only search is in `addFox`, which runs only for a queued ask. Nothing about beds runs per frame.
3. **Search.**
   - `findBed` collects every point of the world 0.25 m grid (`kBedStep`, the grid findBeds uses) within `kAddReach` (2 m) of the picked point. That is about 200 spots.
   - It sorts them by distance with `stable_sort` from row order (z, then x), so ties are fixed, and takes the first that passes.
4. **Bed rules.**
   - The same `Beds` code as findBeds: lawn under the 8-point curl circle, `kSink` 3 cm, and `clearOfScene` at `kBedClearance` through the whole sweep at kSize, tilted.
   - Other foxes: each fox already lying within reach of the click is `take`n, and `clearOfFoxes` keeps the new fox's swept body 0.2 m from theirs, conservatively, exactly as in findBeds.
   - Routes: `bedFree` builds `bodyBox` at the bed (the standing box). It rejects the bed if any box of any other fox's `route_.body` while `walking_` overlaps that box, or touches the `kFootprint` circle at the bed. These are routeFree's boxes, grown the same way. The hero is ignored.
   - The order of checks in findBed is lawn, route, foxes, scene, ground. All must pass, so the order changes only the cost, not the result.
5. **Facing.** `Beds::faceFire` (findBeds' code) turns the curl to the nearest campfire within `kFaceFire`. Otherwise the yaw is `atan2(toCamera) - standTurn_`, so the standing heading (bedYaw + standTurn_) points back along the camera's forward. In the default view this gives −90.0°, the same as `kRuinsFoxYaw3`. A capture at `--fox-wake 25/30` shows the added fox standing with its chest to the camera.
6. **No room.**
   - Nothing is added, and the log reads `pxe: no room for a fox within 2 m of X, Z`.
   - In play, "No room for a fox here" is drawn by the clicked point for `kNoRoomShown` = 2 s, through `SettingsMenu::draw`, which the game already calls every frame. A `--ui-shot` shows it.
7. **New fox.**
   - `addFox` calls `build(model, foxes.size())`, then `findBed`, `spawn` and `setFires(layout.campfires, kCampfireReach)`. That is the same path as startup.
   - `spawn` seeds `kSeed + index` / `kWalkSeed + index` and enters `kRest` with a fresh rest.
   - In play the asks are queued during the frame and run at the top of the next one, before `clock.advance` and the fox update, so nothing is appended during an update or a pose.
   - In captures, adds run before any frame is drawn, between `runFoxes` steps.
8. **Logging.** `pxe: fox I added: bed X, Z, yaw Y deg, D m from PX, PZ (N grass sprites dropped)`.
9. **Grass.**
   - `erase_if` removes kinds < 1.5 (blades 0, tufts 1) within `kFootprint` of the bed. Leaf clusters stay.
   - Then `renderer.uploadGrass(scene)` runs, only if something was dropped. It does not call the full `upload`.
   - The fire grid is not rebuilt. Note that the grid builds lazily the first time a grass fire runs, so a fox added before the first fire of the run does take its fuel with it.
10. **Model loading.** `foxModelWanted`:
    - yes whenever foxes are shown;
    - otherwise no in the hills, the self-test and the bench;
    - otherwise yes in play, and in captures only with `--add-fox`.

    Under `--no-fox` in ruins play the model loads and no fox is shown. Checked with a ui-shot: 0 foxes at the start, and `--add-fox` adds one. The layout (`laidOut`) is kept for the whole run whenever the model loads (it already was). A failed load resets `foxModel`, so later asks are logged as impossible.
11. **Sweep once, and cost.**
    - `Fox::sweep` runs once at model load, whenever a search can happen (play, `--foxes`, `--add-fox`). findBeds, findBed and `--add-fox` all receive that one sweep.
    - Worst-case click in Release: **28.2-29.4 ms** (target ≤ 50 ms). See below.
12. **`--add-fox`.**
    - Repeatable.
    - Play: added once `played` (the sum of the frame dt's) reaches SECS, through the same queue as a click, facing the camera as last drawn.
    - Captures without `--fox-wake`: all added in SECS order before anything else, then held asleep at `--time` with the rest (`holdRest` runs after).
    - Captures with `--fox-wake`: `runFoxes` runs to each SECS, the fox is added there, and the run carries on to `--fox-wake`.
13. **Captures unchanged.** 24 of 24 Release captures and their logs are byte-identical to before. Details below.
14. **Docs.** `--help` has `--add-fox`. The README has it in the usage block, a control row for 4 and the fox paragraph. The window title lists 4.
15. **Build.** The fresh Debug build passes (above).

### Bed check cost (Release, M3 Pro)
- **Method.** Captures with timers around `findBed` (temporary instrumentation, since removed).
  - A capture with `--add-fox` on a 1 m grid and then a 0.5 m grid over x, z ∈ [−24, 18]: 1,849 and 7,225 searches. Foxes are added as they fit, so later searches meet up to 199 foxes.
  - Then the slowest refused click re-run alone, three times.
- **Worst refused click** (no spot within 2 m passes, so all 197 candidates are tried): (14.5, −10) took **28.20 / 28.21 / 29.38 ms**. Next slowest: (2, −9) at 22.96 / 23.65 ms. The slowest successful add: 27.9 ms.
- **What it costs.** Every candidate takes about 0.2-0.3 ms of `clearOfScene` near stonework. The metre grid over the solids takes about 1 ms per click, and `take` about 0.55 ms per neighbouring fox.
- **One optimisation was needed, and it changes no result.**
  - On the first measurement the worst click took 94 ms, almost all in `clearOfFoxes`. That re-checked all 99,725 voxel middles of the candidate against each neighbouring fox's distance field.
  - It now bounds each of the sweep's 1,529 blocks on the plan by its 8 transformed corners, widened 1 mm for rounding.
  - A block is skipped only when the field at the middle of the cells it can reach, less the farthest of those cells' distance from it, is still ≥ `foxNear` + 1 mm. The field is 1-Lipschitz, so then every point in it passes.
  - Every other block's voxels are checked exactly as before.
  - Verified: all 1,849 grid search results (bed, yaw, or no room) and the 193-fox capture are identical before and after the change. `--foxes 8/64/200` captures are identical to baseline (115 of 200 still fit).
- **Startup.** Loading the model and laying out the scene take about 27 ms, and the sweep 70 ms, in Release. Ruins play now always pays both, also with `--no-fox`. Before, `--no-fox` play paid neither, and normal play paid only the first. In Debug they take 0.19 s and 1.0 s.
- **Debug** (the same instrumentation in the fresh Debug build, two runs): the worst refused click (14.5, −10) takes 1,115 / 1,119 ms, and (2, −9) takes 855 / 854 ms. There is no target for Debug.

### Capture comparisons
- **Baseline:** HEAD (2a64757) built in a git worktree, in Release and Debug, with the same commands.
- **The set (24):**
  - plain, `--hour 22`, hills, `--shelter-test --candles`, `--grass-fire -4,-2 --settle 3`, `--no-fox`;
  - `--fox` (plus `--time 3.3`, `--hour 21`, `--yaw 90`);
  - `--fox-wake 2` and `5`;
  - `--fox-walk --fox-wake 6/12/20`;
  - `--foxes 8`, `--foxes 8 --fox-walk --fox-wake 20`, `--foxes 64`, `--foxes 200`;
  - `--hero`, `--hero-move 90,1.5`, `--hero --hero-attack 0.4`, `--fox --hero-move 120,1.2`;
  - a `--brush fire` stroke with `--settle 1`.
- Baseline runs are deterministic (two runs identical).
- **Release vs Release: 24 of 24 byte-identical.** The logs are identical too, apart from the output path, including every walk decision of the 8-fox run.
- **Debug vs Debug: 24 of 24 byte-identical**, logs included (baseline HEAD built in Debug).
- **Not compared:** `--ui-shot`. It is not deterministic run to run (wind time is wall-clock), and with `--menu` it necessarily changes, since the menu gains the Fox radio button and a help line.

### `--add-fox` exercises (Release, default view, captures)
| Run | Log |
|---|---|
| `--add-fox -4,2,0` | `pxe: fox 0 added: bed -5.00, 2.25, yaw -90.0 deg, 1.03 m from -4.00, 2.00 (18 grass sprites dropped)` |
| `--add-fox 2,-3,0` | `pxe: no room for a fox within 2 m of 2.00, -3.00` |
| `--fox-wake 6 --add-fox -2,-10,3` (fox #0 stays in bed) | `pxe: fox 1 added: bed -1.75, -9.75, yaw -40.9 deg, 0.35 m from -2.00, -10.00 (17 grass sprites dropped)` |
| `--fox-walk --fox-wake 6 --add-fox -2,-10,3` (fox #0 out on its walk) | `pxe: fox 0 walks to a point 2.4 m from its bed (target 19): ...` then `pxe: no room for a fox within 2 m of -2.00, -10.00` |
| `--fox-wake 6 --add-fox -4,2,8` | `pxe: the fox added at 8.0 s comes after the shot (--fox-wake 6.0): not added` |
| `--add-fox -4,2,1 --add-fox -4.5,2,0` and the reverse argument order | Both: fox 0 at (−5, 2.25), 0.56 m from (−4.5, 2), then no room near (−4, 2). The captures are identical: the order is by SECS. |
| `--scene hills --add-fox 0,0,0` | `pxe: --add-fox: no fox model (foxes live in the ruins only)` |
| `--ui-shot --add-fox 2,-3,0 --add-fox -4,2,0` (play, 3 hand-placed foxes) | no room at (2, −3) with the note drawn there; `fox 3 added: bed -3.75, 3.25, yaw -90.0 deg, 1.27 m ...` |

- **The route refusal is the rule working, not chance.** Over a 9 × 9 grid of clicks round fox #0's bed, 25 points differ between "fox #0 woken" and "fox #0 walking":
  - 18 go from a bed to no room;
  - 7 move to a farther bed (for example (−4, −5): from (−4.00, −4.75) to (−4.25, −4.50)).
- The added fox at (−1.75, −9.75) faces the campfire, 4.96 m away, so −40.9° is the bearing to the fire.

### Issues addressed
- None (cycle 1).

### How to playtest
- `./build/game/pxe`, then press 4. A cream ring the width of a curled fox follows the cursor over the ground.
  - Click open lawn: a fox appears curled asleep at the nearest bed that fits, and the log prints its number, bed, yaw and distance.
  - Click the paving, the pond, or right next to another fox: "No room for a fox here" shows by the click for about 2 s, and the log says no room.
  - Hold the button and drag: one fox only.
  - Open Tab and click on the menu: no fox.
  - Press 4 again, or 1/2/3, to switch the tool off or to a brush. The menu's "Fox (4)" radio does the same.
- Placement:
  - Near the campfire (within 6 m) the new curl faces the fire.
  - Farther out, rotate the view (Q/E) and click: wait 20-40 s for it to wake, and it stands facing the camera you had when you clicked.
- Several adds in the same spot fill outward until there is no room within 2 m.
- Routes: `--fox-walk --fox-wake 5` puts all three foxes out on walks. Click across fox #0's path (just ahead and left of its bed, behind the campfire): the bed is refused or pushed further away while it walks. Click again once it has lain down and it fits.
- Added foxes wake on their own timers. The hero can't walk through them. Their beds can stop other foxes' later walks.
- The grass under a new curl is gone, and leaves overhead are kept.
- `--no-fox`: no foxes at the start, but 4 still adds them.
- Scripted:
  - `--add-fox -4,2,3` in play adds one about 3 s in.
  - `--capture a.png --add-fox -4,2,0` gives a capture with one added fox.
  - `--capture a.png --fox-walk --fox-wake 6 --add-fox -2,-10,3` shows the refusal in the log.

### Known limitations
- **Startup cost.** Ruins play now always loads the fox model, lays out the scene a second time and sweeps the fox (about 0.1 s in Release, about 1.2 s in Debug), even with `--no-fox`. The spec asks for the load. The sweep is the spec's "once at model load". Captures with only `--fox` skip the sweep, because they never search.
- **Debug clicks are slow** (about 1.1 s for the worst click). The 50 ms target is for Release, but in a Debug build a click visibly hitches.
- **"Near the cursor"** is drawn at the point where the click (or the `--add-fox` point) was, not following the cursor as it moves.
- **`--add-fox` timing in play** follows the sum of frame dt's, which are clamped to 0.05 s as everywhere in the loop. A fox is asked for on the frame that reaches SECS and joins at the start of the next one, so a click or script lands one frame later. `--ui-shot` doesn't wait for later adds: only those due within its 8 frames (SECS ≈ 0) show in the shot.
- **`--add-fox` is ignored in `--bench` and `--self-test`**, with a log line. The spec covers play and captures only.
- **Added foxes don't take `--fox-walk`'s forced walk.** It applies to the foxes shown at startup, whose wake it starts. An added fox's first wake is 20-40 s later and is unforced.
- **`--add-fox` later than `--fox-wake`** isn't added in such a capture, and the log says so. That moment is past the shot.
- **Other foxes near a click are taken from their beds,** and only those within 2 m + 2 × reach (1.65 m) + foxNear + 0.1 m of the click. That is the same distance findBeds uses to skip a bed.
- **The route rule checks the routes committed at the moment of the add.** A fox that later plans a walk still checks the new bed through routeFree, as for any bed.
- **Byte-identical `--ui-shot` was not checked** (not deterministic). With `--menu` the panel necessarily changes.
