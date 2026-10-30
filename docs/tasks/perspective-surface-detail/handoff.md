## Cycle 1
Build: pass (fresh `check` build: only the stb_image_write `sprintf` warning)

### Changes
- engine/include/pxe/scene.hpp: `SurfaceRipples` (binding, with the stated defaults), `Scene::terminator[kSurfCount]` and `Scene::ripples` after the dust fields.
- engine/shaders/lit_perspective.glsl: `uTerminator[kSurfaces]`; `litColour` takes a `terminator` argument; binding `pow` branch on the direct term (skipped at 0).
- engine/shaders/mesh_perspective.frag: ripple uniforms and the binding `rippled()`; `main()` applies it after `oNormal` is written, to scene surfaces only (`vSurface == uRippleSurface`, after the actor branch has returned); scene surfaces pass `uTerminator[vSurface]`, actors pass 0.0.
- engine/src/renderer_perspective.cpp: `bindLit` takes the eye (new `glm::vec3 eye` parameter, passed `camera.eye`) and binds `uTerminator`, the ripple uniforms (heading to `(sin, cos)`, slope to radians) and `uEye`; zeros / surface -1 when `testColours` is on.
- docs/tasks/perspective-surface-detail/scripts/run-captures.sh: the Verification 4 runs through `tools/captures.sh --edit`.
- docs/tasks/perspective-surface-detail/ripples.png, terminator.png: see below (taken at --hour 17.5, not 18).

### Issues addressed
- none (cycle 1)

### Layering
- Engine: `pxe::SurfaceRipples`, `Scene::ripples`, `Scene::terminator`; perspective lit shaders; `bindLit`'s signature in the engine-private `renderer_perspective.cpp`.
- Game: none.
- Misplaced code touched: none

### Verification results
1. Fresh `check` build: pass.
2. `tools/ortho-check.sh c2c6add`: 6/6 identical. `tools/perspective-check.sh c2c6add`: 3/3 identical.
3. `git diff c2c6add -- mesh.frag common.glsl edge.frag edge_perspective.frag renderer_orthographic.cpp game desert examples`: no output.
4. **Not met as written, at --hour 18** (see Spec infeasible). At --hour 18 all three captures (edit A, edit B, off) are byte-identical to the base's. At --hour 17.5 (script `scripts/run-captures.sh <dir> 17.5`): ripples.png, terminator.png and off.png are pairwise different, and off.png is byte-identical to the base's `pxe_desert --hour 17.5 --capture`.
   - ripples.png (heading 200, ripples on sand): fine stripes on the sand in the near ground, running across the wind and wandering slightly, thinning into plain sand by about 20 m and absent on the shaded steep downwind faces of the far dunes.
   - terminator.png (sand terminator 0.35): the lit ground near the eye is smoothly graded and brighter, the shaded dune faces' edge against the lit sand is tighter and the shadow side stays dark.
5. By reading: `terminator > 0.0` guards the `pow` so 0 is today's code path; `rippled` assigns only the local `n` after `oNormal` is written, so the edge pass sees the geometric normal; the actor branch (which covers characters and the view model, `vSurface >= kActorSurface`) returns before the ripple line and passes terminator 0.

### Spec infeasible
Verification 4 only (the implementation is complete and follows the spec). The spec's capture hour 18 cannot show either feature in `pxe_desert`: `sunDirection` (engine/src/lighting.cpp:51) gives a sun with y = 0 exactly at hour 18 (the sun's sunset hour), so `uSunDir.y > 0.0` is false and the direct term, where the terminator acts, is skipped. The ripples then only move the sky-fill term `0.5 + 0.5 n.y`, a change of about 0.003, below one 8-bit step. Measured: with edit A or edit B at --hour 18, the capture's md5 is 1f916c80... same as the base's. The captures at 12, 15, 16.5, 17 and 17.5 all differ with edit A, so the shaders do work. Alternative (not built): change the verification hour to 17.5 (or any hour with the sun up), which is what ripples.png and terminator.png here use.

### How to playtest
- Desert game: nothing changes by default. To see it, temporarily set `scene.ripples.surface = pxe::kSurfSand;` and `scene.ripples.headingDegrees = 200.f;` or `scene.terminator[pxe::kSurfSand] = 0.35f;` at the end of `buildWorld`, and look at the sand at --hour 16.5 to 17.5. Stripes should crawl no more than the view moves (they don't move with time).

### Docs
- docs/engine-guide.md: `Scene::terminator` and `Scene::ripples` (`SurfaceRipples`) under perspective mode's scene controls; both off by default, perspective only, shading only.
- docs/architecture.md: the perspective lit model row can mention the per-surface terminator and ripples.

### Known limitations
- Verification 4 at --hour 18 as written is not achievable (above).
- The ripple pattern is anchored to world xz, so stripes are stable as the eye moves, but they do crawl on the low-res target by a pixel where they get close to its resolution; the fade at 20 m is the spec's mitigation.
