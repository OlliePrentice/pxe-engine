# pxe

A small C++20 engine that draws 3D scenes as crisp pixel art. The camera is orthographic and pitched 30° down. Yaw rests on 45° steps. The scene is rendered at 640×360 and upscaled with a pixel-art filter that stays sharp at integer scales and clean at fractional ones. Camera translation is snapped to the low-res pixel grid, then the snap error is put back as a subpixel shift during the upscale, so panning glides on the display without the pixels swimming.

This is a prototype of the look described by the developer t3ssel8r (low-res 3D, dimetric camera, toon ramps, coloured 1-pixel edges). It is not their engine and it does not use Unity.

## Graphics choice

**GLFW 3.4 + OpenGL 4.1 core.**

OpenGL 4.1 is the newest GL Apple ships, and it is also available on Windows and on Mesa (including llvmpipe). The demo is written in GLSL 410, so the same shaders compile on all three without a cross-compiler. GLFW creates the window and the context; every GL entry point is loaded with `glfwGetProcAddress`, which is required on Windows because `opengl32.dll` only exports GL 1.1.

That is a deliberate trade against the longer-term options:

- **Metal, WebGPU/Dawn, bgfx, sokol with a Metal backend** are the right destination on Apple, where GL is deprecated. They also need a shader toolchain (MSL, WGSL, `shaderc`, `sokol-shdc`) on the first build. Dawn in particular is a large dependency. None of that is required to draw this prototype, and it makes a headless Linux check harder.
- The GL calls live in one place: `engine/src/gl.hpp` (the loader) and `engine/src/renderer.cpp` plus `engine/shaders/`. The camera math, meshes, and the game do not include GL. Replacing that module with Metal or WebGPU later does not require rewriting the snap or the scene.

Khronos `glcorearb.h` is vendored so the build does not depend on a system GL header. GLFW and glm are fetched by CMake.

## Build

Requires a C++20 compiler, CMake 3.20 or newer, and a network connection on the first configure (GLFW and glm are downloaded).

### macOS (Apple Silicon or Intel)

Install the Xcode command line tools and CMake (`brew install cmake` if needed).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game/pxe
```

The window is 1280×720 by default, which is an exact 2× scale of the 640×360 internal image. Retina framebuffers are sized with `glfwGetFramebufferSize`, and the upscale filter handles a non-integer scale when the window is not a multiple of 640×360.

This tree was built and run on Linux with Mesa llvmpipe. It has not been launched on a Mac in this environment. The context is requested as GL 4.1 core, forward-compatible, which is the combination Apple still accepts.

### Linux

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game/pxe
```

Headless (software GL under Xvfb):

```bash
xvfb-run -a -s "-screen 0 1280x720x24" ./build/game/pxe --screenshot shot.png
```

### Windows

From a Developer prompt, with CMake and a C++20 compiler installed:

```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build\game\Release\pxe.exe
```

The generator may place `pxe.exe` directly in `build\game\`.

## Controls

| Input | Action |
|---|---|
| WASD or arrow keys | Pan along the ground, relative to the camera yaw |
| Shift | Pan faster |
| Q / E | Rotate yaw by 45°. The turn animates, then rests on the new step |
| Mouse wheel, `+` / `-` | Zoom through fixed pixels-per-unit steps (12, 16, 22, 30, 42) |
| O | Outlines on / off |
| F1 | Cycle debug view: final, raw low-res (nearest), albedo, normals, depth |
| V | Cycle weather: clear, hazy god rays, rain |
| F12 | Save `pxe-shot.png` |
| Esc | Quit |

Zoom steps change how many low-res pixels cover one world unit and then sit still. A continuous zoom would reshuffle the pixel grid every frame.

## Command line

```text
pxe [--width N] [--height N]
    [--yaw degrees] [--zoom 0..4] [--weather 0|1|2] [--time seconds]
    [--target x,y,z]
    [--screenshot path.png] [--shots dir]
    [--demo] [--demo-frames dir]
    [--self-test] [--no-vsync]
