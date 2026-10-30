# Smooth lighting in perspective mode
Type: feature
Status: complete
Cycle: 2

## Goal
Replace the toon ramp in the perspective render mode with a smooth, stylised lit model, toward the mood of `desert/docs/concept/wanderers.png`: a low warm sun through dusty haze. Surfaces take continuous light falloff: a warm direct sun or moon with soft-edged shadows, a cooler sky fill from above and a bounce from the ground. Distance haze is continuous and exponential, and glows toward the sun. The sky is a continuous gradient with a sun glow and a soft sun disc. The orthographic mode must not change by a single byte.

## Decisions
- **Lit model, not a smoothed ramp.** Each surface and character material has one base colour, its albedo. Light is `albedo × (sun × N·L × shadow × (1 − cloud) + skyFill × up + groundBounce × down)` plus a lightning term, times exposure, clamped. This is what the user described: warm direct sun, cooler sky fill. It also gives a handful of meaningful tunables instead of 3 colours × 4 phases × 12 surfaces.
- **Perspective-mode-wide, no switch.** This becomes the perspective mode's lighting, through a forked mesh shader (`mesh_perspective.frag` with `lit_perspective.glsl`). `mesh.frag`, `common.glsl`, `edge.frag` and the orthographic frame stay untouched (the "Render modes" rule). `pxe_minimal --first-person` changes too, on purpose.
- **Linear light.** Albedo and light colours are authored as sRGB display values, as every engine colour is. They are converted to linear on the CPU, lit in linear, and the mesh shader writes sRGB. The edge pass and the haze mix in sRGB, as today.
- **Albedo** is a new `Scene::albedo[kSurfCount]`, one per surface with no phases, defaulting to each surface's day mid band. The art-style docs already say to choose the base colour as the mid tone, so sand stays sRGB (214, 176, 118). Characters use `ActorMaterial::colour` directly as albedo. The stone-ramp tint (`uActorTint`) isn't used in perspective, so the desert arms' colours will read differently (user accepted). Glowing materials stay unlit at full colour.
- **Light colours vary per phase.** `sunColour`/`sunStrength` (the moon's in the night phase), `skyFill`/`skyFillStrength` and `groundBounce`/`groundBounceStrength` are each `[kPhaseCount]` on `Scene`, blended by sun elevation with the palettes' weights. These are **single values**, not per phase: `hazeDensity`, `sunGlow`, `shadowPenumbra` and `exposure`.
- **Engine defaults** (sRGB colour × linear strength):

  | | day | golden | twilight | night |
  |---|---|---|---|---|
  | sun / moon | (255,244,224) × 1.0 | (255,196,140) × 0.9 | (240,150,140) × 0.35 | (150,170,220) × 0.18 |
  | sky fill | (150,180,230) × 0.30 | (170,170,210) × 0.28 | (140,130,190) × 0.22 | (70,85,130) × 0.12 |
  | ground bounce | (128,128,128) × 0.12 | (128,128,128) × 0.10 | (128,128,128) × 0.05 | (128,128,128) × 0.03 |

  Haze density 0.006 per metre, sun glow 0.6, penumbra 0.3 m, exposure 1.0. (The desert's warm sand bounce is task `desert-lighting`.)
- **Clamp, no tone curve.** An exposure multiplier, default 1, comes before the clamp.
- **No dithering.** Revisit only if banding shows.
- **Soft shadows** with a fixed penumbra width in world metres (`Scene::shadowPenumbra`), filtered with a fixed 16-tap pattern. There is no per-pixel rotation, which would crawl on the low-res target, and no contact hardening. Cloud shadows go continuous, with no 3 steps.
- **Haze** is exponential, `1 − exp(−density × distance)`, and forced to full at the far plane so the end of the ground never shows. Its colour is the horizon colour pulled toward the sun's colour when looking toward the sun, by `sunGlow`.
- **Sky:** a continuous horizon-to-zenith gradient, the same sun glow as the haze, and a small soft-edged sun disc. The moon gets no disc, and there are no stars or clouds.
- **Supported in perspective:** the sun and moon with soft shadows, sky fill and bounce, cloud shadows, the rain overcast (it fades direct light and greys the sky as now), and the lightning flash as a second light with its own shadows. **Dropped in perspective**, and added to `Renderer::render`'s "not yet supported" list: the indoor lighting (window sky light, room fill), dark regions, and firelight on surfaces.
- **No per-surface toon tricks** in perspective. Bark's blotchy noise and the leaves' darker band are gone; every surface is lit by the same rule.
- **The edge pass is unchanged**: outlines, rims, crease ink, bark seam strokes, the view model's ink, and the `uEnabled` switch. Only its haze changes. The mesh shader writes the normal target exactly as `mesh.frag` does, so every edge keeps its surface rules.
- **The orthographic mode stays byte-identical.**

## Layering
- Engine:
  - `pxe::Scene` gains perspective-mode light data: the per-phase sun, fill and bounce colours and strengths, `albedo`, `hazeDensity`, `sunGlow`, `shadowPenumbra` and `exposure`, with engine defaults.
  - `pxe::Lighting` gains the blended linear lights, the sun's direction and display colour, the phase weights, and copies of the single values.
  - The perspective renderer gets its own mesh program and lit shaders, a continuous sky with sun glow and disc, and exponential haze.
  - No game content anywhere: all of it is named for what it does.
- Game: none in this task. The desert keeps the engine defaults until `desert-lighting`. `pxe_minimal --first-person` uses the defaults.

## Approach

### 1. `engine/include/pxe/scene.hpp` and `engine/src/scene.cpp`
Binding declarations, added to `Scene` after `skyZenith`:
```cpp
  // Perspective mode's light (FirstPersonCamera), a smooth lit model rather
  // than the toon ramp. Per phase, blended by sun elevation as the palettes
  // are: the direct light (the sun's; in kPhaseNight the moon's), the sky's
  // fill from above and the light bounced up from the ground, each a display
  // (sRGB) colour and a linear strength. Not used under Camera.
  glm::vec3 sunColour[kPhaseCount]{};
  float sunStrength[kPhaseCount]{};
  glm::vec3 skyFill[kPhaseCount]{};
  float skyFillStrength[kPhaseCount]{};
  glm::vec3 groundBounce[kPhaseCount]{};
  float groundBounceStrength[kPhaseCount]{};
  // Each surface's base colour (albedo, display sRGB) under perspective
  // mode's light: how it looks facing the noon sun. The constructor sets it
  // to each surface's day mid band (palette[kPhaseDay][s].band[1]); a game
  // that changes that band afterwards sets the albedo itself.
  glm::vec3 albedo[kSurfCount]{};
  // Perspective mode's air and exposure, the same at every hour.
  float hazeDensity = 0.006f;   // per metre: haze is 1 - exp(-density * distance), full at the far plane
  float sunGlow = 0.6f;         // 0..1: how far haze and sky toward the sun take its colour
  float shadowPenumbra = 0.3f;  // metres across a soft shadow's edge
  float exposure = 1.f;         // multiplies the light before it is clamped
```
In `Scene::Scene()`, after every palette is set, set the defaults from the Decisions table with the existing `hex` helper (or `glm::vec3{r, g, b} / 255.f`), and loop `albedo[s] = palette[kPhaseDay][s].band[1]`. Update the `skyHorizon`/`skyZenith` comment: "a continuous gradient", not "in flat bands".

### 2. `engine/include/pxe/lighting.hpp` and `engine/src/lighting.cpp`
Binding additions to `Lighting`, after `skyZenith`:
```cpp
  // Perspective mode's light, blended by phase from Scene::sunColour and
  // the rest: linear-light colours times their strengths, ready to multiply
  // a linear albedo. sunLight shines along lightDir (the moon's at night).
  glm::vec3 sunLight{0.f};
  glm::vec3 skyFillLight{0.f};
  glm::vec3 bounceLight{0.f};
  // Toward the sun, whichever of sun and moon lights the scene, and its
  // display colour blended by phase: the sky's sun disc and the glow round
  // it in the sky and the haze.
  glm::vec3 sunDir{0.f, 1.f, 0.f};
  glm::vec3 sunGlowColour{1.f};
  // How much each phase (pxe::Phase) is in effect now; they sum to 1.
  float phaseWeight[kPhaseCount]{};
  // Copied from the Scene.
  float hazeDensity = 0.f;
  float sunGlow = 0.f;
  float shadowPenumbra = 0.f;
  float exposure = 1.f;
```
In `lightingAt(scene, hour)`, after the phase weights `w` are known:
- copy `w` into `phaseWeight`;
- `sunLight += w[ph] * toLinear(scene.sunColour[ph]) * scene.sunStrength[ph]`, and the same for fill and bounce;
- `sunGlowColour += w[ph] * scene.sunColour[ph]` (display values);
- `sunDir = sun`;
- copy the four single values.

`toLinear` is a file-local helper using the exact sRGB curve, per channel: `c <= 0.04045 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4)`. The weather overload changes none of these. The orthographic renderer never reads them, so its output can't change.

### 3. `engine/shaders/lit_perspective.glsl` (new) and `engine/shaders/mesh_perspective.frag` (new)
Add both to `PXE_SHADERS` in `engine/CMakeLists.txt`, in alphabetical order. `mesh_perspective.frag` carries the marker line `// #include "common.glsl"`, which `compose()` (`engine/src/shader.cpp`) replaces with whatever common file `linkProgram` is given. Here that is `lit_perspective.glsl`. Say so in a comment by the marker. `shader.cpp` doesn't change.

`lit_perspective.glsl` is a fork of what the lit model needs from `common.glsl`, copied, not shared:
- the surface constants and `kSurfaceScale`;
- `cloudHash` and `cloudNoise`;
- the shadow and flash-shadow samplers and their matrices;
- the horizon ridge (`ridgeProfile`, `horizonLight`).

It has none of the band, detail, fire, window, dark-region or bark code. Its uniforms:
```glsl
uniform vec3 uSunDir;           // toward the light (lighting.lightDir, normalised)
uniform vec3 uSunLight;         // linear
uniform vec3 uSkyFillLight;     // linear
uniform vec3 uBounceLight;      // linear
uniform float uExposure;
uniform vec3 uAlbedo[kSurfaces];        // linear
uniform mat4 uLightViewProj;
uniform sampler2DShadow uShadowMap;
uniform float uNormalBias;
uniform float uPenumbraUV;      // half the penumbra, in shadow-map UV: shadowPenumbra / 2 / (2 * kShadowReach)
uniform float uPenumbraDepth;   // half the penumbra, metres, in the map's depth units: shadowPenumbra / 2 / depthSpan
uniform vec4 uHorizon; uniform float uHorizonRelief;
uniform float uCloudCover; uniform vec2 uCloudOffset; uniform float uCloudStrength;
uniform float uOvercast; uniform float uRain;
uniform float uFlash; uniform vec3 uFlashDir; uniform mat4 uFlashViewProj; uniform sampler2DShadow uFlashShadowMap;
```

**Soft shadow.** Binding taps and shape:
```glsl
const vec2 kPoisson[16] = vec2[16](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725), vec2(-0.09418410, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464), vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420), vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590), vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790));

// Fraction of the light reaching wpos: 16 hardware-filtered taps over a
// disc the penumbra wide, in a fixed pattern (no per-pixel rotation, which
// crawls on the low-res target). A receiver tilted to the light steps in
// depth across the disc, so each tap's compare depth is pulled toward the
// light by the disc's radius times the tilt's tangent (receiver-plane
// bias), the tangent held to 4.
float softVisible(sampler2DShadow map, mat4 viewProj, vec3 wpos, vec3 n, vec3 L, float biasScale) {
    vec4 ls = viewProj * vec4(wpos + n * uNormalBias * biasScale, 1.0);
    vec3 p = ls.xyz / ls.w * 0.5 + 0.5;
    float c = clamp(dot(n, L), 0.05, 1.0);
    float slope = min(sqrt(1.0 - c * c) / c, 4.0);
    float z = p.z - uPenumbraDepth * slope;
    float v = 0.0;
    for (int i = 0; i < 16; ++i) v += texture(map, vec3(p.xy + kPoisson[i] * uPenumbraUV, z));
    return v / 16.0;
}
```
`sunVisible(wpos, n, biasScale)` = `softVisible(uShadowMap, uLightViewProj, ..., uSunDir, ...)`, faded under rain as `common.glsl`'s `sunLit` fades it: `mix(v, 1.0, smoothstep(0.15, 0.95, uRain))`. `flashVisible` uses the flash map and direction, with no rain fade. The horizon ridge's `step(clears, wpos.y)` becomes `smoothstep(clears - 0.5, clears + 0.5, wpos.y)`, keeping the rain fade.

**Cloud shade**, continuous: the same noise and cover mapping as `common.glsl`'s `cloudShade`, with the three-step line replaced by `shade = smoothstep(t, t + 0.08, n) * uCloudStrength`, then the same overcast combine.

**Light.** Binding formula, in linear:
```glsl
const vec3 kFlashLight = vec3(1.6, 1.75, 2.0);  // a lightning flash at its peak, cold blue-white
vec3 litColour(vec3 albedo, vec3 wpos, vec3 n, float biasScale) {
    float direct = 0.0;
    if (uSunDir.y > 0.0) {
        float ndl = max(dot(n, uSunDir), 0.0);
        if (ndl > 0.0) direct = ndl * sunVisible(wpos, n, biasScale) * horizonLight(wpos);
    }
    float shade = cloudShade(wpos);
    vec3 light = uSunLight * direct * (1.0 - shade)
               + uSkyFillLight * (0.5 + 0.5 * n.y)
               + uBounceLight * (0.5 - 0.5 * n.y);
    if (uFlash > 0.0) {
        float f = max(dot(n, uFlashDir), 0.0);
        if (f > 0.0) light += kFlashLight * uFlash * f * flashVisible(wpos, n, biasScale);
    }
    return albedo * light * uExposure;
}
vec3 toSrgb(vec3 c) {  // the exact sRGB curve, per channel, after clamping to 0..1
    c = clamp(c, 0.0, 1.0);
    return mix(c * 12.92, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), c));
}
```

`mesh_perspective.frag` has the same inputs and outputs as `mesh.frag`, and writes `oNormal` exactly as it does: scene surfaces as `surface + 1`; an actor as `kSurfStone`, or `kSurfActorCrease` when its crease flag is set. Colour:
- `uShowNdl == 1`: as `mesh.frag`.
- Scene surface: `toSrgb(litColour(uAlbedo[vSurface], vWorld, n, flat ? 2.5 : 1.0))`, where `flat` is `common.glsl`'s `flatFaced` rule, copied (built stone takes the bigger bias, as `toonToneFlat` does).
- Actor: its glow flag gives `uActorColour[m]` (sRGB, unlit). Otherwise `toSrgb(litColour(uActorAlbedo[m], vWorld, n, 1.5))`, the 1.5 as `actorTone` uses. Uniforms: `uActorColour[64]` (sRGB, for glow), `uActorAlbedo[64]` (linear) and `uActorFlags[64]`, with the same flag bits as `mesh.frag`.

### 4. `engine/include/pxe/renderer.hpp`, `engine/src/renderer.cpp`, `engine/src/renderer_internal.hpp`, `engine/src/renderer_perspective.cpp`
- `renderer.hpp`:
  - Add a private `unsigned progMeshLit_ = 0;` with the comment "the perspective mode's mesh program (mesh_perspective.frag): the smooth lit model; perspective mode only".
  - Update the perspective `render()` contract comment. Surfaces are lit smoothly: sun or moon with soft shadows, sky fill and ground bounce, continuous haze, and a sky with a sun glow and disc. Add "indoor window and fill light, dark regions, firelight" to the not-yet-supported list.
- `renderer.cpp`:
  - `init()`: in perspective mode only, `progMeshLit_ = linkProgram("mesh.vert", "mesh_perspective.frag", "lit_perspective.glsl")`.
  - `~Renderer()`: delete `progMeshLit_` if set.
  - `bindToon` doesn't change.
- `renderer_internal.hpp`: add `float depthSpan = 1.f;  // metres between the fit's near and far planes` to `LightFit`. `fitLight` (orthographic) leaves it at its default; nothing orthographic reads it.
- `renderer_perspective.cpp`:
  - `fitLightAhead` sets `fit.depthSpan = (back + radius + 10.f) - 1.f`, which is far minus near of its `glm::ortho`.
  - `progMesh_ = progMeshLit_` replaces `toon_[0][0].mesh`.
  - Replace `bindToon(...)` and the actor tint block with a file-local `bindLit(prog, lighting, scene, light, flash, normalBias, testColours)` that sets every uniform in §3:
    - `uPenumbraUV = lighting.shadowPenumbra * 0.5f / (2.f * kShadowReach)` and `uPenumbraDepth = lighting.shadowPenumbra * 0.5f / light.depthSpan`. The flash map shares the fit's size, so the same values serve it.
    - Albedo: every `scene.albedo[s]` and every `params.actorMaterials[i].colour`, converted to linear on the CPU with the same exact curve as §2. Use a file-local `toLinear`; the "no speculative promotion" rule allows the duplicate with `lighting.cpp`.
    - The rest as `bindToon` binds them.
  - `testColours` in perspective: every albedo is white, the sun light pure red (1,0,0), the fill pure green and the bounce pure blue, so each term shows on its own.
  - The sky: give `progBackdrop_` `uSunDir` (`lighting.sunDir`), `uSunColour` (`lighting.sunGlowColour`) and `uSunGlow` (`lighting.sunGlow * (1 - lighting.overcast)`).
  - The edge pass: replace `uHazeSpan` with `uHazeDensity` (`lighting.hazeDensity`) and `uFar`, which it already has. Also pass `uSunDir`, `uSunColour` and `uSunGlow` as for the sky.

### 5. `engine/shaders/backdrop.frag` (perspective only; edit in place)
Remove `kBands`. Binding shape:
```glsl
const float kGlowPower = 8.0;          // how tight the glow round the sun is
const float kDiscRadius = 0.0140;      // radians, about 0.8 degrees: ~7 px tall at a 60 degree view
const float kDiscSoft = 0.0052;        // radians of soft edge, about 0.3 degrees
// The glow toward the sun, 0..1, fading out as the sun sets below the horizon.
float sunGlowAt(vec3 ray) {
    float toward = max(dot(ray, uSunDir), 0.0);
    return clamp(uSunGlow * pow(toward, kGlowPower) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
}
void main() {
    ... ray as now ...
    vec3 sky = mix(uHorizon, uZenith, sqrt(clamp(ray.y, 0.0, 1.0)));
    sky = mix(sky, uSunColour, sunGlowAt(ray));
    float angle = acos(clamp(dot(ray, uSunDir), -1.0, 1.0));
    float disc = (1.0 - smoothstep(kDiscRadius, kDiscRadius + kDiscSoft, angle)) * step(-0.02, uSunDir.y) * (1.0 - uOvercastDisc);
    sky = mix(sky, min(uSunColour * 1.3 + 0.15, vec3(1.0)), disc);
    oColor = vec4(sky, 1.0);
    gl_FragDepth = 1.0;
}
```
`uOvercastDisc` is `lighting.overcast`: rain cloud hides the disc. Rewrite the file's header comment for the continuous sky, glow and disc.

### 6. `engine/shaders/edge_perspective.frag` (perspective only; edit in place)
Replace `distanceHaze`. Binding shape:
```glsl
uniform float uHazeDensity;  // per metre
uniform vec3 uSunDir;        // toward the sun
uniform vec3 uSunColour;     // its display colour
uniform float uSunGlow;      // 0..1, already faded by overcast
const float kGlowPower = 8.0;  // as backdrop.frag
vec3 distanceHaze(ivec2 p, vec3 col) {
    if (texelFetch(uDepth, p, 0).r >= 1.0) return col;
    vec3 ray = rayAt(p);
    float dist = length(ray) * viewDepth(p);
    float t = 1.0 - exp(-uHazeDensity * dist);
    t = max(t, smoothstep(0.85 * uFar, uFar, dist));
    vec3 dir = normalize(ray);
    float glow = clamp(uSunGlow * pow(max(dot(dir, uSunDir), 0.0), kGlowPower) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
    return mix(col, mix(uHaze, uSunColour, glow), t);
}
```
Remove `uHazeSpan`. Update the header's haze paragraph ("continuous and exponential, full at the far plane, glowing toward the sun as the sky does").

### 7. `engine/include/pxe/first_person_camera.hpp`
`setFarPlane`'s comment "Surfaces fade into the sky's horizon colour over the last 40% of it" becomes: "Surfaces fade into the sky's horizon colour with distance (Scene::hazeDensity), fully by the far plane." The class comment's "haze toward the far plane" stays true.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh <cycle base>`: **6/6 identical.**
3. `git diff <cycle base> -- engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/edge.frag engine/src/renderer_orthographic.cpp engine/src/shader.cpp game desert examples`: no output.
4. `tools/perspective-check.sh <cycle base>`: all 3 cases **differ**, which is expected: the perspective look changed. Each case runs and exits 0.
5. `git grep -n 'toon_\[0\]\[0\]' -- engine/src/renderer_perspective.cpp`: no output. `git grep -n uHazeSpan -- engine`: no output. `git grep -n kBands -- engine/shaders/backdrop.frag`: no output.
6. Captures into `docs/tasks/perspective-smooth-lighting/`, each run twice and `cmp`-identical:
   - `pxe_desert --hour 7 --capture d07.png`, `--hour 12 --capture d12.png`, `--capture d1630.png` (default 16.5), `--hour 18.3 --capture d1818.png` and `--hour 21 --capture d21.png`;
   - `pxe_desert --walk 2 --capture walk.png`;
   - `pxe_minimal --first-person --capture example-fp.png`.

   What they should show:
   - No flat bands on the dunes: light falls off continuously across each slope, and lit slopes are warm while slopes turned away are cooler and bluer.
   - The palm's shadow on the sand has a soft edge a few pixels wide, not a 1 px step.
   - The sky is a smooth gradient, brighter toward the sun. In `d1818.png` the sun disc shows if it's in view, and the haze glows on the sunward side.
   - Distance fades smoothly into the horizon colour, from close in.
   - The palm and arms still have their outlines and rims, and the arms' glowing parts are still full colour.

   Describe each capture in the handoff.
7. By reading the code:
   - `mesh_perspective.frag` writes `oNormal` exactly as `mesh.frag` does.
   - `progMeshLit_` is linked in perspective mode only.
   - No orthographic code reads any new `Lighting` field.

## Acceptance criteria
- [x] `Scene` and `Lighting` have the binding fields with the defaults in Decisions. `Scene::albedo` defaults to each surface's day mid band, and `lightingAt` fills the new `Lighting` fields as §2 says. — `scene.hpp`/`scene.cpp` (`Scene::Scene` sets the Decisions table, `albedo[s] = palette[kPhaseDay][s].band[1]`); `lighting.hpp`/`lightingAt` fill `sunLight`, `skyFillLight`, `bounceLight`, `sunDir`, `sunGlowColour`, `phaseWeight` and the single values.
- [x] Perspective mode draws meshes with `mesh_perspective.frag` + `lit_perspective.glsl` implementing the binding lit model, soft shadow (16 fixed taps, receiver-plane bias), continuous cloud shade, the softened horizon ridge, linear lighting with an sRGB write, exposure and clamp. Actors use their colour as albedo, and glow materials stay unlit. — `renderer_perspective.cpp` `progMesh_ = progMeshLit_` with `bindLit`; `lit_perspective.glsl` `litColour`, `softVisible` (16 Poisson taps, receiver-plane bias), continuous `cloudShade`, smoothstep `horizonLight`, `toSrgb`; `mesh_perspective.frag` glow actors unlit.
- [x] `backdrop.frag` draws the continuous gradient, sun glow and soft sun disc as specified. `edge_perspective.frag`'s haze is exponential, full at the far plane, and glows toward the sun. Its edges are otherwise unchanged. — `backdrop.frag` `sunGlowAt` + disc, no `kBands`; `edge_perspective.frag` `distanceHaze` exponential with far-plane ramp and sun glow; edge code otherwise untouched.
- [x] `testColours` in perspective shows the sun, fill and bounce terms as red, green and blue on white. — `bindLit`: with `testColours` albedo white, sun (1,0,0), fill (0,1,0), bounce (0,0,1).
- [x] `Renderer::render`'s perspective contract and `FirstPersonCamera::setFarPlane`'s comment describe the new behaviour, including the dropped indoor, dark-region and firelight lighting. — `renderer.hpp` perspective `render()` comment lists indoor window and fill light, dark regions, firelight as unsupported; `first_person_camera.hpp` `setFarPlane` comment updated.
- [x] Verification 2, 3 and 5 pass. Verification 4's three cases run and differ. Verification 6's captures are in the task folder, deterministic, and described. — Orchestrator: ortho-check 6/6 identical; protected-file diff empty; greps empty; perspective-check 3 cases run and differ; seven captures in the task folder, d1818 re-rendered and cmp-identical; described in handoff.
- [x] Project builds cleanly from a fresh build directory with CMake — Orchestrator fresh `check` build (cycle 2): only the stb_image_write `sprintf` warning.

## Assets
None.

## Out of scope
- Any change to the orthographic mode or the shared shaders `mesh.frag`, `common.glsl` and `edge.frag`.
- The desert's own light values and its menu controls (task `desert-lighting`).
- Dithering, tone curves, contact-hardening shadows, ambient occlusion, a moon disc, stars, clouds in the sky.
- Indoor lighting, dark regions and firelight in perspective mode.
- Textures, or any change to models or materials.

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-30: base 1722db0
- Cycle 1 review, 2026-10-30: build, verification 2-6 pass (orchestrator: ortho 6/6, perspective 3 run and differ, d1818 deterministic); ISSUE-001 (minor, comment).
- Cycle 2, 2026-10-30: base eb84614; outstanding ISSUE-001
- Cycle 2 review, 2026-10-30: ISSUE-001 resolved (comment only; dev ran ortho 6/6 and perspective 3/3 identical vs cycle base); fresh build passes.

## Playtest checklist
- `pxe_desert` at the default hour: dunes shade continuously (no bands), sunlit slopes warm and turned-away slopes cooler; the palm's shadow has a soft edge.
- Turn toward the sun at `--hour 7` and `--hour 18.3`: the sky brightens into a glow round the sun with a small soft disc, and distant sand on that side hazes toward the sun's colour. Nothing like it at `--hour 21` (no moon disc).
- Walk toward the edge: distance fades smoothly into the horizon colour from close in, and the end of the ground never shows.
- The arms keep their outlines and rims; the gauge's LCD and lamp stay full colour. Their colours read differently from before (expected).
- In the settings menu (Tab): Test colours shows the sun term red, fill green and bounce blue on white; cloud cover gives soft-edged cloud shadows; the clock running through dusk and night changes the light smoothly.
- `pxe_minimal --first-person`: the stone, tree and ground are lit smoothly with soft shadows.
- The orthographic game (`pxe`) looks exactly as before.
