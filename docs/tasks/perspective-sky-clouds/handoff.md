## Cycle 1
Build: pass

### Changes
- engine/include/pxe/scene.hpp: `SkyCloudLayer` (binding struct), `Scene::kSkyCloudLayers`, `Scene::skyClouds[2]`, `Scene::skyCloudBrightness` (after the perspective air/exposure fields).
- engine/src/scene.cpp: `Scene::Scene()` sets layer 0 `drift = 1.5`, layer 1 `height 2500, length 2500, width 900, opacity 0.7, drift 1.0`. Both covers stay 0.
- engine/include/pxe/lighting.hpp, engine/src/lighting.cpp: `Weather::skyCloudOffset` (after `cloudOffset`), `Lighting::skyCloudOffset`, copied by the weather overload of `lightingAt`.
- engine/src/renderer_perspective.cpp: the sky block binds `uEye`, `uWindDir` (normalised; (1,0) if length < 1e-4), the per-layer arrays `uCloudHeight/Cover/Opacity/Length/Width` (`glUniform1fv`), `uCloudOffset` (= `lighting.skyCloudOffset * layer.drift`, `glUniform2fv`) and `uCloudBrightness`.
- engine/shaders/backdrop.frag: `cloudHash`, `cloudNoise`, `cloudFbm`, `cloudLayer` (binding body); layer 0 then layer 1 drawn after the glow and the disc. Header comment updated.

### Verification
1. Fresh build (`cmake --preset check --fresh && cmake --build --preset check --clean-first`): only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh 8f2a3a9`: 6/6 identical. `tools/perspective-check.sh 8f2a3a9`: 3/3 identical (desert-default, desert-night, example-first-person).
3. `git diff 8f2a3a9 -- mesh.frag common.glsl edge.frag edge_perspective.frag renderer_orthographic.cpp game desert examples`: no output.
4. Temporary edit in `examples/minimal/main.cpp` (`scene.skyClouds[..].cover` set right after `scene.sunSouth`), then restored with `git checkout` and `touch`ed. The three captures are pairwise different under `cmp`; `clouds-off.png` is from the restored tree, and `perspective-check.sh` (run after the restore) reports example-first-person identical to the base.
   - `docs/tasks/perspective-sky-clouds/clouds-a.png` (covers 0.4 / 0.25): sparse pale streaks in the upper half of the sky, all running the same way (left-down to right-up, along the wind direction (1,0) seen in perspective), long and thin overhead, shrinking to fine flecks that fade out into the pale horizon band; clear sky near the horizon line.
   - `docs/tasks/perspective-sky-clouds/clouds-b.png` (covers 0.7 / 0.5): the same streaks, much more of them: most of the upper sky is covered with layered streaks and patchy gaps of blue, still thinning toward the horizon, which stays clear.
   - `clouds-off.png`: the plain smooth-lighting sky, no clouds.
5. Read: `cloudLayer` returns `sky` when `uCloudCover[i] <= 0.0` (the default), so cover 0 changes nothing. `main()` calls `cloudLayer` after the disc `mix`. `uCloudOffset[i]` = `lighting.skyCloudOffset * scene.skyClouds[i].drift`.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: `pxe::SkyCloudLayer`, `Scene::skyClouds[2]`, `Scene::skyCloudBrightness`, `Weather::skyCloudOffset`, `Lighting::skyCloudOffset`, `backdrop.frag` cloud layers.
- Game: none in this task.
- Misplaced code touched: none

### How to playtest
- Nothing sets cover yet (`desert-sky` does), so nothing changes in game. To see the clouds, set `scene.skyClouds[0].cover` / `[1].cover` in a perspective scene, as in verification 4.
- Edge cases: looking straight up (the drift and scale show most), looking toward the sun at golden hour (clouds should warm toward the sun's colour), at night (clouds dark).

### Docs
- docs/engine-guide.md: `Scene::skyClouds[2]` / `SkyCloudLayer` fields, `Scene::skyCloudBrightness` (perspective sky clouds, default off, cover 0), `Weather::skyCloudOffset` (metres, the game advances it; layer drift multiplies it).
- docs/architecture.md: sky clouds belong with the perspective sky in `backdrop.frag`; they cast no shadows (ground shadows remain `Weather::cloudCover`).

### Known limitations
- Near the horizon the fine streaks at grazing angles are a little noisy (aliased) before `smoothstep(0, 0.12, ray.y)` fades them out; this follows the binding shader.
