## Cycle 1
Build: pass

Cycle base: `5dbaca2` (HEAD~1 before the first commit). Fresh build (`cmake --preset check --fresh && cmake --build --preset check --clean-first`): only the stb_image_write `sprintf` warning.

### Changes
- `engine/include/pxe/first_person_camera.hpp`: new `PerspectiveFrame` (binding fields); `FirstPersonCamera::frame` returns it; includes `<glm/mat4x4.hpp>` and `<glm/vec3.hpp>` instead of `pxe/camera.hpp`; class comment updated.
- `engine/src/first_person_camera.cpp`: `frame` fills only `PerspectiveFrame`'s fields (same computations); `pxe/config.hpp` include dropped.
- `engine/include/pxe/camera.hpp`, `engine/src/camera.cpp`: restored to `main`'s (identical, `git diff main` empty).
- `engine/include/pxe/renderer.hpp`: `enum class RenderMode`, `init(RenderMode)`, `mode()`, `render` overloads for `CameraFrame` and `PerspectiveFrame`, `rayThrough`/`windowPoint` comments say they throw in perspective mode; `draw` removed; `drawPresent(glm::vec2 texelError, ...)`; `mode_`; perspective-only members commented.
- `engine/src/renderer_internal.hpp` (new): `pxe::detail` declarations of `uloc`, `u1i`..`umat`, `drawActors`, `LightFit`, `ramp`, `bindDarkRegions`, `FireUniforms`, `bindToon`, and the constant `kFireShadow`.
- `engine/src/renderer.cpp`: shared machinery only (targets, quads, `init(RenderMode)`, upload, `drawPresent`, passes, `drawMeshes`, readback, the shared helpers' definitions). `init` links `edge_[0..1]` only in orthographic mode, `edgePerspective_` (now `edge_perspective.frag`) and `progBackdrop_` only in perspective mode. `ramp` turned out shared (`bindToon` uses it, and the orthographic grass), so it is in the header; `kFireShadow` is used by `createTargets` and `updateFires`, so it is in the header too. `requireFbo`, `kSkyWindows` and `kDarkRegions` stay in `renderer.cpp`'s anonymous namespace.
- `engine/src/renderer_orthographic.cpp` (new): `rayThrough`, `windowPoint` (mode check added as first statement), `drawSprites`, `updateFires`, `uploadPointLights` (bodies unchanged), `fitLight`, `windSway`, `windRate`, the grass/fire/firefly constants, and `Renderer::render(CameraFrame)` = `main`'s `render` body plus the mode check and `drawPresent(camera.texelError, params)`.
- `engine/src/renderer_perspective.cpp` (new): `fitLightAhead` (takes `PerspectiveFrame`), `kShadowReach`, `overcastSky` and `Renderer::render(PerspectiveFrame)`, written out for perspective as the spec's step 7 orders it.
- `engine/shaders/edge_perspective.frag` (new): cut from the old `edge.frag` with `unifdef -DPERSPECTIVE -UFIRE_GRID`, then god rays, point lights, rain and fire removed (see below), header comment rewritten.
- `engine/shaders/edge.frag`: `main`'s file with `kSurfaces` and `kSurfActorCrease` set to 12 (the only diff against `main`).
- `engine/CMakeLists.txt`: `edge_perspective.frag` in `PXE_SHADERS` (ASCII order, after `edge.vert`); `renderer_orthographic.cpp` and `renderer_perspective.cpp` after `renderer.cpp`.
- `game/src/main.cpp`, `desert/src/main.cpp`: `renderer.init(pxe::RenderMode::kOrthographic)` / `kPerspective`.
- `examples/minimal/main.cpp`: `init` by `firstPerson`; `view` lambda replaced by `renderFrame(params)` (both render calls).
- `docs/tasks/render-mode-split/scripts/`: `split_renderer.py` (cut `renderer.cpp` into the shared and orthographic files by line range), `shaderedit.py` (helpers for pruning the shader), `mode_mismatch.cpp`/`.sh` (throwaway check of the throws), `bench_alt.sh` (alternating `--bench 600`).

### Step 6: switched-off features removed one at a time
Each removal was built and `tools/perspective-check.sh 5dbaca2` run (3/3 PASS) after it, in this order: god rays, point lights, rain, fire. None changed the output, so none had to be put back. All four are gone from `edge_perspective.frag` and from the perspective frame (no `uRays`/`uCanopy`, no point-light buffers or `uploadPointLights`, no rain/cloud/shadow-map uniforms, no `uHeat`/`uFire`, no heat-buffer clear). Removed with them because nothing referenced them any more: `cloudShadow` and its helpers, `hash3`, `valueNoise` (unused already), `pixelOf`, `uOrthoHalf`, `uDepthMetres`, `uSunRain`, `uFlash`, `uTime`, `uCamTexel`, `uYawTexels`, `kSurfaces`, `kSurfWater`, `kSurfLeaf`, `kLeafSprite`. The perspective edge shader stays at the old unifdef'd form's behaviour for `uIndoors`, dark regions, rims, ink, creases, bark strokes and haze.
The perspective checks only run with no rain, so this proves rain/fire/etc. removal for the renderer's actual values (it always set them off); that is what the spec's equivalence rests on.

### Verification
1. Fresh build: pass, only the `sprintf` warning.
2. `tools/ortho-check.sh 5dbaca2`: 6/6 PASS (game-default, game-hills, game-night-foxes, game-night-fires, game-self-test, example-ortho). Also run after every step that built.
3. `tools/perspective-check.sh 5dbaca2`: 3/3 PASS (desert-default, desert-night, example-first-person). Also run after every step.
4. `tools/ortho-check.sh main` (main is `3b5daed`): 6/6 PASS.
5. Structure:
   - `git diff main -- engine/include/pxe/camera.hpp engine/src/camera.cpp`: no output.
   - `git diff main -- engine/shaders/edge.frag`: only the `kSurfaces` and `kSurfActorCrease` lines.
   - The `grep -n -i -E "perspective|PerspectiveFrame|FirstPersonCamera|backdrop|haze|shadowFit|flashFit"` over `renderer_orthographic.cpp`, `edge.frag`, `camera.hpp` is **not empty**, and cannot be with the spec's own instructions. Every hit is text the spec requires: `edge.frag:99,297,476,484,492,493,520` (the word "haze" in `main`'s rain/god-ray comments and variable, `main`'s file kept as is); `renderer_orthographic.cpp:271` (`glm::perspective(90 degrees...)` in `updateFires`' fire-shadow cubes, moved unchanged); `renderer_orthographic.cpp:360` (the binding mismatch message names `kPerspective`). There is no mention of the perspective camera, `PerspectiveFrame`, `FirstPersonCamera`, backdrop, `shadowFit_` or `flashFit_`.
   - `grep -n -E "PERSPECTIVE|FIRE_GRID|CameraFrame" edge_perspective.frag renderer_perspective.cpp`: no output.
   - `grep -n -i perspective engine/src/renderer.cpp`: lines 208 (destructor), 506 and 511 (`init`) only.
   - The `diff` of `Renderer::render` against `main`'s: only the added three-line mode check and `drawPresent(camera.texelError, params)`.
