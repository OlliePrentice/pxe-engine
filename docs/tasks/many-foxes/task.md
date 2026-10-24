# Many foxes
Type: feature
Status: complete
Cycle: 1

## Goal
The engine can show one fox, wired in by hand. Make many foxes work: they share one loaded model, wake and walk independently, keep out of each other's way, block the hero, and render fast enough that 64 foxes and the hero hold 60 fps. This fixes the six blockers found in the ECS assessment (2026-10-22): a model per instance, shared seeds, wiring for one fox only, the 16-slot actor material limit, no avoidance, and CPU skinning cost. No ECS: the user decided to fix the blockers directly and keep `Fox` and `Hero` as classes.

## Decisions
- **Scope: many foxes, not many species.** Don't build general machinery for future species (no model cache keyed by path, no slot allocator). Generalise when a second species exists. (User principle: no speculative abstraction.)
- **Shared model.** `fox.glb` is loaded once. Every `Fox` uses that one `pxe::SkinnedModel` (passed in, not owned) and keeps only its own state: pose, timers, RNGs, route, place.
- **Seeds for each fox.** Fox i seeds its rests with `kSeed + i` and its walks with `kWalkSeed + i`. Fox #0 keeps today's seeds exactly.
- **Hand-placed foxes for play: 3.** Fox #0 stays at `kRuinsFoxSpot` (-2, -7), facing the campfire, unchanged. The developer finds two more beds by search, under the existing bed rules: open lawn; the whole fox through every frame of all five clips, at 2.4× and tilted to the slope, at least 0.2 m clear of every scene object, every fire's stones, logs and dirt patch, and every other bed's fox; the curl facing a fire if one is within 6 m, otherwise a yaw chosen for the default camera. Each spot's margins go in the handoff. If two valid spots can't be found, stop and report rather than relax the rules.
- **`--foxes N`: automatic placement.** It replaces the hand-placed set. Beds are found with a fixed seed (repeatable) under the same bed rules, including clearing every other bed. Each curl faces the nearest fire within 6 m, otherwise a seeded random yaw. If fewer than N valid beds exist, it places as many as fit and prints how many it placed. It works in `--bench` (today `foxShown` always hides the fox in the bench).
- **Which foxes are shown:**
  - In play (ruins): the 3 hand-placed foxes, unless `--no-fox`; `--foxes N` replaces them.
  - In captures: `--fox` shows fox #0 only, so existing fox captures keep their content. `--foxes N` shows N placed automatically.
  - `--fox-wake` and `--fox-walk` apply to every fox shown.
  - Never in the hills.
- **Grass clearing for each bed.** `BuildOptions` takes a list of beds (xz and radius) in place of the single `foxSpot`/`foxRadius`, and no grass grows under any of them.
- **Fires.** A fox's walk keeps `kFireClearance` clear of every fire's dirt patch in the scene, not just one campfire. Having no fire nearby no longer stops a fox walking. The "no fire, no walks" rule (`fireReach_ < 0`) goes.
- **Fox↔fox avoidance: route reservation.** A fox accepts a walk only if its whole route, checked with the same rotated body rectangle `routeClear` already uses (`kBodyBack/Front/Left/Right`), stays clear of every other fox's bed footprint and of every route another fox has committed to and not yet finished. A committed route is released when that fox lies back down. Walks are still planned in full before setting off and timed in closed form. There is no avoidance mid-walk.
- **Hero→fox blocking: pushout.** The hero is pushed out of each fox's body each frame: the `kFootprint` circle when curled or lying down, and a capsule around the current walking/standing body otherwise. A walking fox carries on and the hero is moved aside. Foxes never react to the hero.
- **No fox→hero awareness.** Routes don't avoid the hero or its spawn point. Foxes don't stop for it. (Fox AI stays out of scope, as in earlier fox tasks.)
- **Actor material slots.** Raise `kActorMaterials` from 16 to 64, in both `skinned.hpp` and `mesh.frag`. Keep the fixed per-type offsets (hero 0–7, fox 8–11). All foxes share slots 8–11.
- **GPU skinning for every character.** Skinning moves to the vertex shader for the hero and the foxes alike, and CPU vertex skinning goes away. Each model uploads one static VBO/EBO (bind-pose position, normal, surface, 4 joints, 4 weights) once. Each character draws with its own joint matrices as uniforms (GL 4.1 core: uniform array or UBO, no SSBOs), one draw call per character per pass. Every pass that draws actors today (shadow, reflection, mesh, edge) draws them skinned. `SkinnedModel` still computes joint world matrices on the CPU (cheap), so `jointPoint` keeps working for the hero's sword and the fox's build-time measurements. Sword smears and fragments (`actorEffects`) stay CPU-built meshes. No instancing.
- **Performance target.** `--bench 600 --foxes 64 --hero` in the ruins at default settings must report a mean of ≤ 16.6 ms per frame on the user's Mac. The handoff records before (CPU skinning, if 64 foxes can run before the change) and after numbers, with `PXE_BENCH_PASSES=1` pass times. If the target is missed, stop and report rather than add instancing.
- **Keeping existing behaviour.** Fox #0's bed, seeds and behaviour are unchanged. Captures with no fox are byte-identical to before. Fox and hero captures may differ only by GPU-skinning rounding, which the handoff measures (max per-pixel difference, pixel count) and shows to be visually equivalent.

