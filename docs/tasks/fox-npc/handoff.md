## Cycle 1
Build: pass (fresh Debug build directory, `cmake -S . -B <fresh> -DCMAKE_BUILD_TYPE=Debug && cmake --build <fresh> --parallel`. The only warnings are the existing ones: stb_image_write's `sprintf` deprecation in main.cpp and the linker's duplicate-libglfw note. None are new.)

### Changes
- engine/include/pxe/skinned.hpp, engine/src/skinned.cpp: `kActorMaterials` raised from 8 to 16. `SkinnedModel::load(path, int firstMaterial = 0)` gains a first-slot parameter, plus a `firstMaterial()` accessor. At load time each vertex's surface becomes `kActorSurface + firstMaterial + material`. Material indices are clamped to the table's slots from `firstMaterial` on, and only that many materials are read. An out-of-range `firstMaterial` throws `std::invalid_argument`. Why this approach: the slot belongs to the model, the same way its material order does, so it is set once at load and baked into the skin vertices. `skin()` is unchanged and costs nothing extra per frame. The caller then writes `materials()[i]` into slot `firstMaterial() + i`. Remapping surfaces after every skin would have repeated work every frame, and a parameter on `skin()` would have to be kept consistent with the material writes at each call site. The default of 0 leaves the hero's load exactly as before.
- engine/shaders/mesh.frag: added `const int kActorMaterials = 16`. `uActorColour`/`uActorGlow` are sized by it, and the material clamp is now `0..kActorMaterials-1`. engine/src/renderer.cpp needed no change: it already iterates `params.actorMaterials` and uploads `kActorMaterials` entries. I checked the other passes. Shadow (shadow.vert/frag only tests surface 4), edge.frag (actors already write the stone surface id into the normal target) and water/reflection (same mesh program) make no assumption about actor surfaces ending at 23. Nothing else in C++ reads vertex surfaces.
- game/src/hero.cpp: the smear's white-hot core was `kActorMaterials - 1`, which would have moved to slot 15 once the table was raised. It is now an explicit `kCoreSlot = 7`, used both for the material write and for the smear/shard surfaces. The hero writes its model materials only into slots 0..6. Its slots are exactly as before: 0-5 model, 7 core.
- game/src/fox.hpp, game/src/fox.cpp (new): `game::Fox`.
  - `build()` loads `fox.glb` with `kFirstMaterial = 8`. Like `Hero::build`, it throws on a missing file (via `SkinnedModel::load`). It also throws if `Fox_Rest` is missing, because the rest pose alone is the standing fox.
  - `spawn(scene, xz, yaw)` sets the height from `scene.ground.heightAt` (smooth ground height).
  - `update(dt)` advances the clip clock, wrapped to the clip's length.
  - `pose(actors, materials)` samples `Fox_Rest` looping, skins at 1.6x, pushes the mesh onto `actors` and writes its 4 materials into slots 8-11.
  - It has no movement, AI, collision or input.
- game/src/scene.hpp, game/src/scene.cpp: `game::kRuinsCampfire{-5, -6}` is now shared. `buildRuins` places the campfire from it, so the fox's facing can't drift from the fire. Placement is unchanged, and default captures are pixel-identical.
- game/src/main.cpp:
  - Added `--fox` / `--no-fox` (`Options::fox`, -1/0/1 like `hero`) and a line for them in `--help`.
  - `kFoxSpot{-6.4, -6.5}` is 1.49 m from the fire. `addFox()` builds and spawns the fox there facing the fire (yaw = atan2 toward the campfire), and catches and logs `pxe: no fox: ...` the way the play loop handles the hero.
  - Capture path: the fox is built only with `--fox` and not in `hills`. Its clock is set to the capture's `params.time` (includes `--time` and `--settle`), then it is posed.
  - Play loop: the fox is built unless `--no-fox` or `hills`, `fox.update(dt)` runs each frame, and `fox.pose(params.actors, params.actorMaterials)` runs next to `hero.pose`. That covers shadows, edges and reflection.
  - Bench path: no fox, the same as the hero.
