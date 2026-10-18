# pxe

A small C++20 engine that draws 3D scenes as crisp pixel art. The camera is orthographic and pitched 30° down. Yaw rests on 45° steps. The scene is rendered at 640×360 and upscaled with a pixel-art filter that stays sharp at integer scales and clean at fractional ones. Camera translation is snapped to the low-res pixel grid, then the snap error is put back as a subpixel shift during the upscale, so panning glides on the display without the pixels swimming.

This is a prototype of the look described by the developer t3ssel8r (low-res 3D, dimetric camera, toon ramps, coloured 1-pixel edges). It is not their engine and it does not use Unity.

## Graphics choice

**GLFW 3.4 + OpenGL 4.1 core.**

OpenGL 4.1 is the newest GL Apple ships, and it is also available on Windows and on Mesa (including llvmpipe). The demo is written in GLSL 410, so the same shaders compile on all three without a cross-compiler. GLFW creates the window and the context; every GL entry point is loaded with `glfwGetProcAddress`, which is required on Windows because `opengl32.dll` only exports GL 1.1.

That is a deliberate trade against the longer-term options:

- **Metal, WebGPU/Dawn, bgfx, sokol with a Metal backend** are the right destination on Apple, where GL is deprecated. They also need a shader toolchain (MSL, WGSL, `shaderc`, `sokol-shdc`) on the first build. Dawn in particular is a large dependency. None of that is required to draw this prototype, and it makes a headless Linux check harder.
- The GL calls live in one place: `engine/src/gl.hpp` (the loader) and `engine/src/renderer.cpp` plus `engine/shaders/`. The camera math, meshes, and the game do not include GL. Replacing that module with Metal or WebGPU later does not require rewriting the snap or the scene.

Khronos `glcorearb.h` is vendored so the build does not depend on a system GL header. GLFW and glm are fetched by CMake. cgltf 1.14 is vendored as one header, so the glTF props load without a further download.

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
| Mouse wheel, `+` / `-` | Zoom through fixed pixels-per-unit steps (12, 16, 22, 30, 42). Default is 42 |
| O | Outlines on / off |
| F1 | Cycle debug view: final, raw low-res (nearest), albedo, normals, depth |
| V | Cycle weather: calm, breezy, storm, rain. The change eases in over about three seconds. Default is calm |
| P | Pause / resume the day-night clock |
| `[` / `]` | Slower / faster clock (x1, x4, x16, x64) |
| `,` / `.` | Step the clock back / forward one hour |
| F12 | Save `pxe-shot.png` |
| Esc | Quit |

The window title shows the time of day (`HH:MM` and a phase name: night, dawn, morning, day, dusk), whether the clock is paused, and the speed. A full day lasts 60 seconds at x1.

Zoom steps change how many low-res pixels cover one world unit and then sit still. A continuous zoom would reshuffle the pixel grid every frame.

## Command line

```text
pxe [--width N] [--height N]
    [--yaw degrees] [--zoom 0..4] [--weather 0..3] [--time seconds]
    [--hour 0..24] [--target x,y,z]
    [--screenshot path.png] [--shots dir] [--cycle-frames dir]
    [--weather-clip dir]
    [--demo] [--demo-frames dir]
    [--self-test] [--no-vsync]
```

`--self-test` renders the meadow twice with the camera moved by a fraction of a texel. The low-res image must be identical (the snap absorbed the motion). A further move of one texel must change the image. The snap error reported to the upscale must change in the first case.

`--demo` pans, rotates 45°, and pans again, then exits. `--demo-frames` writes that same move as a 20 fps PNG sequence using a fixed timestep.

## Architecture

```text
engine/        library, no game content
  camera       30° pitch, 45° yaw steps, view-plane pixel snap, second-order pan
  renderer     shadow map, g-buffer, toon light, outlines, water, rays, upscale
  gltf         glTF 2.0 meshes, custom normals, material colour, node transforms
  shaders      GLSL 410
game/          meadow scene and the GLFW loop
assets/props   Blender meshes the meadow instances (.glb)
assets/sprites grass, flower, and leaf flipbook sheets
tools/blender  regenerates those meshes
tools/sprites  regenerates the flipbook sheets
```

**Camera.** Yaw animates with a smoothstep between 45° rests. Pan on the ground plane is smoothed with the second-order system from t3ssel8r's procedural-animation notes (frequency, damping, response), then the eye is snapped. The snap is done in the camera's right/up basis, not in world XZ, so it stays on pixel centres at a 45° yaw. The projection covers a 644×364 target: the 640×360 view plus a two-texel border. The border is real scene, so the upscale can shift by up to half a texel without clamping.

