# desert-poc handoff

## Cycle 1
Build: pass

Clean check (`cmake --preset check --fresh`, `cmake --build --preset check --clean-first`): pass. The only warning is stb_image_write's `sprintf` deprecation.

### Changes
- `engine/include/pxe/mesh.hpp`: `kSurfSand = 11`, `kSurfCount` 11 to 12, on the end so no index changes meaning.
- `engine/src/gltf.cpp`, `engine/include/pxe/gltf.hpp`: material names starting `Sand` load as `kSurfSand`. The header comment's list now has Oak and Sand.
- `engine/src/scene.cpp`: Sand's default palettes (shadow, mid, lit; band, detail and tip the same), following how the stone's shift:
  - day `9c7c6c d6b076 ecca8e` (mid is the decided sRGB 214, 176, 118)
  - golden hour `8a6066 d49a5a f4bc66` (orange)
  - twilight `5a4a68 7c6680 987a8c` (pink-mauve)
  - night `2e3456 3e4666 50587a` (cool blue-grey)
- `engine/src/scene_geometry.cpp`, `engine/include/pxe/scene_geometry.hpp`: `buildSceneMaps` puts sand into `scene.ground`, as it does grass and dirt.
- `engine/shaders/common.glsl`, `engine/shaders/edge.frag`: `kSurfaces` 11 to 12, `kSurfSand = 11` in common.glsl.
- `engine/shaders/mesh.frag`, `engine/shaders/edge.frag`: the normal target's code for an actor's creased material, `kSurfActorCrease`, moves from 11 to 12. Sand now takes 11. The edge pass decodes the code to an integer and turns a crease back into stone before anything else reads it, so nothing else sees the change. Codes 13 and 14 are still free under the leaf sprite's 15.
- `desert/CMakeLists.txt` (new): `pxe_desert` from `src/main.cpp`, `src/player.cpp` and `src/world.cpp`, linked to `pxe_engine` only, with `PXE_ASSET_DIR` set to `desert/assets`.
- `desert/src/world.hpp`, `desert/src/world.cpp` (new): the world.
  - `duneHeight` is three layers of wind-aligned ridges. Wind headings are 200°, 214° and 186° (toward about -z). Wavelengths are 72, 38 and 23 m and heights 3.6, 1.5 and 0.6 m, each swelling between 70% and 100% along the crest. Crest lines wander. The profile is a windward rise to a sharp crest (a kink), then a steep downwind face easing into the trough.
  - Over the ground the height runs 0 to 4.56 m, and slopes reach about 27.6°.
  - `groundHeight` is the drawn surface: `duneHeight` at the 1 m grid's corners, flat across each triangle, with the same diagonal split as the mesh. The eye therefore sits exactly 1.7 m above the faces drawn.
  - `buildWorld` makes a 300 x 300 m Sand grid. Every triangle has its own face normal, giving 180,000 triangles. The function sets `sunEast = -x` and `sunSouth = -z`, loads `date_palm.glb` and raises it to `groundHeight(0, 0)` = 0.44 m. It then calls `buildSceneMaps` and `uploadScene`. Nothing is scattered.
  - The phases were chosen by searching (script in the scratchpad) for three things: a slope of about 10° at the palm's spot, on the windward side of the dune that rises behind it; no ground above the sight line from the spawn eye to the trunk base; and a spawn ground 0.68 m above the palm's.
- `desert/src/player.hpp`, `desert/src/player.cpp` (new): `desert::Player` holds the eye, yaw and pitch.
  - `spawn` stands the eye on the ground.
  - `update` turns with pitch clamped to ±85°. It walks at 3.5 m/s, or 6 m/s sprinting, with diagonal and stick input capped at length 1. It clamps x and z to ±80 m, so the player slides along the edge. It pushes the eye radially out of the palm cylinder, so the player slides round it. It then eases the eye to ground + 1.7 m at rate 10/s.
  - `aim` sets the `FirstPersonCamera`.
- `desert/src/main.cpp` (new): the flags, the window, the renderer, the world and the camera.
  - The camera keeps the default 60° field of view and 300 m far plane.
  - The spawn is (0, -25), facing the palm (yaw 0), with pitch 0.
  - In `--capture`/`--window-png` it renders 4 frames at t = f/60 in a hidden window.
  - The play loop follows the example: Escape releases the cursor, Escape again quits, and a click recaptures it. The player updates only while the cursor is captured, and dt is clamped to 0.05 s.
  - `readInput` holds the input actions:
    - mouse at 0.0015 rad per point;
    - right stick at 2.4 rad/s turn and 1.6 rad/s tilt, pushed up to look up;
    - WASD plus the left stick to walk;
    - Shift or the left stick held in to sprint.
  - Lighting is `lightingAt(scene, hour, Weather{})`. There is no clock, weather, fire or audio.
