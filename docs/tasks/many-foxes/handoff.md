# Handoff: many-foxes

## Cycle 1
Build: pass

Fresh Debug build: `cmake -S . -B <new dir> -DCMAKE_BUILD_TYPE=Debug && cmake --build <new dir> --parallel`, with FetchContent fetching into the empty directory. Both steps exit 0. The only warnings are the two that were already there: stb's `sprintf` deprecation and the linker's duplicate-libglfw note. The repo's `build/` (Release) also builds incrementally with no new warnings.

### Changes

**Engine: GPU skinning, and a new actor interface**
- engine/include/pxe/mesh.hpp, engine/src/mesh.cpp:
  - New `SkinVertex`: the bind-pose position, normal and surface, 4 joints (`uint8`) and 4 weights.
  - New `SkinnedMesh`: a static VBO/EBO, written to look like `Mesh`. Joints go to attribute 3 through `glVertexAttribIPointer`, weights to attribute 4.
  - `Mesh::update`'s comment now names its remaining user, the sword smear.
- engine/src/gl.hpp: loads `glVertexAttribIPointer`.
- engine/include/pxe/skinned.hpp, engine/src/skinned.cpp:
  - `kActorMaterials` 16 → 64.
  - New `kMaxJoints` = 64. `load()` throws if a model has more joints.
  - `load()` uploads the model's `SkinnedMesh` once, so it now needs the GL context. Both callers already did.
  - `skin(pose, model, std::vector<glm::mat4>& joints)` now only computes each joint's skinning matrix (world × inverse bind) into the caller's vector. CPU vertex skinning for drawing is gone.
  - `jointPoint` is now `static jointPoint(joints, joint, restPoint)`.
  - The model holds no per-character state any more: the `mutable world_/skinMats_` are gone. One model can serve any number of characters.
  - `indices()` is gone.
  - New `mesh()` accessor.
  - New `vertexPoints(joints, out)`, documented as "for measuring, not drawing". See Known limitations.
- engine/include/pxe/renderer.hpp, engine/src/renderer.cpp:
  - New `ActorDraw { const SkinnedMesh* mesh; std::span<const glm::mat4> joints; }`.
  - `FrameParams::actors` is now `std::vector<ActorDraw>`. `actorEffects` stays `const Mesh*` (world space, CPU-built).
  - A file-local `drawActors(prog, actors)` sets `uSkinned = 1`, uploads each character's joints to `uJoints`, issues one draw per character, then resets `uSkinned` to 0.
  - It is used in the shadow pass (character overlay) and in `drawMeshes`, which serves both the reflection pass and the main mesh pass.
- engine/shaders/mesh.vert, engine/shaders/shadow.vert:
  - Add `aJoints` (uvec4), `aWeights`, `uSkinned` and `uJoints[64]`.
  - When `uSkinned == 1` they blend the four joint matrices. The mesh shader also renormalises `mat3(skin) * normal`, as the CPU did.
  - Static meshes take the same arithmetic path as before. The captures below are byte-identical.
- engine/shaders/mesh.frag: `kActorMaterials` = 64.
- engine/include/pxe/heightmap.hpp: `min()`/`max()` accessors for the grid's corners, used to bound the bed search.

**Game**
- game/src/hero.hpp, game/src/hero.cpp:
  - The hero keeps its own `joints_` and pushes `ActorDraw{&model_.mesh(), joints_}`. The sword's `jointPoint` calls read `joints_`.
  - New `struct Blocker` (a capsule on the ground, a..b plus a radius).
  - `update(..., const std::vector<Blocker>&)` pushes the hero out of each blocker after it moves. The push goes one axis at a time through the same `blocked()` wall test as its steps, and the hero's velocity into the blocker is removed so it slides round.
- game/src/fox.hpp, game/src/fox.cpp:
  - `static loadModel()`. `build(const SkinnedModel&, int index)` stores a pointer to the shared model and doesn't own it.
  - Seeds are `kSeed + index` and `kWalkSeed + index`.
  - `setFires(centres, reach)` replaces `setFire`. The `fireReach_ < 0` "no fire, no walks" rule is gone.
  - `update/wake` take `std::span<const Fox>`. `wake` no longer runs the time on; `main.cpp`'s `runFoxes` does that.
  - New `untilWalkDecided()`, `body()` and `routeFree()` (reservation), and `static findBeds()` (bed search).
  - Pose sampling moved from `pose()` into a private `skin()`, called at the end of `spawn/update/holdRest/wake`. `pose()` is `const` and only hands over the draw and the colours.
  - New `lie()`/`world()` statics, shared by `spawn`, the pose and the bed search, so all three place the fox the same way.
