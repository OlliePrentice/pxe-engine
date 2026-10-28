## Cycle 1
Build: pass

### Changes
- engine/include/pxe/renderer.hpp: removed `shadowScene_`, `flashScene_` and `ShadowCube::drawnScene`. Added `shadowValid_`, `flashValid_` and `ShadowCube::drawn` validity flags. Documented the contract on `upload()` (drops every scene-derived cache) and `render()` (must be given the scene last passed to `upload()`; caches are not keyed on the scene's identity).
- engine/src/renderer.cpp:
  - `upload()`: clears `shadowValid_`, `shadowStaticValid_` and `flashValid_`. The cubes were already reset to `ShadowCube{}`, which now clears `drawn`. This fixes the stale flash map: `flashScene_` was never reset here.
  - `destroyTargets()`: clears `drawn` on every cube (was `drawnScene = nullptr`), and clears `shadowValid_`, `shadowStaticValid_` and `flashValid_` (was only `shadowScene_ = nullptr`). The flash map's texture is recreated here too, but `flashScene_` was never reset, so that was a second latent stale-flash case.
  - Redraw conditions: cube `!cube.drawn || drawnPos != light.pos`, sun `!shadowValid_ || shadowLight_ != lightDir || crownShadows`, flash `flash > 0 && (!flashValid_ || flashLight_ != flashDir || crownShadows)`. Every non-address trigger is unchanged. Each redraw sets its flag back to true where it used to store `&scene`.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: pxe::Renderer private cache state only (flags listed above). No public API change except comments.
- Game: none.
- Misplaced code touched: none.

### How to playtest
- Normal play should look identical: sun shadows, character shadows over the cached scene map, firelight shadows and lightning shadows all as before.
- Lightning: during a storm, flashes should still cast shadows from the strike direction.
- Anything in the game that calls `upload()` again (e.g. rebuilding the scene) should show correct shadows on the first frame after, including the first lightning flash.

### Known limitations
- `shadowStaticValid_` is now also cleared in `upload()` and `destroyTargets()`. This doesn't change output: the first frame after either always redraws the scene map, and that redraw already set `shadowStaticValid_`.
- Out of scope, not changed: `pxe::FireGrid` (a member of Renderer, though not a nested struct) still holds `const Scene* scene_` (engine/include/pxe/firegrid.hpp:116). It looks like it has the same "keeps a pointer between calls" issue against docs/architecture.md, and may need its own task.
