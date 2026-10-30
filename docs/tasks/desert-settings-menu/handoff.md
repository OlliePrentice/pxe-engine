## Cycle 1
Build: pass

### Changes
- desert/src/player.hpp, player.cpp: `Player::walkSpeed` and `sprintSpeed` (default `kWalkSpeed`, `kSprintSpeed`); `update` uses them.
- desert/src/arms.hpp, arms.cpp: `walk(moved, dt, walkSpeed, sprintSpeed)` blends by the live speeds; new `hold(sprint, phase)` and `release()`; `pose()` samples the held clip at full weight while held. Class comment mentions the hold.
- desert/src/settings.hpp, settings.cpp (new): `Clock`, `PlayerSettings`, `ViewSettings`, `SettingsMenu`, with the sections in the spec's order.
- desert/CMakeLists.txt: `src/settings.cpp` added.
- desert/src/main.cpp: `--menu` (rejected with `--capture`/`--window-png`), usage and header text, `readInput` takes `lookScale`, `frameParams` takes the weather. Headless path passes `opt.hour` and `pxe::Weather{}` and `player.walkSpeed/sprintSpeed` to `arms.walk`, and touches nothing else. Interactive branch builds the overlay, menu, clock, tuning and view settings after the headless block returns; Tab/Start/Escape/click/close-button cursor rules; per-frame application of all settings; fps over half-second windows; `menu.draw` after `render`, only on frames that render. The local `PerspectiveFrame` is now `eyeView` and the settings are `viewSettings`.
- docs/tasks/desert-settings-menu/scripts/walk-compare.sh: compares the three `--walk` captures against the base build kept by perspective-check.sh.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: none. No file under engine/, game/ or examples/ changed.
- Game: main.cpp creates and owns the `std::optional<pxe::Overlay>`, `SettingsMenu`, `Clock`, `PlayerSettings` and `ViewSettings` in the interactive branch only.
- Misplaced code touched: none.

### Verification results
1. Fresh `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning.
2. `tools/perspective-check.sh 1722db0`: 3/3 identical (desert-default, desert-night, example-first-person).
3. `scripts/walk-compare.sh` (base 1722db0 vs worktree): `--walk 2`, `--walk 3 --sprint`, `--walk 2 --hour 21` captures all byte-identical.
4. `git diff 1722db0 --stat -- engine game examples desert/src/world.* desert/assets desert/tools`: no output.
5. `tools/ortho-check.sh 1722db0`: 6/6 identical.
6. `--help` lists `--menu`. `--menu --capture x.png` and `--menu --window-png x.png` print "bad argument: --menu is for play, not with --capture or --window-png" and exit 1. `pxe_desert --menu` starts and runs (killed by a 3 s timeout).
7. Greps: "imgui" (case-insensitive) in desert/src only in settings.cpp (I reworded two comments that mentioned it); no kWalkSpeed/kSprintSpeed in arms.cpp; no game includes in desert; `Overlay` in main.cpp only at line 241, after the headless `return`.
8. Read through main.cpp: overlay and menu are built after the headless block returns; headless frames pass `opt.hour` and `pxe::Weather{}` and don't touch FOV, far plane, edges, showNdl or testColours; `menu.draw` is after the framebuffer-size `continue`. Cursor rule: Tab/Start toggles and sets capture to `!menu.open()` when the state changed; Escape with the menu open closes it (flag `escapeClosed` skips the recapture) and leaves the cursor free; Escape closed-and-captured frees the cursor; Escape closed-and-free quits; a click captures only when the menu is closed; the window's close button is detected by comparing `menu.open()` before and after `draw()` and captures.
Not driven automatically: keyboard, mouse and pad behaviour (needs the playtest below).

### How to playtest
- Play starts exactly as before: cursor held, 16:30, no clouds, arms at rest. `--menu` starts with the menu open and the cursor free.
- Tab opens the menu and frees the cursor, and mouse look and WASD stop. Tab closes it and the cursor is held again. The same with the pad's Start button. The window's close button (x) closes it and recaptures the cursor.
- Escape with the menu open closes it and leaves the cursor free. Escape again quits. A click instead recaptures the cursor. Clicking the menu's widgets never recaptures it.
- Unpause the clock: the light moves through the day, and the presets and speed buttons work.
- Raise cloud cover: cloud shadows cross the sand and the palm, drifting with the wind direction and strength.
- Change walk and sprint speed and walk: the speed changes and the arms still swing in step. The sprint slider can't go below walk + 0.5.
- Field of view, mouse sensitivity and view distance (the haze moves in) each take effect live.
- Arms: hide and show. Preview Walk and Sprint while standing still. Hold pose and scrub the phase. Preview Off returns the arms to rest.
- Edges, N·L and Test colours toggle as expected. The fps line updates.

### Docs
- docs/architecture.md "Where things live" (desert row): add `desert/src/settings.*` (Clock, PlayerSettings, ViewSettings, SettingsMenu on `pxe::Overlay`). docs/engine-guide.md: none. README.md: desert controls (Tab / Start opens the menu, `--menu` flag).

### Known limitations
- Playtest items (input handling, cursor capture, widgets) are unverified by automation; they were checked by reading the code only.
- `arms.cpp` still includes `player.hpp`, now unused (left as is).
