# FireGrid: no Scene pointer between calls
Type: fix
Status: open
Cycle: 1

## Goal
`pxe::FireGrid` keeps `const Scene* scene_` (engine/include/pxe/firegrid.hpp:116) between calls and rebuilds when `advance` is given a different address (engine/src/firegrid.cpp:310). docs/architecture.md says the engine never keeps a pointer to a scene between calls. The renderer already resets the grid in `upload()`, so this is not a live bug, but it is the same pattern the renderer-scene-caches task removed. Replace it with an explicit built flag.

## Decisions
- Contract, matching the Renderer's: `advance` must be given the scene the grid was built for since the last `reset()`. The grid builds on the first `advance` after construction or `reset()`, not by comparing addresses.
- `running()` keeps its meaning: true once the grid is built for a scene, false after `reset()`.

## Layering
- Engine: pxe::FireGrid (engine/include/pxe/firegrid.hpp, engine/src/firegrid.cpp).
- Game: none.

## Reproduction
- Observed: firegrid.cpp:310 `if (scene_ != &scene) build(scene, rain);`, with `scene_` set at firegrid.cpp:294 and cleared by `reset()`.
- Expected: no Scene pointer kept; build on the first advance after reset.

## Acceptance criteria
- [ ] No Scene pointer or reference member remains in pxe::FireGrid.
- [ ] `reset()` makes the next `advance` build the grid; `running()` is false after `reset()` until it does.
- [ ] firegrid.hpp documents the advance-after-reset contract.
- [ ] Renderer's use of the grid (renderer.cpp `grid_.reset()`, `grid_.running()`, `grid_.advance`) is unchanged.
- [ ] Project builds cleanly from a fresh build directory with CMake.

## Assets
None

## Out of scope
- Any other FireGrid change.

## Cycle log
- Cycle 1: 2026-10-26 started, base 75f49c9 (worktree /Users/sampr/Projects/Games/pxe/pxe-engine-firegrid-scene-pointer, branch firegrid-scene-pointer)
