# pxe

A small C++20 prototype of t3ssel8r-style 3D pixel art. The default scene recreates the ruins-and-pond scene from his "Creating a Scene for my 3D Pixel Art Game" video: it is modelled by a Blender script and loaded from glTF. A second scene, `--scene hills`, is the rolling heightfield the grass was first tuned on.

Every surface is lit by a hard three-colour toon ramp per material (grass, dirt, stone, bark, leaf, water, foam), using the same normal and the same sun shadow for the ground and for every sprite on it. Grass is instanced screen-space quads whose blades are drawn procedurally in the fragment shader and lit once at the root, so a clump matches the ground under it and only shows as a saw-tooth fringe where it overhangs the next band. A clump's top pixel turns lighter where it sticks out over darker ground. Sparse V-shaped tufts sit one shade off the ground colour. One travelling wind field sways every blade by whole pixels. Tree crowns get the same treatment with leaf-cluster sprites whose normals are jittered, so crowns go dappled where the light turns. A post pass over the low-res target puts light rims on the top edges of stone and bark and dark outlines on their other silhouettes.

Rendered at 640×360 and copied to the window with nearest-neighbour integer upscaling. The orthographic camera is pitched 30° down, yaws in 45° steps, and snaps to the view-aligned texel grid; the upscale shifts back by the snap error.

No day/night, weather, clouds, water reflections, or god rays.

## The Blender scene

[`tools/blender/build_ruins.py`](tools/blender/build_ruins.py) builds the scene with the modifier tricks from the video and writes `assets/scenes/ruins.glb`:

- terraced ground with a pond basin, generated as a height grid; faces under paving are tagged `Dirt` so no grass grows through stone;
- paving, pillars, steps, planters and walls as boxes chipped by a *subdivide → displace → decimate* stack;
- rocks as cubes put through *smooth subdivision → cloud displacement → heavy decimation*;
- trees as bevelled-curve trunks with crowns of merged spheres, voxel-remeshed into one skin, with normals bent toward one ellipsoid so the crown shades as a single soft mass.

Material names are the contract with the engine. Rebuild the asset after editing the script:

```bash
/Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -P tools/blender/build_ruins.py
```

## Build

C++20, CMake 3.20+, and a network connection the first time (GLFW and glm are fetched). OpenGL 4.1 core, GLSL 410.

### macOS (Apple Silicon or Intel)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game/pxe
```

The window defaults to 1920×1080, an exact 3× scale of the 640×360 target. Retina framebuffers use `glfwGetFramebufferSize`, and the upscale is `floor` of the fit so it stays an integer.

### Linux

```bash
sudo apt install build-essential cmake libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libxext-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/game/pxe
```

Headless capture of the low-res target (not the upscaled window):

```bash
xvfb-run -a -s "-screen 0 1920x1080x24" ./build/game/pxe --capture shot.png
```

## Controls

| Input | Action |
|---|---|
| WASD or arrows | Pan along the ground |
| Shift | Pan faster |
| Q / E | Yaw by 45° |
| F1 | N·L as grey |
| F2 | Hide grass |
| F3 | Draw grass in magenta |
| F4 | Write `shot.png` (640×360) |
| Esc | Quit |

```text
pxe [--capture out.png] [--window-png out.png]
    [--scene ruins|hills] [--yaw degrees] [--slide metres] [--target x,y,z]
    [--min-dist metres] [--hide-grass] [--magenta] [--ndl]
    [--test-colours] [--no-tufts] [--no-edges] [--stamps] [--no-depth] [--time seconds]
    [--self-test] [--no-vsync]
```

`--self-test` renders twice (the low-res image must be identical), nudges the camera by a fraction of a texel (still identical), then by one texel (the image shifts by exactly one pixel).
