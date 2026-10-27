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