```

`--self-test` renders the meadow twice with the camera moved by a fraction of a texel. The low-res image must be identical (the snap absorbed the motion). A further move of one texel must change the image. The snap error reported to the upscale must change in the first case.

`--demo` pans, rotates 45°, and pans again, then exits. `--demo-frames` writes that same move as a 20 fps PNG sequence using a fixed timestep.

## Architecture

```text
engine/     library, no game content
  camera    30° pitch, 45° yaw steps, view-plane pixel snap, second-order pan
  renderer  shadow map, g-buffer, toon light, outlines, water, rays, upscale
  shaders   GLSL 410
game/       meadow scene and the GLFW loop
```

**Camera.** Yaw animates with a smoothstep between 45° rests. Pan on the ground plane is smoothed with the second-order system from t3ssel8r's procedural-animation notes (frequency, damping, response), then the eye is snapped. The snap is done in the camera's right/up basis, not in world XZ, so it stays on pixel centres at a 45° yaw. The projection covers a 644×364 target: the 640×360 view plus a two-texel border. The border is real scene, so the upscale can shift by up to half a texel without clamping.

**Upscale.** The present pass samples the low-res colour with the box filter he describes for pixel-art magnification: the sample is snapped to a texel centre, and it only blends in a band around the texel edge whose width is `fwidth` of the texel coordinate, clamped to one texel. The blend is a `smoothstep` (the integral of a quadratic window). At 2× or 3× the sample points miss that band, so the pixels stay hard. The snap error is a shift of the UV window into this filter.

**Lighting.** A single directional sun. Opaque colour is a 4-step ramp on wrapped N·L. Shadowed pixels ignore the ramp and use a teal monochrome Lambert, so shadow pools stay shaped. Leaves are lit from the canopy normal and do not sample the shadow map, because the canopy is a shadow caster they sit inside. Grass is lit from the ground normal under the tuft, which does not change as the camera moves.

**Outlines.** A 4-neighbour pass on linear depth and view-space normals. A pixel closer than its neighbours is darkened (the near side of a silhouette). A convex crease, on the nearer side only, is brightened, and the amount follows how lit that pixel already is. Grass, flowers, and leaves opt out. Outline colour is a shade of the local colour, not black. A small lift in the present pass keeps the darkest pixels off pure black.

**Scene.** Terraced height field, a ruined cloister, faceted rocks, a pond with a 1-pixel-scale shore highlight, instanced grass and flowers, and trees (a trunk, a jagged shadow sphere, and camera-facing leaf cards). Grass sways from the tip; leaves bob. Wind is animation, so those pixels are allowed to change. Static surfaces do not, while the camera pans by less than a pixel.

## What this prototype does not do

Verified here on Linux with llvmpipe, including the snap self-test. Not launched on macOS.

- God rays are a short march through the sun shadow map with hard steps. They are not the light-aligned plane projection from the reference, and they are easy to overdo, so the hazy preset keeps them quiet.
- Rain is a grade plus 1-pixel streaks and flecks, not the full set of splashes, mist, and lightning.
- No reflection probe, refraction map, point lights, or day/night palette swap.
- Grass placement is a jittered grid, not blue noise. Leaves are analytic blobs, not a drawn sprite sheet.
- Terrain is generated in code. There is no editor and no marching-squares authoring tool.
- No characters, smear frames, or gameplay.

## Next steps

1. Replace the ray march with a handful of light-aligned planes sampled from the shadow map, and mask a tiling dust texture with that result.
2. A world-space cloud shadow (flat-relief FBM blended with the real projection) so the meadow changes without the camera.
3. The rest of the rain stack: shore splashes, top-edge spatter, water ripples.
4. Quantised additive point lights on top of the ramp, updated at a low frame rate.
5. A water reflection probe and a one-pixel horizontal refraction wiggle.
6. A Metal (or WebGPU) backend behind `renderer.cpp`, now that the snap and the passes are isolated.
7. An authoring path for the terrace height field, then a character animated on stepped keys.
