## Cycle 1
Build: pass (fresh Debug build directory, `cmake -S . -B <new dir> -DCMAKE_BUILD_TYPE=Debug && cmake --build <new dir> --parallel`; no CMakePresets.json. The only warnings are the existing ones from `third_party/stb/stb_image_write.h` (sprintf) and the linker's duplicate-glfw note. Nothing new.)

### Changes
1. **`kSurfOak` engine surface**
   - `engine/include/pxe/mesh.hpp`: `kSurfOak = 10`, and `kSurfCount` goes from 10 to 11. It is added at the end, so no existing index changes meaning.
   - `engine/src/gltf.cpp` `surfaceFor`: `Oak` (prefix match, as the others) maps to `kSurfOak`.
   - `engine/shaders/common.glsl`: `kSurfaces = 11`, a new `kSurfOak`, and `flatFaced()` includes Oak.
   - `engine/shaders/edge.frag`: `kSurfaces = 11`, a new `kSurfOak`. `timbered()` and `built()` are now `(Wood..Roof) || Oak`, not a range that would have missed index 10. So Oak is outlined, never rimmed, and takes the hard rain haze and splashes.
   - Normal-alpha encoding: Oak writes (10+1)/16. That is below the leaf-sprite code (15) and the actor offset (16), so it decodes correctly.
   - Every C++ array sized `kSurfCount` picks up the new size: `uBand`, `uDetail`, `uTip`, `uFireBand`, `uFlyBand`, and `Lighting`/`Scene` palettes and bands. `main.cpp`'s fire, firefly and flash loops run over `kSurfCount`, so Oak gets its firelight colour automatically.
   - `game/src/scene.cpp` `layOutRuins`: Oak is added to the ruins' `scene.ground` surface list. Nothing outdoors uses Oak today. Roof is still excluded.
2. **Palettes** (`game/src/scene.cpp` `setPhasePalettes`)
   - Oak day is `8a6640 / b48c5a / d4b07a`. Golden, twilight and night use Wood's per-channel, per-band phase/day ratios applied to Oak's day ramp:
     - golden `7f5b39 / a47c55 / e1a864`
     - twilight `5e4834 / 5b5566 / 656388`
     - night `3e312c / 333c5c / 3d4d82`
   - Roof day is `3c4454 / 5a6676 / 7e8a98`. The other phases use Stone's ratios the same way:
     - golden `30344c / 565662 / 938d73`
     - twilight `272a44 / 303762 / 485088`
     - night `161d3f / 1d2a5d / 2e4388`
   - Wood's lines are untouched; `git diff` shows no `kSurfWood` line changed. The palette comment is updated.
   - Only `tavern_exterior.glb` and `tavern_interior.glb` (on `Roof_ShadowOnly`) use `Roof`. I checked this by parsing every glb under `assets/`.
3. **Sconces and candles** (`game/src/scene.cpp` `buildTavern`)
   - New `kRoomSconces`, with the four flame bases from assets.md in room coordinates, untransformed like `kRoomCandles`, and `kSconceSize = 0.35`.
   - Each sconce is pushed as a fire, and its `sheltered` flag is set from the room's shelter, as the candles' are. All four log "sheltered".
   - `kCandleSize` (0.2) is shared with the ruins' `--candles` test option, so it is unchanged. The tavern now uses its own `kRoomCandleSize = 0.3`.
   - The startup log labels the sconce fires "sconce".
   - The hearth and the brazier are unchanged.
   - There are now 13 fires. All are below `Renderer::kFireLights` (16), and those smaller than 1 are drawn as point lights, as before.
4. **Indoor fill light**
   - `engine/include/pxe/scene.hpp`: new `Scene::fillDir`.
   - `engine/include/pxe/lighting.hpp` and `engine/src/lighting.cpp` `lightingAt`: new `Lighting::fill`, computed as `fade(elev, kNightAt, kGoldenAt)`. It is 1 from the golden key (6°) up, 0.5 at the twilight key (-4°), and 0 at the night key (-14°) and below.
   - `engine/src/renderer.cpp` `bindToon`: sets `uFill` (0 unless `scene.indoors`) and `uFillDir`.
   - `engine/shaders/common.glsl` `indoorTone()`:
     - The fill is `uFill * clamp(n·fillDir, 0, 1)`, reaching at most the mid tone.
     - Direct light is screened over it: `tone = fill + lit * (1 - fill/2)`. Shade alone gets the fill, full sun is still 2, and a sunlit patch is always brighter than the fill around it.
     - Flat faces stay unquantised, and the rest are quantised.
   - `engine/shaders/mesh.frag`: applies `indoorTone` only `if (uIndoors != 0)`.
     - `fireLit` still gets the direct-light-only tone (`sunTone`), so the fill doesn't drown the fires by day.
     - Outdoors `sunTone == tone` and the code does exactly what it did before.
   - `buildTavern`: `fillDir = normalize(-0.8, 0.35, -0.45)`. At the 45° design yaw this comes from a little above and from the camera's right (the -x side).
     - The faces the camera sees lift by: -x sides ≈ 0.81, -z sides ≈ 0.46, tops ≈ 0.36.
     - I first tried a higher fill, with tops at 0.5. In a capture it lifted the dark floor so far that the window patches nearly vanished, so I lowered it.
     - Measured at 16:00 with no rain: the floor is (97,71,52) under the fill, against Wood's shadow (76,54,38) before. The window patch is still Wood's lit tone (168,128,88).
5. **Crease ink** (`engine/shaders/edge.frag` `edges()`; `engine/src/renderer.cpp` sets the new `uIndoors` on the edge program)
   - It applies indoors only, to Oak and Wood.
   - A 4-neighbour counts as a crease when it is a mesh pixel with depth within `kSilhouette` (no outline step) and `dot(n, nq) < cos 40°`.
   - To keep the line one pixel wide:
     - If the neighbour is not Oak or Wood, the ink goes on this pixel.
     - Otherwise the ink goes on the side that faces the camera less.
     - If both sides face the camera alike (a table's two near sides at the 45° yaw), the side with the larger normal x takes it.
   - Outdoors `creases` is false, so `edges()` does exactly what it did before.
   - An outdoor capture (default view, 10:00, 17:30 and 22:00) differs from HEAD's build only inside a 48×36 px box in the pond's reflection. That is where the tavern roof's new slate colour is reflected. Nothing else differs.
6. **Wind indoors** (`game/src/main.cpp` main loop): indoors `heard.wind = 0`. Rain is still `*= kIndoorWeather` (0.4). Outdoors both are the settings as they are, and thunder is unchanged. The comments are updated.
7. **Exterior integration**
   - `kTavernSpot`, `kTavernYaw`, `kTavernDoorway`, `kTavernVestibule`, `kDoorHeadroom` and the hinge figures are untouched.
   - `scene.ground` still leaves out `kSurfRoof`. The solids are still every exterior triangle.
   - The shelter still takes Stone and Roof. Only a local variable and a comment were renamed from "thatch" to "slates".
   - Captured the exterior at 14:00, 18:30 and 23:00: the slate roof, cross-gable and dormer read, and the door and walls are as before.
- `README.md`: the tavern paragraph and the materials list now describe the slate roof, Oak, the sconces, the fill light, the crease ink and the silent indoor wind.

### Issues addressed
- None (cycle 1).

### How to playtest
- `--scene tavern --hour 12` (also 8, 16, 17.5):
  - Furniture is light oak against the dark plank floor.
  - On a table or the bar, the top, the right-hand (-x) side and the left-hand (-z) side are three different tones.
  - Thin ink lines run along the bar's and tables' top edges and where legs meet the floor.
- `--hour 16 --rain 0`: the sun patches on the floor near the bar and right of the hearth are clearly brighter than the floor round them.
- Run the clock from 17:00 to 20:00: the fill fades through dusk. At night (`--hour 23`) the room is lit only by the hearth, the brazier, the candles and the four sconces, two on each far wall at about head-and-a-half height.
- Walk outside: the roof is blue-grey slate with a gable over the door. Going in and out still works, through the same doorway and vestibule.
- Audio, in rain with wind up (`--rain 0.6 --wind-strength 3`): outside, wind and rain are both full. Inside, the wind is silent and the rain is quieter (40%). Thunder is the same.

### Known limitations
- **The floor's plank V-grooves are inked too.** Their bevels sit about 50° from the plank tops, past the 40° threshold, so indoors every groove and butt joint draws as a thin ink line, like a plank grid. It makes the planks read, but it adds a lot of lines to the floor. If it is too busy, either change:
  - ink only creases where one side is Oak;
  - raise the threshold past 50°. That would also drop some furniture creases.
  This needs a decision; I didn't guess.
- **Roof's golden lit tone is warm.** Scaled exactly as Stone's (`dcb070` against day `bcac94`), it comes out as a warm grey-gold, `938d73`. Its shadow and mid tones are blue-grey. This follows the "scaled the way Stone's phases are" decision literally. The acceptance line's "blue-grey slate ramps in golden" could be read as wanting the lit tone cooler too.
- **The hero takes no fill light.** Characters use their own `actorTone`, so indoors by day the hero keeps the old shading, while the furniture round them is lifted.
- The fill direction is a constant tuned for the room's fixed 45° yaw. The camera can't turn indoors, so this holds.

## Cycle 2
Build: pass (fresh Debug build directory; no CMakePresets.json. The only warnings are the existing `stb_image_write.h` sprintf warning and the linker's duplicate-glfw note.)

### Changes
- `engine/shaders/edge.frag`: `creased()` is now `surface == kSurfOak` (was Oak or Wood). Only Oak pixels take crease ink. An Oak pixel creasing against Wood, Plaster or Stone inks on its own side, and Oak against Oak keeps the one-pixel tie-break. The header comment, the `creased()` comment and the tie-break comment now describe this.
- `README.md`: the crease sentence now says the oak is inked along its sharp creases and where it meets the floor, and the floor's plank grooves are not.

### Issues addressed
- ISSUE-001: crease ink is on Oak pixels only, so the Wood floor's plank grooves are no longer inked.

### How to playtest
- `--scene tavern` (or walk in) by day. The floor's planks should show no dark line along each groove or plank end, as before this task.
- The bar top against its front, table tops against their sides, chair and stool legs, the shelves and the barrels should still show a one-pixel dark crease line. Where furniture meets the floor, the line is on the furniture, not the floor.
- Outdoors nothing changes: crease ink is indoors only.
- Captures used: `--scene tavern --time 16 --capture`. Against cycle 1, 26,882 px changed, all floor groove ink removed. With crease ink forced off, 4,522 px differ, all on oak pieces.

### Known limitations
- None.


## Cycle 3
Build: pass (fresh Debug build directory; no CMakePresets.json. The only warnings are the existing `stb_image_write.h` sprintf warning and the linker's duplicate-glfw note.)

### Changes
- `engine/shaders/edge.frag`: `creased()` is now Oak indoors and Roof outdoors (same test: 4-neighbour, depth within `kSilhouette`, normals > 40° apart, one pixel wide by the existing tie-break). Crease ink is tracked apart from silhouette ink. Indoors Oak keeps its 0.35 mix exactly. Outdoors, slate creases use a 0.65 mix (orchestrator note) and are clamped to at most 0.8 × the slate colour, because the slate's shadow tone (`3c4454`) is darker than the ink (`4c4c66`) and would otherwise draw light lines. Silhouette outlines are unchanged.
- `game/src/scene.cpp`:
  - Oak ramps replaced. Day `8c5a30/b47a42/d49c5c`, golden `8a5028/b4703a/e49c52`, twilight `5c3c24/70482c/825838`, night `3c2818/4a3220/583c28`. Every tone has R > G > B.
  - `buildTavern` only: Stone twilight `524a46/625850/74685c` and night `342e2c/3e3734/4a423c` (warm greys, R ≥ G ≥ B). `setPhasePalettes`' Stone is untouched, so the ruins' Stone is byte-identical.
  - `buildTavern`: `kRoomWindows` (the four openings from assets.md: inner-face centres (-9.9, 2.35, 8.5), (1.65, 2.35, 8.5), (4.7, 2.35, 8.5) and (11.3, 2.35, -6.6), 1.4 × 1.7 m, walls 0.4 m) go into `scene.windows`. It also sets the per-phase `skyTint` (day 1.02/0.98/0.90, golden 1.12/0.82/0.52, twilight 0.80/0.70/1.12, night 0.60/0.74/1.10) and `skyColour` (`c4dcec`, `ecb874`, `5c4c84`, `1a2050`).
  - The brazier's `FireSite` is `nightOnly`.
- `engine/include/pxe/scene.hpp`: `FireSite::nightOnly`; new `SkyWindow`; `Scene::windows`, `skyTint[]`, `skyColour[]`.
- `engine/include/pxe/lighting.hpp`, `engine/src/lighting.cpp`: `Lighting::skyLight`, `skyTint` and `skyColour`. The tint and colour blend by the phase weights. `skyLight = 0.15 + 0.85 × fade(elev, -14°, 0°)`: full until sunset, fading through twilight to the moonlight floor (0.15) at -14°. It never reaches 0. `fill` is unchanged.
- `engine/shaders/common.glsl`, `engine/shaders/mesh.frag`:
  - `indoorTone` is replaced by `indoorColour`. Ambient = fill (same as before) + `skyLit` (per window: from the nearest point of the opening, × how squarely the point is in front of it, × N·L toward the opening, × (1 − d/7 m)², gain 2.6, × `uSkyLight`), capped at 1.5 tone. It is screened under the direct light as before, so sun patches still reach the lit tone.
  - The colour is `direct + (lifted − direct) × uSkyTint`, so the fill and the sky light carry the time-of-day tint and the direct light keeps the palette.
  - With tint 1 and no sky light this is exactly the old result. The outdoor path (`uIndoors == 0`) is untouched.
- `engine/src/renderer.cpp`, `engine/include/pxe/renderer.hpp`, new `engine/shaders/sky.vert` and `sky.frag`:
  - `bindToon` now takes the Scene and uploads the window uniforms, `uSkyLight` and `uSkyTint`. Outdoors these are 0 windows, sky 0 and tint 1.
  - Window openings: `upload()` builds one quad per window, 2 cm outside the wall's outer face and 5 cm bigger than the opening. Indoors it is drawn after the meshes with `gl_FragDepth = 1`, `GL_LEQUAL`, no depth write, and normal-target writes masked off. So it fills only background pixels seen through an opening, in the sky colour (greyed by the rain's overcast). The edge pass sees those pixels exactly as background (same depth and normal), so the outlines, lights and ink round the windows are unchanged. It is not in any shadow map.
- `game/src/settings.hpp`, `game/src/settings.cpp`:
  - `WorldClock::Fire` has `nightOnly`, `night` (0..1) and `drawnLevel()`.
  - `followSun(elev, dt)` eases `night` toward 1 while elev < 0 and toward 0 otherwise, over `kNightFireFade` = 2 s. With dt < 0, or for freshly placed fires, it settles at once, so entering the tavern by day doesn't show the brazier dying.
  - `placeFires` copies `nightOnly`.
- `game/src/main.cpp`:
  - `fireDraws` uses `drawnLevel()`. At level 0 no core or flames are made and the light strength is 0, so the flame itself goes, not just its light.
  - At all four draw sites the lighting is computed first, then `followSun`, then `fireDraws`. Single-frame captures settle; `--settle` and play fade.
- `README.md`: tavern paragraph covers the night brazier, honey oak, warm hearth stone, sky light, window sky, tinted fill and roof crease ink.

### Issues addressed
- None open. Cycle-3 revisions implemented, plus the orchestrator's note: Roof crease mix 0.65.

### How to playtest
- `--scene tavern --hour 10`:
  - The window openings are pale blue.
  - The floor and barrels in front of each window are softly lifted.
  - The brazier (by the door) is unlit and shows no flame.
  - The furniture is honey oak.
- `--hour 17`: the openings are gold, and the sky light and fill are amber. The sun patches on the floor still stand out.
- `--hour 18.5`: violet openings, and a blue-violet glow on the floor and barrels in front of the hearth-wall windows. The brazier is lit.
- `--hour 19` to `20`: the window light fades to dark. At `--hour 23` the openings are navy, the room is lit by the hearth, brazier, candles and sconces with a faint cool moonlight at the windows, and the hearth stone is warm grey, not blue.
- In play, run the clock through 18:00 and 06:00 (speed it up in the menu): the brazier catches over about 2 s as the sun sets and dies over about 2 s as it rises. The hearth, candles and sconces burn at every hour.
- `--rain 0.6` indoors by day: the sky in the openings goes greyer.
- Outdoors, `--target -11.4,3,-28` (also `--yaw 300`), any hour: every slate course lip and tile joint draws a dark one-pixel line. Nothing else outdoors changes.
- Captures I checked:
  - Tavern at 10, 17, 18, 18.5, 19, 20.5, 23 against a HEAD build.
  - Exterior at 10, 17.5 and 22 at yaw 0 and 300, against a HEAD build loading the same revision-4 glb. Every changed pixel is darker and was blue-grey (slate); none are lighter.
  - Default ruins and hills views: 0 px changed.

### Known limitations
- The sky light has no occlusion. It is a soft area term, so it reaches faces behind furniture that face a window.
- The hero takes neither the fill nor the sky light (as before, characters use `actorTone`). Indoors at twilight and night the hero's tint follows the tavern's warm Stone ramp, since actors are tinted by Stone.
- The Stone rim colour at night is still the scene's blue-violet rim (`7084c0`). The hearth's top edges show a faint bluish rim line, though the Stone palette itself has no blue-dominant tone.
- The settings menu's fire count still counts the brazier as burning by day (its `lit` flag), and a fire brush on it by day does nothing visible until dusk.
- The window panes assume nothing else is drawn just outside a window's outer face. True for `tavern_interior.glb` today.
