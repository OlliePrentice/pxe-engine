# Sky clouds in perspective mode
Type: feature
Status: complete
Cycle: 1

## Goal
Add stylised clouds to the perspective mode's sky, toward `desert/docs/concept/wanderers.png`: two layers of soft streaks, each on its own flat plane at altitude. The streaks are aligned with and drifting on the wind, shaded smoothly from the light (pale bodies, warm edges toward the sun), and thin and fade into the horizon. They draw in the sky behind the scene, cast no shadows, and are off by default. The orthographic mode doesn't change.

## Decisions
- **Builds on `perspective-smooth-lighting`**, whose continuous sky gradient, sun glow, sun disc and exponential haze stay exactly as specced. This task adds only the clouds.
- **Two layers** of streaks, drawn in `backdrop.frag`. Each is stretched noise projected onto a flat plane at its own altitude: a high, thin layer and a lower, fuller one. The streaks run along the wind direction (`Lighting::windDir`). They thin toward the horizon and fade into the sky colour there.
- **No ground shadows.** Ground cloud shadows stay `Weather::cloudCover`, separate from these layers.
- **Drift** comes from a new `Weather::skyCloudOffset` (metres), which the game advances. Each layer moves by it times its own `drift` (defaults: high 1.5, low 1.0), which gives parallax between them. An empty `Weather{}` has offset 0, so captures are deterministic.
- **Shading comes from the light, with no cloud colours of its own.** A cloud takes the sky's horizon colour, slightly lifted, pulled toward the sun's glow colour on the side facing the sun. Thicker parts fall toward a darker underside. One `skyCloudBrightness` scales the result. At night the clouds go dark against the night sky; at golden hour their sunward edges glow orange.
- **Off by default.** Both layers' cover is 0, so a perspective scene that sets nothing draws exactly the `perspective-smooth-lighting` sky.
- **The sun disc and glow sit behind the clouds**: the clouds are drawn over them.
- **No stars or moon disc.**
- **The orthographic mode stays byte-identical.**

## Layering
- Engine:
  - `pxe::SkyCloudLayer` and `Scene::skyClouds[2]`, `Scene::skyCloudBrightness`: the scene's cloud look, as data.
  - `Weather::skyCloudOffset`, copied to `Lighting::skyCloudOffset`: the drift, which the game advances.
  - `backdrop.frag` draws the layers.