- game/src/scene.hpp, game/src/scene.cpp:
  - `FoxBed` and `BedFootprint` added. `BuildOptions::beds` replaces `foxSpot/foxRadius`.
  - New `SceneLayout` and `laidOut(options)`.
  - `buildRuins` is split into `setUpRuins` (camera, sun, palettes), `layOutRuins` (glb, campfires, shelter, candles, height maps, which also collects the solids and campfires) and the rest. The order of every RNG draw is unchanged.
  - The new constants are `kRuinsFoxSpot2`, `kRuinsFoxSpot3` and `kRuinsFoxYaw3`.
- game/src/main.cpp:
  - `--foxes N`. `foxShown` is replaced by `foxesShown` → `FoxSet {kNone, kFirst, kHandPlaced, kFound}`.
  - The model is loaded once (`std::optional<pxe::SkinnedModel>`). `std::vector<game::Fox>`.
  - New `runFoxes` and `foxBodies` helpers.
  - The capture, play and bench paths handle every fox. The bench now also builds the hero with `--hero`.
  - In play, foxes update before the hero.
- README.md: covers GPU skinning, three foxes, the reservation, the hero pushout, `--foxes N`, the new meaning of `--fox` in captures, 64 colour slots, the usage line and the bench line.

### What changed, per acceptance criterion
1. **One `fox.glb`.**
   - `main.cpp` calls `Fox::loadModel()` once into `std::optional<pxe::SkinnedModel> foxModel`.
   - Each `Fox::build(*foxModel, i)` stores `const SkinnedModel* model_`. Fox no longer has a model or mesh member.
2. **Seeds.**
   - `spawn` seeds `rng_` with `kSeed + index_` and `walkRng_` with `kWalkSeed + index_`. Fox #0 (index 0) gets 20261021 and 20261022, as before.
   - Checked: every fox #0 capture is unchanged, and in a 4-hour simulation fox #0 among three foxes matches fox #0 alone in every frame (below).
3. **Container.**
   - `std::vector<game::Fox> foxes`.
   - Capture path: `holdRest` for each fox, or `wake` for each followed by `runFoxes`; then `pose` for each.
   - Play loop: `update` for each fox (with the span), then the hero with `foxBodies(foxes)`, then `pose` for each.
   - Bench: the same as play.
4. **Three hand-placed foxes.**
   - Fox #0 is unchanged at `kRuinsFoxSpot`, facing the campfire.
   - Fox #1 is at `kRuinsFoxSpot2` (-5.5, -3.75), facing the campfire.
   - Fox #2 is at `kRuinsFoxSpot3` (-6.0, 1.25) with yaw −90°, facing the camera.
   - Margins are in the next section.
5. **`--foxes N`.**
   - `Fox::findBeds`: candidate spots every 0.25 m over the lawn within the scene focus ±20 m, Fisher-Yates shuffled by `draw01` (raw mt19937, the same on every platform) with seed `kBedSeed` = 20261023.
   - Each spot draws a yaw. A spot with a campfire within 6 m faces the nearest one instead.
   - The same bed rules apply, including clearance from every bed already accepted.
   - When it falls short it prints `pxe: only K of N foxes fit` (115 fit when 200 are asked for).
   - It replaces the hand-placed set and works in `--bench`, captures and play.
6. **Captures.**
   - `--fox`, `--fox-wake` and `--fox-walk` show fox #0 only (`FoxSet::kFirst`).
   - `--fox-wake` and `--fox-walk` apply to every fox shown: `forceWalk()` on each, and `wake()` on each followed by `runFoxes`.
7. **Grass.** `BuildOptions::beds` is a `std::vector<BedFootprint>`. The grass erase drops any blade within any bed's radius. With 1 bed it removes the same 20 sprites as before, so `--fox` captures are byte-identical.
8. **Fires.**
   - `routeClear` loops over every campfire's centre with `kCampfireReach + kFireClearance`, using the same box-to-point arithmetic as before. With one fire the result is bit-identical.
   - `SceneLayout::campfires` lists the main campfire and the `--shelter-test` ones.
   - A fox with no campfire nearby walks: the hand-placed fox #2 (7.3 m away) takes about two walks in five wakes, and some `--foxes` beds far from the fire walk too.
