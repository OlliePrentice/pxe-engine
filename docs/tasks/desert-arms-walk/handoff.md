
## Cycle 1 (game-developer: code, steps 2-4)
Build: pass

Verification 1 (`cmake --preset check --fresh && cmake --build --preset check --clean-first`): only the stb_image_write `sprintf` warning. `tools/build.sh release` also builds clean.

### Changes
- desert/src/player.hpp, player.cpp: `Player::moved` (binding text). `update()` keeps the xz before moving; after the clamp and `outsidePalm`, `moved = glm::length(xz - before)`. It is 0 with no input; the height easing is not counted.
- desert/src/arms.hpp, arms.cpp: the binding interface and constants. `load()` looks up `Arms_Walk` and `Arms_Sprint` and throws `std::runtime_error` if either is missing. `walk()` is the binding body. `pose()` skins `rest_` when `moving_ == 0.f`; otherwise it samples both clips at `phase_ * clipDuration`, blends walk to sprint by `sprinting_`, then rest to that by `moving_` (the engine's `blend` allows `out` to be an input), and skins `pose_`. The eye transform, `viewModel` and materials are unchanged. The class comment now says the arms walk.
- desert/src/main.cpp: `Options::walk`, `Options::sprint`; `--walk S` (0 < S <= 10) and `--sprint` parsed; `--sprint` without `--walk`, and either without `--capture` or `--window-png`, is a bad argument; usage lines as specified; the binding fixed-step walk before the capture frames; `arms.walk(player.moved, dt)` after `player.update` in the play loop and `arms.walk(0.f, dt)` while the cursor is free; header and `--help` text say the arms walk with the player.
- docs/tasks/desert-arms-walk/*.png: the five captures. scripts/arm_rows.py: finds the arms' pixels in a capture (see below).

### Issues addressed
- none (cycle 1; no issues.md)

### Verification results
- **3.** `tools/perspective-check.sh 980bdd5`: PASS desert-default, desert-night, example-first-person. `perspective-check: 3/3 cases identical against 980bdd5`.
- **4.** `tools/ortho-check.sh 980bdd5`: PASS game-default, game-hills, game-night-foxes, game-night-fires, game-self-test, example-ortho. `ortho-check: 6/6 cases identical against 980bdd5`.
- **5.** `git diff 980bdd5 -- engine game examples desert/src/world.hpp desert/src/world.cpp`: no output (0 bytes).
- **6.** `--help` lists `--walk S` and `--sprint`. Each exits 1 with `pxe_desert: bad argument: ...` and writes no file:
  - `--sprint --capture x.png`: "bad argument: --sprint needs --walk";
  - `--walk 0 --capture x.png`: "bad argument: --walk";
  - `--walk 11 --capture x.png`: "bad argument: --walk";
  - `--walk 2` alone: "bad argument: --walk and --sprint need --capture or --window-png".
  - (`--walk 2 --sprint` alone exits 1 the same way.)
- **7.** The five captures are in the task folder. Two runs of each are byte-identical (`cmp`), and I re-ran all five against the final binary: identical again. For each, the walked distance and phase come from replaying `Player`/`Arms::walk`'s arithmetic (fixed 1/60 s steps; the eased sprint weight sets the cycle length). I found the arms' pixels by diffing each capture against the same render with `arms.pose` temporarily skipped in a scratch edit (restored, not committed; `scripts/arm_rows.py`). Rows are in the 960x540 view; nothing is drawn above row 351 in any of them, and every hand and forearm is whole, with the forearms leaving through the bottom edge.

  The rest capture, `docs/tasks/desert-arms-detail/detail-day.png`, has its arms from row 425 down on both sides, with the palm 25 m ahead showing its crown. Against it:

  | Capture | Walked | Phase | Arms' top row (left / right) | What it shows |
  |---|---|---|---|---|
  | `walk-low.png` (`--walk 2`) | 7.00 m | 0.500 | 457 / 433 | A footfall, right hand forward. The left hand sits 32 rows lower than at rest and the right 8 rows lower, so both are lower. The right hand is further forward and higher in the view. The left is back. |
  | `walk-left.png` (`--walk 2.29`) | 7.99 m | 0.996 | 433 / 457 | Mirrored from `walk-low`: the left hand forward and the right back, both low. |
  | `walk-mid.png` (`--walk 2.14`) | 7.47 m | 0.733 | 408 / 406 | The hands level with each other, the same size and height, at the top of the bob. Both are raised 17-19 rows above rest. |
  | `sprint.png` (`--walk 3 --sprint`) | 18.0 m | 0.114 | 398 / 428 | Larger offsets than the walks, the left hand raised 27 rows above rest and the hands closer in. The fingers are visibly curled into loose fists. The trunk is close (7 m away) and fills the view, because 18 m was walked. |
  | `walk-night.png` (`--walk 3 --hour 21`) | 10.5 m | 0.250 | 407 / 407 | The same pose family as `walk-mid` at night. The gauge's three cyan windows and the lamp still glow, and the compass dial reads. |

  In each, the scene has moved forward (the palm is bigger and its crown has left the top of the view in the 7-8 m captures and the sprint) because the eye walked. The camera's pitch and the view are unchanged.
- **8.** By reading: `walk()` snaps `moving_` to 0 only under `if (moving == 0.f && moving_ < kAtRest)`, i.e. when the target is 0, and `pose()` skins `rest_` (the model's own rest pose, not a blend) exactly when `moving_ == 0.f`. Standing still from the start never leaves `moving_` at 0 and the first frame skins `rest_`, which `perspective-check` confirms byte-identical.

### Layering
- Engine: none.
- Game: `desert::Player` records `moved`; `desert::Arms` owns the clips' poses, the phase and the eased weights; `main.cpp` drives both and owns the options.
- Misplaced code touched: none.

### How to playtest
- Run `build/release/desert/pxe_desert`. Standing still, the arms are the rest pose (compare with the earlier build: nothing moves, no breathing).
- Hold W: the hands bob twice per stride and swing fore and aft in turn, the left forward as the right goes back, the forearms rocking a little. Hold Shift too: bigger swings, hands lifted, fingers curling into loose fists as the speed rises from 3.5 to 6 m/s. The swing follows your steps' distance, so there is no foot-sliding feel at either speed.
- Release the keys: the walk eases out over about half a second and the arms settle back to the exact rest pose.
- S and A/D swing the arms too. Walk into the play-area edge (80 m from the centre) or into the palm trunk and keep pushing: the arms stay still. Slide along the trunk: they swing. Walk up and down the dunes without moving the keys: no swing from the height easing.
- Press Escape to free the cursor while walking: the arms settle to rest. A click resumes.
- The view and the horizon do not bob.
- `--capture x.png --walk 2` (and `--sprint`) to see the walk without playing.
- Edge cases I know about: `sprinting_` decays to 0 about as fast as `moving_`, but it isn't snapped to 0; a stop-start within a fraction of a second begins with a little sprint weight left over, which is invisible (weights under 0.1% in practice).

### Docs
- docs/architecture.md: the desert's `Arms` row: it now plays two locomotion clips by distance, driven by `Player::moved`. Nothing engine-side.
- docs/engine-guide.md: none (no engine change).
- README.md: the desert's flags: `--walk <seconds>` and `--sprint` (headless only, need `--capture` or `--window-png`), and that the arms walk with the player.

### Known limitations
- The spec's header text of the `--help` Play line grew by a few words ("and the arms walk with you"); the wrap is mine.
- Mouse-turning alone (no movement) doesn't swing the arms; by design.
- Keyboard, mouse and gamepad play isn't exercised by the checks; the playtest list above is for the user.
