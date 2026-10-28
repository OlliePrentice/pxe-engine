# First-person desert game: proof of concept
Type: feature
Status: open
Cycle: 2

## Goal
A second game on the pxe engine, separate from `game/`. It is first person, in the same art style, and set in a desert. This proof of concept gets the camera and controls right: the player walks over procedural sand dunes at eye height, with no body, arms, tools or HUD, and the scene is open desert with a single date palm. It uses the perspective camera from task `perspective-camera` and starts only after the user has approved that task's look gate. It also makes the project's docs and tooling hold more than one game, with each game owning its own art style doc.

## Decisions
- **Name and layout.** Top-level folder `desert/`, executable `pxe_desert`, namespace `desert`, its own `desert/CMakeLists.txt` added from the root `CMakeLists.txt`. `tools/build.sh` gains a `desert` run target. `game/` is not changed.
- **Assets live with the game.** The desert's build scripts go in `desert/tools/blender/` and its exports and previews in `desert/assets/scenes/`. `desert/CMakeLists.txt` points its own asset directory there. The existing game's `assets/` and `tools/blender/` stay where they are.
- **Camera.** `pxe::FirstPersonCamera` at the default 60° vertical FOV and 300 m far plane, with the engine's sky and haze.
- **Scale.** Real-world scale, not the existing game's 1.6× hero scale: the eye is 1.7 m above the ground.
- **Controls.**
  - Keyboard and mouse: WASD moves at 3.5 m/s, Shift sprints at 6 m/s, no jump. The cursor is captured while playing. Esc releases the cursor, and Esc again while released quits. Clicking in the window while released captures it again.
  - Gamepad: left stick moves, right stick looks, and pressing the left stick sprints while held.
  - Look: pitch clamped to ±85°, Y not inverted, sensitivity a constant (no settings menu).
  - The eye height follows the terrain with light smoothing, so walking over dunes doesn't jolt.
  - Movement uses the frame time clamped to at most 0.05 s, so a stall (a dragged window) never jumps the player.
  - Looking and walking pause while the cursor is released, as in the example.
- **Terrain.** Dunes are generated in the desert game's code from a height function, as `examples/minimal` builds its ground. They are long, smooth, wind-aligned ridges: a few layered sine-like ridges, each with a sharper crest on the downwind face, 2–6 m from crest to trough. They are built on a 1 m grid with smooth normals, so the toon ramp cuts each slope into a few clean bands that follow the dune's curve (changed from face normals after cycle 1, when the 1 m facets showed as sawtooth band edges and a triangle checker; user decision). There are no ripples, rocks or scrub. The ground runs to ±380 m, centred on the origin, so from anywhere in the play area its edge is past the far plane and fully hazed (changed from 300 × 300 m after cycle 1, when the edge showed as a hard line; user decision). The middle ±120 m is on the 1 m grid; outside it the cells are 4 m, joined to the 1 m grid without cracks or T-junction gaps. The player is held inside the middle 160 × 160 m. Walking reads the same height function.
- **Sand surface.** A new engine surface, `Sand` (a glTF material name too), with default palettes. The day mid tone is a warm ochre, about sRGB (214, 176, 118). Golden hour pushes it orange, twilight pink-mauve, and night cool blue-grey, following how the existing surfaces' palettes shift. No grass is scattered on it.
- **Date palm.** One static Blender asset at real scale:
  - A curved trunk about 11 m tall, tapering slightly, with stepped rings of cut frond stubs for a faceted, scaly read.
  - About 18 fronds in a full crown, each 3–4 m long: a central rib with a row of blades either side, from flat triangles, the outer fronds drooping.
  - Two or three blocky date clusters hanging below the crown.
  - Materials: trunk and dates `Bark`, fronds `Leaf`, so no new surface for the palm. A `Frond` surface may follow later only if the green looks wrong. No leaf sprites are scattered on the palm.