9. **Reservation.**
   - `routeFree()` runs after `routeClear()` on every candidate route.
   - Every check sample (each 0.1 m, plus the target) is a rotated body box (`kBodyBack/Front/Left/Right`) grown by 0.140 m. That growth is the furthest any corner can move in half a check step at the tightest turn, so the samples together cover the whole walk.
   - Each box must miss every other fox's `kFootprint` circle, its standing box in the bed, and every box of that fox's committed route while it is `walking_`.
   - A route is committed from the wake's plan until `kWalkBack` → `kLieDown`, which sets `walking_ = false` (released as it lies down).
   - Verified by simulation: 3 foxes for 4 hours (228 walks) and 64 found beds for 1 hour (243 walks, up to 6 out at once). There were 0 frames where two walking bodies overlapped, and 0 where a walking body crossed another fox's bed.
   - Before the growth was added, the 64-fox run had 6 frames with a sliver of overlap between samples.
10. **Pushout.**
    - `Fox::body()` returns a `Blocker`: the `kFootprint` (0.68 m) circle at the bed in `kRest`/`kLieDown`. Otherwise it is a capsule from the hips joint to the head joint (`jointPoint` on this frame's joints) with `kBodyRadius` 0.3 m (half the 0.58 m walking width).
    - The hero's radius is its footprint, `kFoot × size` (0.24 m at 1.6).
    - Foxes never read the hero.
    - Checked:
      - Running into sleeping fox #0 head-on stops the hero at x = −1.08, exactly 0.68 + 0.24 m from the bed.
      - With a 0.3 m offset it slides round and carries on.
      - Running into the standing fox (`--fox-wake 3`) slides the hero round behind it.
11. **Slots.** `kActorMaterials` is 64 in `skinned.hpp` and `mesh.frag`. The hero keeps 0–7 and every fox shares 8–11 (`kFirstMaterial` = 8, 4 materials).
12. **GPU skinning.**
    - The shadow pass (character overlay on the cached map, in both the plain and FIRE_GRID shadow programs), the reflection pass and the mesh pass all draw `ActorDraw` through `drawActors`.
    - The edge pass draws no geometry. It reads the normals and depth the skinned mesh pass wrote, so characters reach it already skinned.
    - No per-frame CPU vertex skinning is left. The hero's and every fox's mesh is uploaded once, in `SkinnedModel::load`.
    - `jointPoint` still drives the sword smear and sparks (hero attack captures are byte-identical), the fox's build-time pivot, and the fox's body capsule.
13. **Bench.** 3.56 ms in Release and 6.68 ms in Debug, both under the 16.6 ms target. Numbers below.
14. **Captures.** 75 of 76 are byte-identical, in Release and in Debug. Details below.
15. **Docs.** `--help` has `--foxes N` and the new `--fox` meaning. The README's fox paragraph now describes three foxes, the reservation and the pushout.
16. **Build.** A fresh Debug build passes (above).

### Bed spots and margins
**Method.** A scratch tool (not in the repo) compiles `game/src/fox.cpp` into itself and uses the game's own `laidOut`, `Fox::lie`, `Fox::world` and `SkinnedModel`.
- The fox: every one of its 1984 vertices, skinned at 2.4×, sampled every 1/60 s plus at each clip's end through all five clips. That is 1,521,728 vertex positions.
- Its placement: the game's spawn height and tilt.
- Obstacles: every triangle of every ruins.glb node except `Ground`, plus the campfire's patch, stones and logs as the game builds them. These match `SceneLayout::solids` with error 0.
- Distances are exact point-to-triangle in 3D.
- The search ran over a 0.25 m grid within the default view, with the bed rules plus one extra condition: the new bed (curl circle and standing box) and every clear route from it (400 sampled targets) had to miss fox #0's bed and every clear fox #0 route (20,000 sampled targets). That keeps fox #0's behaviour unchanged.
- 2,164 spots passed, so the rules did not need relaxing.

**Facing.** A spot within 6 m of the campfire faces it. Otherwise the yaw is −90°, which makes the standing fox face +z, straight at the yaw-180 camera.

| Fox | Bed (xz) | Yaw | Curl faces | Standing faces | From fire |
|---|---|---|---|---|---|
| #0 | (−2.0, −7.0) | −71.6° | the fire | (0.316, 0.949), 18.4° off the camera | 3.16 m |
| #1 | (−5.5, −3.75) | 167.5° | the fire | (−0.976, −0.217), screen-left, in profile | 2.30 m |
| #2 | (−6.0, 1.25) | −90.0° | −x | (0, 1), straight at the camera | 7.32 m |

Nearest objects (exact, every vertex and every 1/60 s):

| Fox | 1st | 2nd | 3rd | Campfire |
|---|---|---|---|---|
| #0 (check) | Paving.026 **0.204 m** (LieDown 1.22 s) | Paving.030 0.303 m | Paving.020 0.652 m | > 1.0 m |
| #1 | Paving.014 **0.338 m** (StandUp 0.53 s) | Paving.009 0.356 m | Paving.020 0.983 m | dirt patch 0.954 m, stones 1.104 m, logs 1.379 m |
| #2 | Paving.003 **0.370 m** (StandUp 0.43 s) | BushD 0.478 m | Flag.011 0.526 m | > 7 m |

- Fox #0's figure matches the fox-walk handoff's 0.204 m to Paving.026, which validates the tool.
- **Fox to fox.** Measured top-down between the 1 cm voxel middles' 2 cm plan cells (a lower bound in 3D):
  - #0–#1: 2.459 m
  - #1–#2: 2.526 m
  - #0–#2: 6.644 m
- **Ground.** The lowest vertex relative to the smooth ground height is −0.5 mm (#0), −4.4 mm (#1) and −7.5 mm (#2), all in LieDown. All are within the 3 cm sink rule.
- **Independence from fox #0.** Over 20,000 targets each, 0 of fox #1's 994 clear routes and 0 of fox #2's 3,971 clear routes touch fox #0's bed or routes. Fox #0 has 1,300 clear targets out of 20,000 both alone and with the other two beds.
- **Walk rates** (clear targets out of 20,000, with the other beds present):
  - #0: 6.5%, so about 1 wake in 5 walks, as before.
  - #1: 5.3%.
  - #2: 20%, so about 2 wakes in 5.

### Bench (`--bench 600 --foxes 64 --hero`, ruins, default settings, M3 Pro)
"Before" is the current tree with characters CPU-skinned into streamed world-space meshes each frame, as the old code did. It is a scratch variant: 64 foxes couldn't run before this change. Each figure is the mean of 600 frames; wall times repeated within 0.05 ms.

| Build | Before (CPU skinning) | After (GPU skinning) |
|---|---|---|
| Release | 3.61 ms/frame | **3.56 ms/frame** |
| Debug | 87.5 ms/frame | **6.68 ms/frame** |

For reference, the old `--bench 600` with no characters was 2.69 ms (Release).

`PXE_BENCH_PASSES=1` GPU timers, in ms (the timers block every frame, so wall time with them is 4.56 Release / 10.26 Debug after):

| Pass | Release before | Release after | Debug before | Debug after |
|---|---|---|---|---|
| shadow | 0.87 | 0.89 | 1.71 | 0.92 |
| reflection | 0.68 | 0.68 | 1.98 | 0.74 |
| meshes | 0.19 | 0.22 | 0.59 | 0.17 |
| water | 0.00 | 0.00 | 0.02 | 0.00 |
| sprites | 0.82 | 0.71 | 2.32 | 0.99 |
| edge | 0.36 | 0.36 | 0.94 | 0.37 |
| present | 0.13 | 0.16 | 0.28 | 0.00 |

- In Release the CPU skinning was cheap. GPU skinning mainly removes the 64 per-frame buffer re-uploads and the Debug-build cost.
- Shadow is about 0.9 ms with any character, before and after: that is the existing cache-blit-and-overlay path running every frame.

### Captures
- The baseline is HEAD (dd1559f), built in a git worktree. It was compared with this tree using the same set of 76 captures:
  - 10 without characters: plain, night, hills, shelter-test + candles, 12 extra fires, rain settle, grass fire, no grass, yaw 90, lightning flash.
  - 23 with the hero: 4 idle (day, night, size 2.4, lightning flash), 8 moves at 7 bearings (walking and running), and 11 attack moments (cuts, combos, a charge, 2 spin moments).
  - 43 with fox #0: asleep at 11 moments, waking at 13, on a forced walk at 17 (including night and rain), and 2 with the hero.
- Each build was deterministic run-to-run.
- **Release vs Release: 75 of 76 byte-identical. Debug vs Debug: the same.**
- The only difference is `--fox-walk --fox-wake 6` (the fox walking): **1 pixel, max channel difference 9**, at (383, 271). One cream chest pixel takes its neighbour's ramp shade, (236,186,172) → (227,183,172). Side by side at 8× it can't be told apart.
- Every no-character capture is byte-identical, and so is every hero capture and every other fox capture.
- Debug and Release builds differ from each other for moving characters (CPU float in their motion and the camera following them). That is not new: it happens at HEAD too, so each build type was compared with itself.

### Issues addressed
- None (cycle 1).

### How to playtest
- `./build/game/pxe`: three foxes asleep.
  - #0 is behind-right of the campfire, as before.
  - #1 is in front-left of the fire, facing it.
  - #2 is on the lawn below-left, in front of the paving.
  - Watch them wake on their own timers. The first walks: fox #2 often, and fox #0 on its second wake, about 71.5 s in.
- `--fox-walk --fox-wake 5`: all three wake together and take a walk if one is clear. Watch #1 and #2. Their walk areas overlap, so a later wake may find its first target blocked by the other's route and try another target. Check that they never cross while out.
- Hero vs foxes:
  - Run (WASD) into a sleeping fox: you should stop at its curl and slide round it.
  - Stand next to fox #1 or #0 when it stands up. You'll be pushed out, possibly by most of a metre in one frame.
  - Walk into a walking fox's path. It carries on and pushes you aside. Pin yourself between a fox and a wall: the wall wins.
  - The tail (brush) isn't part of the body capsule, so you can stand on it.
- `--foxes 64` (in play, or `--capture x.png --foxes 64`): 64 foxes scattered round the ruins, the same every run. `--foxes 200` prints `only 115 of 200 foxes fit`.
- `--bench 600 --foxes 64 --hero` (add `PXE_BENCH_PASSES=1` for pass times).
- Captures:
  - `--capture a.png --fox` shows fox #0 alone, as before.
  - `--capture a.png --foxes 8 --fox-walk --fox-wake 20` shows several foxes mid-walk, and the log lists who walks.
- Night (`--hour 22`), rain and the reflection: the foxes by the pond side should reflect and cast shadows like the hero.

### Known limitations
- **`SkinnedModel::vertexPoints` stays: CPU vertex positions for measuring.**
  - `Fox::findBeds` needs every vertex of every pose to apply the "whole fox through every frame" rule at run time for `--foxes`. It runs once at startup, only with `--foxes`, and never for drawing.
  - I read "no CPU vertex skinning remains" as being about drawing. If the orchestrator reads it more strictly, the alternative is precomputing the swept fox offline, which would be an asset change.
- **`--foxes` startup cost**: about 0.2 s extra in Release and about 5 s in Debug (1.5 M vertex positions, then sorted into voxels). Plain `--fox`/play adds 0.03 s Release / 0.2 s Debug, because the ruins' geometry is laid out twice (`laidOut`, then `buildScene`).
- **My reading of the bed rules where the spec is silent:**
  - "Open lawn" is: Ground-node lawn under the whole curl circle, and no vertex more than 3 cm under the smooth ground height (`kSink`).
  - "Fires" for beds and walks are the campfires, which have dirt patches. `--extra-fires` bare flames are ignored: a `--foxes` bed or a walk can sit on one, and an extra fire can be placed on a bed.
  - Candles are scene geometry, so beds clear them. Walks don't, which was already the case.
  - The search area is the scene focus ±20 m, about what the default view shows. The spec gave none, and the whole lawn (±68 m) put most foxes off-screen.
- **Fox-to-fox clearance in `findBeds` is conservative**, not exact. It is checked top-down between 1 cm voxels' 2 cm plan cells, with 4.6 cm of slack. Beds that would just pass an exact check can be refused. Never the reverse.
  - Scene clearance is exact: a voxel is decided by its middle when that is clear-cut, and otherwise by every vertex in it.
- **Many `--foxes` beds can't walk.** In an 8-fox run, 5 of the 8 found no clear route in 256 forced tries (banks, edges, paving). The spec doesn't require walkable beds.
- **Fox #0 independence is sampled, not proven.**
  - 20,000 targets per fox were checked against a 5 cm raster, plus a 4-hour frame-by-frame simulation in which fox #0 is identical to running alone.
  - The target space is continuous, so a never-sampled route could in principle interact.
- **The reservation is conservative.** Boxes are grown by 0.140 m and beds count as taken even while their fox is away. This is a small cost to fox #1 and fox #2's walk rates.
- **Pushout is instant and positional.**
  - A fox standing up next to the hero shoves it out in one frame.
  - Against a wall the hero may stay inside a fox.
  - The capsule ignores the brush.
  - The hero's footprint (0.24 m), not its cape, sets how close it gets.
- **Multi-fox `--fox-wake` stepping.** `runFoxes` steps all foxes to the earliest moment any of them may plan a walk (`untilWalkDecided`, a lower bound). A plan sees lower-index foxes at that moment and higher-index ones at the step's start, the same as one frame of play. A lone fox still takes one step, as before.
- **`--bench --hero` now has a hero.** It used to be ignored in the bench, so this changes that one bench command even without `--foxes`.
