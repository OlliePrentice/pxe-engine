# Terminator sharpness and surface ripples in perspective mode
Type: feature
Status: complete
Cycle: 2

## Goal
Give the perspective mode's lit model (`perspective-smooth-lighting`) two surface controls a game can set:
- **Terminator sharpness**, per surface. It tightens the change from lit to shadow, so a crest's lit face against its shadowed face reads as a line, while lit faces stay smoothly graded.
- **Ripples** on one chosen surface. A shading-only normal ripple with an asymmetric profile, running across a wind heading, wandering slightly, fading out with distance and absent on steep slopes.

Both are off by default, so nothing changes until a game asks. The orthographic mode doesn't change.

## Decisions
All from the planning interview with the user, 2026-10-30.
- **Terminator:** `Scene::terminator[kSurfCount]`, 0..1, default 0, which is the plain smooth falloff, exactly as now. When it's above 0 the direct term becomes `pow(N·L, 1 − 0.85·terminator)`. That has a steep rise at the terminator, so the edge reads crisp, and a smooth grade across lit faces.
- **Ripples:** `Scene::ripples`, a `pxe::SurfaceRipples` for one surface (`surface` −1 means none, the default). They are shading only: the geometry and the shadows don't change.
  - **Profile:** each wavelength has a gentle side facing into the wind (75% of it) and a steep side facing downwind (25%).
  - **Wander:** the crest lines wander by noise, so they don't read as ruled lines.
  - **Fade:** full to `fadeStart`, gone by `fadeEnd` (metres from the eye), because past about 20 m a 0.25 m ripple would crawl on the low-res target.
  - **Slopes:** absent above `maxSlopeDegrees`, fading over the last 4°, since real slip faces are smooth.
- **Perspective only**, in `mesh_perspective.frag` and `lit_perspective.glsl`. Scene surfaces only, never characters or the view model.
- **Defaults change nothing:** every perspective capture stays byte-identical. The orthographic mode stays byte-identical.

