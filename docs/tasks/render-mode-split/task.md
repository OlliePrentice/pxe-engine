# Orthographic and perspective as separate render modes
Type: alignment
Status: open
Cycle: 1

## Goal
The perspective camera (task `perspective-camera`) was added inside the orthographic renderer. `Renderer::draw` is one function with `perspective` branches mixed through it. `edge.frag` serves both cameras through eight `#ifdef PERSPECTIVE` blocks inside shared rules. `CameraFrame` carries fields that mean nothing to one camera or the other. The orthographic output has stayed byte-identical so far, but any future tuning of either camera would have to trade off against the other.

Make them two separate engine render modes, so work on one cannot reach the other:
- a renderer is created in one mode for its whole life;
- each camera has its own frame type;
- each mode has its own frame function in its own source file, and its own edge-pass shader;
- the orthographic code reads as it does on `main` again.

Both modes share only the machinery underneath: targets, mesh upload, mesh drawing and the toon shaders, the shadow-map textures, present. The split is a pure refactor. Both modes' output stays byte-identical, and the orthographic frame time is not measurably worse than `main`'s.

## Decisions
All from the planning interview with the user, 2026-10-29.
- **What "no negative effect" means** (Q1):
  - (a) Output: the orthographic standard cases are byte-identical. A hard requirement.
  - (b) Performance: the game's `--bench` frame time is not measurably worse than `main`'s. Measured once in this task and recorded. A median more than 2% slower than `main`'s is an issue to fix in this task (Q11).
  - (c) Structure: a perspective-only change can't touch code that orthographic rendering runs. A hard requirement.
- **The mode is chosen once, at start-up** (Q2). `Renderer::init(RenderMode)` fixes it for the renderer's life:
  - An orthographic renderer never links perspective programs, and the reverse.
  - A game that wanted both views would need two renderers. No game needs that now; if one does, it is its own task.
- **How deep the split goes** (Q3). These are forked per mode:
  - the frame orchestration: which passes run, in what order, with which uniforms;
  - the screen-space shaders: the edge pass (and the sky backdrop, already perspective-only).

  This machinery stays shared:
  - render targets, upload, `drawMeshes`, and the mesh, toon and `common.glsl` shaders;
  - the shadow-map textures and shader, present, and the window-pane sky.

  If a perspective feature later needs a shared shader to behave differently, that shader is forked for perspective at that point. No mode branch is added to it.
- **The orthographic files go back to `main`'s code** (Q4):
  - `engine/shaders/edge.frag` is `main`'s file plus only the two Sand surface-count constants.
  - `engine/include/pxe/camera.hpp` and `engine/src/camera.cpp` are `main`'s files exactly.
  - The orthographic frame function is `main`'s `Renderer::render` body.
- **Separate frame types** (Q7):
  - `CameraFrame` goes back to `main`'s orthographic-only struct.
  - `FirstPersonCamera::frame` returns a new `PerspectiveFrame` holding only what perspective uses.
  - `Renderer::render` has one overload per frame type.
  - Calling the overload that doesn't match the mode given to `init` throws `std::invalid_argument` before drawing anything. So do `rayThrough` and `windowPoint` in perspective mode.
- **No output changes in either mode** (Q8). `tools/ortho-check.sh` and `tools/perspective-check.sh` (task `render-mode-checks`) pass against the cycle base. `tools/ortho-check.sh main` passes too.
- **Three renderer files** (Q9):
  - `renderer.cpp` keeps the shared machinery;
  - `renderer_orthographic.cpp` holds the orthographic frame;
  - `renderer_perspective.cpp` holds the perspective frame.

  The pass sequence the two frames have in common (shadow pass, colour pass set-up, edge-pass uniforms) is duplicated between the two files, not factored out.
- **Shared scene data stays where it is** (Q10):
  - `Scene::skyHorizon`/`skyZenith` and `Lighting::skyHorizon`/`skyZenith` don't move.
  - `FrameParams` stays one struct. The perspective mode ignores the fields it doesn't support, as it does now.
- **`init` takes the mode with no default.** Every caller names its mode: the game orthographic, the desert game perspective, the example by its `--first-person` flag. A default would hide which mode a program is in.
- **The perspective edge shader drops the passes perspective switches off.** These are god rays, point lights, rain and fire. The renderer sets them all to "off" under perspective today, so removing their code should leave the output unchanged. Any one whose removal changes the perspective output stays in the shader, and the handoff records which and why.

