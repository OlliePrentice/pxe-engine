# Assets: fox-walk

## fox
Source: tools/blender/build_fox.py (reproducible: `/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_fox.py`; add `PXE_FOX_PREVIEW=<dir>` to also render previews. In an open Blender, run `Builder.STEPS` one by one, as in the script's docstring.) Every headless build checks the fox and stops with an error if any check fails:
  - `check`: attachment (no open ends showing, no floating pieces) in the bind pose and at sampled frames of all five clips. Fox_Walk is checked at every frame.
  - `check_motion`: all five clips, interpolated as the engine does it. No joint may turn more than 45 degrees between adjacent keys, and no vertex may be more than 1 cm below the ground at any half-key.
  - `check_walk` (new in revision 6): Fox_Walk sampled at 240 Hz as the engine plays it. It checks:
    - Each planted paw's contact point moves back at the walk's speed within 3%.
    - It strays no more than 1 mm from a point fixed on the ground.
    - At the keys it is on its designed track within 1 mm.
    - Every leg bends the way it bends standing.
Export: assets/characters/fox.glb
Preview: assets/characters/fox_walk.png (new; 3600x800, 9 columns x 2 rows. Top row: from the fox's left side. Bottom row: the game's default view of the fox at its bed: 30 degrees down, as the ruins camera sees the standing fox, 19.4 degrees off straight at the camera. Columns:
  1. The standing pose (Fox_StandUp's last frame), for comparison with column 2.
  2. Walk frame 0 (0.0 s): left fore contact. The clip starts here.
  3. Frame 3 (0.1 s): right hind passing.
  4. Frame 9 (0.3 s): right hind contact.
  5. Frame 12 (0.4 s): right fore passing.
  6. Frame 18 (0.6 s): right fore contact.
  7. Frame 21 (0.7 s): left hind passing.
  8. Frame 27 (0.9 s): left hind contact.
  9. Frame 30 (1.0 s): left fore passing.)
Preview: assets/characters/fox.png and assets/characters/fox_wake.png, unchanged from revision 5.
Format: glb (glTF 2.0 binary)
Units and scale: 1 unit = 1 metre, real red-fox size. Standing: 0.41 m at the shoulder, 1.11 m nose to tail tip. The engine draws the fox at 2.4x.
Axes: +Y up in the glb. The fox's original facing (the curl's face) is +Z in the engine (-Y in Blender), and its original left is +X. Standing and walking, it faces model +X.
Origin: ground level, centred under the curl. Unchanged.
Triangles: 968 (unchanged; budget < 3,000)
Materials (glTF order = the fox's actor material index; solid colours, no textures, none emissive). Unchanged:
  0 `Fox_Fur`   sRGB (208, 98, 40)
  1 `Fox_Cream` sRGB (238, 220, 188)
  2 `Fox_Dark`  sRGB (54, 38, 33)
  3 `Fox_Black` sRGB (20, 17, 17)
Nodes and objects: unchanged. One skinned mesh node `Fox` (skin of 27 joints), parented to the armature node `FoxRig` (identity transform, the scene root).
  Checked against revision 5's glb, these are identical:
  - the nodes, skin, materials and mesh JSON;
  - the inverse bind matrices;
  - every primitive's positions, normals, joints, weights and indices.
  Joints: root; hips > spine.1 > spine.2 > chest > neck > head > ear.L, ear.R; spine.2 > ribcage; chest > upperarm.L/R > forearm > forepaw; hips > thigh.L/R > shin > hindpaw; hips > tail.1 > ... > tail.5.
  The Fox_Black primitive's triangle order can still vary between builds (pre-existing, documented in revision 5): same triangles, same winding.

Animations: exactly five, at 30 fps. Every joint is keyed on every frame (translation, rotation and scale), LINEAR. The names are exact. Index order in the file is alphabetical: LieDown 0, LookAround 1, Rest 2, StandUp 3, Walk 4. The first four indices are unchanged; look clips up by name (`SkinnedModel::clip`).
  `Fox_Rest`, `Fox_StandUp`, `Fox_LookAround`, `Fox_LieDown`: unchanged. Every channel's times and values are identical to revision 5's glb (largest difference 0). Their descriptions, joins (bit-identical), the quarter turn left on `root`, the planted standing paws and the leg bends are as in docs/tasks/fox-wake-look/assets.md (revision 5).
  `Fox_Walk` (new): 0-36, **1.200 s**, 37 samples, **loops** (sample with `loop = true`). The last sample equals the first exactly, so the seam is invisible.
  - Gait: an unhurried four-beat lateral-sequence walk.
    - One clip is one gait cycle: each paw steps once, giving two strides (a left and a right).
    - Footfalls are a quarter cycle apart: left fore, right hind, right fore, left hind.
    - Each paw is on the ground for 2/3 of the cycle (0.8 s) and swings for 0.4 s. Two or three paws are always down.
  - Clip times (touchdown -> lift-off):

    | Paw | Touchdown | Lift-off |
    |---|---|---|
    | Left fore | 0.0 s | 0.8 s |
    | Right hind | 0.3 s | 1.1 s |
    | Right fore | 0.6 s | 0.2 s (next cycle) |
    | Left hind | 0.9 s | 0.5 s (next cycle) |

  - In the swing, the forepaws fold back at the wrist and the hind paws hang from the hock. Late in each stance the heel lifts and the paw rolls over the front of its pad.
  - The body is carried slightly lower than standing (1.5 cm real), with:
    - a 4 mm bob twice a cycle;
    - the hips swaying over the planted hind paw;
    - a slight side-to-side flex of the back.
  - The head is carried low and level, with a small nod twice a cycle.
  - The ears are up. The brush is held out low behind and sways once a cycle.
  - In place: the `root` joint is constant throughout (translation (0.08, 0, 0.05) and the quarter turn left), identical to Fox_StandUp's last frame (rotation difference 0). There is no forward root motion.
  - **Forward is the standing facing: model +X** (world (cos yaw, -sin yaw) for spawn yaw `yaw`). Move the fox along that direction.
  - **Stride length per cycle: 0.405 m real, 0.972 m at 2.4x.**
  - **Ground speed at 2.4x: 0.972 m / 1.2 s = 0.810 m/s** (0.3375 m/s real). Derive it in code as stride x 2.4 / `clipDuration`.
  - Stance paw speed: while a paw is down, its contact point moves straight back (model -X) at stride / duration. Measured two ways:
    - In the build, at 240 Hz with engine-style interpolation: 0.3350-0.3398 m/s against 0.3375 real (-0.74% to +0.68%).
    - Through `pxe::SkinnedModel::sample` + `jointPoint` at 240 Hz and 2.4x: 0.8040-0.8155 m/s against 0.8100 (-0.74% to +0.68%), all four paws, 192 samples each.
    - The sideways/vertical velocity component peaks at 0.029 m/s (2.4x). That is a sub-millimetre arc between keys from the slerped joints.
    - What matters: each planted contact point strays at most **0.28 mm** (at 2.4x) from a point fixed on the ground while the fox moves at 0.810 m/s, so there is no visible foot slide.
  - Contact points (the front of each pad), for `jointPoint(joint, restPoint)`, in engine rest coordinates at real size:
    - `forepaw.L` (0.058, 0.004, 0.247), `forepaw.R` (-0.058, 0.004, 0.247)
    - `hindpaw.L` (0.06, 0.004, -0.106), `hindpaw.R` (-0.06, 0.004, -0.106)

    The paw joints themselves also move back at the same constant speed until the heel lifts. After that they rise and roll forward over the contact point, as a paw does.
  - First frame vs the standing pose (Fox_StandUp's last frame, through `SkinnedModel` at 2.4x):
    - Largest joint rotation difference: **43.1 degrees** (`forearm.R`; also `upperarm.R` 41.1, `forepaw.R` 37.8, `shin.R` 34.2). The right forepaw is late in its stance with the heel lifting, and the right hind has just lifted.
    - Largest vertex distance: **0.299 m** (mean 0.080 m).
    - The root is identical.
    - I chose the start phase as the frame of the cycle with the smallest largest-vertex distance (every frame was measured). No frame of a walking stride is closer: the planted paws are always spread up to 0.13 m real either side of where they stand.
  - Between keys (build, `check_motion`): largest per-key joint turn 35.6 degrees (`forepaw.R`, folding in the swing; limit 45). The lowest half-key point is 3.9 mm above the ground (real). Through `SkinnedModel` at 240 Hz and 2.4x, the lowest vertex is 9.3 mm above the ground. Pads stay 4 mm real above the ground when planted, as standing.
  - Leg bend, through `SkinnedModel` at 240 Hz and 2.4x, every sample:
    - Elbows are always behind the shoulder-to-forepaw line, by 0.083-0.224 m.
    - Knees are always in front of the hip-to-hind-paw line, by 0.155-0.244 m.
    - Measured literally, along the heading against the paw joint:
      - Knees are always in front of the hind paw (hock) joint, by +0.049 to +0.286 m.
      - Elbows are behind the forepaw (wrist) joint except late in each fore stance, when the leg is swept back: there the elbow is up to 0.046 m ahead of the wrist (2.4x). That is what a real fox's foreleg does at that point, and it still bends the standing way. `check_walk` tests the bend direction (the elbow's side of the shoulder-paw line), not the literal heading comparison.

Footprint at 2.4x, for route checks. Measured by skinning every vertex through `pxe::SkinnedModel` with only the 2.4x scale applied, in model space: y up, +x = the heading (standing and walking), +z = the fox's right side. "Length" runs along x, "width" along z. The body's centre line is at z = +0.12 (the root sits 0.05 m real to the right of the origin).
  - Walking (Fox_Walk, every 1/240 s): x -1.359 to +1.423 (2.78 m long), z -0.172 to +0.412 (0.58 m wide), y up to 1.425.
  - Standing still (Fox_StandUp's last frame): x -1.313 to +1.387 (2.70 m), z -0.158 to +0.398 (0.56 m).
  - Standing and looking round (Fox_LookAround, every 1/60 s; the head swings out both sides): x -1.313 to +1.396 (2.71 m), z -0.314 to +0.573 (0.89 m).
  - **For the outing (walk, then look around at the destination), use the union: x -1.359 to +1.423, z -0.314 to +0.573.** That is 2.78 m by 0.89 m, rotated to the heading.
  - The getting-up and lying-down footprint at the bed is unchanged (revision 5's 16-gon).

Revision: 4, 2026-10-21, three waking clips added (task fox-wake-look). See docs/tasks/fox-wake-look/assets.md.
Revision: 5, 2026-10-21, leg IK fixed between keys (fox-wake-look REQ-001). See docs/tasks/fox-wake-look/assets.md.
Revision: 6, 2026-10-22, `Fox_Walk` added (task fox-walk, cycle 1).
  - New looping clip `Fox_Walk`, built in build_fox.py:
    - `walk_track`: each paw's contact point, heel lift and swing.
    - `walk_body`: bob, sway, head, ears and brush.
    - `walk_clip`: the body pose plus each paw planted on its track through revision 5's `Builder.plant`, which bends each leg in its standing bend plane from `legs_planted`. The forelegs' shoulders slide 1.2 cm with the paw (a scapula), so the near-straight forelegs can reach.
  - New `check_walk` in every headless build. `check` and `check_motion` now also cover Fox_Walk.
  - New preview fox_walk.png.
  - Unchanged: the mesh, materials, joints, the other four clips (sample-identical to revision 5), fox.png and fox_wake.png.
  - Built step by step in the user's live Blender session (Builder.STEPS; same checks, same figures), then rebuilt and exported headless. Two headless builds give an identical Fox_Walk.
