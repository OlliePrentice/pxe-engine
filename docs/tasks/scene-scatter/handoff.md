## Cycle 1
Build: pass

### Changes
- engine/include/pxe/scatter.hpp, engine/src/scatter.cpp (new):
  - `pxe::poissonDisk(lo, hi, minDist, rng, accept)` is the former `poisson`.
  - `pxe::grassPatchiness(x, z)` is the former `patchiness`.
  - `pxe::GrassScatter` holds `clumpSpacing`, `tuftSpacing`, `tufts` and `height`. Its defaults are 0.34, 2, true and `grassPatchiness`.
  - `pxe::scatterGrass(scene, ground, params, rng)` is the former `scatterGrass`. It keeps the "clumps N, tufts N" log.
  - `pxe::dropGrassUnder(scene, geometry, {surfaces})` is the stone-and-bark pass over the scene's bounds on the 0.1 m grid. It returns the count of sprites dropped.
  - `pxe::scatterLeaves(scene, crowns, density, rng)` is the leaf clump scattering. Its constants are unchanged: 0.7 clumps per m², 0.55 m clump radius, 1 m clump grid, 1.6 candidates, 0.12/0.88 keep, tilts, the 0.3/0.35 m stand-proud and 0.17 detail shade. It returns the leaf count.
  - All code is moved verbatim: the same float expressions and the same order of RNG draws and erases. One small difference: crown triangle areas are now computed inside `scatterLeaves` from the triangles, with the same expression, as a separate value before the running sum, so no new FMA contraction.
- engine/CMakeLists.txt: added `src/scatter.cpp`.
- game/src/scene.cpp:
  - Removed `poisson`, `scatterGrass`, `patchiness`, the stone-and-bark drop block, the leaf clump block, `RuinsGeometry::leafArea`, and the now-unused `<functional>` and `<unordered_map>` includes.
  - Added the `grassScatter(options)` helper, which maps `BuildOptions` (minDist, tuftDist, tufts) to `pxe::GrassScatter` with `pxe::grassPatchiness`.
  - `buildHills` calls `pxe::scatterGrass`. `buildRuins` makes `std::mt19937 rng(7)`, then calls `pxe::scatterGrass`, then `pxe::dropGrassUnder(..., {kSurfStone, kSurfBark})`, whose count it prints with the exact old wording. Then come the firefly homes, the fire-pit and fox-bed clears (unchanged), then `pxe::scatterLeaves(scene, geo.leaves, options.leafDensity, rng)` with the same `rng`. The "leaf clusters" log is still in its old place.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: `pxe::poissonDisk`, `pxe::grassPatchiness`, `pxe::GrassScatter`, `pxe::scatterGrass`, `pxe::dropGrassUnder`, `pxe::scatterLeaves` (scatter.hpp). No game names, positions or asset paths.
- Game: scene.cpp decides which triangles are lawn and crowns and which surfaces grass is dropped under. It owns the seed-7 generator and passes it to grass, then leaves. It keeps firefly homes, fire-pit and fox-bed clears, extra fires, and the "dropped" and "leaf clusters" log lines.
- Misplaced code touched: game/src/scene.cpp. All of its scattering is now in the engine, so the architecture.md row "Grass and leaf scattering, Poisson sampling, dropping grass under stone and bark" can be marked in place (`pxe::scatter.hpp`). That doc belongs to the orchestrator.

### Verification (cycle base 5fcfd91, built from `git archive` in /private/tmp/claude-501/scatter-base, Debug)
- Captures, `pxe --capture <png> <args>`, base vs this cycle, compared with `cmp`. All seven PNGs are byte-identical. The stderr logs are identical once the GPU name, target size and "wrote" lines are filtered out:
  - `--hour 15`: identical (clumps 137172, tufts 4042; 1186 dropped under stone or trunks; 37748 leaf clusters; 177737 sprites)
  - `--hour 21`: identical
  - `--hour 7 --rain 0.3`: identical
  - `--scene hills`: identical (clumps 99170, tufts 2891; 102061 sprites)
  - `--scene hills --stamps`: identical
  - `--hour 21 --candles --shelter-test --extra-fires 6`: identical (1280 dropped; 177615 sprites)
  - `--foxes 3`: identical (1186 dropped, 51 under 3 foxes; 177686 sprites)
- `--self-test`: PASS on both, with identical output.
- Fresh build directory (Debug): builds. The only warning is the existing stb_image_write `sprintf` deprecation.

### How to playtest
- Play the default ruins and the hills. Lawn, tufts, tree crowns, grass round the paving and trunks, fire pits and fox beds should all look exactly as before.

### Known limitations
- `buildHills` still declares an unused `std::uniform_real_distribution<float> unit` from before this task. I left it alone.