## Acceptance criteria
- [x] `fox.glb` is loaded once per run, and every `Fox` refers to that one `SkinnedModel` rather than owning one — `main.cpp` loads `Fox::loadModel()` once into `std::optional<pxe::SkinnedModel>`; `Fox::build(model, i)` keeps `const SkinnedModel* model_`
- [x] Fox i seeds its rest and walk RNGs with `kSeed + i` and `kWalkSeed + i`; fox #0's seeds equal today's — `Fox::spawn` seeds `rng_`/`walkRng_` with `kSeed`/`kWalkSeed + index_`; index 0 gives today's seeds
- [x] `main.cpp` holds the foxes in a container, and every place that updated or posed the single fox (capture path and play loop) handles all of them — `main.cpp` `std::vector<game::Fox> foxes`; capture path (holdRest/wake + `runFoxes`, pose), play loop and bench loop all iterate it
- [x] Play in the ruins shows 3 hand-placed foxes: fox #0 at `kRuinsFoxSpot` unchanged, and two more whose spots, facing and measured clearances (≥ 0.2 m through all clips from objects, fires and the other beds) are documented in the handoff — `scene.hpp` `kRuinsFoxSpot2` (-5.5, -3.75) and `kRuinsFoxSpot3` (-6, 1.25, yaw -90°); margins 0.338 m / 0.370 m, fox-to-fox ≥ 2.46 m (handoff cycle 1)
- [x] `--foxes N` places up to N foxes deterministically under the bed rules, faces each curl toward the nearest fire within 6 m or a seeded yaw, prints the count placed when it's short, replaces the hand-placed set, and works with `--bench` — `Fox::findBeds` (seeded shuffle `kBedSeed`, same rules, faces campfire within `kFaceFire`); `main.cpp` `foxesShown` → `FoxSet::kFound` incl. bench; prints `only K of N foxes fit`
- [x] In captures, `--fox` shows fox #0 only; `--fox-wake` and `--fox-walk` apply to every fox shown — `main.cpp` `foxesShown`: captures → `FoxSet::kFirst`; `forceWalk()`/`wake()` applied to every fox
- [x] `BuildOptions` takes a list of bed footprints, and grass is cleared under every bed — `scene.hpp` `BuildOptions::beds` (`std::vector<BedFootprint>`); `buildRuins` erases grass under any bed
- [x] Fox walks keep clear of every fire in the scene; a fox with no fire nearby can still walk — `Fox::setFires` over `SceneLayout::campfires`; `routeClear` checks every fire; `fireReach_ < 0` rule removed
- [x] A fox commits to a walk only if its whole route (rotated body rectangle) clears every other fox's bed and every other committed, unfinished route; a route is released when its fox lies down — `Fox::routeFree` (rotated boxes grown 0.14 m vs other beds and committed `route_.body`); `walking_ = false` on kWalkBack → kLieDown in `Fox::update`
- [x] The hero is pushed out of every fox's body each frame (circle when curled, capsule otherwise); foxes never change their behaviour because of the hero — `Hero::update` pushout loop over `Blocker`s; `Fox::body()` circle in kRest/kLieDown, hips-head capsule otherwise; fox code never reads the hero
- [x] `kActorMaterials` is 64 in `skinned.hpp` and `mesh.frag`; the hero keeps slots 0–7 and the fox 8–11 — `skinned.hpp` and `mesh.frag` `kActorMaterials = 64`; `Fox::kFirstMaterial = 8`
- [x] Skinning runs in the vertex shader for the hero and all foxes in the shadow, reflection, mesh and edge passes; no CPU vertex skinning remains; each model's vertex data is uploaded once; `jointPoint` still works for the hero's sword — `mesh.vert`/`shadow.vert` skin by `uJoints`; `renderer.cpp` `drawActors` in shadow overlay and `drawMeshes` (reflection + main); edge pass is screen-space; `SkinnedMesh` uploaded once in `SkinnedModel::load`; `jointPoint` static over the hero's `joints_`. `vertexPoints` remains for the `--foxes` bed search only (accepted: the criterion is about drawing)
- [x] `--bench 600 --foxes 64 --hero` reports a mean of ≤ 16.6 ms per frame in the ruins; before and after numbers and pass times are in the handoff — Handoff cycle 1: 3.56 ms Release, 6.68 ms Debug (M3 Pro), pass times recorded
- [x] Captures without foxes are byte-identical to before; fox and hero captures differ only by measured GPU-skinning rounding (documented in the handoff) — Handoff cycle 1: 75 of 76 captures byte-identical; `--fox-walk --fox-wake 6` differs by 1 pixel (max channel 9)
- [x] `--foxes N` and the changed meaning of `--fox` (captures: fox #0 only) are in `--help` and the README, and the README's fox paragraph describes several foxes and their avoidance — `main.cpp` `printUsage` and README usage, bench line and fox paragraph updated
- [x] Project builds cleanly from a fresh build directory with CMake — Orchestrator fresh Debug build, cycle 1: exit 0, only the existing stb/glfw warnings

## Assets
None. `fox.glb` and `hooded_hero.glb` are used as they are.

## Out of scope
- An ECS, or any general character framework
- A second species, a model cache keyed by path, or a material-slot allocator
- Instancing (only by a spec change if the performance target is missed)
- Fox awareness of or reaction to the hero (AI), avoidance mid-walk, and pathfinding
- The sword hitting foxes
- Changes to the fox's clips, mesh or fox #0's bed

## Cycle log
- Cycle 1: 2026-10-22 started
- Cycle 1: complete. Fresh build passes; review found no issues.