- `CMakeLists.txt`: `add_subdirectory(desert)`.
- `tools/build.sh`: adds a `desert` run target, and its help and binary listing include `pxe_desert`.
- `docs/architecture.md`: the project is described as an engine plus two games, with each game's assets and style doc. "The game" in the rules means each game. The include rule now covers `desert/`. New rows cover the Sand surface (engine) and, for `desert`, the world, the player, the input actions and the command-line modes.
- `docs/engine-guide.md`: the root CMake adds both games. The Asset contract's Sand no longer has the "(added by the desert-poc task)" note. `kSurfSand` is in the surface list and the `mesh.hpp` reference, and sand is in `buildSceneMaps`' ground list. Walking points at the desert's player as a walker with collision.
- `README.md`: the repository layout (engine and two games), sand in the surface list, and a new "The desert game" section covering the dunes, the palm, the spawn, the controls, the flags and the build. The Build section and `tools/build.sh` usage now include `pxe_desert`.
- `CLAUDE.md`: both games with their binaries and asset folders, both in the build commands, the desert flags, and `pxe_desert --capture` as a standard byte-identical case.
- `docs/tasks/desert-poc/capture_hour10.png`, `capture_hour16.5.png`, `capture_hour19.png`: spawn captures.
- `docs/tasks/desert-poc/asset-requests.md` (new): REQ-001.

### Verification
- **Byte-identical against the cycle base 2f89e9b.** I built it from `git archive` in the scratchpad (`dbase`). These cases match in both PNG and log, apart from the line naming the output file:
  - the game: default, `--scene hills`, `--hour 21 --foxes 3`, and `--hour 21 --candles --shelter-test --extra-fires 6`;
  - the example: `pxe_minimal --capture` and `pxe_minimal --first-person --capture`;
  - `--self-test`: its log is identical and both exit 0.
  
  The engine and example code at 2f89e9b are identical to 1fc8c9c, whose only later change is documentation.
- **Against the 4b0c221 build** at `scratchpad/base`: the same game cases and `pxe_minimal --capture` all match. That build has no `--first-person`.
- **Determinism.** Two `pxe_desert --capture` runs give identical PNGs (cmp). The debug (`build-check`) capture is identical to the release one.
- **Flags.** `--help` exits 0. `--hour 25` and unknown flags print usage and exit 1. `--window-png` writes 1920x1080. `tools/build.sh desert -- --help` runs it.
- **Captures.** The whole palm is in frame at all three hours.
  - At 16.5 the trunk's thin shadow runs across the sand to the view's right.
  - At 10 it falls to the left: morning sun from the right.
  - At 19 the sun is about 12° down, so the scene is in night colours and the sand is one flat blue-grey.
- **Frond speckle: yes, they speckle.** At 25 m each of the 15 blades per side is about 1 px wide or less, drawn as broken dotted diagonals with sky between them, and the drooping outer fronds' tips dissolve into single pixels. The ribs and the inner blades hold as shapes. The eye isn't texel-snapped under perspective, so I expect these to crawl when turning and walking; captures can't show that. I raised REQ-001 (non-blocking) for the decided cut to about 6 broader blades per side. The user should judge it from `capture_hour16.5.png` first. A 4x crop of the crown is at `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/ec57ae00-c324-42d2-ba94-85bb37f58fcd/scratchpad/desert/crown_zoom.png`.

### Issues addressed
- None (cycle 1).

### Layering
- Engine:
  - `pxe::kSurfSand` (`kSurfCount` 12).
  - glTF material `Sand`.
  - Sand's default palettes in `Scene::Scene`.
  - Sand in `buildSceneMaps`' ground.
  - Shader surface constants raised, and the internal normal-target code for an actor's crease moved 11 to 12.
  
  Nothing else changed in the engine. No game content is named in it ("desert ground" is a generic noun, like "campfire").
- Game (`desert/`, namespace `desert`):
  - `desert::World` (`world.cpp`) owns the scene, the dune ground and the placed palm.
  - `desert::Player` (`player.cpp`) owns the eye, walking and collision.
  - `main.cpp` owns the input actions, the flags, the fixed hour and the `FirstPersonCamera`.
  
  It includes only engine headers. Nothing is shared with `game/`.
- Misplaced code touched: none.

