# desert-arms-walk assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py`.
The build runs every `desert-arms-detail` check in the rest and test poses, then the clip checks. It exports the glb, appends the clips, reads the glb back and checks it, and renders the six previews. Environment switches: `PXE_SKIP_PREVIEW=1`, `PXE_SKIP_EXPORT=1` (stop after the checks), `PXE_DESERT_OUT=<dir>`, and `PXE_ARMS_BEFORE=<glb>`. The last one names the glb whose rest pose the new one must equal. By default that is the glb about to be overwritten, else the shipped one. The build is deterministic: two runs give byte-identical glbs (`cmp`).
Export: desert/assets/characters/arms.glb (1,560,736 bytes, up from 1,419,732 for the clips)
Preview: beside the glb. Workbench, back faces culled, Standard view transform.
- `arms_eye.png`, `arms.png`, `arms_left.png`, `arms_right.png`: the rest pose, rendered as before. Their pixels are identical to `desert-arms-detail`'s, so the committed files are kept.
- `arms_walk.png`: a 2x4 sheet of the eye view (60 degree vertical field of view, each tile 640x360, 2 px dark lines between tiles) through `Arms_Walk`, sampled as the engine samples. The top row shows phases 0, 1/8, 2/8 and 3/8, left to right. The bottom row shows 4/8 to 7/8.
- `arms_sprint.png`: the same for `Arms_Sprint`.

Format: glb (glTF 2.0 binary), one skinned mesh on one armature, exported +Y up, flat-shaded, no textures. Unchanged.
Units and scale: 1 unit = 1 metre, real adult scale. Unchanged.
Axes: a view model. Engine +Z is the view's forward, +Y is up, and +X is the player's left. Unchanged.
Origin: the eye, with the `root` joint at the origin. Unchanged.
Triangles: 15,844. Unchanged.
Materials: the 14 of `desert-arms-detail`, slots 0-13 in the same order and colours (see `docs/tasks/desert-arms-detail/assets.md`). Unchanged.
Nodes and objects: mesh node `Arms` and armature node `Arms_Rig`, with the same 39 joints. Unchanged.

### Unchanged: the read-back proof
Rest-pose read-back against the glb before the clips (a copy of the shipped `arms.glb` at 55838cc):
- 29,479 vertex positions, max difference 0.0 m (rule 1e-7);
- every joint node's translation, rotation and scale identical;
- inverse bind matrices identical (max difference 0.0).

The exporter writes the glb without animations, exactly as before. The build then appends the clips (`add_clips`), so the mesh, skin and materials bytes are the exporter's own.

## Animations
| Clip | Length | Keys | Cycle length (metres walked per cycle, two steps) |
|---|---|---|---|
| `Arms_Walk` | 1.0 s, 30 fps | 31 (0, 1/30 ... 1.0 s), LINEAR | **2.0 m** |
| `Arms_Sprint` | 1.0 s, 30 fps | 31, LINEAR | **3.0 m** |

- Both loop: the last key equals the first exactly, on every channel.
- Every one of the 39 joints has a translation, rotation and scale channel in each clip (117 channels).
- Sample a clip at phase x 1.0 s.
- **Timing:** at phase 0 the left hand is furthest forward, and at phase 0.5 the right hand is. Phases 0 and 0.5 are the low points of the bob (the footfalls).
- Only `shoulder`, `upper_arm`, `forearm` and `hand` (both sides) turn. In `Arms_Sprint`, every finger and thumb joint is also held 25 degrees curled on every key.
- `root` and every other joint are keyed at exactly their rest node rotation. Every translation and scale key is exactly the node's rest value.

### The motion (`CLIPS` in build_arms.py)
Hand offsets are the head of `hand.L`/`hand.R` from rest, in engine axes. Below, `s` is +1 for the left hand and -1 for the right.
- **Bob:** y = lift - A cos(4 pi phase).
- **Counter-swing:** z = s S cos(2 pi phase).
- **Elbow:** the forearm opens by s E cos(2 pi phase) about the elbow's hinge. It opens as the hand swings forward.
- **Wrist trail:** the hand pitches about the same hinge by s 6 sin(2 pi phase) degrees. It lags the forearm by a quarter cycle of its own swing.
- **Shoulder and upper arm:** they take the least turn (equal weights) that puts the wrist exactly on its target at each key. The solve is a Gauss-Newton least-norm solve in float64, to 1e-12 m.

