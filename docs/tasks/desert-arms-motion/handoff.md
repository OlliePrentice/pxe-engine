# desert-arms-motion handoff

## Cycle 2 (designer), 2026-10-31, base d5bccf4
Spec revision 1 built as written. Both clips pass every check at **k 1.0** (`clip_scale: {"Arms_Walk": 1.0, "Arms_Sprint": 1.0}`); no fallback was needed.

### Changes
- `desert/tools/blender/build_arms.py`: `CLIPS["Arms_Sprint"]` (swing 0.035, back 0.080, drift 0.030; the rest already as the spec), `CLIP_TARGETS["Arms_Sprint"]` (z_max -0.045, z_min -0.115, x_in_max 0.030), `TUBE_R_END = -0.195`, `ELBOW_CLOSE_TEST = 30.0`. Docstring edits: the clip paragraph's sprint sentence in the Decisions' words, and the test-pose sentence ("closed 10" -> "closed 30"). Arms_Walk, `carry()`, `clip_checks`, `fit_clips`, solve-miss recording and the retry on any failing check are as in db6e434. `ELBOW_SPAN` is not in `clip_checks`.
- `desert/docs/art-style.md`: the third replacement (elbows "opened 30° and closed 30°"). The first two were made in cycle 1 (db6e434).
- `desert/assets/characters/arms.glb`, `arms_walk.png`, `arms_sprint.png` rebuilt. The four rest previews rendered pixel-identical and were left as committed.

### Verification
1. Fresh build (`cmake --preset check --fresh`, `--clean-first`): exit 0, the only warning stb_image_write's `sprintf`.
2. Headless build exit 0, every check passing. Two runs into scratch `PXE_DESERT_OUT` dirs: glbs `cmp`-identical, and identical to the committed one. clip_scale recorded in assets.md.
3. Rest pose: the build's clip read-back gives rest_vertex_max_diff 0.0, no joint node changed, rest IBM diff 0.0. `scripts/rest_compare.py` (both commits' scripts through runpy up to `rig`): 13,454 vertices and 11,438 polygons on both; every vertex outside `R.tube.1` moved 0.0 m; 16 of the tube's 408 vertices moved, all in its last two rings (49 and 50 of 51), up to 11.4 mm. `tools/glb.py compare --tolerance 1e-6` against the base glb: inverse bind matrices and joint transforms largest difference 0. Only primitive 4 (`Arms_Glow`) differs, which is expected. Output in `scripts/rest_compare.txt`.
4. `tools/glb.py info`: identical text to the base glb's. Both clips 1.000 s, 117 channels, 31 LINEAR keys.
5. `tools/ortho-check.sh d5bccf4`: 6/6 identical. `tools/perspective-check.sh d5bccf4`: 3/3 identical.
6. `git diff d5bccf4 -- engine game examples desert/src`: no output.
7. `tools/captures.sh --twice --distinct --out docs/tasks/desert-arms-motion scripts/captures.txt`: walk-a, walk-b, sprint-a, sprint-b each deterministic (cmp-identical on the second run), and pairwise different.
8. `git diff d5bccf4 -- desert/docs/art-style.md` shows one replacement, the test-pose line. The other two were already committed in db6e434, before the cycle base. `git diff 0a63800 -- desert/docs/art-style.md` (the task's original base) shows exactly the three replacements and nothing else.

Negative run (scripts/negative_runs.txt): sprint `back=0.070` fails the z-span check at every k (exit 1).

### Finding: the build's checks don't catch the old tube end
With `TUBE_R_END=-0.205` (the old value) the whole build still passes (exit 0). `scripts/elbow_close_sweep.py` / `.txt` shows why. With both elbows closed and the rest of the arm at rest, the old end shows outside the sleeve at 18-27 degrees but is clean again at 30 and 33. So the 30 degree test pose misses it. In the sprint's own 8 attachment phases the shoulder and upper arm turn with the elbow, and those phases are clean too. The new end, -0.195, is clean at every angle swept (10, 18, 22, 27, 30, 33). The spec is met as written. If the tube's sleeving is meant to be guarded by the build, a closed-elbow sweep (for example every 3 degrees up to 33) would be needed instead of a single 30 degree pose. That is a spec decision, so I didn't add it.

### Preview sheets (Verification 7)
Each sheet is 4x2 tiles at phases 0/8..7/8, read left to right, top row first, through the eye camera.
- `arms_walk.png`: the arms stay in the lower band, as in `gear-sprint.png`'s rest pose. The left hand is forward and slightly lower at phase 0 (the footfall dip). The hands swap fore and aft across the sheet in opposition. At the tops of the swing the hands rise slightly on the pendulum arc. The forward hand's fingertips tip up a little a tile after its peak (hand lag 0.15). The elbow give is small (6 degrees) and shows as the forearm angling a little steeper just after the forward peak. The gauge and compass stay in view throughout.
- `arms_sprint.png`: a clearly different pose. The hands are drawn back and lifted, with the elbows bent more, so more glove and less forearm show. Every finger is curled into a loose fist, unlike the open, relaxed hands of `gear-sprint.png` (the old sprint). At phase 0 the left fist is driven up and in toward the middle, the highest point of either clip (NDC y -0.158). The right fist is back, lower and out at the edge, and the two swap by phase 4/8. The elbow closes to 27 degrees at the front and opens to 7 at the back, so the forward fist sits close and high, and the back arm is longer and lower. The wrists cock fingertips up just after each forward peak.
- In game: `sprint-a.png` / `sprint-b.png` show the fists high and pumping, compared with `docs/tasks/desert-arms-gear/gear-sprint.png`, where the hands are open, lower and level. `walk-a.png` / `walk-b.png` show the gentle swing at the old height.

### Playtest checklist (Verification 9)
- [ ] Walk (W): the arms swing from the shoulder in opposition to the stride, on a slight arc, with a small sharp dip at each footfall. The hands follow through at the wrist just after each swing.
- [ ] Sprint (Shift+W): a distinctly different pump. The arms are more bent, the fists are driven up and in at the front and back and out behind, with a harder footfall. The hands are in loose fists.
- [ ] Stop: the arms settle back to rest, open-handed.
- [ ] Menu (Tab) > Arms preview: Walk, Sprint and Hold pose with the phase scrub each show the clip standing still, with no tube or strap popping out of the sleeve at any phase.
