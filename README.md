# pxe

A small C++20 prototype of t3ssel8r-style 3D pixel art. The default scene recreates the ruins-and-pond scene from his "Creating a Scene for my 3D Pixel Art Game" video: it is modelled by a Blender script and loaded from glTF. A second scene, `--scene hills`, is the rolling heightfield the grass was first tuned on.

Every surface is lit by a hard three-colour toon ramp per material (grass, dirt, stone, bark, leaf, water, and foam for the water's contact lines), using the same normal and the same sun shadow for the ground and for every sprite on it. Grass is instanced screen-space quads whose blades are drawn procedurally in the fragment shader and lit once at the root, so a clump matches the ground under it and only shows as a saw-tooth fringe where it overhangs the next band. A clump's top pixel turns lighter where it sticks out over darker ground. Sparse V-shaped tufts sit one shade off the ground colour. One travelling wind field sways every blade by whole pixels. Tree crowns are covered in clusters of big almond-shaped leaf sprites, grown in clumps with darker gaps between them; a clump shares a tilt so its leaves light together, some leaves stand proud of the surface to rag the outline, and every cluster bobs and flutters with the wind gusts. Each leaf takes a whole toon band from its own tilted normal, so a crown's light breaks up leaf by leaf; leaves turned straight at the sun catch a highlight, and about one in six takes a lighter shade so the shadowed side still shows separate leaves. The crown body sits a band darker than its leaves, so gaps read as depth. A post pass over the low-res target puts light rims on the top edges of stone and dark outlines on silhouettes; bark gets only a faint outline plus a soft light stroke along the few seams the Blender script marks sharp on each trunk. Bark is painted rather than carved: smooth light pushed around by noise stretched up the trunk and cut into half-band steps, so trunks break into soft, watercolour-like blotches of close shades.

Rendered at 960×540, 24 pixels per metre, and copied to the window with nearest-neighbour integer upscaling. That matches the finished frame of t3ssel8r's scene video, where every art pixel is 2×2 on a 1920×1080 recording. Grass and leaf sprites are sized in pixels, so their spacing in metres follows the pixel density. The orthographic camera is pitched 30° down, yaws in 45° steps, and snaps to the view-aligned texel grid; the upscale shifts back by the snap error.

A procedural day/night cycle, after t3ssel8r's day/night video, moves the sun along an arc by day and the moon along the same arc by night: the moon rises an hour after sunset and sets an hour before sunrise, and whichever is higher lights the scene, so they swap in the dark and moonlight catches the pillar tops and sinks down them just like the sunrise. Every material has day, golden-hour, twilight and night palettes that are blended by sun elevation, so the art direction stays in hand-picked colours at every hour. Cloud shadows are a fractal-noise layer that drifts with the wind. Their density is cut into three steps, so each cloud has a stepped edge that thickens toward the middle, and a strength setting decides how far a thick cloud pulls colours toward shadow. The grass sways downwind, with gusts rolling across the field in the same direction as the clouds.

Chipped stone keeps the normal of the box side each face came from, so a block side shades as one flat face while its silhouette stays chipped. The shadow map is a fixed square around the scene, so its texel size does not change as the sun turns. The light moves continuously. Cast shadows are hard, pixel-crisp edges, taken by thresholding a 3x3 filtered shadow kernel so they do not crawl as the sun turns. Curved surfaces quantise N·L into in-between shades, so a band edge moves across them pixel by pixel. Cut stone, whose faces each have one flat normal, follows the sun's angle continuously, so a pillar face fades smoothly from one shade to the next by sun or moonlight instead of jumping a step at a time. The ruins sit inside a ring of off-screen hills, computed analytically in the shaders rather than modelled: the sun has to clear that ridge before it reaches a point. At sunrise the light finds the pillar tops first, a crisp edge sinks down the shafts as the sun climbs, and the lawn lights last; sunset runs the same in reverse. The ridge is kept smooth so that edge crosses each face as a straight line.

Water reflects the scene: a second pass draws everything above the water plane mirrored (meshes and leaves, not grass), clipped and scissored to the pond's area on screen. The water pass also reads a copy of the scene behind it and its depth, so it knows how deep the water is at each pixel: the shallows show the pond bed darkly through a blue tint, deeper water shows the reflection, or the sky's stepped blotches where nothing is mirrored, with a one-pixel ripple, and a thin light contact line traces the shore and everything standing in the water.

Rain, after t3ssel8r's rain shader, has one severity from drizzle to downpour. It brings a murky, even overcast under which cast shadows from the sun and moon soften with severity and are gone in a downpour (drifting cloud shadows fade out as it sets in) and a cool, hazy grade; sparse one-pixel streaks in two layers that hold still and are redrawn about ten times a second, for a stepped, low-frame-rate flicker, leaning with the wind's direction across the screen and its strength; a soft light haze along the upper outlines of stone and trunks; occasional three-stage splashes (a dot, a little crown, two droplets) on stone and bark, and small ones scattered on crowns and the lawn; ripple rings on the water; and upward-facing leaves bobbing as drops hit them. Splashes are hashed on cells of the surfaces themselves, rebuilt from depth, and the two streak layers drift at their own depths as the camera turns, so the rain stays with the world as the camera pans or turns.

Lightning flashes now and then in rain and downpour, more often the heavier the rain, and can be switched off in the menu. Each flash is a second light from a random direction, anywhere around the sky between 30 and 65 degrees up, with its own shadow map, so it casts hard shadows through the rain: lit faces go a cold blue-white and the shadow side stays storm-dark. As in the video it comes on at full in one frame and sometimes dips and comes back once. It then fades over most of a second, blended over the sun or moon by its strength, so its shadows lighten a quarter band at a time into the scene's own light instead of snapping off.

Weather sound is synthesised as it plays rather than read from recordings, so each layer follows its setting continuously ([miniaudio](https://github.com/mackron/miniaudio) opens the output). Wind is mostly a mild rustle of leaves, a soft band of noise that swells with each gust and flutters gently within it, over a quiet low rush; nothing in it starts suddenly, so it never clicks. Rain is a steady hiss and a patter of drops that grows with the severity, plus a low roar in a downpour. Each lightning flash is followed by thunder, panned toward the side of the screen the flash lit from: a crack and a deep rumble when it is close, and later, quieter and lower when it is far off. The menu has volume sliders for each, and M mutes. `--audio-wav out.wav` renders 20 seconds of the weather set on the command line, with a near and a far thunderclap, so the sound can be checked without running the game.

A small campfire burns on the lawn left of the pond: a scorched dirt patch, a ring of rough stones and four logs leant into a teepee. The fire follows t3ssel8r's procedural pixel art fire: a plain particle system of about a hundred particles drawn as soft discs into a heat buffer, which the post pass cuts into three flat colours (a pale yellow heart inside orange inside a crimson rim), with embers as single red pixels drifting up on the wind. Its three parts step at their own low frame rates (the core at 8 fps, the flames at 12, the embers at 10) so it moves like hand animation, and flames cool as they rise so the top breaks into separate tongues. A slower layer of very faint smoke starts well above the flames, swells and drifts off on the wind, drawn as a two-step haze over what is behind it. The fire lights the scene around it as in the video: faces turned to it go a soft orange while faces turned away keep the night's colours, and the light fades smoothly with distance through pinks into the night, breathing gently rather than jumping with the flames. It casts hard shadows from a cube of distances drawn once round the firelight (the scene does not move), so stones, logs, rubble and trunks throw shadows away from it. Direct sun drowns it out, so by day it shows in the shade round the fire (a tree's shadow over the fire pit fills with its glow) and not on sunlit lawn; at night it lights everything near and carries farther. The menu's Campfire checkbox or `--no-fire` puts it out.

No lightning or god rays.

## The Blender scene

[`tools/blender/build_ruins.py`](tools/blender/build_ruins.py) builds the scene with the modifier tricks from the video and writes `assets/scenes/ruins.glb`:

- terraced ground with a pond basin, generated as a height grid; faces under paving are tagged `Dirt` so no grass grows through stone;
- paving, pillars, steps, planters and walls as boxes chipped by a *subdivide → displace → decimate* stack;
- rocks as cubes put through *smooth subdivision → cloud displacement → heavy decimation*;
- trees with low-poly, flat-shaded bark trunks (a twisted, knotted, smooth-shaded tube with a flared, root-lobed base and two or three long sharp seams for bark strokes; the shader paints the bark) that fork into two or three branches, with lumpy crowns of a dozen merged spheres around a smaller core, voxel-remeshed into one skin, with normals bent toward one ellipsoid so the crown shades as a single soft mass.

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

The window defaults to 1920×1080, an exact 2× scale of the 960×540 target. Retina framebuffers use `glfwGetFramebufferSize`, and the upscale is `floor` of the fit so it stays an integer.

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
| Tab | Settings menu: time of day, time speed, clouds, wind, rain |
| Space | Pause or resume time |
| R | Cycle rain: none, drizzle, rain, downpour |
| L | Lightning strike now |
| M | Sound on/off |
| [ / ] | Halve or double the speed of time |
| WASD or arrows | Pan along the ground |
| Shift | Pan faster |
| Q / E | Yaw by 45° |
| F1 | N·L as grey |
| F2 | Hide grass |
| F3 | Draw grass in magenta |
| F4 | Write `shot.png` at the low resolution |
| Esc | Quit |

```text
pxe [--capture out.png] [--window-png out.png]
    [--scene ruins|hills] [--hour H] [--clouds C] [--rain R] [--no-fire] [--no-lightning] [--flash AZ,EL[,LEVEL]] [--mute] [--audio-wav out.wav] [--wind DEG] [--wind-strength S]
    [--menu] [--ui-shot out.png]
    [--bench N] [--bench-paused]
    [--yaw degrees] [--slide metres] [--target x,y,z]
    [--min-dist metres] [--hide-grass] [--magenta] [--ndl]
    [--test-colours] [--no-tufts] [--no-edges] [--stamps] [--no-depth] [--time seconds]
    [--self-test] [--no-vsync]
```

`--bench N` renders N frames at the window's size with vsync off and the day running, and prints the wall-clock time per frame; `--bench-paused` holds the time of day, which lets the shadow map be reused. Close other GPU-heavy apps first (a video playing in a browser skews it badly). Set `PXE_BENCH_PASSES=1` for rough per-pass GPU timers.

`--self-test` renders twice (the low-res image must be identical), nudges the camera by a fraction of a texel (still identical), then by one texel (the image shifts by exactly one pixel).
