# desert-arms-pose assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py`.
The change is `docs/tasks/desert-arms-pose/scripts/build_arms-unblock.patch` exactly, plus the module docstring (the held pose's numbers, the gloves' bulk and taper, the bracer plate reaching under the cuff, the compass strap over the middle of the cuff, the sprint's 20 mm bob with no lift) and one phrase in `compass()`'s docstring ("over the middle of the cuff", was "over the cuff's back end"). `git apply --check -R` of the patch passes on the result. No search in the build: every value is a fixed constant.
Built step by step in the live Blender session first (Arms scene, through the MCP): `check()` and `clip_checks()` returned no problems, with the same measurements as the headless build below.
Export: desert/assets/characters/arms.glb (1,605,488 bytes, up from 1,603,936). Deterministic: two runs into `PXE_DESERT_OUT` scratch dirs gave `cmp`-identical glbs, also identical to the committed one.
Preview: beside the glb, all six rewritten because the asset changed: `arms_eye.png`, `arms.png`, `arms_left.png`, `arms_right.png`, `arms_walk.png`, `arms_sprint.png`. Workbench, back faces culled, Standard view transform.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, flat-shaded, no textures. Unchanged.
Units and scale: 1 unit = 1 metre, real adult scale. Unchanged.
Axes: a view model. Engine +Z is the view's forward, +Y is up, +X is the player's left. Unchanged.
Origin: the eye, with the `root` joint at the origin. Unchanged.
Triangles: 16,396 (budget 12,000-20,000; trial 16,396). Up 120 from 16,276.
Materials: the 15 as shipped, same order, colours and emissive flags (glb read-back exact; `tools/glb.py compare` against the cycle base: "materials: equal (15)").
Nodes and objects: mesh node `Arms`, skin `Arms_Rig`, the same 39 joints in the same order. Joint transforms and inverse bind matrices change (new rest pose), as expected.
Animations: `Arms_Walk`, `Arms_Sprint`, 1.0 s, 117 channels, 31 LINEAR keys each, regenerated from the new rest pose. `Arms_Walk` as shipped; `Arms_Sprint` `lift` 0.0, `bob` 0.020 (was 0.015, 0.025).
Revision: 1, 2026-10-30, re-posed (hands pointing forward on a firm wrist, wrists closer and higher), gloves 1.15 chunkier tapering into an unscaled cuff, gauge, bracer plate and compass moved toward the hand, sprint damped.

### Constants (Acceptance criteria)
`HAND_YAW` -8.0, `HAND_PITCH` 4.0, `HAND_ROLL` 90.0, `WRIST` (0.200, -0.330, -0.150), `GLOVE_BULK` 1.15 (finger `w0`/`t0`, `PALM` rings 2-7, `THUMB_W`/`THUMB_T`; `CUFF` unscaled), `GAUGE_S` -0.035, `GAUGE_TURN` 0.55, `BRACER_S` (-0.160, -0.020), `COMPASS_STRAP` (-0.0155, -0.011, -0.0065), `CLIPS["Arms_Sprint"]` bob 0.020 lift 0.0, `CLIP_TARGETS["Arms_Sprint"]` mean 0.0 bob 0.020.

Rest pose measured in the live build: elbow bent to 87 degrees (was 99), elbow 9 cm below the shoulder, 24 cm ahead, 16 cm out; hand vertices span 0.296-0.500 m ahead of the eye.

### Measured against the spec's expected values (unblock trial)
| Value | Expected | Measured |
|---|---|---|
| Highest NDC y in view, rest | -0.410 | -0.410 |
| Nearest in view, rest | 0.258 m | 0.258 m |
| Index knuckle top, L / R | -0.469 | -0.469 / -0.469 |
| Hand centre NDC, L / R | (±0.494, -0.686) | (-0.494, -0.686) / (0.494, -0.686) |
| Forearm exit \|NDC x\| | 0.714 | 0.714 / 0.714 |
| Readout dots | 38 | 38 |
| Readout NDC x | -0.842 to -0.681 | -0.842 to -0.681 |
| Readout NDC y | -0.952 to -0.857 | -0.952 to -0.857 |
| Readout dot size (min) | 4.56 x 4.70 px | 4.56 x 4.70 px |
| Readout facing (min) | 0.954 | 0.954 |
| Compass centre NDC | (0.637, -0.864) | (0.637, -0.864) |
| Compass ahead | 0.276 m | 0.276 m |
| Compass dial | 50.9 px | 50.9 px |
| Compass lamp | 5.8 px | 5.8 px |
| Compass face vertices blocked | 0 of 216 | 0 of 216 (all in view; normal cos to eye 0.999; lid top NDC y -0.619) |
| `Arms_Walk` top | -0.314 | -0.314 (nearest 0.246 m) |
| `Arms_Sprint` top | -0.313 | -0.313 (nearest 0.219 m) |
| Triangles | 16,396 | 16,396 |

Other build measurements: hand box NDC L x -0.646..-0.341, y -0.963..-0.410; wrist NDC L (-0.590, -0.787) at 0.33 m; gauge toggles 3/3 seen, knurl dial seen (10 of 16 ridges), screws 2 of 4 seen; smallest compass tick 4.4 px; clip hand offsets: walk y ±0.015 z ±0.025, sprint y ±0.020 (mean 0) z ±0.050; sprint fingers 25 degrees.

### Verification
- 2: headless build exit 0, every check passing; two `PXE_DESERT_OUT` scratch runs `cmp`-identical, and identical to the committed glb.
- 3: `python3 tools/glb.py info`: 15 materials as shipped; skin `Arms_Rig` 39 joints; `Arms_Walk` and `Arms_Sprint` 1.000 s, 117 channels, keys {31}, LINEAR; total triangles 16,396.
- 6: `tools/captures.sh --twice --distinct --out docs/tasks/desert-arms-pose docs/tasks/desert-arms-pose/scripts/captures.txt`: `pose-day.png`, `pose-golden.png`, `pose-walk.png`, `pose-sprint.png`, each deterministic, pairwise different. Against `docs/tasks/desert-arms-revision/revision-day.png`: hands now point forward and converge toward the screen centre (fingertips rise toward the horizon instead of pointing across the view), palms facing and thumbs up; wrists straighter; less forearm shows; gloves visibly bulkier; the readout "47%" and the compass face are wholly in view and readable at rest (day and golden).
  Observed, not checked by the build or the spec: in `pose-walk.png` (walk 2 s) the gauge is below the bottom edge, and in `pose-sprint.png` (walk 3 s, sprint) the compass face is mostly below it. The clip checks hold only the top of the arms (NDC y at or below -0.30), not the gauge or compass, during the clips.
- 8: `git apply --check -R docs/tasks/desert-arms-pose/scripts/build_arms-unblock.patch` passes.