### How to playtest
Run `tools/build.sh release desert` (or `./build/release/desert/pxe_desert`).
1. **Start.** You stand on sand facing the palm 25 m ahead, looking level, with the whole palm in view. The cursor is hidden and held.
2. **Look.**
   - Moving the mouse right turns right, and moving it up looks up.
   - Looking straight up or down stops at about 85°, with no flip.
   - Check the sensitivity feels reasonable. It is a fixed constant (0.0015 rad per point, as in the example).
3. **Walk.**
   - W, A, S and D move forward, left, back and right relative to the view direction.
   - Diagonals are no faster than straight.
   - Holding Shift sprints, clearly faster: 6 against 3.5 m/s.
   - There is no jump, and Space does nothing.
4. **Dunes.**
   - Walk up and over a ridge, especially down its steep downwind face (toward -z, behind the spawn).
   - The eye should rise and fall smoothly, with no jolt at the crest kink.
   - It should never dip into the sand.
5. **Palm.**
   - Walk straight into the trunk. You stop about 0.7 m from its centre, with no bark clipping through the view.
   - Walk into it at an angle. You slide round it.
   - Look up from beside the trunk: every frond shows from below, with none missing because of back-face culling.
6. **Play-area edge.**
   - Walk about 105 m forward (north) from the spawn, or 80 m to either side. You stop at the edge of the middle 160 x 160 m.
   - Walking along it at an angle slides along it.
   - Try a corner.
7. **Cursor.**
   - Escape shows the cursor and freezes looking and walking. WASD and the mouse do nothing.
   - Clicking in the window captures the cursor again and play resumes.
   - Escape twice (release, then quit) closes the game.
8. **Stall.** Drag the window around for a few seconds while holding W. When you let go, the player has not jumped forward.
9. **Gamepad.**
   - The left stick walks, at analogue speed for a partial push.
   - Pushing the right stick up looks up, and pushing it right turns right.
   - Clicking the left stick in and holding it sprints.
   - The gamepad does nothing while the cursor is released.
10. **Hours.**
    - `--hour 10`, `--hour 17.6` (golden hour) and `--hour 18.2` (twilight) show the sand ochre, orange-rose and pink-mauve.
    - `--hour 21` shows the night blue-grey.
    - The clock doesn't move.
11. **Frond look.** Turn slowly while looking at the crown from the spawn. Do the fine blades crawl? That decides REQ-001.

### Known limitations
- **The ground's edge is not hazed in all directions.** The haze starts at 60% of the far plane, 180 m from the eye. From the spawn, the ground's edge is 125–175 m away. From the play-area edge it is only 70 m away. So the sand doesn't fade before it ends: where it stops there is a pale horizon-coloured strip just below the horizon line. From the spawn it shows only as a thin wedge at the right. From a corner of the play area, looking out, the dunes end at a crisp line at the horizon. I checked this with a temporary capture from (-80, 80), with the code restored afterwards.

  The task's "300 × 300 m ... so it reaches the haze" and the 300 m far plane don't fit with the 160 m play area. Fixing it needs a decision:
  - a larger ground (a half-width of at least 80 + 180 = 260 m for the haze to have started at the edge, or 380 m for it to be complete), or
  - a shorter far plane.
  
  I left both as decided.
- **REQ-001 (non-blocking):** the frond blades speckle (see Verification).
- **The faceted dunes look jagged.** The two triangles of each 1 m cell take their own tones, so band edges are saw-toothed. Looking along the dunes from far out, the slopes show a fine checker of light and dark triangles, which is strongest at grazing angles. This is the "faceted by a 1 m grid" look as specified, but the user may find it busy. Smooth per-vertex normals, as the example's ground uses, would give smooth band edges if that's preferred. It is a game-side change in `appendGround`.
- **The orange shows little at the lowest sun.** With the sun under about 5–10° (for example at 17.6), almost all the sand falls into its shadow tone, a rose `8a6066` in golden hour, and the orange mid tone shows only on slopes facing the sun. This is the engine's ramp thresholds with a low sun, the same as for other surfaces, and the palette follows their violet-shadow shift.
- **The collision radius is wider than the trunk.** The palm cylinder is 0.40 m (assets.md) plus a 0.3 m body radius (`kBodyRadius`), so the eye stops 0.7 m from the axis. That keeps the 0.1 m near plane well clear of the bark. The task names only the trunk's radius, so this extra radius is my choice.
- **The gamepad can't release the cursor or quit.** Nothing in the task assigns those to it. Escape and the click are keyboard and mouse only.
- **Looking and walking pause whenever the cursor is released**, so a gamepad-only player must keep the cursor captured. If the window loses focus, the cursor stays in whatever state it was in.