- **Placement and spawn.** The palm stands at the world origin, on the side of a low dune rather than on a crest. The player spawns 25 m south of it (engine -Z, the default `Scene::sunSouth`), facing it, looking level (pitch 0°). This is also the `--capture` pose. Changed from 14 m and 5° down after the palm was built: the fronds sit 9.3–14.65 m up, out of frame from there (user decision). The glb's origin goes at the ground height at the palm's spot; its trunk runs 0.5 m below that.
- **Frond detail is judged in the first capture.** If the 15 thin blades per side speckle or crawl, the designer cuts them to about 6 broader blades per side as a follow-up asset request (user decision). The sun direction (`Scene::sunEast`, `sunSouth`) puts the palm's shadow across the sand to the view's right at the default hour.
- **Collision.** The palm is a vertical cylinder at its trunk base, with the radius recorded in `assets.md`, widened by a 0.3 m body radius so the near plane never cuts into the bark (developer's choice, accepted in the cycle 1 review). The player slides along it and along the play-area boundary. Nothing else blocks.
- **Time and weather.** A fixed hour from `--hour <0-24>`, default 16.5. The clock doesn't run. There is no weather, rain, wind, fire or audio, and no copy of the existing game's `WorldClock`.
- **Flags.** `--help`, `--capture <png>` (deterministic, from the spawn pose), `--hour <0-24>` and `--window-png <png>`. No self-test, bench or audio.
- **Each game owns its art style doc.** Done by the orchestrator at the start of cycle 1, before the designer runs:
  - `docs/art-style.md` moves to `game/docs/art-style.md`, with its text unchanged apart from the asset contract moving out.
  - The engine's asset contract moves into a new "Asset contract" section of `docs/engine-guide.md`: glTF 2.0 binary, +Y up, 1 unit = 1 m, Blender −Y = engine +Z; surface material names (with `Sand` once this task adds it); node names reaching the game; actor material slots and `kActorMaterials`; skinned-mesh material order and clips looked up by name. Both games' style docs link to it and keep only style. `_ShadowOnly` stays in the existing game's doc: the game, not the engine, reads that suffix (`game/src/scene.cpp`). Done at cycle 1 start.
  - `desert/docs/art-style.md` starts as a copy of the existing game's style doc and is changed for this game: real scale with a 1.7 m eye, the sand palette, palm guidance. What doesn't apply (hero scale, cutaway rooms, the tavern roof and masonry) is dropped.
  - `CLAUDE.md`, `.claude/skills/feature/SKILL.md` and `.claude/agents/designer.md` read the style doc of the game a task is about.
- **Standard byte-identical cases.** `pxe_desert --capture` is added to the refactor cases in `CLAUDE.md`.

## Layering
- Engine: a `Sand` surface: a new `pxe::Surface` value, the glTF material name `Sand`, default palettes for every phase, and the shader surface count raised to match. Nothing else. The existing game's output stays byte-identical.
- Game (`desert/`, namespace `desert`):
  - Owns its world: the scene with the dune ground (from the height function), the palm loaded from its glb and placed at the origin on the ground, the sun direction and the fixed hour.
  - Owns the player: eye position, yaw, pitch, walking, sprint, ground following, collision with the palm cylinder and the play-area boundary. It drives `pxe::FirstPersonCamera` each frame.
  - Owns its input actions (what each key, mouse and pad input does), using `pxe::Input` and the cursor capture from `perspective-camera`.
  - Owns its command-line modes: `--help`, `--capture`, `--hour`, `--window-png`.
  - Follows `docs/engine-guide.md`'s start-up order, frame loop and scene build order.

## Acceptance criteria
- [ ] `desert/` builds `pxe_desert` in namespace `desert`, added from the root `CMakeLists.txt`, includes only engine headers, and sets its own asset directory under `desert/assets`
- [ ] `tools/build.sh` builds and can run `desert` with arguments, and its help lists it
- [ ] `pxe::Surface` has a Sand value, glTF material `Sand` loads as it, and its default palettes match the decided colours. The shader surface constants match
- [ ] The dune height function gives layered wind-aligned ridges 2–6 m high; the ground is Sand with smooth normals, on a 1 m grid within ±120 m and 4 m cells out to ±380 m with no cracks between them, with no grass
- [ ] The palm loads from `desert/assets/scenes/date_palm.glb` at the origin, standing on the ground height there
- [ ] The player spawns 25 m south of the palm, facing it, pitch 0°, with the eye 1.7 m above the ground
- [ ] WASD at 3.5 m/s, Shift sprint at 6 m/s; left stick move, right stick look, left stick press sprint; pitch clamped to ±85°; Y not inverted
- [ ] The cursor is captured while playing; Esc releases it, a second Esc quits, a click captures it again
- [ ] The eye follows the terrain height with smoothing
- [ ] The player cannot enter the palm's cylinder or leave the middle 160 × 160 m, and slides along both
- [ ] `--help`, `--capture <png>`, `--hour <0-24>` (default 16.5) and `--window-png <png>` work. Two `--capture` runs give identical PNGs
- [ ] No weather, clock, audio or `WorldClock` copy in `desert/`
- [ ] The existing game's standard cases and `pxe_minimal --capture` (both modes) are byte-identical to the cycle base
- [ ] `docs/art-style.md` is moved to `game/docs/art-style.md`, `desert/docs/art-style.md` exists, `docs/engine-guide.md` has the asset contract including `Sand`, and `CLAUDE.md`, the feature skill and the designer agent point at the per-game style docs
- [ ] `docs/architecture.md` describes the project as an engine with two games, and its table covers `desert/`. `README.md` documents the desert game, its build and its flags. `CLAUDE.md` describes both games, their binaries, and `pxe_desert --capture` as a standard byte-identical case
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **date_palm**: a single date palm as described in `## Decisions`. Follow `desert/docs/art-style.md`: low-poly, faceted, flat-shaded, one solid colour per material.
  - Script: `desert/tools/blender/build_date_palm.py`, built step by step in the live Blender session, then rebuilt and exported headless from the script. The script is the source of truth.
  - Export: `desert/assets/scenes/date_palm.glb`, with a preview render at `desert/assets/scenes/date_palm.png`, plus a view from below the crown at eye height.
  - Format: glTF 2.0 binary, exported +Y up.
  - Units and axes: 1 unit = 1 metre at real size; Blender −Y is engine +Z.
  - Origin: trunk base centre at ground level.
  - Materials: `Bark` (trunk, stubs, dates) and `Leaf` (fronds) only.
  - Fronds are seen from below in first person, and meshes are drawn with back faces culled. Every frond must show from above and below, as two-sided geometry (a reversed duplicate) or thin closed volumes.
  - Budget: about 3,000 triangles.
  - Animations: none.
  - Record in `assets.md`: the trunk radius at the base (for collision), the overall height, and the crown's extent.

## Out of scope
- Arms, tools, a body, a HUD
- Interaction, inventory, moisture-farming gameplay, NPCs
- Audio, weather, a running clock, fires
- Jumping, a settings menu, sensitivity or invert-Y options
- Moving the existing game's `assets/` or `tools/blender/` under `game/`
- Porting more passes to perspective
- Ripples, rocks or scrub on the sand

## Cycle log
- Look gate approved by the user 2026-10-28.
- Cycle 1: 2026-10-28 started, base 4030291 (art style docs split first, by the orchestrator)
- Cycle 1 review: fresh build clean; existing game and example byte-identical and desert capture deterministic (rerun by the orchestrator). REQ-001 (broader frond blades) done in e1e3506 and checked in an engine capture. ISSUE-001, ISSUE-002 open from user look decisions. The gamepad has no release or quit: out of scope.
- Cycle 2: 2026-10-28 started, base b19bc75, fixing ISSUE-001, ISSUE-002