| | bob A | swing S | lift | elbow E | wrist trail | fist |
|---|---|---|---|---|---|---|
| `Arms_Walk` | 1.5 cm | 2.5 cm | 0 | 4 deg | 6 deg | none |
| `Arms_Sprint` | 2.5 cm | 5.0 cm | 1.5 cm | 8 deg | 6 deg | 25 deg on every finger and thumb joint, about the curl test pose's hinge |

The spec sets no wrist trail for the sprint. I used the walk's 6 degrees, within "up to 6 degrees".

### Clip checks (measured over the 61 sampled frames: each key and each half-key, sampled as the engine does)
| Check | Rule | `Arms_Walk` | `Arms_Sprint` |
|---|---|---|---|
| Hand y offset, both hands | mean +- bob, within 3 mm | -0.0150 .. +0.0147, mean 0.000 (want -0.015..0.015) | -0.0100 .. +0.0395, mean +0.0150 (want -0.010..0.040) |
| Hand z offset | +- swing, within 3 mm | -0.0250 .. +0.0250 | -0.0500 .. +0.0500 |
| Left z peak | phase 0 +- 1/30 | 0.0 | 0.0 |
| Right z peak | phase 0.5 +- 1/30 | 0.5 | 0.5 |
| y lowest (each hand) | phases 0 and 0.5 +- 1/30 | 0.0 and 0.5 | 0.0 and 0.5 |
| Highest NDC y in view | <= -0.30 | -0.500 | -0.383 |
| Nearest in view | >= 0.15 m | 0.270 m | 0.247 m |
| Forearms leave through the bottom edge | every frame | yes, at NDC x 0.675..0.767 | yes, at NDC x 0.680..0.843 |
| Hand turn from rest | <= 30 deg about each test-pose axis | 5.97 total (twist 0.00, flex 1.74, sideways 4.79) | the same |
| Elbow from rest | 10 closed .. 30 opened | -4.0 .. +4.0 | -8.0 .. +8.0 |
| Finger and thumb joints from rest | <= 60 deg | 0 | 25.0 |
| Largest turn between adjacent keys | <= 45 deg | 1.25 | 2.23 |
| Joints that change between keys | limbs only | shoulder, upper_arm, forearm, hand (L, R) | the same (the digits are curled but held) |
| Attachment | phases 0, 1/8 ... 7/8 | pass | pass |

The bob's top at the half-key samples is 0.3 mm short of the key values (+0.0147). That is the slerp between keys, within the 3 mm tolerance.

New test pose: **each elbow closed 10 degrees** (98.7 to 88.7 degrees). It passes attachment, as do all the `desert-arms-detail` test poses and the rest-pose framing, gauge and compass checks (rest framing unchanged: highest NDC y -0.571, nearest 0.281 m).

### glb read-back (after the clips are appended)
- Exactly the two animations, `Arms_Walk` and `Arms_Sprint`. Each is 1.0 s, with 117 channels of 31 LINEAR keys at times 0..1.0, and on joint nodes only.
- The last key equals the first on every channel.
- Translation and scale keys, and the rotation keys of every joint the clip doesn't turn, equal the node's rest values exactly (as float32).
- Forward kinematics from the glb's own keys puts both hands within 2.1e-7 m of where the build posed them, at keys 0, 7, 15 and 22.
- Blender's glTF importer re-imports it as one skinned mesh `Arms` (15,844 triangles), the 39-joint `Arms_Rig`, and the actions `Arms_Sprint` and `Arms_Walk` (`docs/tasks/desert-arms-walk/scripts/reimport_glb.py`).

### Notes for the developer
- Look the clips up by name. `clipDuration` is 1.0 for both. Sample at `phase * 1.0f`. The cycle lengths are 2.0 m (`Arms_Walk`) and 3.0 m (`Arms_Sprint`).
- At phase 0.5 the right hand is forward and both hands are at their lowest. At phase 0.75 the hands are level fore and aft, at the top of the bob.
- With the walk's weight at 0, the rest pose is the joint nodes' own values. Keys of non-moving joints equal them exactly, so a blend toward rest has nothing to drift on.

Revision: 3, 2026-10-29, desert-arms-walk: added the clips `Arms_Walk` and `Arms_Sprint` and the closed-elbow test pose. Mesh, skin, materials and rest pose unchanged. (Revision 2 was `desert-arms-detail`.)
