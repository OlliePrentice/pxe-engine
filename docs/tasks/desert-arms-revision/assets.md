# desert-arms-revision assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py`.
The build runs every existing check (framing, attachment in the rest and test poses, covered, closed, crease, compass, small parts, clip checks, glb read-back) with `KNUCKLE_Y` widened to (-0.70, -0.40) and the readout's checks in place of the bars'. Environment switches: `PXE_SKIP_PREVIEW=1`, `PXE_SKIP_EXPORT=1` (stop after the checks), `PXE_DESERT_OUT=<dir>`. `PXE_ARMS_BEFORE` is gone: the clip read-back now compares the glb with clips against the glb just exported, before `add_clips`. The build is deterministic: two runs into `PXE_DESERT_OUT` scratch dirs gave `cmp`-identical glbs, also identical to the committed one.
Export: desert/assets/characters/arms.glb (1,603,936 bytes, up from 1,560,736)
Preview: beside the glb, all six rewritten because the asset changed: `arms_eye.png`, `arms.png`, `arms_left.png`, `arms_right.png`, `arms_walk.png`, `arms_sprint.png`. Workbench, back faces culled, Standard view transform.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, exported +Y up, flat-shaded, no textures. Unchanged.
Units and scale: 1 unit = 1 metre, real adult scale. Unchanged.
Axes: a view model. Engine +Z is the view's forward, +Y is up, and +X is the player's left. Unchanged.
Origin: the eye, with the `root` joint at the origin. Unchanged.
Triangles: 16,276 (budget 12,000-20,000; spec estimate about 16,280). Up 432 from 15,844: the bars' 36 removed, the screen's 12 and the 38 dots' 456 added.
Nodes and objects: mesh node `Arms` and armature node `Arms_Rig`, the same 39 joints. New pieces (vertex groups of the build, not glTF nodes): `L.screen` and 38 `L.lcd.<glyph>.<row><col>` dots, rigid to `forearm.L`.
Animations: `Arms_Walk`, `Arms_Sprint`, 1.0 s, 31 LINEAR keys per channel, 117 channels, every value as `desert-arms-walk`, regenerated from the new rest pose.

### Pose
`HAND_ROLL` used: **90.0** (no fallback needed: compass face vertices blocked 0 of 216). `HAND_YAW`, `HAND_PITCH`, `FINGERS`, `THUMB_*`, `WRIST`, `SHOULDER`, `ELBOW_SWING` unchanged. `KNUCKLE_Y` (-0.70, -0.40).

### Materials (glTF order = actor slot)
| Slot | Name | sRGB | Emissive |
|---|---|---|---|
| 0 | Arms_Wrap | (206, 194, 168) | no |
| 1 | Arms_Glove | (78, 54, 40) | no |
| 2 | Arms_Strap | (118, 84, 56) | no |
| 3 | Arms_Brass | (176, 132, 64) | no |
| 4 | Arms_Glow | **(70, 255, 110)**, was (90, 230, 210): the readout's dots | yes |
| 5 | Arms_Steel | (92, 96, 102) | no |
| 6 | Arms_Copper | (178, 98, 56) | no |
| 7 | Arms_Rubber | (44, 40, 38) | no |
| 8 | Arms_Thread | **(132, 100, 70)**, was (196, 170, 124) | no |
| 9 | Arms_Worn | (112, 82, 62) | no |
| 10 | Arms_Grime | (160, 146, 120) | no |
| 11 | Arms_Dial | (222, 212, 186) | no |
| 12 | Arms_Paint | (170, 64, 44) | no |
| 13 | Arms_Amber | (255, 170, 60) | yes |
| 14 | **Arms_Screen** (new) | (30, 46, 36) | yes |

The glb read-back returned every colour and emissive flag above exactly.

### Measured against the spec's expected values
| Value | Expected (planning trial) | Measured |
|---|---|---|
| Highest NDC y in view, rest | -0.478 | -0.478 |
| Highest NDC y, `Arms_Walk` | -0.406 | -0.406 |
| Highest NDC y, `Arms_Sprint` | -0.320 | -0.320 |
| Index knuckle top, L / R | -0.518 / -0.518 | -0.518 / -0.518 |
| Hand centre NDC, L / R | \|x\| 0.432 at y -0.688 | (-0.432, -0.688) / (0.432, -0.688) |
| Nearest in view, rest | 0.281 m | 0.281 m (walk 0.270, sprint 0.247) |
| Compass face vertices blocked | 0 | 0 of 216 |
| Compass centre | - | NDC (0.630, -0.840), 0.301 m ahead, normal cos 1.0 to the eye, dial 46.7 px |
| Readout dot size on screen | about 4-5 px | min 4.21 px across, 4.35 px up (rule 3.0) |

`m["lcd"]`: `{"dots": 38, "ndc": {"x": [-0.838, -0.69], "y": [-0.96, -0.867]}, "px_min": [4.21, 4.35], "facing_min": 0.963}` (facing rule `FACE_COS` 0.87). Every dot is seen from the eye, the glyphs' mean NDC x strictly increases 4, 7, %, cursor, and row 1 projects above row 5 in each glyph.

Negative runs (scratch copies, `PXE_SKIP_EXPORT=1`): `LCD_MIN_PX` 5.0 failed on dot size (`L.lcd.1.11: ... 4.5 x 4.6 px`); `right` flipped failed reading order (`mean NDC x [-0.7124, -0.7481, -0.7843, -0.8218]`).

### Kept pieces (Verification 3, against `git show 8b57c1b:desert/assets/characters/arms.glb`)
```
kept pieces: 266, vertices: 4952, shipped vertices: 29479
furthest kept vertex from the shipped glb: 0 m (None)
kept pieces: OK
```

### Clip read-back (against the glb just exported)
30,372 rest vertices, max difference 0.0 m; no joint node changed; inverse bind matrices max difference 0.0. Hands within 2.1e-7 m (walk) and 1.9e-7 m (sprint) of the build.

### `tools/glb.py info`
15 materials, `Arms_Screen` last (slot 14, one 12-triangle primitive); skin `Arms_Rig` with 39 joints; `Arms_Walk` and `Arms_Sprint` 1.000 s, 117 channels, keys {31}, LINEAR; total triangles 16,276.

### Preview check (`arms_eye.png`, `arms_left.png`)
The readout reads `47%` and a solid block cursor left to right, bright green on the dark green-grey panel. The palms face each other with the thumbs on top. The stitching is a darker brown, quiet on the straps. The compass sits where it did, wholly seen.

Revision: 4, 2026-10-30, desert-arms-revision: hands rolled 90 degrees (palms facing, thumbs up), the gauge's bars replaced by an LCD screen (`Arms_Screen`, slot 14) reading 47% and a cursor in `Arms_Glow` phosphor green, `Arms_Thread` darkened, 15 materials, 16,276 triangles. Everything behind the wrists unchanged to 0 m. (Revision 3 was `desert-arms-walk`.)