6. Mode mismatch: `docs/tasks/render-mode-split/scripts/mode_mismatch.sh` (throwaway, not in the build) PASSES: orthographic renderer given a `PerspectiveFrame`, perspective renderer given a `CameraFrame`, and perspective `rayThrough`/`windowPoint` all throw `std::invalid_argument` with the spec's exact messages; the ordinary calls draw. Two renderers in different modes also coexist on one context.
7. Performance, `--bench 600`, `ms/frame` wall, five alternating runs, main's first (main binary from `tools/ortho-check.sh main`'s base build, split from `build/release`):

   | run | main | split |
   | - | - | - |
   | 1 | 3.78 | 3.79 |
   | 2 | 3.80 | 3.79 |
   | 3 | 3.76 | 3.81 |
   | 4 | 3.81 | 3.78 |
   | 5 | 3.83 | 3.76 |
   | median | 3.80 | 3.79 |

   Ratio split/main = 0.997, under the 1.02 limit.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: `pxe::RenderMode`, `Renderer::init(RenderMode)`, `Renderer::mode()`, `Renderer::render(const Scene&, const PerspectiveFrame&, ...)` overload, `pxe::PerspectiveFrame` (returned by `FirstPersonCamera::frame`), `renderer_orthographic.cpp`, `renderer_perspective.cpp`, private `renderer_internal.hpp`, shader `edge_perspective.frag`. `CameraFrame` lost `Projection`, `projection`, `fovY` (back to `main`'s).
- Game: `game/src/main.cpp` inits orthographic; `desert/src/main.cpp` perspective; the example by `--first-person`. Nothing else changed.
- Misplaced code touched: none.

### How to playtest
- Run the game (`tools/build.sh release game`): it should look and play exactly as before (rain, fire, god rays, fireflies, water, grass fire, tavern). Right-click ray picking / click-to-move uses `rayThrough`, so check that it still works.
- Run the desert (`tools/build.sh release desert`): walk, look around, check edges, haze and the shadow follow the eye as before.
- Run the example with and without `--first-person`.
- A program that gives a renderer the wrong frame type now gets `std::invalid_argument`; none of ours does.

### Docs
- `docs/engine-guide.md`: start-up order is `renderer.init(pxe::RenderMode::...)`; the first-person section: `PerspectiveFrame`, the mode being fixed at `init`, the throws on a mismatched frame, `rayThrough`/`windowPoint` throw in perspective mode, and the example snippets (`init` by mode, the `renderFrame` lambda) matching `examples/minimal/main.cpp` word for word (the `init` call also has two comment lines above it); header reference for `RenderMode`, `PerspectiveFrame`.
- `docs/architecture.md`, "Where things live": the perspective row names `RenderMode`, `PerspectiveFrame`, `renderer_perspective.cpp`/`edge_perspective.frag` and drops `CameraFrame::projection`; the rendering row can name `renderer_orthographic.cpp`.
- `README.md`: only if it shows `renderer.init()`.

### Known limitations
- Verification 5's third grep cannot give empty output (hits listed above, all spec-mandated text).
- The perspective checks only cover rain 0, but the renderer sets the removed passes off in every perspective frame, so no input can reach the removed code.
- `edge_perspective.frag` keeps the old forward declaration of `worldAt`, now defined near the end; it is unused-forward but harmless.

## Cycle 2
Build: pass

### Changes
- `docs/tasks/render-mode-split/scripts/__pycache__/`: removed from the branch with `git rm -r` (ISSUE-001). No code changes.

### Issues addressed
- ISSUE-001: the committed `shaderedit.cpython-314.pyc` is gone; `.gitignore` untouched.

### Verification
- Fresh build (`cmake --preset check --fresh && cmake --build --preset check --clean-first`): only the stb_image_write `sprintf` warning.
- Revised Verification 5, third bullet:
  - `grep -n -E "PerspectiveFrame|FirstPersonCamera|PERSPECTIVE|edgePerspective|progBackdrop|backdrop|fitLightAhead|shadowFit|flashFit|distanceHaze|uHaze" engine/src/renderer_orthographic.cpp engine/shaders/edge.frag engine/include/pxe/camera.hpp`: no output (exit 1).
  - `grep -n -i perspective` over the same files: `renderer_orthographic.cpp:271` (`glm::perspective` in `updateFires`) and `:360` (the mode-mismatch message). Nothing else.

### Layering
- Engine: none (unchanged from cycle 1).
- Game: none.
- Misplaced code touched: none.

### How to playtest
- Nothing changed since cycle 1.

### Docs
- Same as cycle 1.

### Known limitations
- The cycle 1 limitation about Verification 5's third grep is resolved by Replan 1. The other cycle 1 limitations stand.
