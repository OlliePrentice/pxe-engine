# Renderer: no Scene pointers between calls
Type: fix
Status: complete
Cycle: 1

## Goal
pxe::Renderer remembers the address of the last Scene it drew (`shadowScene_`, `flashScene_`, `ShadowCube::drawnScene`) and uses address comparison to decide when scene-derived shadow maps must be redrawn. docs/architecture.md says the engine "never keeps a pointer to one between calls". It is also a bug: `upload()` resets `shadowScene_` and the shadow cubes but not `flashScene_`, so after uploading a new scene that happens to sit at the same address, the lightning-flash shadow map stays stale until the flash direction changes. Replace the address checks with explicit invalidation.

## Decisions
- Contract: `render(scene, ...)` must be given the scene last passed to `upload()`. Scene-derived caches are invalidated by `upload()` and by the existing triggers (light direction change, crown shadows, light position for cubes), not by comparing addresses.
- Remove `shadowScene_`, `flashScene_` and `ShadowCube::drawnScene`. Use a validity flag (or equivalent) per cache, cleared in `upload()` and wherever the GL resources behind it are recreated (renderer.cpp around line 214-223).
- Rendered output for the normal path (upload once, then render every frame) must be unchanged.

## Layering
- Engine: pxe::Renderer cache invalidation (engine/include/pxe/renderer.hpp, engine/src/renderer.cpp).
- Game: none.

## Reproduction
- Observed: `Renderer::upload` (renderer.cpp:497-506) sets `shadowScene_ = nullptr` and resets `cubes_`, but leaves `flashScene_` and `flashLight_`. The flash map redraw test at renderer.cpp:959 is `flashScene_ != &scene || flashLight_ != lighting.flashDir || crownShadows`, so a newly uploaded scene at the same address with the same flash direction reuses the old scene's flash shadow map.
- Expected: after `upload()`, the first frame with a flash redraws the flash shadow map.

## Acceptance criteria
- [x] No `const Scene*` (or other Scene pointer or reference) member remains in pxe::Renderer or its nested structs. (65a9e47: renderer.hpp now has shadowValid_, flashValid_, ShadowCube::drawn; grep finds no Scene pointer member)
- [x] `upload()` invalidates the main shadow map (including `shadowStaticValid_`'s map), every shadow cube, and the flash shadow map. (Renderer::upload clears shadowValid_, shadowStaticValid_, flashValid_ and resets cubes_)
- [x] The places that delete and recreate the shadow framebuffers/textures (renderer.cpp ~208-226) also invalidate the matching caches, as the current `drawnScene = nullptr` / `shadowScene_ = nullptr` lines do. (Renderer::destroyTargets clears cube.drawn and all three flags; flash flag is new there, fixing a second stale-map path)
- [x] The redraw conditions at the current renderer.cpp:799, :926 and :959 keep every non-address trigger they have today (light position, light direction, flash direction, crownShadows). (updateFires drawnPos, render sceneShadows lightDir/crownShadows, flash flashDir/crownShadows all kept)
- [x] renderer.hpp documents the upload-then-render contract on `render()` or `upload()`. (comments on upload() and render())
- [x] Project builds cleanly from a fresh build directory with CMake (fresh build-check, exit 0; no new warnings)

## Assets
None

## Out of scope
- Any other renderer refactor or caching change.
- Detecting a caller passing a scene that wasn't uploaded (caller error, not checked).

## Cycle log
- Cycle 1: 2026-10-26 started, base 868ccd4
- Cycle 1: review passed, no issues; complete. Follow-up noted: pxe::FireGrid keeps const Scene* scene_ (firegrid.hpp:116), out of scope