**Upscale.** The present pass samples the low-res colour with the box filter he describes for pixel-art magnification: the sample is snapped to a texel centre, and it only blends in a band around the texel edge whose width is `fwidth` of the texel coordinate, clamped to one texel. The blend is a `smoothstep` (the integral of a quadratic window). At 2× or 3× the sample points miss that band, so the pixels stay hard. The snap error is a shift of the UV window into this filter.

**Lighting.** One directional light orbits through the day. Morning and noon are a warm-white sun with a cool shadow tint, chosen so lit grass lands on `#a8bd6d`, the mid band on `#83a555`, and shadow on `#628474`. Stone is lavender-grey in the light and a darker purple in shadow. Dawn and dusk drop the sun and shift the lit side gold. Night keeps a dim cool moon so the meadow stays readable. Ground takes three hard steps from the shadow map, the cloud field, and how far the slope faces the sun. There is no dither. Leaf cards are lit from the transferred canopy normal in four steps (shadow, mid, lit, a top highlight) and do not sample the shadow map.

**Clouds.** No cloud bodies. Two low-frequency noise octaves are projected along the sun and cut into the same three colours as the sun shadow. Islands are rare and large. The mid contour is a thin ring on that same low-frequency field, not a speckled band. The field drifts under one native pixel per second. Grass tufts, not the noise, make the edge jagged.

**Grass.** Sprites are vertex-shader billboards, GPU instanced, scattered once with blue noise at about 0.12 world units so neighbouring 12px tufts overlap. Each tuft is a fan of blades, one flat colour: the terrain albedo and the ground lighting band sampled at its root. Inside a band the tuft matches the ground and disappears. On a boundary, lit tufts poke into the shadow and shadow tufts poke into the light. Flowers are a purple four-petal X or a yellow dot, placed in clusters. They alpha-test and write depth. The canopy mesh is not drawn; four camera-facing leaf cards are spawned on each canopy vertex, jittered along that vertex's normal.

**Wind.** One state (direction, speed, strength, gustiness, step rate, rain) is shared by grass and leaves. The flipbook is four frames. The baseline is 2 fps, so a pose holds for half a second, and each sprite has its own phase. Higher wind picks a stronger-lean row and raises the step rate (storm is 4 fps). The quad is never rotated or bent. Calm stays on the planted frame. A crown sways by at most two pixels, on that same clock. Rain drops upward-facing leaves by one pixel. `V` blends between calm, breezy, storm, and rain over about three seconds. `--weather 0..3` selects one directly. `--weather-clip dir` writes a 12 fps sequence of each.

**Rain.** Storm and rain are one post pass plus ripples on the water. The grade drops the scene to about half brightness and shifts it blue (grass toward `#4e705b`, stone toward `#384a76`, water `#33668f`). Streaks are 1px, about 6° off vertical, a new set at 15 Hz, blended toward `#d1ecff`. Splashes on the ground last three of those ticks. Short dashes sit on horizontal rims. A wide soft mist drifts by a few levels. Water rings grow and break at 30 Hz. Lightning is an instant flash with a half-life of about 0.4s: bright pixels go lavender and darker pixels drop further.

The shadow camera is snapped to shadow-map texels, and the sun angle is quantized, so a moving light does not swim the shadow edge inside a pixel. Polygon offset plus a normal bias keeps the ground free of acne.

**Outlines.** A 4-neighbour pass on linear depth and view-space normals. Lit edges — the near side of a silhouette, and the convex lip of a step or pillar — are pushed brighter than the surface, tinted by the sun. Edges in shadow are darkened. Grass, flowers, and leaves opt out. Outline colour is a shade of the local colour, not black.

**Scene.** A large height field with shelves and short lips, so a slope breaks into light bands. Flat colours: one grass green, lavender-grey stone, light-blue water. The ruin, rocks, and trees are glTF props authored in Blender (see below): stacked courses with chipped tops and moss caps, rubble and pebbles, upright tapered trunks, and canopies whose normals were transferred from a smooth enclosing sphere. Those crowns are shadow casters only; the picture is the leaf cards. Grass is the blue-noise billboards above. The pond is a clear blue plane with a 1-pixel white shore where it meets the bank. The canopy mesh and the shadow map do not move with the wind, so a pan of less than a pixel does not shimmer.

## Blender props

The meadow does not build pillars, rocks, or trees in code. Those meshes live in `assets/props/` as glTF binary (`.glb`). `game/src/scene.cpp` loads them and places them on the height field. The same vertices go to the colour pass and the shadow map, so a canopy casts the shadow you see.

Regenerate the set (Blender 4.0 or newer, tested on 4.0.2):

```bash
blender --background --python tools/blender/generate_props.py
```

On macOS the binary is usually inside the app:

```bash
/Applications/Blender.app/Contents/MacOS/Blender --background --python tools/blender/generate_props.py
```

The script writes `assets/props/*.glb` and `assets/props/stairs.meta` (step count, width, depth, rise, thickness). The stair cut in the height field reads that file, so the ground stays tucked under the treads. Details of the modifiers are in `tools/blender/README.md`.