- Game: none in this task (`desert-sky` sets the desert's values and advances the offset).

## Approach

### 1. `engine/include/pxe/scene.hpp` and `engine/src/scene.cpp`
Binding, at namespace level before `struct Scene`:
```cpp
// One layer of perspective mode's sky clouds (Scene::skyClouds): soft
// streaks on a flat plane `height` metres above the eye, stretched along the
// wind (Lighting::windDir), drifting by Weather::skyCloudOffset times
// `drift`. Coloured from the light; they cast no shadows. Not drawn under Camera.
struct SkyCloudLayer {
  float height = 8000.f;   // metres above the eye
  float cover = 0.f;       // 0 none .. 1 most of the sky
  float opacity = 0.6f;    // 0..1 where a cloud is thickest
  float length = 6000.f;   // metres a streak runs along the wind
  float width = 900.f;     // metres across it
  float drift = 1.f;       // times Weather::skyCloudOffset
};
```
In `Scene`, after the perspective light fields from `perspective-smooth-lighting`:
```cpp
  // Perspective mode's sky clouds: [0] a high, thin layer, [1] a lower,
  // fuller one. Both have cover 0 (none) unless the game sets it.
  static constexpr int kSkyCloudLayers = 2;
  SkyCloudLayer skyClouds[kSkyCloudLayers];
  float skyCloudBrightness = 1.f;  // scales the clouds' colour
```
`Scene::Scene()` sets layer 1's non-default fields to `height = 2500`, `length = 2500`, `width = 900`, `opacity = 0.7`, `drift = 1.0`, and layer 0's `drift = 1.5`.

### 2. `engine/include/pxe/lighting.hpp` and `engine/src/lighting.cpp`
- Add to `Weather`, after `cloudOffset`: `glm::vec2 skyCloudOffset{0.f};  // metres the sky clouds have drifted (Scene::skyClouds), advanced by the game`.
- Add to `Lighting` beside `cloudOffset`: `glm::vec2 skyCloudOffset{0.f};`.
- The weather overload of `lightingAt` copies it. Nothing orthographic reads it.

### 3. `engine/src/renderer_perspective.cpp`
In the sky block, also give `progBackdrop_`:
- `uEye`;
- `uWindDir` (`lighting.windDir`, normalised; if its length is under 1e-4, use (1, 0));
- per layer, uniform arrays of 2: `uCloudHeight`, `uCloudCover`, `uCloudOpacity`, `uCloudLength`, `uCloudWidth`, and `uCloudOffset` (`lighting.skyCloudOffset * layer.drift`);
- `uCloudBrightness`.

Bind the scalar arrays with `glUniform1fv` and the offsets with `glUniform2fv` (both in `gl.hpp`; check that `glUniform2fv` is used elsewhere, as it is in `renderer.cpp`).

### 4. `engine/shaders/backdrop.frag`
After the glow and the disc, draw layer 0, then layer 1, over the sky. Binding shape:
```glsl
uniform vec3 uEye;
uniform vec2 uWindDir;
uniform float uCloudHeight[2], uCloudCover[2], uCloudOpacity[2], uCloudLength[2], uCloudWidth[2];
uniform vec2 uCloudOffset[2];
uniform float uCloudBrightness;

float cloudHash(vec2 p) { ... as common.glsl's ... }
float cloudNoise(vec2 p) { ... as common.glsl's ... }
// Four octaves, amplitude 0.5 halving, frequency doubling, offset (17.1, 9.7) per octave: about 0.2..0.8.
float cloudFbm(vec2 p);

// One layer over `sky` along view ray `ray` (ray.y > 0 only).
vec3 cloudLayer(vec3 sky, vec3 ray, int i) {
    if (uCloudCover[i] <= 0.0 || ray.y <= 0.0) return sky;
    float t = uCloudHeight[i] / max(ray.y, 0.02);
    vec2 p = uEye.xz + ray.xz * t + uCloudOffset[i];
    vec2 across = vec2(-uWindDir.y, uWindDir.x);
    vec2 q = vec2(dot(p, uWindDir) / uCloudLength[i], dot(p, across) / uCloudWidth[i]);
    float n = cloudFbm(q + float(i) * vec2(31.7, 11.3));
    float edge = mix(0.75, 0.25, uCloudCover[i]);
    float d = smoothstep(edge, edge + 0.15, n) * smoothstep(0.0, 0.12, ray.y);
    if (d <= 0.0) return sky;
    float thick = clamp((n - edge) / 0.3, 0.0, 1.0);
    float toward = pow(max(dot(ray, uSunDir), 0.0), 3.0) * smoothstep(-0.10, 0.05, uSunDir.y);
    vec3 lit = mix(uHorizon * 1.15, uSunColour, toward);
    vec3 under = mix(uHorizon, uZenith, 0.5) * 0.85;
    vec3 cloud = min(mix(lit, under, 0.5 * thick) * uCloudBrightness, vec3(1.0));
    return mix(sky, cloud, d * uCloudOpacity[i]);
}
```
`uHorizon`, `uZenith`, `uSunDir` and `uSunColour` are the uniforms `perspective-smooth-lighting` already binds (overcast-greyed). Update the file's header comment: the clouds, drawn over the glow and disc, and casting no shadows.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: **3/3 identical**, because both layers default to cover 0 and every early return is taken.
3. `git diff <cycle base> -- engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/edge.frag engine/shaders/edge_perspective.frag engine/src/renderer_orthographic.cpp game desert examples`: no output.
4. Clouds on, by a temporary edit. In `examples/minimal/main.cpp`, right after the scene is built for the first-person view, add `scene.skyClouds[0].cover = 0.4f; scene.skyClouds[1].cover = 0.25f;` (use the scene variable's real name). Build and capture `pxe_minimal --first-person --capture clouds-a.png`. Change the covers to `0.7f` and `0.5f`, rebuild, and capture `clouds-b.png`. Then restore the file (`git checkout -- examples/minimal/main.cpp`), rebuild, and capture `clouds-off.png`.
   - The three PNGs are pairwise different (`cmp`).
   - `clouds-off.png` equals the base's `example-first-person` capture.
   - `clouds-a.png` and `clouds-b.png` show soft streaks running one way, thinning to nothing at the horizon, with more cloud in b.

   Keep `clouds-a.png` and `clouds-b.png` in `docs/tasks/perspective-sky-clouds/` and describe them in the handoff. If the example's view shows too little sky to judge, run the same edit with `desert/src/world.cpp`'s scene instead (the desert's capture shows more sky), named the same way.
5. By reading the code:
   - `cloudLayer` returns `sky` unchanged when cover is 0.
   - The clouds are drawn after the disc.
   - `skyCloudOffset` reaches the shader times each layer's `drift`.

## Acceptance criteria
- [x] `SkyCloudLayer`, `Scene::skyClouds[2]` (defaults as §1) and `Scene::skyCloudBrightness` exist as binding. `Weather::skyCloudOffset` and `Lighting::skyCloudOffset` exist and are copied. Evidence: `engine/include/pxe/scene.hpp` (struct, fields), `Scene::Scene()` in `engine/src/scene.cpp`, `lighting.hpp`, `lightingAt(scene, hour, weather)` in `lighting.cpp`.
- [x] `backdrop.frag` draws the two layers with the binding `cloudLayer`, over the glow and disc, coloured from the light, faded at the horizon, aligned with and drifting on the wind. Evidence: `cloudLayer` in `engine/shaders/backdrop.frag`, called twice after the disc in `main`; `clouds-b.png`.
- [x] Verification 2 and 3 pass. Verification 4's three captures are pairwise different, `clouds-off.png` equals the base, and a and b are kept and described. Evidence: orchestrator's ortho-check 6/6 and perspective-check 3/3 identical against 8f2a3a9; diff of the listed paths empty; captures pairwise different per handoff, example-first-person identical to base.
- [x] Project builds cleanly from a fresh build directory with CMake Evidence: orchestrator's `check` preset build, only stb `sprintf` warning.

## Assets
None.

## Out of scope
- Ground shadows from sky clouds; any change to `Weather::cloudCover`'s ground shadows.
- Per-phase cloud colours, stars, moon disc, volumetric clouds.
- The desert's values and menu (task `desert-sky`).
- The orthographic mode.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 8f2a3a9
- Cycle 1 review: build clean, ortho 6/6 and perspective 3/3 identical, diff matches the binding code, no issues. Complete.

## Playtest checklist
- Nothing changes in play: both cloud layers default to cover 0, so `pxe_desert` and `pxe_minimal --first-person` look exactly as before. The clouds become visible to play when `desert-sky` sets the desert's values.
- Optional: set `scene.skyClouds[0].cover = 0.4f; scene.skyClouds[1].cover = 0.25f;` in a first-person scene and look round the sky: streaks run one way, fade to nothing at the horizon, warm on the sun side.
