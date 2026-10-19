# pxe

A small C++20 prototype of t3ssel8r-style 3D pixel art. The default scene recreates the ruins-and-pond scene from his "Creating a Scene for my 3D Pixel Art Game" video: it is modelled by a Blender script and loaded from glTF. A second scene, `--scene hills`, is the rolling heightfield the grass was first tuned on.

Every surface is lit by a hard three-colour toon ramp per material (grass, dirt, stone, bark, leaf, water, and foam for the water's contact lines), using the same normal and the same sun shadow for the ground and for every sprite on it. Grass is instanced screen-space quads whose blades are drawn procedurally in the fragment shader and lit once at the root, so a clump matches the ground under it and only shows as a saw-tooth fringe where it overhangs the next band. A clump's top pixel turns lighter where it sticks out over darker ground. Sparse V-shaped tufts sit one shade off the ground colour. One travelling wind field sways every blade by whole pixels. The menu's Wind strength sets how far the grass and leaves sway and how fast the gusts roll (near still at 0, as it always was at the default 1, bending hard and quick in a gale at 6); the gusts run on a phase that advances faster the harder it blows, so moving the slider never makes the grass jump. The same strength lays the campfire's flames over and streams its sparks downwind; its smoke then starts lower and downwind, at the leaning flame tips, and each smoke beat lets out a short run of smaller puffs along the path it covers, so it streams away as one plume rather than a row of bubbles, and from a moderate breeze up, patches of fine wavelets drift downwind across the pond and break its reflection up a little more. Blades are depth-tested as if upright, each pixel up a sprite a little nearer the camera than its root, so a stone face in front hides the blades behind it, and grass rooted under paving, plinths or trunks is dropped. Tree crowns are covered in clusters of big almond-shaped leaf sprites, grown in clumps with darker gaps between them; a clump shares a tilt so its leaves light together, some leaves stand proud of the surface to rag the outline, and every cluster bobs and flutters with the wind gusts. The leaves move together on one 12 fps beat and turn between a few set poses, so neighbouring clusters do not shimmer against each other. Each leaf takes a whole toon band from its own tilted normal, so a crown's light breaks up leaf by leaf; leaves turned straight at the sun catch a highlight, and about one in six takes a lighter shade so the shadowed side still shows separate leaves. The crown body sits a band darker than its leaves, so gaps read as depth. A post pass over the low-res target puts light rims on the top edges of stone and dark outlines on silhouettes; bark gets only a faint outline plus a soft light stroke along the few seams the Blender script marks sharp on each trunk. Bark is painted rather than carved: smooth light pushed around by noise stretched up the trunk and cut into half-band steps, so trunks break into soft, watercolour-like blotches of close shades.

Rendered at 960×540, 24 pixels per metre, and copied to the window with nearest-neighbour integer upscaling. That matches the finished frame of t3ssel8r's scene video, where every art pixel is 2×2 on a 1920×1080 recording. Grass and leaf sprites are sized in pixels, so their spacing in metres follows the pixel density. The orthographic camera is pitched 30° down, yaws in 45° steps, and snaps to the view-aligned texel grid; the upscale shifts back by the snap error.

A procedural day/night cycle, after t3ssel8r's day/night video, moves the sun along an arc by day and the moon along the same arc by night: the moon rises an hour after sunset and sets an hour before sunrise, and whichever is higher lights the scene, so they swap in the dark and moonlight catches the pillar tops and sinks down them just like the sunrise. Every material has day, golden-hour, twilight and night palettes that are blended by sun elevation, so the art direction stays in hand-picked colours at every hour. Cloud shadows are a fractal-noise layer that drifts with the wind. Their density is cut into three steps, so each cloud has a stepped edge that thickens toward the middle, and a strength setting decides how far a thick cloud pulls colours toward shadow. The grass sways downwind, with gusts rolling across the field in the same direction as the clouds.

Chipped stone keeps the normal of the box side each face came from, so a block side shades as one flat face while its silhouette stays chipped. The shadow map is a fixed square around the scene, so its texel size does not change as the sun turns. The light moves continuously. Cast shadows are hard, pixel-crisp edges, taken by thresholding a 3x3 filtered shadow kernel so they do not crawl as the sun turns. Curved surfaces quantise N·L into in-between shades, so a band edge moves across them pixel by pixel. Cut stone, whose faces each have one flat normal, follows the sun's angle continuously, so a pillar face fades smoothly from one shade to the next by sun or moonlight instead of jumping a step at a time. The ruins sit inside a ring of off-screen hills, computed analytically in the shaders rather than modelled: the sun has to clear that ridge before it reaches a point. At sunrise the light finds the pillar tops first, a crisp edge sinks down the shafts as the sun climbs, and the lawn lights last; sunset runs the same in reverse. The ridge is kept smooth so that edge crosses each face as a straight line.

Water reflects the scene: a second pass draws everything above the water plane mirrored (meshes and leaves, not grass), clipped and scissored to the pond's area on screen. The water pass also reads a copy of the scene behind it and its depth, so it knows how deep the water is at each pixel: the shallows show the pond bed darkly through a blue tint, deeper water shows the reflection, or the sky's stepped blotches where nothing is mirrored, with a one-pixel ripple, and a thin light contact line traces the shore and everything standing in the water.

Rain, after t3ssel8r's rain shader, has one severity from drizzle to downpour. It brings a murky, even overcast under which cast shadows from the sun and moon soften with severity and are gone in a downpour (drifting cloud shadows fade out as it sets in) and a cool, hazy grade; sparse one-pixel streaks in two layers that hold still and are redrawn about ten times a second, for a stepped, low-frame-rate flicker, leaning with the wind's direction across the screen and its strength; a soft light haze along the upper outlines of stone and trunks; occasional three-stage splashes (a dot, a little crown, two droplets) on stone and bark, and small ones scattered on crowns and the lawn; ripple rings on the water; and upward-facing leaves bobbing as drops hit them. Splashes are hashed on cells of the surfaces themselves, rebuilt from depth, and the two streak layers drift at their own depths as the camera turns, so the rain stays with the world as the camera pans or turns.

Lightning flashes now and then in rain and downpour, more often the heavier the rain, and can be switched off in the menu. Each flash is a second light from a random direction, anywhere around the sky between 30 and 65 degrees up, with its own shadow map, so it casts hard shadows through the rain: lit faces go a cold blue-white and the shadow side stays storm-dark. As in the video it comes on at full in one frame and sometimes dips and comes back once. It then fades over most of a second, blended over the sun or moon by its strength, so its shadows lighten a quarter band at a time into the scene's own light instead of snapping off.

Weather sound is synthesised as it plays rather than read from recordings, so each layer follows its setting continuously ([miniaudio](https://github.com/mackron/miniaudio) opens the output). Wind is mostly a mild rustle of leaves, a soft band of noise that swells with each gust and flutters gently within it, over a quiet low rush; nothing in it starts suddenly, so it never clicks. Rain is a steady hiss and a patter of drops that grows with the severity, plus a low roar in a downpour. Each lightning flash is followed by thunder, panned toward the side of the screen the flash lit from: a crack and a deep rumble when it is close, and later, quieter and lower when it is far off. The menu has volume sliders for each, and M mutes. `--audio-wav out.wav` renders 20 seconds of the weather set on the command line, with a near and a far thunderclap, so the sound can be checked without running the game.

God rays follow t3ssel8r's: the sun's shadow map is sampled through the air rather than ray-marched. In the post pass the air in front of each pixel is cut into 32 level slices from the ground to the treetops, and each slice is one lookup in the shadow map. Light reaches a slice only through gaps in a canopy above the frame, a tiling pattern fixed in the light's own space, so it falls in separate shafts along the sun, and the scene's own shadows (crowns, trunks, pillars) cut the shafts further. The share of lit air is cut into three hard steps of warm haze, so the beams keep crisp pixel-art edges, and each pixel's slices are offset by a world-anchored pattern so the steps never line up into streaks. The rays come only from the sun: strong in the morning and at dusk while it is low, barely there with it high, and stronger looking toward it. A drizzle catches the light and brings them out more; from the Rain setting up they are gone. Cloud shadows block them where they fall. The menu's God rays checkbox or `--no-rays` turns them off.

Fireflies (off by default; the menu's Fireflies checkbox or `--fireflies`) wander slowly round the lawn at knee to head height, about forty of them, each blinking on its own beat: lit for a second or two out of every few, easing on and off. Their paths and blinks are fixed functions of time. Each lit one is a pale yellow-green pixel or two in a faint halo, drawn into the fire's heat buffer and depth-tested against the scene. Their light follows t3ssel8r's dynamic particle lighting: rather than in every shader, it is lit in the post pass from the depth and normals already drawn, like deferred light volumes, so many small lights cost little. Faces turned to a firefly within a couple of metres move smoothly toward their colour in its yellow-green light. As with the campfire, direct sun drowns it out (N·L to the sun, its shadow map and the cloud shadows say how much each pixel already gets), so by day the glow shows only in the shade and at night it lights everything round them.

A small campfire burns on the lawn left of the pond: a scorched dirt patch, a ring of rough stones and four logs leant into a teepee. The fire follows t3ssel8r's procedural pixel art fire: a plain particle system of about a hundred particles drawn as soft discs into a heat buffer, which the post pass cuts into three flat colours (a pale yellow heart inside orange inside a crimson rim), with embers as single red pixels drifting up on the wind. Its three parts step at their own low frame rates (the core at 8 fps, the flames at 12, the embers at 10) so it moves like hand animation, and flames cool as they rise so the top breaks into separate tongues. A slower layer of very faint smoke starts well above the flames, swells and drifts off on the wind, drawn as a two-step haze over what is behind it. The fire lights the scene around it as in the video: faces turned to it go a soft orange while faces turned away keep the night's colours, and the light fades smoothly with distance through pinks into the night, breathing gently rather than jumping with the flames. It casts hard shadows from a cube of distances drawn once round the firelight (the scene does not move), so stones, logs, rubble and trunks throw shadows away from it. Direct sun drowns it out, so by day it shows in the shade round the fire (a tree's shadow over the fire pit fills with its glow) and not on sunlit lawn; at night it lights everything near and carries farther. Rain and Downpour put it out: over about eight seconds in rain and three and a half in a downpour its flames grow fewer, smaller and cooler, its light fades, a few red coals glow in the logs at the end, and it smokes harder as it dies. Once out it wisps smoke from the wet logs for about twenty seconds. It stays out until the menu's Relight fire button relights it (not while it is still raining hard), catching again over a couple of seconds. The Campfire checkbox or `--no-fire` hides it altogether, and `--fire-level L,S` holds it part-burnt (L, 0 out to 1) with smoke S for captures.

Small lights without shadows (fireflies, candles, and campfires beyond the sixteen brightest) are point lights lit in the post pass, like the firefly light: the CPU cuts the screen into 16-pixel tiles and lists, per tile, the lights whose reach touches it, and each pixel tries only its tile's lights, so hundreds cost little. A campfire that loses its firelight slot to a brighter one fades over a third of a second into a point light (and the other way), so no light jumps. A candle is a fire of a fifth the size: the same flames scaled down, plus a point light that carries about half as far. Rain puts out candles open to the sky in about two seconds; those under a roof burn on. `--candles` stands candles on stone ledges and the lawn of the ruins, and with `--shelter-test` two more under its roof.

Grass fire follows t3ssel8r's fire, rain and grass: the lawn is a grid of quarter-metre cells kept in render textures on the GPU, stepped twenty times a second by fragment-shader passes that ping-pong between two textures. Each cell knows how hard it burns, how much dry grass it has left and how wet it is. A burning cell sets the dry cells round it alight at random, sooner the more fire is near and faster downwind, flares up, eats its grass in a few seconds and dies down, so the fire runs outward as a ragged ring with burnt ground behind it. Burning cells paint a burn texture that only ever darkens, pushed about by a fixed noise so the burnt edge is ragged; the grass and the lawn read it, so blades go a scorched brown and then char black and burn down to stubble, each at its own point. The flames are drawn straight from the grid into the campfire's heat buffer and cut into the same flame colours: per burning cell a low glow and two short tongues that step at 12 fps, now and then a puff of smoke, sparks flying up, and a few red embers glowing on the burnt ground after. Each burning patch of two metres lights the scene through a point light of its own, set at the middle of its fire, which glides as the fire moves; the grid is read back a step late without stalling, and everything drawn from it is blended between its last two steps, so the light changes smoothly every frame. Rain soaks every cell open to the sky and puts the fire out in three or four seconds; wet grass will not catch, and cells under a roof stay dry. `--grass-fire X,Z` sets the lawn alight at a point (it can be given more than once; the ruins' campfire lawn is at about -6,-2), G does it at the middle of the view, and `--rain-from S` keeps a `--settle` run dry for S seconds before the `--rain` starts. Without a grass fire none of it runs and the scene is drawn by the same shaders as before.

Trees burn on the same grid. Each cell also holds the tree crown over it, if any - its leaves taken from the leaf clusters in it - and the crowns are a second layer stepped alongside the grass. A crown warms while grass burns under it and catches once warm through, or from a burning crown next to it, spreading faster downwind; it takes hold more slowly than grass, burns for longer and rains embers on the grass below. Its flames, smoke and sparks rise from near the top of the leaves, pulled toward the camera so they show through the crown, and each burning crown patch adds a stronger point light up among them. The burn texture gains a second channel for the crowns: leaves char and then fall one by one, the crown body burns away in ragged holes (in the sun's shadow map too, so light comes through as it goes), and the trunk underneath blackens, leaving a bare charred tree. Rain puts crowns out as it does grass, the water brush douses them, and the regrowth brush grows the leaves back.

A fireball, like the one t3ssel8r rolls through his grass, spreads the fire wherever you take it. F sets one rolling from the middle of the view, away up the screen; I, J, K and L push it up, left, down and right on screen, and the view follows it (C toggles that; panning stops it). It rolls over the ground and down slopes of its own accord, slides along and bounces off anything standing more than a step high, and sets the grass alight along its track. It is a small fire whose base moves with it, so its flames trail behind and its light moves smoothly with it. Rain puts it out in about two seconds, and rolling into the pond at once; it rolls to a stop, smokes and is gone. `--fireball X,Z,DEG[,TURN]` rolls one for captures and the bench, from X,Z pushed toward compass bearing DEG and turning TURN degrees a second (`-6,-1,90,-50` loops round the campfire).

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
| G | Set the grass alight at the middle of the view |
| F | Roll a fireball from the middle of the view |
| I / J / K / L | Push the fireball up, left, down, right on screen |
| C | Follow the fireball with the view, or stop |
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
    [--scene ruins|hills] [--hour H] [--clouds C] [--rain R] [--no-fire] [--fire-level L,S] [--no-rays] [--fireflies] [--no-lightning] [--flash AZ,EL[,LEVEL]] [--mute] [--audio-wav out.wav] [--wind DEG] [--wind-strength S]
    [--shelter-test] [--extra-fires N] [--candles] [--grass-fire X,Z] [--fireball X,Z,DEG[,TURN]] [--settle S] [--rain-from S]
    [--menu] [--ui-shot out.png]
    [--bench N] [--bench-paused]
    [--yaw degrees] [--slide metres] [--target x,y,z]
    [--min-dist metres] [--hide-grass] [--magenta] [--ndl]
    [--test-colours] [--no-tufts] [--no-edges] [--stamps] [--no-depth] [--time seconds]
    [--self-test] [--no-vsync]
```

`--bench N` renders N frames at the window's size with vsync off and the day running, and prints the wall-clock time per frame; `--bench-paused` holds the time of day, which lets the shadow map be reused. Close other GPU-heavy apps first (a video playing in a browser skews it badly). Set `PXE_BENCH_PASSES=1` for rough per-pass GPU timers.

`--self-test` renders twice (the low-res image must be identical), nudges the camera by a fraction of a texel (still identical), then by one texel (the image shifts by exactly one pixel).
