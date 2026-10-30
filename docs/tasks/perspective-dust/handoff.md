## Cycle 1
Build: pass (fresh `check` preset build: only stb_image_write's `sprintf` warning)

### Changes
- engine/include/pxe/scene.hpp: `Scene::dustDensity` (0) and `Scene::dustSize` (1), as binding.
- engine/shaders/dust.vert, dust.frag (new), engine/CMakeLists.txt: the binding vertex shader (motes from `gl_VertexID`, wrapped box of 40 m, fade 10..40 m, haze colour pulled toward the sun glow, 1..2 px near, 1 px far) and a fragment shader writing `vColour` to location 0 only; both added to `PXE_SHADERS` after `common.glsl`.
- engine/include/pxe/renderer.hpp: private `kDustMax = 6000`, `progDust_`, `dustVao_`, `dustOffset_`, `dustTime_` with comments; the perspective `render()` contract mentions the dust.
- engine/src/renderer.cpp: `init()` (perspective only) links `progDust_` and generates the empty `dustVao_`; the destructor frees both; `upload()` resets `dustOffset_` and `dustTime_`.
- engine/src/renderer_perspective.cpp: file-local `kDustSpeed = 2.f`; the dust block after the sky and window panes and before the view model, as the spec's sketch (offset advances every frame, no draw at count 0, blend/depth/mask state restored after). Uniforms use the normalised `sunDir` and the shared `sunGlow` the sky already computes, and `overcastSky(skyHorizon, overcast)` for the haze.
- docs/tasks/perspective-dust/dust-a.png, dust-b.png, dust-off.png, scripts/dust-captures.sh: Verification 4's captures and the script that makes them.

### Issues addressed
- none (cycle 1)

### Verification
1. Fresh build (`cmake --preset check --fresh && cmake --build --preset check --clean-first`): only the `sprintf` warning.
2. `tools/ortho-check.sh 010f23d`: 6/6 identical. `tools/perspective-check.sh 010f23d`: 3/3 identical.
3. `git diff 010f23d -- engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/edge.frag engine/src/renderer_orthographic.cpp game desert examples`: no output.
4. Dust-on captures (`pxe_desert --hour 18 --capture`, temporary `world.scene.dustDensity` in `buildWorld`), by `scripts/dust-captures.sh`:
   - dust-a (density 0.4): a-b, a-off, b-off all differ under `cmp`; a and b each rendered twice are `cmp`-identical; dust-off equals the base's `--hour 18 --capture`. `desert/src/world.cpp` restored and `git status` shows nothing under `desert/`.
   - dust-a: 69 pixels differ from dust-off, all single isolated pixels (none with a changed neighbour), all brighter than what they sit on (the pale haze colour over the dark sand and over the dusk sky), scattered across the whole width. 56 of them are above the horizon (y < 250); none are in the rows of the arms (y > 400), so none in front of the arms. The motes are faint at this low-contrast dusk view and are the haze-coloured specks over the sand and sky.
   - dust-b (density 1.0, size 2): 183 differing pixels, denser; 4 of them touch a neighbour (the few 2-pixel motes within 4 m of the eye, which are rare); none in the arms' rows. Motes sit only inside the 40 m box and fade out before 40 m, so none reach the far sky or horizon.
   - The warm glint toward the low sun is subtle: the capture looks along +z-ish toward the palm with the sun off to one side, and the mote's glint also depends on `sunGlow` (0.6 default) times the pow-6 lobe.
5. By reading the code: the dust block is after `drawMeshes`, the backdrop and the indoor window panes, and before the view-model block; it sets `glDepthMask(GL_FALSE)` and `glColorMaski(1, FALSE...)` and restores both; it draws only inside `if (count > 0)`; `dustOffset_` is advanced from `lighting.windDir` and `windStrength` times the clamped frame step on every frame, outside the `count > 0` check.

### Layering
- Engine: `Scene::dustDensity`, `Scene::dustSize`; `dust.vert`/`dust.frag`; `Renderer::kDustMax`, `progDust_`, `dustVao_`, `dustOffset_`, `dustTime_`; the dust pass in `renderer_perspective.cpp`.
- Game: none in this task (`desert-sky` sets the desert's values).
- Misplaced code touched: none

### How to playtest
- Nothing in the game or the desert turns the dust on yet (default 0), so play is unchanged. To see it, temporarily add `world.scene.dustDensity = 1.f;` (and optionally `dustSize = 2.f`) at the end of `buildWorld` in `desert/src/world.cpp`, then `tools/build.sh release desert -- --hour 18`. Walk and sprint: motes should stream past on the wind, never run out, never appear in front of the arms, and fade to nothing by 40 m. `--hour 12` and `--hour 6` show other glints.

### Docs
- docs/architecture.md, "Where things live", perspective row: add the airborne dust (`Scene::dustDensity`, `dustSize`, `dust.vert`/`dust.frag`, drawn after the scene and sky and before the view model, off by default).
- docs/engine-guide.md: document `Scene::dustDensity` (0..1, times 6000 motes) and `Scene::dustSize` (1..2 px for motes nearer than 4 m), perspective mode only, no draw call at 0, drawn depth-tested with no depth or normal writes (so no outlines, but the edge pass's haze still reads the scene depth behind each mote).
- README.md: none.

### Known limitations
- The dust is drawn into the colour target before the edge pass, so the edge pass's distance haze (from the depth behind the mote, which the dust doesn't write) is applied over it as over the surface behind. That is what the spec's order produces.
- Visual look at the desert's default light is subtle; tuning belongs to `desert-sky`.
