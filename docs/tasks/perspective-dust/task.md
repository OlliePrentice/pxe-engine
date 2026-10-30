# Airborne dust in perspective mode
Type: feature
Status: queued
Cycle: 0

## Goal
Add drifting airborne dust to the perspective mode, toward `desert/docs/concept/wanderers.png`. Thousands of tiny half-transparent motes fill a box round the eye and drift on the wind. They're depth-tested against the scene, fade with distance, and are coloured from the haze, glinting warm toward the sun. The dust is off by default. The orthographic mode doesn't change.

## Decisions
- **A new engine capability, perspective mode only.** It is stateless motes generated in a vertex shader from their index, so the engine keeps no particle state and holds no pointer to game data. The only state is a wind offset the renderer accumulates, like the orthographic grass's wind phase, as engine machinery.
- **The box** reaches 40 m from the eye each way, 80 m across, and wraps round the eye so the motes never run out. Motes fade from full at 10 m to nothing at 40 m.
- **Density** is `Scene::dustDensity`, 0..1, times `kDustMax = 6000` motes. At 0 nothing is drawn, with no draw call.
- **Size** is `Scene::dustSize`, 1..2 pixels for motes nearer than 4 m; farther motes are always 1 pixel.
- **Motion:** each mote drifts at 2 m/s × `Lighting::windStrength` along `Lighting::windDir`, plus a small per-mote wobble.
- **Look:** half-transparent (alpha 0.5 × the distance fade), coloured from the haze colour (the sky's horizon colour, overcast-greyed) and pulled toward the sun's glow colour when it lies toward the sun, so the motes glint at golden hour.
- **Drawn after the scene and sky, before the view model**, so the arms stay in front. They are depth-tested against the dunes and the palm, write no depth and leave the normal target alone, so they take no outlines.
- **Off by default** (`dustDensity` 0), so every existing perspective capture stays byte-identical.
- **The orthographic mode stays byte-identical.**

## Layering
- Engine:
  - `Scene::dustDensity`, `Scene::dustSize`: the scene's dust, as data.
  - `dust.vert` and `dust.frag`, and a dust pass in the perspective frame with its own program and empty VAO.
  - `kDustMax` in the renderer.
  - No game content.
- Game: none in this task (`desert-sky` sets the desert's values).

## Approach

### 1. `engine/include/pxe/scene.hpp`
In `Scene`, after the sky cloud fields from `perspective-sky-clouds` (binding):
```cpp
  // Perspective mode's airborne dust: tiny half-transparent motes drifting
  // on the wind round the eye, out to 40 m, coloured from the haze and
  // glinting toward the sun. Not drawn under Camera.
  float dustDensity = 0.f;  // 0 none .. 1 the most (Renderer's kDustMax motes)
  float dustSize = 1.f;     // pixels for motes nearer than 4 m, 1..2; farther ones are 1
```

### 2. `engine/shaders/dust.vert` and `engine/shaders/dust.frag` (new)
Add both to `PXE_SHADERS` in `engine/CMakeLists.txt`, in alphabetical order. They have no common file.

`dust.vert`, binding shape:
```glsl
#version 410 core
uniform mat4 uViewProj;
uniform vec3 uEye;
uniform vec3 uOffset;     // metres the wind has carried the dust
uniform float uTime;      // FrameParams::time
uniform float uSize;      // Scene::dustSize
uniform vec3 uHaze;       // the haze colour (as the edge pass's uHaze)
uniform vec3 uSunDir;     // toward the sun
uniform vec3 uSunColour;  // its glow colour
uniform float uSunGlow;   // as the sky's, already faded by overcast
out vec4 vColour;

const float kReach = 40.0;    // the box reaches this far from the eye each way
const float kFadeFrom = 10.0; // full until here, nothing at kReach
const float kNear = 4.0;      // nearer than this, a mote is uSize pixels

float hash(uint n) {  // integer hash to [0, 1)
    n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4; n *= 0x27d4eb2du; n ^= n >> 15;
    return float(n & 0xffffffu) / 16777216.0;
}

void main() {
    uint i = uint(gl_VertexID);
    vec3 seed = vec3(hash(3u * i), hash(3u * i + 1u), hash(3u * i + 2u));
    float span = 2.0 * kReach;
    vec3 wobble = vec3(sin(uTime * 0.7 + seed.x * 6.2832) * 0.3,
                       sin(uTime * 0.5 + seed.y * 6.2832) * 0.15,
                       cos(uTime * 0.6 + seed.z * 6.2832) * 0.3);
    vec3 p = seed * span + uOffset + wobble;
    vec3 rel = mod(p - uEye + kReach, span) - kReach;   // the copy nearest the eye
    float dist = length(rel);
    float fade = 1.0 - smoothstep(kFadeFrom, kReach, dist);
    vec3 dir = rel / max(dist, 1e-3);
    float glint = clamp(uSunGlow * pow(max(dot(dir, uSunDir), 0.0), 6.0) * smoothstep(-0.10, 0.05, uSunDir.y), 0.0, 1.0);
    vColour = vec4(min(mix(uHaze, uSunColour, glint) * 1.15, vec3(1.0)), 0.5 * fade);
    gl_PointSize = dist < kNear ? floor(uSize + 0.5) : 1.0;
    gl_Position = uViewProj * vec4(uEye + rel, 1.0);
    if (fade <= 0.0) gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // outside the clip volume
}
```
`dust.frag` writes `vColour` to `layout(location = 0)` and nothing to location 1.

### 3. `engine/include/pxe/renderer.hpp`, `engine/src/renderer.cpp`
- `renderer.hpp`:
  - private members `unsigned progDust_ = 0; unsigned dustVao_ = 0; glm::vec3 dustOffset_{0.f}; float dustTime_ = -1.f;` and `static constexpr int kDustMax = 6000;`, with comments (perspective mode only; the offset is the wind's accumulated carry, so a change of wind never makes the dust jump);
  - the perspective `render()` contract adds the dust (`Scene::dustDensity`).
- `renderer.cpp`:
  - `init()`, perspective mode only: `progDust_ = linkProgram("dust.vert", "dust.frag")`, and `glGenVertexArrays(1, &dustVao_)`. The VAO is empty: the shader reads only `gl_VertexID`.
  - `~Renderer()`: delete both.
  - `upload()`: reset `dustOffset_` to 0 and `dustTime_` to -1.

### 4. `engine/src/renderer_perspective.cpp`
After the window panes and before the view model, inside the meshes pass (sketch, binding order):
```cpp
  // Airborne dust, over the scene and sky, behind the view model.
  {
    const float dt = dustTime_ < 0.f ? 0.f : std::clamp(params.time - dustTime_, 0.f, 0.25f);
    dustTime_ = params.time;
    const glm::vec2 wind = glm::length(lighting.windDir) > 1e-4f ? glm::normalize(lighting.windDir) : glm::vec2{1.f, 0.f};
    dustOffset_ += glm::vec3{wind.x, 0.f, wind.y} * (kDustSpeed * lighting.windStrength * dt);
    const int count = static_cast<int>(std::lround(std::clamp(scene.dustDensity, 0.f, 1.f) * kDustMax));
    if (count > 0) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDepthFunc(GL_LEQUAL);
      glDepthMask(GL_FALSE);
      glColorMaski(1, GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
      glUseProgram(progDust_);
      ... uniforms: uViewProj, uEye, uOffset (dustOffset_), uTime, uSize (std::clamp(scene.dustSize, 1.f, 2.f)),
          uHaze (overcastSky(lighting.skyHorizon, lighting.overcast)), uSunDir (lighting.sunDir),
          uSunColour (lighting.sunGlowColour), uSunGlow (lighting.sunGlow * (1 - lighting.overcast)) ...
      glBindVertexArray(dustVao_);
      glDrawArrays(GL_POINTS, 0, count);
      glBindVertexArray(0);
      glColorMaski(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      glDepthMask(GL_TRUE);
      glDepthFunc(GL_LESS);
      glDisable(GL_BLEND);
    }
  }
```
`kDustSpeed = 2.f` (metres a second at wind strength 1) is a file-local constant. The offset advances every frame, even with no dust, so turning the dust on mid-play doesn't make it jump.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: **3/3 identical** (density 0 draws nothing).
3. `git diff <cycle base> -- engine/shaders/mesh.frag engine/shaders/common.glsl engine/shaders/edge.frag engine/src/renderer_orthographic.cpp game desert examples`: no output.
4. Dust on, by a temporary edit. In `desert/src/world.cpp`, at the end of `buildWorld` before the return, add `world.scene.dustDensity = 0.4f;` (use the variable's real name). Build and capture `pxe_desert --hour 18 --capture dust-a.png`. Change it to `1.0f`, add `world.scene.dustSize = 2.f;`, rebuild, and capture `dust-b.png`. Then restore the file (`git checkout -- desert/src/world.cpp`, then `touch` it), rebuild, and capture `dust-off.png`.
   - The three are pairwise different (`cmp`).
   - `dust-off.png` equals the base's `pxe_desert --hour 18 --capture`.
   - Each of `dust-a.png` and `dust-b.png`, rendered twice, is `cmp`-identical.
   - `dust-a` shows scattered single pixels in the air, warmer and brighter toward the low sun, none in front of the arms, and none on the sky past 40 m. `dust-b` is denser, with some 2-pixel motes close in.

   Keep both in `docs/tasks/perspective-dust/` and describe them in the handoff.
5. By reading the code:
   - The dust is drawn after the scene, sky and window panes and before the view model.
   - It writes no depth and leaves colour attachment 1 alone.
   - No draw call is made at count 0.
   - `dustOffset_` advances by the wind each frame.

## Acceptance criteria
- [ ] `Scene::dustDensity` and `Scene::dustSize` exist as binding, defaulting to 0 and 1.
- [ ] `dust.vert` and `dust.frag` implement the binding shape. The perspective frame draws the dust as §4 says, with the blend, depth and mask state restored after.
- [ ] `progDust_` and `dustVao_` are created in perspective mode only, and freed. `upload()` resets the dust offset.
- [ ] Verification 2 and 3 pass. Verification 4's captures behave as stated, and the two kept captures are described.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None.

## Out of scope
- Dust in the orthographic mode; blowing sand along the ground; dust that lights or shadows anything; gusts.
- The desert's values and menu (task `desert-sky`).

## Spec revisions
None.

## Cycle log
