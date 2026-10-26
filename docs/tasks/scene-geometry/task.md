# Engine scene geometry: from glTF primitives to a drawable, queryable Scene
Type: feature
Status: open
Cycle: 1

## Goal
game/src/scene.cpp holds generic machinery for turning loaded geometry into a `pxe::Scene`; docs/architecture.md marks it engine and misplaced. This task moves the geometry half into the engine: merging glTF primitives into one vertex/index list per surface, the triangle index used to find the ground under a point, and finishing a scene from its geometry (meshes and water uploaded, bounds, water level and extent, shelter, ground and water height maps). The game keeps everything specific to its scenes: which file, the "Ground" node convention, the props it adds (campfires, logs, boxes, candles, the test shelter), where they go, the palettes and camera, and the scene layout for foxes. Grass and leaf scattering is the next task. Rendered output must not change by a single bit.

## Decisions
- The game appends its props to the per-surface lists between loading and finishing (scene.cpp layOutRuins), so the engine API is staged: build the per-surface geometry, let the game add to it, then finish the scene. No callback framework.
- Engine capabilities, named for what they do (exact names the developer's choice, following engine style):
  - Per-surface geometry: the vertex and index lists for each `pxe::Surface`, appending a `GltfPrimitive` (index offset as today), the triangle corners of one surface (today's `cornersOf`), and bounds of what it holds.
  - The triangle grid that finds the triangle under an (x, z) and returns the interpolated position and smooth normal (today's `Triangle` + `GroundIndex`, scene.cpp:100-170), public, because game placement code (campfires, shelter, candles, fireflies, extra fires) queries it.
  - Finishing a scene from per-surface geometry (today's scene.cpp:777-788 and 823-838): a mesh per non-empty surface with water going to `scene.water`; water bounds and level; `shelter` from stone; `ground` from grass, dirt, stone, bark and foam; `waterTop` from water. Which surfaces feed which map are engine defaults, unchanged.
- Bounds: today `computeBounds` runs per primitive as it is loaded (scene.cpp:666) and never sees the props added later. Keep exactly that: bounds come from the loaded primitives only.
- What tells the engine which triangles are lawn (today the `Ground` node, scene.cpp:671, 690) is a game asset convention: it stays in game code, which collects lawn and crown triangles from the primitives itself or passes a predicate. The engine never names a node.
- Kept in the game, single-use for now: `addQuad`, `addBox`, `addLog`, `addCampfire`, `addCandle`, `addShelter`, `StoneTops`, `hillHeight` and the hills builder, `setUpRuins`/`setPhasePalettes`, `SceneLayout`/`laidOut`, the solids list.
- The hills scene uses the same engine pieces where they apply (bounds, upload, ground map, triangle grid).
- Bit-identical output: keep the order in which primitives, props and triangles are appended, and the float maths, exactly as today.

## Layering
- Engine: new header(s) under engine/include/pxe/ for the per-surface geometry, the triangle grid and finishing a scene; sources in engine/src; engine/CMakeLists.txt. No game content in names, APIs or comments.
- Game: scene.cpp builds the ruins and hills with them: loads ruins.glb, appends it, collects lawn and crown triangles by its own conventions, adds its props, finishes the scene, then (until scene-scatter) scatters grass and leaves as today.

## Acceptance criteria
- [ ] `GroundIndex`/`Triangle`, `cornersOf`, `computeBounds`, the per-surface merge and the scene-finishing code no longer exist in game/src/scene.cpp; their engine equivalents do.
- [ ] No engine file names Ground, ruins, campfire, candle, fox or any asset path.
- [ ] `game::laidOut` returns the same layout as before (same campfires, solids count, ground, lawn, bed rectangle); check by logging or comparing in a scratch build, and record in the handoff.
- [ ] Captures are byte-identical to the cycle base: `--capture` with `--hour 15`, `--hour 21`, `--hour 7 --rain 0.3`, `--scene hills`, `--scene hills --stamps`, `--hour 21 --candles --shelter-test --extra-fires 6`, and the default with `--foxes 3`. Build log lines (clumps/tufts counts, grass dropped, candles, fires, leaf clusters, sprites) identical too. Record commands and results in the handoff.
- [ ] `--self-test` passes.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- Grass and leaf scattering, Poisson sampling, dropping grass under stone and bark (scene-scatter task).
- Any change to the scenes' content or look.

## Cycle log
