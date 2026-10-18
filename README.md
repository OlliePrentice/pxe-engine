# pxe

A small C++20 prototype of t3ssel8r-style contoured grass. The terrain is a smooth rolling heightfield. The contour lines are a hard three-colour toon ramp on the same normal and the same sun shadow for the ground and for every grass tuft. Grass is instanced screen-space sprites, lit once at the root.

Rendered at 640×360 and copied to the window with nearest-neighbour integer upscaling. The orthographic camera is pitched 30° down, yaws in 45° steps, and snaps to the view-aligned texel grid; the upscale shifts back by the snap error.

No day/night, weather, clouds, water, trees, props, or outlines.

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
    [--yaw degrees] [--slide metres] [--target x,y,z]
    [--min-dist metres] [--hide-grass] [--magenta] [--ndl]
    [--test-colours] [--no-accent] [--stamps] [--no-depth]
    [--self-test] [--no-vsync]
```

`--self-test` renders twice (the low-res image must be identical), nudges the camera by a fraction of a texel (still identical), then by one texel (the image shifts by exactly one pixel).