## Layering
- Engine: `pxe::SurfaceRipples`, `Scene::ripples` and `Scene::terminator`, as data, and their shading in the perspective lit shaders. No game content: "ripples" are named for what they do, and the game picks the surface and the heading.
- Game: none in this task (`desert-ground` sets the desert's values).

## Approach

### 1. `engine/include/pxe/scene.hpp` and `engine/src/scene.cpp`
Binding, at namespace level before `struct Scene`:
```cpp
// Ripples on one surface under perspective mode's light (Scene::ripples):
// a shading-only tilt of the normal in an asymmetric wave, a gentle side
// facing into the wind and a steep side facing downwind, the crest lines
// wandering a little. Fades out with distance and on steep slopes. Not
// drawn under Camera.
struct SurfaceRipples {
  int surface = -1;              // the Surface rippled; -1 none
  float wavelength = 0.25f;      // metres crest to crest
  float depth = 0.35f;           // how far the steep side tilts the normal, 0..1 (the gentle side a third of it)
  float headingDegrees = 0.f;    // the wind blows toward this, compass-like: 0 toward +z, 90 toward +x
  float fadeStart = 6.f;         // metres from the eye: full to here
  float fadeEnd = 20.f;          // and gone from here
  float maxSlopeDegrees = 22.f;  // none on ground steeper than this, fading over the last 4 degrees
};
```
In `Scene`, after the dust fields from `perspective-dust`:
```cpp
  // Perspective mode's surface shading. terminator: per surface, 0 (the
  // smooth falloff) .. 1, how sharply light turns to shadow; lit faces stay
  // graded. ripples: on one surface, none by default.
  float terminator[kSurfCount]{};
  SurfaceRipples ripples;
```

### 2. `engine/shaders/lit_perspective.glsl` and `engine/shaders/mesh_perspective.frag`
New uniforms:
- `uniform float uTerminator[kSurfaces];`
- `uniform int uRippleSurface;`
- `uniform float uRippleWavelength, uRippleDepth, uRippleFadeStart, uRippleFadeEnd, uRippleMaxSlope;` (the slope in radians)
- `uniform vec2 uRippleWind;` (the unit heading in xz: `(sin h, cos h)`)
- `uniform vec3 uEye;`

`litColour` takes the surface's terminator (actors pass 0). Binding change to its direct term:
```glsl
float ndl = max(dot(n, uSunDir), 0.0);
if (terminator > 0.0 && ndl > 0.0) ndl = pow(ndl, 1.0 - 0.85 * terminator);
```
With 0 the code path is exactly today's, so captures stay byte-identical.

Ripples, in `mesh_perspective.frag`, for scene surfaces only, before `litColour`. Binding shape:
```glsl
vec3 rippled(vec3 n, vec3 wpos) {
    float dist = length(wpos - uEye);
    float slope = acos(clamp(n.y, -1.0, 1.0));
    float fade = (1.0 - smoothstep(uRippleFadeStart, uRippleFadeEnd, dist))
               * (1.0 - smoothstep(uRippleMaxSlope - radians(4.0), uRippleMaxSlope, slope));
    if (fade <= 0.0) return n;
    vec2 wind = uRippleWind;
    float along = dot(wpos.xz, wind);                      // across the crests, with the wind
    float side = dot(wpos.xz, vec2(wind.y, -wind.x));       // along the crests
    float wander = 0.6 * cloudNoise(vec2(side, along) / (uRippleWavelength * 6.0));
    float u = fract(along / uRippleWavelength + wander);
    // Gentle rise over 0..0.75 (tilting toward the upwind side by a third of
    // the depth), steep fall over 0.75..1 (tilting downwind by the full depth),
    // with short smooth turns between them.
    float steep = smoothstep(0.70, 0.78, u) * (1.0 - smoothstep(0.96, 1.0, u));
    float tilt = uRippleDepth * mix(1.0 / 3.0, -1.0, steep);
    return normalize(n - vec3(wind.x, 0.0, wind.y) * tilt * fade);
}
```
In `main()`, for a scene surface: `if (vSurface == uRippleSurface) n = rippled(n, vWorld);`. Only the colour's normal changes: `oNormal` keeps the geometric normal, so the edge pass sees no ripples.

### 3. `engine/src/renderer_perspective.cpp`
In `bindLit` (from `perspective-smooth-lighting`), bind:
- `uTerminator` from `scene.terminator`, as zeros when `testColours` is on;
- the ripple uniforms from `scene.ripples`, with `uRippleSurface` −1 when `testColours` is on, and the heading converted to `(sin, cos)`, the slope to radians;
- `uEye` (`camera.eye`).

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: **3/3 identical**.
3. `git diff <cycle base> -- engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/edge.frag engine/shaders/edge_perspective.frag engine/src/renderer_orthographic.cpp game desert examples`: no output.
4. On, by temporary edits in `desert/src/world.cpp` at the end of `buildWorld` (the scene variable's real name):
   - **Edit A:** `scene.ripples.surface = pxe::kSurfSand; scene.ripples.headingDegrees = 200.f;` → capture `pxe_desert --hour 17.5 --capture ripples.png`.
   - **Edit B:** replace edit A with `scene.terminator[pxe::kSurfSand] = 0.35f;` → capture `terminator.png` (also `--hour 17.5`).
   - **Restore** with `git checkout -- desert/src/world.cpp` and `touch` it, rebuild, and capture `off.png` (`--hour 17.5`).

   Then:
   - the three are pairwise different (`cmp`);
   - `off.png` equals the base's `pxe_desert --hour 17.5 --capture`;
   - `ripples.png` shows fine stripes on the sand near the eye, running across the wind and wandering slightly, gone by 20 m and absent on the steep downwind faces;
   - `terminator.png` shows tighter light/shadow edges along the crests, with lit slopes still graded.

   Keep `ripples.png` and `terminator.png` in `docs/tasks/perspective-surface-detail/` and describe them in the handoff.
5. By reading the code:
   - with terminator 0 the `pow` branch is skipped;
   - the ripples change only the colour's normal, never `oNormal`;
   - actors and the view model never ripple.

## Acceptance criteria
- [x] `SurfaceRipples`, `Scene::ripples` and `Scene::terminator` exist as binding, with the defaults stated. (`engine/include/pxe/scene.hpp`, word for word)
- [x] The lit shaders implement the binding terminator and ripple shading, scene surfaces only, with `oNormal` unchanged. (`lit_perspective.glsl` litColour; `mesh_perspective.frag` rippled() applied after oNormal is written, actors return earlier with terminator 0)
- [x] Verification 2–5 pass, and the two captures are kept and described. (orchestrator: ortho 6/6, perspective 3/3, V3 diff empty; V4 at 17.5 per revision 1: three captures pairwise different, off.png identical to base, kept PNGs reproduced byte for byte; described in handoff.md)
- [x] Project builds cleanly from a fresh build directory with CMake (orchestrator check build: only the stb sprintf warning)

## Assets
None.

## Out of scope
- Ripples in the geometry, the shadows or the edge pass; more than one rippled surface; ripples that move.
- Orthographic mode.
- The desert's values and menu (`desert-ground`).

## Spec revisions
- Revision 1, 2026-10-30: Verification 4 captures at `--hour 17.5` instead of 18, because at 18 the sun is exactly on the horizon and the direct term (where the terminator acts) is skipped, so the edits can't show; see replan-1.md.

## Cycle log
- Cycle 1, 2026-10-30, base c2c6add
- Cycle 2, 2026-10-30, base de4ff92, after replan 1; no outstanding issues (cycle 1's code stands; verification re-run at 17.5)
- Cycle 2 review: build clean, verification passes at 17.5, no issues. Complete.

## Playtest checklist
Nothing in either game changes yet (`desert-ground` turns these on). To see them in the desert now, add temporarily at the end of `buildWorld` in `desert/src/world.cpp`, then rebuild and run `--hour 17.5`:
- `scene.ripples.surface = pxe::kSurfSand; scene.ripples.headingDegrees = 200.f;`: fine stripes on the near sand running across the wind, wandering slightly, gone by about 20 m and absent on steep slip faces. Walk: they stay fixed on the ground, and shouldn't crawl or shimmer at the fade.
- `scene.terminator[pxe::kSurfSand] = 0.35f;`: tighter light/shadow edges along the crests, lit slopes still graded.
- The arms never ripple; outlines don't pick up the ripples.
- Note: at `--hour 18` the sun is exactly on the horizon, so neither shows (no direct light at all).