- game/CMakeLists.txt: added src/fox.cpp.
- README.md:
  - New paragraph on the two characters: the fox's look, the fact that it is set dressing, its flags and capture behaviour, its 1.6x scale, and the 16-slot table split (hero 0-7, fox 8+).
  - A note in "The Blender scene" on the character scripts, with the build_fox.py command.
  - `[--hero] [--no-hero] [--fox] [--no-fox]` added to the usage block.
  - Note: the README had no hero documentation before this, so there was nothing to sit "alongside". The paragraph introduces both characters briefly.

### Placement check
- I raycast `ruins.glb` in headless Blender over a 5 x 5 m grid round the fire. Everything from x -7.5..-2.5, z -8.5..-4.75 is lawn (Ground/Grass). Paving starts around x > -3, z > -4.5. The nearest other objects are the Rock (bbox x <= -7.36, about 0.5 m beyond the fox's far edge), BushD (z >= -2.75) and PondTrunk (z <= -7.68 at its widest, base not on that spot).
- The curl at 1.6x is about 0.61 x 0.78 m and lies radial to the fire, so it spans about 1.1-1.9 m from the fire's centre. The stones reach about 0.8 m and the dirt disc 0.96 m, so the curl is clear of the ring, logs and dirt.
- Ground at the spot is about 0.13 m. It rises toward -z at about 7.5%. That slope runs across the fox's width (about ±2.3 cm at its edges), which is within the asset's 3.5 cm sink.
- Captures: `--capture x.png` without `--fox` is pixel-identical to the pre-change build. With `--fox`, it differs only in a 17x13 px box at the fox. A `--hero --hero-attack 0.12` capture (mid-slash, smear on screen) is pixel-identical before and after the table change, which confirms the hero's slots and smear are unaffected.

### Issues addressed
- None (cycle 1).

### How to playtest
- `./build/game/pxe`: in the default ruins view, the fox lies curled on the lawn just left of the campfire and a little behind it. It is side-on, with its face (nose under the cream tail tip) toward the fire. Watch for a slow breath every 4 s, an ear flick at about 2.4-2.85 s into each loop, and a slow tail-tip stir. The loop should show no hitch at the wrap.
- Walk the hero (WASD/stick) round it and through it. Nothing happens, by design (no collision). The hero's colours and its sword smear (white core, cyan trail) should look exactly as before, and the fox's colours shouldn't change while the hero attacks.
- Rotate the view with Q/E: the fox casts a shadow, takes outlines like the hero, and is lit by the campfire at night (try `--hour 22`). Where the view puts the pond behind it, check the reflection.
- `--no-fox`: no fox. `--scene hills` (with or without `--fox`): no fox.
- Captures: `--capture a.png` has no fox. `--capture a.png --fox` has one. `--fox --hide-grass` shows the whole curl (see the first limitation).

### Known limitations
- **Needs a decision:** lawn grass sprites cover the lower third or so of the curl. The fox is only about 0.5 m tall, and grass blades are depth-tested as if upright, so blades rooted in front of and under it draw over it and break up its outline at pixel scale. It is clear with `--hide-grass`. The hero has the same effect at its feet, but there it is a small fraction of the figure. One fix is to drop grass sprites under the fox's footprint, the same way grass under stone and in fire pits is dropped. But the grass is built with the scene, before the fox exists, and doing it only when the fox is shown means either passing the fox spot into `BuildOptions`, or a renderer/grass mask. Doing it always would change existing captures. That is outside this task's scope, so I left it alone. The orchestrator or user should decide.
- At the default view the fox is small (about 15-20 px across at 24 px/m), so the face and ear flick read only faintly. That comes from the asset's real size at 1.6x, as specified.
- `--extra-fires N` (a test option) scatters bare flames at least 1.5 m from other fires but doesn't know about the fox, so one could land on it. `--candles` keeps lawn candles 2.5 m from the campfire, so at least 0.6 m from the fox. I left both unchanged because changing them would alter existing test captures.
- In captures, `--time` sets both the wind time and the fox's clip time, so the breath phase follows `--time`.
- The hero writes only slots 0-6 from its model. If the hero's glb ever gains more than 7 materials, its extra vertices would land in slots 8+ and share the fox's colours. Today it has 6.
- No asset requests raised.

## Cycle 2
Build: pass (fresh Debug build directory. The only warnings are the existing stb `sprintf` deprecation and the linker's duplicate-libglfw note.)

### Changes
- game/src/fox.hpp, game/src/fox.cpp:
  - `kSize` is now a public `Fox::kSize = 2.4f`.
  - New `Fox::kFootprint = 0.68f`: the assets.md circle covering the revision-3 curl at 2.4x.
  - `spawn()` now tilts the curl to the ground's normal. It samples `heightAt` 0.41 m either side in x and z, and the centre stays on `heightAt`. The revision-3 curl rests at y = 0 with no sink, and the lawn there slopes about 8%, so at 2.4x the downhill rim would otherwise float about 5 cm.
- game/src/scene.hpp:
  - New `kRuinsFoxSpot{-6.65, -6.95}` next to `kRuinsCampfire`. It is the single source of truth for the spot, with its clearance figures in the comment.
  - `BuildOptions` gains `foxSpot` / `foxRadius` (radius 0 means no fox).
- game/src/scene.cpp: when `foxRadius > 0`, lawn sprites (clumps and tufts) rooted within the radius are dropped right after the fire-pit drop and before leaves are added. That is the same `erase_if` mechanism, applied after scattering, so the rest of the grass is identical.
- game/src/main.cpp:
  - Replaced `addFox`/`kFoxSpot` with `foxShown(opt)`: never in hills, the bench or the self-test; captures only with `--fox`; play unless `--no-fox`.
  - One `game::Fox` is now loaded before the scene build (errors caught and logged as before). `build.foxSpot`/`foxRadius` are set only if it loaded. It is spawned facing the fire after the scene is built, and both the capture path and the play loop use it.
- README.md: the fox paragraph now says about two metres from the fire, 2.4 times real size, and that grass is cleared under it only when it is shown.

### Issues addressed
- ISSUE-001: grass under the fox's 0.68 m footprint is dropped only when the fox is shown. Runs without the fox are pixel-identical to HEAD.
- ISSUE-002: the fox is drawn at 2.4x and moved to (-6.65, -6.95), 1.90 m from the fire, still facing it. The curl circle is 0.26 m clear of the dirt patch (more from the stones and logs) and 0.46 m clear of the rock, the nearest scene object.

### Verification
- Capture diffs against a HEAD build:
  - Identical: default ruins, `--hero --hero-attack 0.12`, `--scene hills`, `--candles --extra-fires 6`, and `--scene hills --fox` (against HEAD's hills).
  - `--fox` differs from HEAD only in a 34x21 px box at the fox. With the grass gone, the curl reads as one whole ball.
- `--ui-shot` play runs:
  - The default logs "17 grass sprites under the fox dropped".
  - `--no-fox` and `--scene hills` log no fox and drop no grass.

### How to playtest
- `./build/game/pxe`: the fox is curled asleep on the lawn left of the campfire and a little behind it, about two fire-ring widths from the stones. Check that:
  - it is noticeably bigger than in cycle 1 (1.5x, about 1.05 x 0.85 m);
  - its face (nose behind the brush) points at the fire;
  - no grass blades draw over it;
  - it sits on the slope without a gap under its downhill edge.
- Watch a few 4 s loops: a slow breath, an ear flick at about 2.4-2.85 s, a tail-tip stir, and no hitch at the wrap. Also check the revision-3 curl: legs attached, head on the ground, tail across the face.
- Q/E to rotate: look at its shadow, outline, and campfire light at night (`--hour 22`). Look at the bare patch where the grass was cleared from other angles. It is a 0.68 m circle, mostly under the curl, but its rim may show as a thin ring of bare lawn round the fox, especially on the side away from the camera.
- `--no-fox`: no fox, and the lawn is fully grassed there. `--scene hills` (with or without `--fox`): no fox.
- Captures: `--capture a.png` has no fox and is unchanged. `--capture a.png --fox` has the fox with its grass cleared.

### Known limitations
- The cleared circle (0.68 m) is a little larger than the curl's actual outline (1.05 x 0.85 m box), so up to 0.16-0.25 m of bare lawn can show beyond the ball's flatter sides. Matching the outline exactly would need an elliptical or oriented clear. I used the circle from assets.md as instructed.
- The tilt follows the average slope across the footprint, not small bumps under it.
- `--extra-fires N` (a test option) still doesn't avoid the fox (unchanged from cycle 1).
- The hero writes only slots 0-6 from its model. If its glb ever has more than 7 materials, the extras would reach the fox's slots. Today it has 6.
- In captures, `--time` drives the fox's breath phase as well as the wind.
- No asset requests raised.
