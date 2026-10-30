## Cycle 1
Build: pass (fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`; the only warning is stb_image_write's `sprintf`)

### Changes
- `desert/src/world.cpp`: `buildWorld` sets `groundBounce` and `groundBounceStrength` for the four phases from the Decisions table (a local `rgb` lambda divides by 255, as the engine's scene.cpp does); comment updated.
- `desert/src/settings.hpp`: `SettingsMenu::draw` has the binding signature (`pxe::Scene& scene, const pxe::Lighting& lighting` before `fps`); `#include "pxe/scene.hpp"`.
- `desert/src/settings.cpp`: new "Lighting" section after "Rendering" and before the fps line (phase name line, sun/moon, sky fill, ground bounce, horizon, zenith, haze density, sun glow, shadow penumbra, exposure, "Print values"); `printLighting` writes the binding format; the View distance note now reads "The haze is full at it".
- `desert/src/main.cpp`: `menu.draw(clock, tuning, viewSettings, world.scene, params.lighting, fps)`; `params` is the frame's existing `FrameParams`, already in scope.
- `desert/docs/art-style.md`: exactly the three replacements.
- `docs/tasks/desert-lighting/{d1630,d1818,d21}.png`, `scripts/png-diff.py`.

### Issues addressed
- none (cycle 1)

### Verification
1. Fresh build: pass, only the `sprintf` warning.
2. `git diff 8f2a3a9 -- engine game examples`: no output. `tools/ortho-check.sh 8f2a3a9`: `ortho-check: 6/6 cases identical against 8f2a3a9`.
3. `tools/perspective-check.sh 8f2a3a9`: `PASS example-first-person`, `FAIL desert-default: png differs`, `FAIL desert-night: png differs` (expected: the bounce changed), `1/3 cases identical`.
4. Bounce-only proof: with every `scene.groundBounce*` line deleted from world.cpp and rebuilt, `pxe_desert --capture nobounce.png` is `cmp`-identical to the base's `desert-default.png`. With the file restored, touched and rebuilt, the capture differs from `nobounce.png`.
5. d1630.png, d1818.png and d21.png (`--capture`, `--hour 18.3`, `--hour 21`): each run twice, `cmp`-identical. Versus `docs/tasks/perspective-smooth-lighting/` at the same hour, the difference is slight, in the ground only. Mean change over the lower ground area (`scripts/png-diff.py`, RGB, 0-255): d1630 (+0.66, +0.21, -0.02) on a mean of (127, 102, 72): the sand faces and the slope away from the sun are a touch warmer and redder. d1818 (+0.17, -0.01, 0): the dark mauve-brown sand is a touch warmer. d21 (-0.21, -0.09, -0.04): the night sand is a hair darker and bluer, as the engine's grey bounce (128,128,128) is replaced by (60,65,85). The palm, sky, arms and shadows look unchanged to the eye in all three.
6. `git grep -n "Haze starts at 60%" -- desert`: no output. `git diff 8f2a3a9 --stat -- desert/docs/art-style.md`: 3 insertions, 3 deletions (the three replacements).
7. By reading `settings.cpp`: the Lighting section indexes every `scene` array with `ph`, the largest `lighting.phaseWeight` (first index wins a tie), and edits only the four single values besides; labels switch to "Moon" and "Moon strength" when `ph == kPhaseNight`; `printLighting` writes the binding format for Day, Golden, Twilight, Night in order, then the four singles, and flushes. The menu is play-only and was not run (no way to drive a window here).

### Layering
- Engine: none.
- Game: `desert/src/world.cpp` sets the bounce on the world's own `Scene`; `SettingsMenu` edits that scene through the reference `main.cpp` passes it each frame.
- Misplaced code touched: none

### How to playtest
- `tools/build.sh release desert -- --menu`; scroll to "Lighting". Change the Hour slider (or the presets) and see "Editing: Day/Golden/Twilight/Night" follow the clock. At night the first two controls read "Moon" and "Moon strength".
- Drag each control and see it take effect at once: sun, sky fill, bounce, horizon, zenith, haze, glow, penumbra, exposure.
- Press "Print values" and see the block in the terminal; paste it into `buildWorld` in place of the bounce block, rebuild, and confirm it builds and looks the same.
- Edge case: while the clock runs, the dominant phase can change mid-drag; the sun and moon controls keep one widget ID, so a drag in progress carries on, now editing the new phase's value.

### Docs
- `docs/architecture.md`: the desert-world row in "Where things live" could mention the warm sand ground bounce set in `buildWorld`; the desert settings-menu row could add the Lighting section. The standing rule in the Decisions ("the desert's lighting tunables live in this menu") should be added under "Standing rules per game" if it isn't covered by the existing desert-tunables rule, which it already is in substance.
- `docs/engine-guide.md`, `README.md`: none.

### Known limitations
- Labels for the sun controls use `###` IDs (`"Sun###sunColour"`, `"Moon###sunColour"`, `"Sun strength###sunStrength"`, ...) instead of the spec's `"Sun##c"`: ImGui hashes the whole string after `##`, so `Sun##c` and `Moon##c` would be different IDs, and the spec says the IDs stay the same. Only `###` hashes the part after it alone.
- The menu itself was not exercised at run time (play-only); it was checked by reading and compiling.