### Add your own model

1. Model in Blender with metric units and unit scale 1. One Blender metre is one world unit. Blender's Z is up; the exporter turns that into glTF Y-up, which is the engine's up axis.
2. Put the object's origin at the base, on the ground, so the meadow can drop it onto the height field. Apply scale before export.
3. One flat material per colour. Principled BSDF, metallic 0, roughness 1, no image textures. Name the material so the engine knows what it is:

   | Name contains | Treated as |
   |---|---|
   | `Leaf`, `Canopy`, `Bush`, `Foliage` | Canopy. Lit from the smooth normal, no outline, no shadow-map sample. Each vertex gets a leaf sprite |
   | `Bark`, `Trunk`, `Wood` | Trunk |
   | `Dirt`, `Path` | Path |
   | `Grass`, `Ground`, `Terrain` | Ground |
   | anything else (`Stone`, `Step`, `Slab`, `Rock`, `Wall`, …) | Stone |

4. The colour you pick in the Base Color swatch is the albedo. Blender stores it as linear, glTF writes that linear value, and the loader converts it back to sRGB because the toon pass has no gamma step. Noon lighting multiplies it (stone is also lifted a little), so the framebuffer is not the raw swatch. Set the viewport to Standard if you want the Blender view to match the swatch. AgX will not match.
5. Canopies: join the spheres, then add a Data Transfer modifier from a smooth enclosing mesh (Face Corner Data → Custom Normals, mapping Poly Interpolated Nearest, or the nearest-poly equivalent). Apply it. On Blender 4.0, turn on Auto Smooth at 180° first or the modifier does nothing. Do not join that mesh onto a flat-shaded trunk afterwards; that clears the custom normals. Export the trunk and the crown as two objects in one file if they share a pivot.
6. File → Export → glTF 2.0 (.glb), with:
   - Format: glTF Binary (`.glb`)
   - Include: the prop only (selected objects is fine)
   - Transform: **+Y Up** on
   - Data → Mesh: Apply Modifiers on, UVs optional, Normals on, Tangents off, Vertex Colors off
   - Data → Material: Export
   - Animation, skinning, shape keys, cameras, and lights off
7. Save the file under `assets/props/`. Instance it in `game/src/scene.cpp` next to the other `putOn` calls:

```cpp
putOn(loadProp("props/your_prop.glb"), x, z, yawRadians, scale, blockRadius, scatterLeaves);
```

`yawRadians` is a turn around Y. `blockRadius` keeps grass off the footprint (`0` skips that, a negative value uses the mesh bounds). `scatterLeaves` spawns four leaf cards on every canopy vertex and leaves that mesh out of the colour pass. A material named `Moss` shades as ground. Rebuild with the same CMake commands as before; the asset directory is compiled in, so the working directory does not matter.

The grass, flower, and leaf sheets are `assets/sprites/grass.png`, `flower.png`, and `leaf.png`. They are masks (white RGB, alpha coverage). Regenerate them with `python3 tools/sprites/make_sprites.py`. Columns are the four wind frames. Rows are shape, then lean. The engine samples them with nearest filtering.

## What this prototype does not do

Verified here on Linux with llvmpipe, including the snap self-test. Not launched on macOS.

- God rays are a short march through the sun shadow map with hard steps. They are not the light-aligned plane projection from the reference. The weather cycle does not turn them on.
- Rain is screenspace hashes plus ellipse ripples, not his textures. Lightning darkens the existing shadow shapes; it does not cast a second shadow map. There are no puddles.
- No reflection probe, refraction wiggle, or quantised point lights. The day/night cycle is a single orbiting light and a palette shift, not a texture swap.
- The flipbook sheets are drawn to the extracted silhouettes (blade fans, a purple X, yellow dots, jagged leaf lobes). They are not his sprite textures. The cloud field is low-frequency value noise projected along the sun, not his exact noise. The meadow layout is a stand-in, not his scene. God rays stay off. Rain does not cast a second lightning shadow and there are no puddles. The pond is often outside the default frame.
- Terrain and the grass scatter are still generated in code. Props are Blender meshes; there is no in-engine editor and no marching-squares authoring tool.
- No characters, smear frames, or gameplay.

## Next steps

1. Replace the ray march with a handful of light-aligned planes sampled from the shadow map, and mask a tiling dust texture with that result.
2. A second shadow map for the lightning bolt, and puddles if a reference for them shows up.
3. Quantised additive point lights on top of the ramp, updated at a low frame rate.
4. A water reflection probe and a one-pixel horizontal refraction wiggle.
5. A Metal (or WebGPU) backend behind `renderer.cpp`, now that the snap and the passes are isolated.
6. An authoring path for the terrace height field, then a character animated on stepped keys. Props already load from glTF.