## Layering
- Engine:
  - `pxe::RenderMode` and `Renderer::init(RenderMode)`, `Renderer::mode()`;
  - `Renderer::render` overloads for `CameraFrame` (orthographic) and `PerspectiveFrame` (perspective);
  - `pxe::PerspectiveFrame`, returned by `FirstPersonCamera::frame`;
  - the renderer split into `renderer.cpp`, `renderer_orthographic.cpp` and `renderer_perspective.cpp`, with the private helper header `renderer_internal.hpp`;
  - the new shader `engine/shaders/edge_perspective.frag`.

  All are named for what they do. No game content.
- Game (`game/`): `game/src/main.cpp` calls `renderer.init(pxe::RenderMode::kOrthographic)`. Nothing else changes.
- Desert (`desert/`): `desert/src/main.cpp` calls `renderer.init(pxe::RenderMode::kPerspective)`. Nothing else changes, since it already uses only `FirstPersonCamera::frame`.
- Example: `examples/minimal/main.cpp` picks the mode from `--first-person` at `init`, and draws through a lambda that calls the overload for its camera.
- Docs (the run updates these after the merge, as usual):
  - `docs/engine-guide.md`:
    - start-up order: `init(RenderMode)`;
    - the first-person section: `PerspectiveFrame`, the mode, and the snippets matching the example word for word;
    - the header reference.
  - `docs/architecture.md`, "Where things live": the perspective row names `RenderMode`, `PerspectiveFrame` and the two frame files, and drops `CameraFrame::projection`.

## Approach
Work in this order. Run `tools/perspective-check.sh <cycle base>` and `tools/ortho-check.sh <cycle base>` after each numbered step that builds, not only at the end, so a step that changes output is found while it is small.

### 1. `PerspectiveFrame` and `camera.hpp` restored
- `engine/include/pxe/first_person_camera.hpp`: declare `PerspectiveFrame` above `FirstPersonCamera` (binding):
  ```cpp
  // One perspective camera update, from FirstPersonCamera, for a renderer in
  // RenderMode::kPerspective. The eye is not snapped to the texel grid.
  // Basis as FirstPersonCamera's: forward along the view, right and up
  // across it; a positive pitch looks down.
  struct PerspectiveFrame {
    glm::mat4 view{1.f};
    glm::mat4 proj{1.f};
    glm::vec3 eye{0.f};
    glm::vec3 forward{0.f, 0.f, 1.f};
    glm::vec3 right{-1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 1.f, 0.f};
    float yawRadians = 0.f;
    float pitchRadians = 0.f;
    float fovY = 0.f;        // vertical field of view, radians
    float nearPlane = 0.1f;  // metres
    float farPlane = 300.f;  // metres
  };
  ```
  - `FirstPersonCamera::frame(int rtW, int rtH) const` returns `PerspectiveFrame`.
  - The header includes `<glm/mat4x4.hpp>` and `<glm/vec3.hpp>`, not `pxe/camera.hpp`.
  - Update the class comment: it produces a `PerspectiveFrame` for a renderer in perspective mode, rather than "a perspective CameraFrame".
- `engine/src/first_person_camera.cpp`: `frame` fills only `PerspectiveFrame`'s fields, with the same computations as today: the basis, the view matrix rows and `glm::perspective`. Drop `target`, `texelWorld`, `pixelsPerUnit` and `texelError`, and the `pxe/config.hpp` include if nothing else uses it.
- Restore `main`'s versions: `git show main:engine/include/pxe/camera.hpp > engine/include/pxe/camera.hpp` and `git show main:engine/src/camera.cpp > engine/src/camera.cpp`. This removes `CameraFrame::Projection`, `projection` and `fovY`.

### 2. `RenderMode` and the `Renderer` interface
`engine/include/pxe/renderer.hpp`:
- Include `pxe/first_person_camera.hpp` for `PerspectiveFrame`.
- Above `class Renderer` (binding):
  ```cpp
  // Which camera a renderer draws through, fixed by init() for the renderer's
  // life. Each mode has its own frame and edge pass; they share the scene,
  // mesh, shadow-map and present machinery. Work on one mode never changes
  // the other's output (docs/architecture.md, "Render modes").
  enum class RenderMode { kOrthographic, kPerspective };
  ```
