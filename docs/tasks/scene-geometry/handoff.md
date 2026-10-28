## Cycle 1
Build: pass

### Changes
- engine/include/pxe/triangle_grid.hpp, engine/src/triangle_grid.cpp (new): `pxe::Triangle` (corners and vertex normals), `pxe::trianglesOf(vertices, indices)` (an indexed list's triangles in index order), and `pxe::TriangleGrid`, the xz bucket grid that finds the triangle under (x, z) and returns the interpolated position and smooth normal. The grid's code is the former `game::GroundIndex`, moved verbatim (same float maths, same bucket order).
- engine/include/pxe/scene_geometry.hpp, engine/src/scene_geometry.cpp (new): `pxe::SurfaceGeometry` (one vertex and index list per surface; `append(GltfPrimitive)` and `append(surface, vertices, indices)` with the index offset as before; `corners(surface)`, the former `cornersOf`; `boundsMin/boundsMax` grown only by `append`, so geometry the game pushes into the lists directly does not count, as before). `pxe::buildSceneMaps(scene, geometry)` builds `shelter` from stone, `ground` from grass, dirt, stone, bark and foam, `waterTop` from water. `pxe::uploadScene(scene, geometry)` grows the scene's bounds over the appended primitives (min/max with the scene's existing bounds, which start at the origin, exactly as the old per-primitive `computeBounds`), uploads a mesh per non-empty surface with water going to `scene.water`, and sets the water's extent and level.
- engine/CMakeLists.txt: the two new sources.
- game/src/scene.cpp: `Triangle`, `GroundIndex`, `cornersOf`, `computeBounds`, the per-surface merge loop and the scene-finishing code removed. `RuinsGeometry` holds a `pxe::SurfaceGeometry`. `layOutRuins` appends each primitive, collects solids (non-`Ground` nodes), lawn triangles (all grass primitives) and `Ground` lawn corners and leaf triangles and areas by its own conventions via `pxe::trianglesOf`, adds its props to the lists as before, then calls `pxe::buildSceneMaps` and marks fires sheltered. `buildRuins` calls `pxe::uploadScene` where the upload and water code was, then scatters grass and leaves as before. `buildHills` builds its index list, appends it to a `SurfaceGeometry`, calls `buildSceneMaps` and `uploadScene`, and builds its `TriangleGrid` from `trianglesOf(verts, indices)`. `addQuad`, `addBox`, `addLog`, `addCampfire`, `addCandle`, `addShelter`, `StoneTops`, the hills builder, palettes, `SceneLayout`/`laidOut` stay in the game, unchanged apart from the type names.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::Triangle`, `pxe::trianglesOf`, `pxe::TriangleGrid` (triangle_grid.hpp); `pxe::SurfaceGeometry`, `pxe::buildSceneMaps`, `pxe::uploadScene` (scene_geometry.hpp). No game names, node names or asset paths in them.
- Game: scene.cpp loads ruins.glb, owns the `SurfaceGeometry` (inside `RuinsGeometry`), applies the `Ground` node convention, adds all props, places fires, and holds the lawn `TriangleGrid`.
- Misplaced code touched: game/src/scene.cpp. Its geometry half is now in the engine. Still misplaced there (scene-scatter task): `poisson`, `scatterGrass`, `patchiness`, the grass-under-stone-and-bark drop in `buildRuins`, and leaf clump scattering. The architecture.md row for "Turning a .glb into a pxe::Scene" can be narrowed to scattering and clearing grass (orchestrator's doc).

### Verification (cycle base 8702818, built from `git archive` in /private/tmp/claude-501/scene-geometry/base-build, Debug)
- Captures, `pxe --capture <png> <args>`, base vs this cycle, compared with `cmp`. All seven PNGs are byte-identical. The stderr logs are identical too (clumps/tufts, grass dropped, candles, fires, extra fires, leaf clusters, sprites, snap error). Only the GPU name, target size and "wrote" lines were filtered out:
  - `--hour 15`: identical
  - `--hour 21`: identical
  - `--hour 7 --rain 0.3`: identical
  - `--scene hills`: identical
  - `--scene hills --stamps`: identical
  - `--hour 21 --candles --shelter-test --extra-fires 6`: identical
  - `--foxes 3`: identical
- `--self-test`: PASS on both, with identical output.
- `laidOut`: scratch copies of both trees had a dump added at the end of `laidOut`. It wrote FNV-1a hashes of the campfires and the solids bytes, plus the ground and lawn HeightMaps (min/max, then `cellTop` and `heightAt` on a 0.1 m grid), plus the bed rectangle. Run with `--foxes 3`, and with `--foxes 3 --hour 21 --candles --shelter-test --extra-fires 6`. The output is identical in both cases. Default: 1 campfire at (-5, -6), 178992 solids corners, beds -18,-26 to 22,14. Shelter test + candles: 3 campfires at (-5,-6), (-8.5,-9), (-2,-10.5), 180876 solids corners.
- Fresh build directory (Debug): builds. The only warning is the existing stb_image_write `sprintf` deprecation, which the base build has too.

### How to playtest
- Play the default ruins and the hills. Nothing should look or behave differently: lawn, campfire, foxes on their beds, rain on fires, water reflections, hills grass.
- `--shelter-test --candles`: the roofed fire and the candles under the roof are sheltered, the rest are open to the rain, as before.

### Known limitations
- `laidOut` no longer sets the throwaway scene's bounds (they are now set in `uploadScene`, which `laidOut` never calls). Nothing reads them there.
- `buildSceneMaps` in the hills also builds an empty shelter and water-top map. This is equivalent to the defaults (both `build` calls clear, and nothing else is set).
