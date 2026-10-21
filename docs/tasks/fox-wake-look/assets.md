# Assets: fox-wake-look

## fox
Source: tools/blender/build_fox.py (reproducible; `/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_fox.py`, add `PXE_FOX_PREVIEW=<dir>` to also render previews. In an open Blender, run `Builder.STEPS` one by one, as in the script's docstring.) Every headless build checks the fox and stops with an error if any check fails:
  - `check`: it holds together in the bind pose and at sampled frames of all four clips (Rest and LookAround every 6th frame, StandUp and LieDown every 3rd, both ends always).
  - `check_motion` (new in revision 5): between every pair of keys in all four clips, interpolated as the engine does it (each joint's local rotation slerped the short way, translation and scale lerped), no joint turns more than 45 degrees from one key to the next, and at every half-key no vertex is more than 1 cm below the ground. The build prints the largest turn and the lowest half-key point per clip.
  For this revision every frame of the three new clips was also checked for attachment, with no problems.
Export: assets/characters/fox.glb
Preview: assets/characters/fox.png (2000x400, unchanged from revision 3: the curl as the game sees it | from above | low from the face side | low from the side | the standing bind pose)
Preview: assets/characters/fox_wake.png (5600x800, 14 columns. Top row: the fox from its back-right, 50 degrees down (the view of the revision-4 spot; at the current kRuinsFoxSpot the default view sees the standing fox from the front instead). Bottom row: from the side away from the fire.
  Columns 1-8, key poses:
  1. StandUp 0.5 s: lying on its belly, head up.
  2. StandUp 0.9 s: front up on planted front paws.
  3. StandUp end: standing.
  4. LookAround 1.0 s: looking left.
  5. LookAround 2.33 s: looking right, toward the fire.
  6. LookAround 3.53 s: sniffing the ground.
  7. LieDown 0.8 s: sinking onto its belly.
  8. LieDown 1.4 s: rolling onto its side, curling round.
  Columns 9-14 are half-key frames at revision 4's leg flips (REQ-001): StandUp keys 22.5, 26.5, 34.5, then LieDown keys 9.5, 13.5, 22.5. In each, the elbows are behind the forepaws and the knees in front of the hind paws. Blender draws these half-keys with linear interpolation of the key values; the engine slerps, which for these small per-key turns (under 24 degrees) looks the same.)
Format: glb (glTF 2.0 binary)
Units and scale: 1 unit = 1 metre, real red-fox size. Standing: 0.41 m at the shoulder, 1.11 m nose to tail tip. The engine draws the fox at 2.4x (about 1 m at the shoulder and 2.7 m nose to tail).
Axes: +Y up in the glb. The fox's original facing (the curl's face) is +Z in the engine, -Y in Blender. Its original left is +X.
Origin: ground level, centred under the curl, unchanged. The curl (Fox_Rest) is exactly as in revision 3.
Triangles: 968 (unchanged; budget < 3,000)
Materials (glTF order = the fox's actor material index; solid colours, no textures, none emissive). Unchanged:
  0 `Fox_Fur`   sRGB (208, 98, 40)
  1 `Fox_Cream` sRGB (238, 220, 188)
  2 `Fox_Dark`  sRGB (54, 38, 33)
  3 `Fox_Black` sRGB (20, 17, 17)
Nodes and objects: unchanged. One skinned mesh node `Fox` (skin of 27 joints), parented to the armature node `FoxRig` (identity transform, the scene root). The joint names and order, inverse bind matrices, node transforms, vertex positions, normals, joints and weights are all identical to revision 3. The one difference in the mesh is the order of the Fox_Black primitive's triangles (the nose and eyes): same triangles, same winding. That order already varies between runs of revision 3's own script, so it is pre-existing build nondeterminism and has no effect.
  Joints: root; hips > spine.1 > spine.2 > chest > neck > head > ear.L, ear.R; spine.2 > ribcage; chest > upperarm.L/R > forearm > forepaw; hips > thigh.L/R > shin > hindpaw; hips > tail.1 > ... > tail.5.

Animations: four clips at 30 fps, every joint keyed on every frame (translation, rotation and scale), LINEAR. Each action is its own glTF animation with exactly these names, and no other animations are in the file. The one-shot clips run exactly from 0 to their length, with no padding. Look clips up by name (`SkinnedModel::clip`). Their index order in the file is alphabetical (LieDown 0, LookAround 1, Rest 2, StandUp 3), not the order they play in.
  `Fox_Rest`: 0-120, 4.000 s, 121 samples, loops. Unchanged: its samples in the glb are identical to revision 3's. Sample with `loop = true`.
  `Fox_StandUp`: 0-45, 1.500 s, 46 samples, one-shot (`loop = false`). Starts on Fox_Rest's first pose. In order:
  - 0-0.27 s: the head lifts out from under the brush and the ring loosens.
  - by about 0.6 s: it swings round onto its belly, head up, turning.
  - 0.54-0.84 s: the front paws step out and plant.
  - by about 1.0 s: the front end is up.
  - 0.66-0.99 s: the hind paws step out from under it and plant.
  - by 1.38 s: the hind end rises to standing.
  - 1.44-1.5 s: the tail settles. The clip ends in the standing pose.
  `Fox_LookAround`: 0-120, 4.000 s, 121 samples, one-shot. It starts and ends exactly on StandUp's last pose. Ears up and alert throughout.
  - It looks left (away from the fire) from 0.24 to 1.6 s, fully turned (about 46 degrees, neck and head) from 0.64 to 1.2 s.
  - It looks right, toward the fire, from 1.68 to 2.96 s, fully turned (about 49 degrees) from 2.08 to 2.56 s.
  - It sniffs the ground, with small nods, from 3.12 to 3.88 s.
  - The tail is held low and sways gently twice across the clip.
  - The body, legs and paws don't move at all.
  `Fox_LieDown`: 0-60, 2.000 s, 61 samples, one-shot. It starts on the standing pose.
  - From the start: the hind end sinks with the paws still planted.
  - Hind paws lift from 0.48 s, front paws from 0.52 s.
  - By about 1.0 s: it is down on its belly.
  - About 1.4 s: it rolls half onto its side and turns back.
  - 2.0 s: it ends curled with its head tucked under the brush, exactly on Fox_Rest's first pose.
  Join differences, measured in the glb (every joint's translation, rotation and scale at the two samples), and again through `pxe::SkinnedModel::sample` (largest component difference, sign-aware for rotations):
    Fox_Rest frame 0 -> Fox_StandUp first:   0 (bit-identical)
    Fox_StandUp last -> Fox_LookAround first: 0 (bit-identical)
    Fox_LookAround last -> Fox_LieDown first: 0 (bit-identical)
    Fox_LieDown last -> Fox_Rest frame 0:    0 (bit-identical)
  The joins also carry no sudden motion. StandUp eases into the standing pose and holds it for its last frames. LookAround's motions all ease in from and out to rest. LieDown eases out of standing. StandUp starts as Fox_Rest wraps, during the breath's pause. The crossfade is only a safety net.
  Turning: the root rotation is on the `root` joint, as in the curl. Standing, the fox has turned a quarter turn to its left: +90 degrees about +Y, counter-clockwise seen from above. It faces model +X, side-on to the original facing, with its right side toward the fire and its tail toward its original right (-X). LieDown turns it back.
  Why left: in the curl the fox's hips and head already point that way (the hips about 129 degrees and the head about 67 degrees left of the curl's facing), so it rises facing roughly the way its spine lies. It turns only about 20-40 degrees as it gets up, and nothing passes over the fire. Turning right would mean spinning the whole fox about 160 degrees as it rises (sliding paws), and would put its head and look-around on the rock's side.
  At the current kRuinsFoxSpot (-3.3, -6.6), on the fire's pond side, the curl faces the fire (yaw -70.6 degrees) and the standing fox faces world (0.33, 0.94), toward the default camera. Its face shows as it looks round.
  Paws planted while standing: from 0.99 s into StandUp (all four paws planted), through all of LookAround, to 0.48 s into LieDown, the paws don't slide or lift. Measured on the paw joints through `SkinnedModel` at 2.4x, the drift is 0 in all three clips. The pads stay 4 mm above the ground, as in the bind pose.
  Leg bend: for as long as the IK holds a leg (StandUp from 0.54 s for the front legs and 0.66 s for the hind, to the end; all of LookAround; LieDown until 0.92 s front and 0.9 s hind), the elbows stay behind the forepaws and the knees in front of the hind paws. Measured through `SkinnedModel` at 240 Hz and 2.4x: elbows at least 0.036 m behind, knees at least 0.18 m in front. No leg joint turns more than 24 degrees between adjacent keys, in any clip.
  Between keys (through `SkinnedModel::sample`/`skin` at 240 Hz, 2.4x): the lowest point in any clip is 4 mm below the ground (StandUp about 0.48 s). The largest departure of any vertex from the straight line between its two key positions is 0.039 m, in StandUp at 0.15 s: the head's quick arc as it lifts out of the curl, not a flip. Revision 4 showed 0.750 m and 0.115 m below the ground on the same test.
  Ground: at the keys, nothing goes more than 1 mm into the ground in any clip. While it rolls from its side onto its belly, the body heaves up by up to a few centimetres where a folded leg would otherwise pass into the ground.

Standing footprint at 2.4x (all four clips, every frame, skinned through `pxe::SkinnedModel` with only the 2.4x scale applied. Model space: y up, +z = original facing = toward the fire, +x = the original left. This matches the Blender measurement to the millimetre):
  All clips:      x -1.445 to +1.643 m (3.09 m across the original facing), z -1.092 to +0.703 m (1.80 m along it), y -0.001 to 1.535 m.
  StandUp:        x -1.436 to +1.426, z -1.067 to +0.703, y up to 1.502.
  LookAround:     x -1.313 to +1.396, z -0.314 to +0.573, y up to 1.535 (the standing fox itself).
  LieDown:        x -1.445 to +1.643, z -1.092 to +0.687, y up to 1.502.
  Fox_Rest:       x -0.523 to +0.523, z -0.431 to +0.427 (unchanged).
  The z extent toward -z (away from the fire) comes from the brush and rump swinging round as the curl unrolls (StandUp about 0.2-0.4 s, LieDown about 1.3-1.6 s). The large x extent is the standing body, nose at +x and brush at -x.
  Covering polygon: a convex 16-gon round every vertex of every frame of all four clips. Model space at 2.4x, (x, z) in metres, counter-clockwise from +x:
    (1.65, 0.14), (1.60, 0.25), (1.35, 0.50), (0.85, 0.71), (-1.04, 0.71), (-1.21, 0.64), (-1.38, 0.47), (-1.45, 0.29),
    (-1.45, 0.21), (-1.31, -0.15), (-0.81, -0.65), (0.28, -1.10), (0.70, -1.10), (1.07, -0.94), (1.47, -0.54), (1.65, -0.11)
  A covering circle round the origin has radius 1.644 m. Don't use it for the fire: a circle that size round kRuinsFoxSpot (-3.3, -6.6) reaches within 0.16 m of the fire's centre.
  Clearance at kRuinsFoxSpot (-3.3, -6.6), facing the campfire, on flat ground (no slope tilt). These are my figures; the developer re-checks them with the slope tilt.
    - The fire's middle is 1.803 m ahead, and the dirt patch's ragged rim reaches at most 0.96 m (addCampfire).
    - Every object in ruins.glb other than the ground was sampled above the lawn (from 2 cm below it) and below 1.6 m, within 4 m.
    Every vertex, every frame:
    - 0.372 m beyond the dirt patch's largest rim (1.332 m from the fire's middle, LieDown about 1.17 s).
    - 0.396 m from the paving (Paving.020, the head in LieDown about 1.23 s).
    - 1.28 m from the pond trunk.
    - The stones and logs stop inside the dirt patch.
    The 16-gon: 0.133 m beyond the dirt patch and 0.384 m from the paving. Being convex, it is a looser bound than the fox itself, so check against the skinned vertices.
  Fox::spawn's slope tilt samples the ground 0.41 m out (0.6 x kFootprint), sized for the curl. The standing fox reaches about 1.6 m out.
Revision: 4, 2026-10-21, three waking clips added (task fox-wake-look, cycle 1).
  - New clips: `Fox_StandUp`, `Fox_LookAround` and `Fox_LieDown`, with bit-identical joins to each other and to Fox_Rest's frame 0.
  - They are built from key poses (the curl, UNCURL, LIE, CROUCH, STAND), each part of the body (root, body, head, tail, front legs, hind legs) on its own timing, with 2-bone IK (`Builder.plant`) planting the paws where they stand.
  - New `pitch` pose control in `set_pose`.
  - `check()` now covers all four clips (`check_frames`).
  - New preview fox_wake.png.
  - Mesh, materials, joint names and Fox_Rest are unchanged, as are fox.png and the curl's footprint.
  - Built step by step in the user's live Blender session, with build_fox.py the source of truth, then rebuilt and exported headless. Two headless builds give identical clips.
Revision: 5, 2026-10-21, leg IK fixed between keys (REQ-001).
  - What was wrong: in Fox_StandUp and Fox_LieDown the planted legs flipped between adjacent keys (90-176 degree turns; elbows in front of the forepaws for about 0.25 s; mid-key paws lifting 0.45 m or dipping 0.115 m into the ground). There were three causes:
    - `Builder.plant` took each leg's bend direction from the blended pose, which is nearly straight, so the side flipped.
    - It swung each bone from its posed orientation, so a bone could roll half a turn.
    - LIE's hind legs folded so tight that the hock sat at the hip, where the IK has no direction.
  - The fix:
    - Each leg now bends in the plane it bends in standing, held in the chest or hips frame (`legs_planted`).
    - Each bone swings from its unturned orientation on its parent.
    - As the IK takes a leg over or hands it back, the leg eases between its posed angles and the IK's (`IK_EASE`).
    - LIE's hind legs rest with the hocks on the ground about 0.1 m from the hips.
  - `check_motion` added to the build (per-key turn limit of 45 degrees; half-key ground limit of 1 cm).
  - fox_wake.png gains six half-key frames at the former flip points.
  - Unchanged:
    - Clip names and lengths.
    - The bit-identical joins.
    - The quarter-turn left on `root`.
    - Fox_Rest (identical to revision 3).
    - Mesh, materials, joints.
    - fox.png.
    - The footprint and covering polygon at 2.4x (recomputed; identical).
  - The clearance figures are now given at the developer's new kRuinsFoxSpot.
  - Built step by step in the user's live Blender session, then rebuilt and exported headless.