- Public interface (binding signatures; write the comments in the header's style):
  ```cpp
  void init(RenderMode mode);
  RenderMode mode() const { return mode_; }
  ...
  // Orthographic mode only: throws std::invalid_argument in perspective mode.
  glm::vec3 rayThrough(const CameraFrame& camera, int fbW, int fbH, glm::vec2 px) const;
  glm::vec2 windowPoint(const CameraFrame& camera, int fbW, int fbH, glm::vec3 world) const;
  // Draw a frame through Camera, in orthographic mode. <today's render()
  // comment, minus its perspective paragraph>. Throws
  // std::invalid_argument, before drawing anything, in perspective mode.
  void render(const Scene& scene, const CameraFrame& camera, const FrameParams& params);
  // Draw a frame through FirstPersonCamera, in perspective mode. <the
  // perspective paragraph from today's render() comment: sky, haze, the
  // 60 m shadow reach, and the list of what is not supported and skipped
  // whatever params says>. The same contract on `scene` and
  // params.present as the orthographic render. Throws
  // std::invalid_argument, before drawing anything, in orthographic mode.
  void render(const Scene& scene, const PerspectiveFrame& camera, const FrameParams& params);
  ```
- Private:
  - remove `draw`;
  - change `drawPresent` to `void drawPresent(glm::vec2 texelError, const FrameParams& params);`;
  - add `RenderMode mode_ = RenderMode::kOrthographic;`;
  - comment `edgePerspective_`, `progBackdrop_`, `shadowFit_` and `flashFit_` as perspective mode only.

  Everything else stays as it is.
- The mode-mismatch messages (binding wording):
  - `"Renderer::render: a CameraFrame needs RenderMode::kOrthographic, but init() chose kPerspective"`;
  - `"Renderer::render: a PerspectiveFrame needs RenderMode::kPerspective, but init() chose kOrthographic"`;
  - for `rayThrough` and `windowPoint`, `"Renderer::rayThrough: orthographic mode only"` and the same for `windowPoint`.

### 3. The private helper header
`engine/src/renderer_internal.hpp` (new, private like `shader.hpp` and `gl.hpp`) declares, in `namespace pxe::detail`, the helpers and constants from `renderer.cpp`'s anonymous namespace that more than one of the three renderer files uses. That will be at least:
- `uloc`, `u1i`, `u1f`, `u2f`, `u3f`, `u3fv`, `umat`;
- `drawActors`, `LightFit`, `bindToon`, `FireUniforms`, `bindDarkRegions`.

Small constants and `struct`s can be defined in the header (`inline constexpr`). Functions are declared there and defined once in `renderer.cpp`. A helper only one file uses moves into that file's own anonymous namespace instead:
- `fitLight`, `windSway`, `windRate`, `kFireflyReach` and `ramp` go to the orthographic file, if only it uses them;
- `fitLightAhead`, `kShadowReach` and `overcastSky` go to the perspective file.

Let the compiler settle which is which. Each of the three `.cpp` files says `using namespace detail;` once, inside `namespace pxe`, so the moved code compiles with its text unchanged (binding: this is what keeps the orthographic body identical to `main`'s).

### 4. `renderer.cpp`: shared machinery
`renderer.cpp` keeps:
- the destructor, `destroyTargets`, `makeColor`, `makeDepth`, `createTargets`, `createQuads`;
- `init`, `upload`, `uploadGrass`, `drawPresent`, `PassTimes::name`, `beginPass`, `endPass`, `drawMeshes`;
- `readPresented`, `readView`;
- the shared helpers' definitions.

`init(RenderMode mode)`:
- sets `mode_` first;
- links every program it links today, except that `edge_[0]` and `edge_[1]` are linked only in orthographic mode, and `edgePerspective_` and `progBackdrop_` only in perspective mode;
- `edgePerspective_` is now `linkProgram("edge.vert", "edge_perspective.frag")`.

The destructor keeps deleting every program that is non-zero.

`drawPresent` takes `texelError` and uses it where it used `camera.texelError`. Nothing else in its body changes.

### 5. `renderer_orthographic.cpp`: the orthographic frame
Move these here unchanged (bodies byte-for-byte as they are now):
- `rayThrough`, `windowPoint`, `updateFires`, `uploadPointLights`, `drawSprites`;
- the orthographic-only helpers from step 3.

Add the mode check to `rayThrough` and `windowPoint` as their first statement.

`Renderer::render(const Scene&, const CameraFrame&, const FrameParams&)`:
- its first statement is the mode check;
- the rest is `main`'s `Renderer::render` body, copied from `git show main:engine/src/renderer.cpp`, with exactly one line changed: `drawPresent(camera, params)` becomes `drawPresent(camera.texelError, params)`.

`main`'s body has no `perspective`, `fitLightAhead`, `shadowFit_`, `flashFit_`, backdrop or haze code. Don't carry any of it over.

### 6. `edge_perspective.frag` from today's `edge.frag`, then `edge.frag` restored
Do this before restoring `edge.frag`, since the perspective shader is cut from today's file.
1. `unifdef -DPERSPECTIVE -UFIRE_GRID engine/shaders/edge.frag > engine/shaders/edge_perspective.frag`. `unifdef` exits 1 when it changed something; that's expected. This keeps every `PERSPECTIVE` branch and drops the orthographic and grass-fire ones.
2. Remove the switched-off features from `edge_perspective.frag`, one at a time, in this order:
   - god rays (`godRays`, `canopyGap` and the cloud helpers only it uses);
   - point lights (`pointLights`);
   - rain (`rain`, `splash`, `streaks`);
   - fire (`fire`).

   For each one:
   - remove its call from `main()`;
   - remove every function, constant and uniform no longer referenced in the file;
   - remove the matching uniform and texture set-up from the perspective frame (step 7);
   - build, and run `tools/perspective-check.sh <cycle base>`.

   If the check fails, put that feature back, shader and renderer both, and record it in the handoff with the failing case. Keep `hash3` and `valueNoise` if anything left still uses them.
3. Rewrite the header comment of `edge_perspective.frag`. It describes:
   - the edge pass under a perspective camera (`RenderMode::kPerspective`): outlines, rims, creases and bark strokes from the perspective depth buffer, with depth-step tolerances growing with distance and slope;
   - the dark regions;
   - the distance haze.

   It is a fork of `edge.frag`, kept separate so neither mode's tuning changes the other. Changes to the shared rules (rim, ink, crease) are made in each file separately, on purpose. Drop the "Built with PERSPECTIVE" paragraph.
4. Restore the orthographic shader: `git show main:engine/shaders/edge.frag > engine/shaders/edge.frag`. Then make only these two changes, with the same comments as today's file:
   - `const int kSurfaces = 11;` becomes `12`;
   - `const int kSurfActorCrease = 11;` becomes `12`.
5. `engine/CMakeLists.txt`:
   - add `edge_perspective.frag` to `PXE_SHADERS`, in alphabetical order;
   - add `src/renderer_orthographic.cpp` and `src/renderer_perspective.cpp` after `src/renderer.cpp` in `add_library(pxe_engine ...)`.

### 7. `renderer_perspective.cpp`: the perspective frame
Holds `fitLightAhead` (taking `const PerspectiveFrame&`), `kShadowReach`, `overcastSky` and:

`Renderer::render(const Scene&, const PerspectiveFrame&, const FrameParams&)`. Its first statements are the mode check, then today's `params.present` framebuffer check, with the same message. Then it draws what today's `draw` draws with `perspective` true and the unsupported `FrameParams` already cut. Write that out directly, rather than cutting a copy of `params`. In order (binding):
1. `Lighting lighting = params.lighting`. Indoors, `cloudStrength` and `rays` are zeroed as today.
2. `light` and `flash` from `fitLightAhead`. Also `normalBias` and `viewProj`.
3. **The shadow pass,** as today's, with the grass-fire crown branch removed (`progShadow_` always). Keep:
   - the `shadowFit_` and `flashFit_` comparisons;
   - the actors' static-map copy;
   - the lightning map.

   These comparisons now exist only in this file.
4. **No** wind phase, no `updateFires`, no `grid_` calls. The programs are `progEdge_ = edgePerspective_` and `progMesh_ = toon_[0][0].mesh`. Use an empty `FireUniforms`. A perspective renderer never runs `updateFires`, so its `fireLights_` is always empty and today's `toon_[0][fireLights_.size() > 1]` is `toon_[0][0]`.
5. **The colour pass set-up** as today:
   - `fbo_` cleared;
   - shadow, flash and fire-cube textures bound on units 1, 6 and 7;
   - `progMesh_` uniforms, `bindToon`, and the actor tint and material block.

   No grass uniforms, and no `bindBurn`.
6. Every `beginPass`/`endPass` pair in today's order (`kReflection`, `kMeshes`, `kWater`, `kSprites`, `kEdge`, `kPresent`), with `kReflection`, `kWater` and `kSprites` left empty. With profiling on, the readback at the end reads every query, so every pair must run.
7. **`kMeshes`:** `drawMeshes`, then the backdrop block as today, then the indoor window-pane block as today.
8. **The heat buffer and fire particles:** none, unless step 6 kept fire in the shader. In that case keep the heat-buffer clear and set `uFire` to 0.
9. **The edge pass** into `fboFinal_`: colour, normal and depth textures on units 0 to 2. Set every uniform `edge_perspective.frag` still declares, with the values today's `draw` gives it under perspective. Where today's code reads `camera.texelWorld`, use `1.f / RenderConfig::kPixelsPerUnit`, which is what `FirstPersonCamera` put there.
10. `if (params.present) drawPresent(glm::vec2{0.f}, params);`, then the profiling readback and `pxeGlCheck("render")`, as today.

### 8. Callers
- `game/src/main.cpp`: `renderer.init()` becomes `renderer.init(pxe::RenderMode::kOrthographic)`.
- `desert/src/main.cpp`: `renderer.init()` becomes `renderer.init(pxe::RenderMode::kPerspective)`.
- `examples/minimal/main.cpp`:
  - `renderer.init(firstPerson ? pxe::RenderMode::kPerspective : pxe::RenderMode::kOrthographic);`.
  - Replace the `view` lambda, which can no longer return one type for both cameras (sketch, keep the example's comment style):
    ```cpp
    // Draw a frame through whichever camera this run looks through: each
    // has its own frame type, for the renderer's mode.
    auto renderFrame = [&](const pxe::FrameParams& params) {
      if (firstPerson) {
        renderer.render(world.scene, eyeCamera.frame(renderer.rtW(), renderer.rtH()), params);
      } else {
        renderer.render(world.scene, camera.frame(renderer.rtW(), renderer.rtH()), params);
      }
    };
    ```
    Both `renderer.render(world.scene, view(), params)` calls become `renderFrame(params)`.
- Grep for any other `renderer.init(` or `.init()` on a `pxe::Renderer`, and for `CameraFrame::Projection`, `.projection` or `fovY` on a `CameraFrame`, in `engine/`, `game/`, `desert/` and `examples/`. Fix every one.

### 9. Performance
On the finished build, in release:
1. `tools/ortho-check.sh main`. It must pass (see Verification), and it prints the base directory, `<main-base>`.
2. Alternate 5 runs each:
   - `<main-base>/build/release/game/pxe --bench 600`;
   - `build/release/game/pxe --bench 600`.

   Start with `main`'s, and don't run anything else heavy meanwhile.
3. In the handoff, record each run's ms per frame, and each side's median.

The split passes if its median is at most 1.02 × `main`'s. If it doesn't, the reviewer opens a `major` issue.

## Verification
Run from the task's worktree.
1. Fresh build, `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning.
2. `tools/ortho-check.sh <cycle base>`: every case `PASS`.
3. `tools/perspective-check.sh <cycle base>`: every case `PASS`.
4. `tools/ortho-check.sh main`: every case `PASS`. The orthographic output is identical to `main`'s, which is the user's original concern.
5. Structure, each to give exactly what is stated:
   - `git diff main -- engine/include/pxe/camera.hpp engine/src/camera.cpp`: no output.
   - `git diff main -- engine/shaders/edge.frag`: only the `kSurfaces` and `kSurfActorCrease` lines change.
   - `grep -n -E "PerspectiveFrame|FirstPersonCamera|PERSPECTIVE|edgePerspective|progBackdrop|backdrop|fitLightAhead|shadowFit|flashFit|distanceHaze|uHaze" engine/src/renderer_orthographic.cpp engine/shaders/edge.frag engine/include/pxe/camera.hpp`: no output. `grep -n -i perspective` over the same files: only `glm::perspective` in `updateFires` and the mode-mismatch message.
   - `grep -n -E "PERSPECTIVE|FIRE_GRID|CameraFrame" engine/shaders/edge_perspective.frag engine/src/renderer_perspective.cpp`: no output.
   - `grep -n -i perspective engine/src/renderer.cpp`: only lines in `init` and the destructor.
   - `diff <(git show main:engine/src/renderer.cpp | sed -n '/^void Renderer::render(/,/^}/p') <(sed -n '/^void Renderer::render(/,/^}/p' engine/src/renderer_orthographic.cpp)`: only the added mode check and the `drawPresent(camera.texelError, params)` line.
6. Mode mismatch: a renderer in each mode, given the other mode's frame, throws `std::invalid_argument` with the message in step 2. There is no test harness, so the reviewer checks this by reading the two `render` overloads, `rayThrough` and `windowPoint`. The developer may confirm it with a throwaway program in the task's `scripts/`, which is not committed to the engine.
7. Performance numbers recorded as in step 9 of the Approach, with the split's median at most 1.02 × `main`'s.

## Acceptance criteria
- [ ] `pxe::RenderMode { kOrthographic, kPerspective }` exists. `Renderer::init(RenderMode)` takes it with no default, and `Renderer::mode()` returns it
- [ ] `Renderer::render` has a `CameraFrame` overload and a `PerspectiveFrame` overload. Each throws `std::invalid_argument` with the spec's message, before drawing, when the renderer is in the other mode. `rayThrough` and `windowPoint` throw in perspective mode
- [ ] An orthographic renderer links neither `edge_perspective.frag` nor `backdrop.frag`, and a perspective renderer doesn't link the `edge.frag` programs
- [ ] `PerspectiveFrame` is declared in `first_person_camera.hpp` with exactly the fields in step 1, and `FirstPersonCamera::frame` returns it
- [ ] `camera.hpp` and `camera.cpp` are identical to `main`'s
- [ ] `edge.frag` differs from `main`'s only in `kSurfaces` and `kSurfActorCrease`
- [ ] `edge_perspective.frag` exists and is in `PXE_SHADERS`. It has no `#ifdef`s for `PERSPECTIVE` or `FIRE_GRID`, and none of the god-ray, point-light, rain or fire code, except a feature the handoff records as kept because removing it changed the output
- [ ] The orthographic frame is in `renderer_orthographic.cpp`, its body equal to `main`'s `Renderer::render` but for the mode check and the `drawPresent` argument (Verification 5's `diff`). The perspective frame is in `renderer_perspective.cpp`. `renderer.cpp` names perspective only in `init` and the destructor
- [ ] Helpers shared between the renderer files are in `engine/src/renderer_internal.hpp`, in `pxe::detail`
- [ ] `game/src/main.cpp` inits orthographic, `desert/src/main.cpp` perspective, and the example by `--first-person`
- [ ] `tools/ortho-check.sh <cycle base>`, `tools/perspective-check.sh <cycle base>` and `tools/ortho-check.sh main` all pass
- [ ] The handoff records five `--bench 600` runs per side and their medians. The split's median is at most 1.02 × `main`'s
- [ ] The engine names no game content and includes no game headers
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Any visual change in either mode, including perspective edge crawl, and porting grass, water, rain, fire or point lights to perspective
- Moving `Scene::skyHorizon`/`skyZenith` or `Lighting`'s sky colours, or splitting `FrameParams`
- Forking `mesh.frag`, `common.glsl`, the shadow shader or any other shared shader
- One renderer drawing both modes, or switching mode after `init`
- Trimming the shared programs a perspective renderer links but doesn't use (toon variants for the grass fire, fire programs)
- `RenderConfig` (target size, pixels per unit), which stays shared

## Spec revisions
- Revision 1, 2026-10-29: Verification 5's third grep narrowed to the perspective mode's own identifiers, because `main`'s `edge.frag` ("haze" in its rain code), `updateFires`' `glm::perspective` and the binding mismatch message all match the old one. See replan-1.md.

## Cycle log
- Cycle 1: 2026-10-29 started, base 5dbaca2
- Cycle 1 review: fresh build clean (stb warning only); orchestrator reran ortho-check (6/6 vs 5dbaca2 and vs main 3b5daed) and perspective-check (3/3); structure checks 5.1, 5.2, 5.4-5.6 as specified; bench median 3.79 vs main's 3.80 ms. Replan 1 (Verification 5.3 grep). ISSUE-001 open.
