## Cycle 1
Build: pass

### Changes
- engine/include/pxe/firegrid.hpp: replaced `const Scene* scene_` with `bool built_`. `reset()` clears it, `running()` returns it. Documented the contract on `advance`: the first advance after construction or `reset()` builds the grid from the scene it is given, and every advance until the next `reset()` must be given that same scene. Reworded the `reset()` comment and added one for `running()`.
- engine/src/firegrid.cpp: `build()` sets `built_ = true` where it used to store the scene address; `advance()` builds when `!built_` instead of comparing addresses.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: pxe::FireGrid only. Private member `scene_` became `built_`; public API (`reset`, `running`, `advance`) has the same signatures.
- Game: none.
- Misplaced code touched: none.

### How to playtest
- Start the game, set grass alight: the fire grid should build and burn as before.
- Anything that re-uploads the scene (Renderer::upload calls `grid_.reset()`) should rebuild the grid on the next frame, as before.

### Known limitations
- Behaviour differs from before only if a caller passes a different scene to `advance` without calling `reset()`: the old code rebuilt, the new code keeps the grid built for the first scene. That case is now outside the documented contract; the Renderer always resets in `upload()`, so it does not occur. renderer.cpp is unchanged.
