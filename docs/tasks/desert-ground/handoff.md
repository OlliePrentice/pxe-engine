## Cycle 1
Build: pass (fresh: `cmake --preset check --fresh && cmake --build --preset check --clean-first`; the only warning is stb_image_write's `sprintf`)

### Changes
- `desert/src/world.hpp`: `kFineCell` 0.5 and its comments; declares `glm::vec2 duneGradient(float x, float z)` (public, as the report in main.cpp needs it); the height comment says 4-12 m.
- `desert/src/world.cpp`: `kRidges` has the four layers (phases 0.875, 0.30, 0.55, 0.25; swell 50-100%, so `0.75 + 0.25 sin`); `duneGradient` implements the binding math; `duneNormal` uses it and `kNormalStep` is gone; stitch `ratio` is 8; `buildWorld` sets the sand albedo (212,180,136), the warm sky fill and bounce strengths, the golden and day sky colours, `terminator[kSurfSand] = 0.35` and `ripples = {kSurfSand, 0.25, 0.35, 200, 6, 20, 22}`. Night keeps the engine's fill.
- `desert/src/main.cpp`: `--terrain-report` (Options, parseArgs, `--help`, and `terrainReport()` run before the window). With `--capture`, `--window-png`, `--menu` or `--walk` it is a bad argument.
- `desert/src/settings.cpp`: "Ground" section after "Sky and dust": Sand colour, Terminator sharpness, and the six ripple values ("Ripples gone by" is kept at least 1 m past "full to", in both directions), plus "Dune shapes are fixed in world.cpp". The Lighting section's "Print values" (`printLighting`) now also writes the sand albedo, terminator and each `scene.ripples` field.
- `desert/docs/art-style.md`: exactly the two replacements (Terrain bullets, Sand bullet).
- `docs/tasks/desert-ground/scripts/`: `tune_phases.py` (Python mirror of `duneHeight`; grid search of the four phases against the three constraints), `gradient_check.cpp` (`duneGradient` against a central difference of `duneHeight`), `cases.txt` (the six captures for `tools/captures.sh`).
- `docs/tasks/desert-ground/ground-*.png`: the six captures.

### Phases
Chosen with `tune_phases.py` (main layer 0.875 puts a crest about 22 m behind the palm; the three others are free within the constraints) and confirmed by the real `--terrain-report`. The script's slope and rise agreed with the C++ output to two decimals (5.40 deg, 4.82 m).

### --terrain-report output (exit 0)
```
terrain: play area height 0.74 .. 10.12 m (crest to trough 9.38 m)
terrain: steepest slope in play area 33.81 deg
terrain: palm slope 5.40 deg, rises away from spawn: ok
terrain: dune behind palm: rises 4.82 m within 5-60 m: ok
terrain: sight line from spawn eye to palm: clears by 0.50 m: ok
```
`--terrain-report --capture x.png` prints `pxe_desert: bad argument: --terrain-report stands alone` and exits 1. `--help` lists it.

### Verification
- `git diff c5d362b -- engine game examples`: empty.
- `tools/ortho-check.sh c5d362b`: 6/6 identical.
- `tools/perspective-check.sh c5d362b`: `example-first-person` identical; `desert-default` and `desert-night` differ (as expected).
- `git grep -n kNormalStep -- desert`: no output. `git diff c5d362b -- desert/docs/art-style.md`: the two replacements only.
- Six captures, each rendered twice by `tools/captures.sh --twice`: all deterministic (cmp-identical).
- By reading: `duneGradient` follows the binding math term by term (checked numerically too: `scripts/gradient_check.cpp` agrees with a central difference of `duneHeight` except in the 0.2% of samples within a few cm of a crest, where the profile has its kink). `groundHeight` and `appendGround` both use `kFineCell` and the (i,j)-(i+1,j+1) diagonal, so the drawn triangles and the sampled height still match; `nf` is 480 and the stitch's `ratio` 8, so `mid` is 4.
- The Ground section and print lines are as specified; the settings menu is play-only so I could not run it, only build it.

### Captures against desert/docs/concept/wanderers.png
All are from the spawn facing the palm, 960x540.
- `ground-1630.png` (16.5 h, the default): sand is a warm beige-ochre, the shadowed ripple faces and the palm's side stay brown-orange, no mauve. The sky is pale blue-grey with cream streaks, dusty but cooler than the concept's apricot sky. Ripples are clear in the lower right, hundreds of fine lines, and fade out by mid-distance; they are absent on the left where the slope is steep. A crest line is visible at the far right and a soft crest runs behind the palm; the horizon is a long, low swell, not the concept's layered silhouettes: from the spawn constraints the view is mostly the gentle slope up to the 22 m crest.
- `ground-golden.png` (18 h): 18 h is mostly twilight in the engine's phase weights, so it is a dim, dark brown ground under a mauve-lilac sky. It is not the concept's glowing golden look. The ground stays brown, not mauve, but flat, with no visible crest edge or ripples at this exposure. The golden values (the concept's 17.6-18 h look) read better at about 17-17.6 h; I did not capture that because the spec fixes 18 h.
- `ground-noon.png` (12 h): bright, pale sand with warm tan shading, a pale dusty blue-grey sky with cream clouds, a clean short palm shadow. Ripples show as fine arcs ("moire" bands) from 3 to 15 m; the crest edge is soft because the sun is high.
- `ground-morning.png` (7 h): the long shadow of the palm runs to the upper left, shadowed faces warm tan, a low sun picks out the crest edges and the ripples (the strongest ripple read of the set), sky pale with warm streaks. This is the closest to the concept's sunlit-sand-against-dusty-sky mood.
- `ground-night.png` (21 h): dark blue sky and grey-blue ground, faint ripples near the eye, the palm's moon shadow to the left. Unchanged in intent: the night fill is the engine's default.
- `ground-walk.png` (--walk 4): 14 m in, the trunk fills the view's centre, a crest edge runs behind it across the whole width, ripples run across the near ground, the palm shadow is a sharp line to the right, and the ground rises to a low horizon on the left, so the dunes do overlap in depth. Moiré banding of the ripples is visible in the middle distance at 4 to 12 m (a sampling effect of a 0.25 m wavelength at the 960x540 view).

### Layering
- Engine: none.
- Game: `desert/src/world.cpp` (dunes, grid, normals, values); `main.cpp` (`--terrain-report`); `settings.cpp` (Ground section and print lines). `duneGradient` is declared in `world.hpp` because the report needs it (the spec said both "file-local" and "declared in world.hpp"; the declaration won).
- Misplaced code touched: none.

### How to playtest
- Walk over several crests (go behind the palm, up the 22 m crest and beyond); the eye should follow the taller dunes smoothly, with no jitter at the crest edges.
- Ripples should appear near the eye and fade out between 6 and 20 m as you move; they should not show on steep faces.
- Open the menu (Tab), "Ground": drag every control (Sand colour, Terminator sharpness, wavelength, depth, heading, full to, gone by, slope cutoff) and see it take effect at once. Drag "Ripples full to" above "gone by": the other should be pushed 1 m clear.
- "Print values" in Lighting: check the terminal also shows the `scene.albedo`, `scene.terminator` and `scene.ripples.*` lines.
- Set the hour to 17.6 (the Golden preset) and compare with the concept art; try 7 and 12 too.
- `pxe_desert --terrain-report` prints the block above and exits 0.

### Docs
- `docs/architecture.md` and `docs/engine-guide.md`: nothing (no engine change).
- `README.md` and the desert's flag list: `--terrain-report` for `pxe_desert` (`CLAUDE.md` lists the desert's flags too: add it there).
- The desert's menu has a Ground section; if README lists the menu's sections, add it.

### Known limitations
- The spawn's view mostly shows a gentle slope and one crest 22 m behind the palm; the layered silhouettes of the concept appear when you walk, not from the spawn view. The spawn, the palm and the phases' constraints fix that.
- The ripples show moiré bands in the mid distance (`ground-noon.png`, `ground-walk.png`).
- `--hour 18` is a twilight look; see `ground-golden.png`.
- The steepest slope in the play area is 33.8 deg (a downwind face), as the 6.5 m main layer implies; the ripples' 22 deg cutoff keeps them off it.
