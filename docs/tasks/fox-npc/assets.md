# Assets: fox-npc

## fox
Source: tools/blender/build_fox.py (reproducible; `/Applications/Blender.app/Contents/MacOS/Blender -b -P tools/blender/build_fox.py`, add `PXE_FOX_PREVIEW=<dir>` to also render previews. In an open Blender, run `Builder.STEPS` one by one, as in the script's docstring.) Every headless build checks that the fox holds together, in the bind pose and at 21 frames across `Fox_Rest`, and stops with an error if anything comes apart.
Export: assets/characters/fox.glb
Preview: assets/characters/fox.png (2000x400, five panels left to right: the curl as the game sees it, from the front 50 degrees down | the curl from above | low from the face side, showing the brush across the face and the head on the ground | low from the side, showing the legs and the ball resting on the ground | the standing bind pose from the side)
Format: glb (glTF 2.0 binary)
Units and scale: 1 unit = 1 metre, real red-fox size. Standing: 0.41 m at the shoulder, 1.11 m nose to tail tip. The engine draws the fox at 2.4x.
Curled footprint (Fox_Rest, measured by skinning through `pxe::SkinnedModel` at 2.4x, model space, y up, face toward +z):
  x -0.523 to +0.523 m (1.05 m wide), z -0.427 to +0.427 m (0.85 m deep), y 0 to 0.53 m high (0.60 m at the top of the ear flick, about 2.5 s).
  The footprint is centred on the origin and holds steady through the loop (breathing moves it under 1 mm). For clearing grass or keeping clear of the fire, a circle of radius 0.68 m around the origin covers the whole curl at 2.4x.
  At real size: 0.44 x 0.36 m, 0.22 m high.
Axes: +Y up in the glb. The fox faces -Y in Blender, which is +Z in the engine. Its left is +X.
Origin: ground level, centred under the body. In Fox_Rest the ball is turned and moved (on the `root` joint) so it sits centred over the origin, with the head (behind the brush) at the front (+Z in the engine). Turning the model's +Z toward the campfire puts the face, tucked behind the brush, toward the fire. The curl rests on the ground: its lowest point is at y = 0 and the head's underside 2 mm above it at real size (5 mm at 2.4x).
Triangles: 968 (budget < 3,000)
Materials (glTF order = the fox's actor material index; solid colours, no textures, none emissive):
  0 `Fox_Fur`   sRGB (208, 98, 40)   rust-orange coat
  1 `Fox_Cream` sRGB (238, 220, 188) muzzle, cheeks, throat, chest, belly, tail tip, inner ears
  2 `Fox_Dark`  sRGB (54, 38, 33)    leg stockings, paws, ear backs
  3 `Fox_Black` sRGB (20, 17, 17)    nose, closed eyes
  `SkinnedModel::load` gives these vertices surfaces kActorSurface + 0..3 (16..19). The fox must add 8 to put them in actor slots 8..11, and write `materials()[i]` into actor material 8 + i.
Nodes and objects: one skinned mesh node `Fox` (skin of 27 joints), parented to the armature node `FoxRig` (identity transform, the scene root).
  Joints (glTF names; `SkinnedModel::joint(name)`):
    root
    hips > spine.1 > spine.2 > chest > neck > head > ear.L, ear.R
    spine.2 > ribcage        (leaf; animated scale for breathing, weights only on the mid-trunk)
    chest > upperarm.L > forearm.L > forepaw.L     (and .R)
    hips > thigh.L > shin.L > hindpaw.L            (and .R)
    hips > tail.1 > tail.2 > tail.3 > tail.4 > tail.5
  The bind (rest) pose is the fox standing (paws within 4 mm of the ground), ready for future walk/run clips. Every bone lying along the body has its local z axis up, and every leg bone has it facing forward, as on the hero.
Animations:
  `Fox_Rest`: 0-120 at 30 fps, duration 4.000 s, 121 linear samples on every joint (translation, rotation and scale channels). The last sample equals the first, so it loops seamlessly with `sample(clip, t, pose, true)`.
  Pose: a tight round ball, as a red fox sleeps. It lies on its right side with the body curled round in a ring and the legs folded in, the lower legs resting on the ground. The head lies on the ground at the end of the ring, chin and cheek down, ears folded back, with the nose at the root of the brush. The brush leaves the rump, wraps round the outside of the ball and passes across the front of the face.
  Motion: one slow breath per loop (in over 0-1.6 s, out over 1.6-3.6 s, then a pause), with the ribcage scaling up to 1.07 across. A double flick of `ear.L`, lifting off the head, at about 2.4-2.85 s. A slow stir of the tail tip (`tail.5`) once round the loop.
  The curl is entirely in the clip. `restPose()` alone shows the fox standing, so always sample Fox_Rest.
Revision: 1, 2026-10-21, initial build.
Revision: 2, 2026-10-21, the curl tightened (cycle 1, user review). The head is lowered and laid flat across the middle of the curl with its ears folded back, and the nose is clearly under the brush's cream tip. The trunk is bent into a tighter C so the middle of the ring is covered from above. The footprint shrinks from 0.57 x 0.40 m to 0.38 x 0.49 m, and the height from 0.44 m to 0.32 m. Built step by step in the user's live Blender session, with build_fox.py still the source of truth. Joint names, material order, and the clip name, duration and loop are unchanged.
Revision: 3, 2026-10-21, the curl rebuilt (cycle 2, user review: the hind legs came apart, the head lay on the back, the tail didn't wrap the face).
  - Each leg is now one continuous tube from inside the body to the toes, instead of three separate open tubes, so it can't split at the knee, hock or elbow.
  - The skin weights anchor the top of each leg in the hips or chest, both ends of the neck in the chest and head, and the face on the head. With that, no open end or seam shows and no piece floats, in the bind pose or at any frame of the loop. The build now checks this.
  - The curl is laid out on a ring (`curl_aims`): a tight ball on its right side, the head on the ground with the nose at the root of the brush, and the brush wrapped round the outside across the face.
  - New `aim` and `tuckS`/`hipTuckS` pose controls in `set_pose`.
  - The paw pads now touch the ground in the standing pose.
  - The preview now has five panels, including low views of leg attachment and head contact. Footprint figures are now given at 2.4x.
  - Built step by step in the user's live Blender session, with build_fox.py still the source of truth. Joint names, material order, and the clip name, duration, seamless loop, breathing, ear flick and tail stir are unchanged (the tail stir is a little stronger).
