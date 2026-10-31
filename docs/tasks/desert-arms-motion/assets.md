# desert-arms-motion assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py` (exit 0, every check passing).
Export: desert/assets/characters/arms.glb (2,244,816 bytes). Deterministic: two runs into scratch `PXE_DESERT_OUT` dirs gave `cmp`-identical glbs, identical to the committed one.
Preview: beside the glb. `arms_walk.png` and `arms_sprint.png` rewritten (4x2 sheets, phases 0/8..7/8 through the eye camera). `arms_eye.png`, `arms.png`, `arms_left.png`, `arms_right.png` are pixel-identical to the base's (`tools/png-diff.py`: 0 differing pixels), so the committed files were kept. Workbench, back faces culled, Standard view transform.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, flat-shaded, no textures. Unchanged.
Units and scale: 1 unit = 1 metre, real adult scale. Unchanged.
Axes: a view model. Engine +Z is the view's forward, +Y up, +X the player's left (Blender -Y forward, +Z up). Unchanged.
Origin: the eye, with the `root` joint at the origin. Unchanged.
Triangles: 24,208. Unchanged.
Materials: the same 15 in the same order (slot = glTF index, 0 Arms_Wrap ... 14 Arms_Screen; see desert-arms-gear/assets.md). Unchanged.
Nodes and objects: mesh node `Arms`, skin `Arms_Rig`, the same 39 joints in the same order. `tools/glb.py compare --tolerance 1e-6` against the cycle base (d5bccf4): materials, joints, joint transforms and inverse bind matrices identical (largest difference 0); every primitive identical except primitive 4 (`Arms_Glow`, which holds `R.tube.1`): same vertex and index counts, positions differ by up to 8.3 mm (the tube's shortened end). `tools/glb.py info` is identical to the base's.
Mesh change: `R.tube.1`'s end along the right forearm, `TUBE_R_END`, -0.205 -> -0.195. Same vertex count (13,454) and polygon count (11,438) in Blender; only the tube's last two rings moved (16 of its 408 vertices, up to 11.4 mm); every other vertex identical (`scripts/rest_compare.py` / `.txt`).
Animations (cycle length = metres walked per 1.0 s clip; the game drives phase by distance):
- `Arms_Walk`: 1.0 s, frames 0-30 at 30 fps, 31 LINEAR keys, 117 channels, **2.0 m per cycle**, **clip_scale 1.0**. As built in db6e434.
- `Arms_Sprint`: 1.0 s, frames 0-30 at 30 fps, 31 LINEAR keys, 117 channels, **3.0 m per cycle**, **clip_scale 1.0**.
- Left hand furthest forward at phase 0, footfalls at phases 0 and 0.5. Only `shoulder`, `upper_arm`, `forearm`, `hand` turn (and the 30 digit joints in the sprint, curled 25 degrees); everything else is keyed at exactly rest.
Revision: 3, 2026-10-31, new walk and sprint clips (task desert-arms-motion, spec revision 1) and `R.tube.1` shortened 10 mm.

### Measured clip values (clip_checks, both sides symmetric)
| | Arms_Walk | Arms_Sprint | Sprint target (CLIP_TARGETS) |
|---|---|---|---|
| hand z offset (m) | -0.035..0.035, max at phase 0 (L) / 0.5 (R) | -0.115..-0.045, max at 0 / 0.5 | -0.115..-0.045 |
| hand y at forward / back phase (m) | -0.006 / -0.006 | 0.018 / -0.007 | 0.018 / -0.007 |
| hand x toward middle (m) | 0.000..0.006 | -0.010..0.030 | -0.010..0.030 |
| elbow closed (deg) | 0..5.97, most at 0.033 / 0.533 | 7.0..27.0, most at 0.033 / 0.533 | most at 0.03 / 0.53 |
| hand fingertips up (deg) | +-4.97, most at 0.133 / 0.633 | +-8.0, most at 0.100 / 0.600 | most at 0.10 / 0.60 |
| wrist target misses | 0 | 0 | 0 |
| largest key step (deg) | 4.79 | 6.51 | <= 45 |
| highest NDC y in view | -0.335 | -0.158 | <= -0.10 (walk -0.30) |
| nearest in view (m) | 0.235 | 0.164 | >= 0.15 |
| forearm exit NDC x (L / R) | -0.733..-0.661 / 0.661..0.733 | -0.955..-0.692 / 0.692..0.955 | bottom edge |
Attachment clean at the 8 `ATTACH_FRAMES` phases of each clip, and in the closed-elbow test pose (30 degrees).

### Notes for the developer
- No code change needed: clip names, lengths and cycle metres are as `desert/src/arms.cpp` expects.
- The sprint's hands sit higher in view than the walk's (up to NDC y -0.158, allowed to -0.10 by the art style).
