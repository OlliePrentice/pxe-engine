## REQ-001: fox: leg joints flip between adjacent keys in Fox_StandUp and Fox_LieDown
Status: done
Blocking: yes

### Problem
In some frames of `Fox_StandUp` and `Fox_LieDown`, a leg chain's local rotations jump by 90-176 degrees from one 30 fps key to the next. The IK solution flips: the elbow or knee swaps to the other side of the paw. The clips only look right at the keys themselves. The game samples at 60 Hz or faster, so it draws the in-between poses. In those poses the legs swing far out or fold into the body, then snap back. Even at the keys, the elbow or knee jumps up to 0.6 m in one frame.

I measured this through `pxe::SkinnedModel::sample`/`skin` at 2.4x. The engine slerps rotations the short way (sign-aware), so these are real rotation differences, not quaternion sign flips. Each row lists the key pair (frame k to k+1), the largest per-key rotation change on the joints involved, and the largest distance of any vertex from the straight line between its two key positions at the half-key:

| Clip | Keys | Joints (both sides, same angle) | Rotation change | Mid-key deviation |
|---|---|---|---|---|
| Fox_StandUp | 22-23 | thigh, hindpaw | 175.9, 174.3 deg | 0.336 m |
| Fox_StandUp | 26-27 | upperarm, forearm, forepaw | 105.2, 164.7, 91.2 deg | 0.750 m |
| Fox_StandUp | 34-35 | upperarm, forearm, forepaw | 63.6, 122.0, 57.6 deg | 0.092 m |
| Fox_LieDown | 9-10 | upperarm, forearm, forepaw | 68.8, 130.6, 61.5 deg | 0.105 m |
| Fox_LieDown | 13-14 | upperarm, forearm, forepaw | 77.7, 145.7, 68.9 deg | 0.130 m |
| Fox_LieDown | 22-23 | thigh, hindpaw | 119.9, 118.0 deg | 0.174 m |

(Fox_Rest and Fox_LookAround: the worst mid-key deviation is 2 mm. They are fine.)

What the joints do (model space at 2.4x, after the root turn):
- StandUp 26-27: the forearm joint (elbow) moves from x 0.38 to 0.86 in one key. Between the keys, the planted forepaw (x 0.62, y 0.12 at both keys) lifts to y 0.57 at 26.5 and comes back down.
- StandUp 27-34 and LieDown 10-13: the elbow stays at x of about 0.80, in front of the forepaw (x 0.62). The front legs are bent the wrong way for about 0.25 s in each clip. The flip into and out of this state (StandUp 26-27 and 34-35, LieDown 9-10 and 13-14) is a visible pop. Mid-key, a forepaw vertex dips up to 0.115 m into the ground (LieDown 0.45 s, StandUp 1.15 s).
- StandUp 22-23: the knee (shin joint) jumps from (0.06, 0.50) to (-0.40, 0.09). Mid-key, it passes through the ground (y 0.00 at 22.75). LieDown 22-23 does the reverse, (-0.24, 0.04) to (0.10, 0.45).

In the game (`--fox-wake 0.8833 --hide-grass`, the half-key between StandUp keys 26 and 27), the front legs fold up into the chest for a frame and then reappear planted. At 60 fps this shows as a flicker of the front legs in each StandUp and LieDown. The designer's `check()` samples whole frames and tests only attachment, so it can't catch this.

### Required change
- In `Fox_StandUp` and `Fox_LieDown`, keep each leg's IK bend direction consistent: elbows behind the forepaws, knees in front of the hind paws, as in the standing pose. For example, add pole targets or fix the bend in `Builder.plant`, so no leg joint's local rotation changes by more than about 30 degrees between adjacent keys while its paw is planted.
- Extend `check()` to fail when any joint's rotation changes by more than about 45 degrees between adjacent keys, or when any vertex at a half-key time goes below the ground by more than 1 cm.
- Keep everything else as revision 4: the clip names and lengths, the bit-identical joins, the quarter-turn left on `root`, Fox_Rest, and the mesh. If the standing footprint at 2.4x changes, update it in assets.md. The developer re-runs the clearance check at kRuinsFoxSpot (-3.3, -6.6).

### Designer notes
Fixed in fox revision 5 (assets.md).

Cause, reproduced with your figures: the same joints, keys and angles on a new between-key check in the build. There were three faults in the leg IK (`Builder.plant`):
1. It took each leg's bend direction from the blended pose. That pose's legs are nearly straight, so the direction was unstable and the elbow or knee swapped sides.
2. It swung each bone by the least turn from the bone's posed orientation. When that was far from the IK's answer, the bone rolled up to half a turn: the 176-degree thigh and hind-paw flips.
3. LIE's hind legs folded so tight that the hock sat at the hip (7 mm apart, the IK's minimum reach). There the hip-to-paw direction is meaningless, which produced the StandUp 22-23 and LieDown 22-23 flips.

Changes in tools/blender/build_fox.py:
- `legs_planted()` now also records each leg's bend plane from the standing pose, held in the chest or hips frame. `plant()` always bends in that plane: elbows behind the forepaws, knees in front of the hind paws.
- `plant()` swings each bone from its unturned orientation on its parent. Both changes reproduce the standing legs exactly, so the joins stay bit-identical.
- As the IK takes a leg over at the start of a step (or hands it back in LieDown), the joints ease from the posed angles to the IK's over the first 40% of the step (`IK_EASE`). Without this the hand-over still turned the thighs 45 degrees in one key.
- LIE's hind legs rest with the hock on the ground about 0.1 m from the hip.
- New `check_motion()`, run on all four clips by every headless build:
  - Between keys it interpolates as the engine does (local rotations slerped the short way, translation and scale lerped).
  - It fails on any joint turning more than 45 degrees between adjacent keys, or any vertex more than 1 cm below the ground at a half-key.

Results:
- Largest per-key joint turn, any joint: StandUp 23.9 degrees, LieDown 19.5, LookAround 4.1, Rest 12.4. The 23.9 and 19.5 are a forepaw turning while it steps.
- Every leg joint turns less than 24 degrees per key throughout.
- Through `pxe::SkinnedModel::sample`/`skin` at 240 Hz, 2.4x:
  - Lowest point: -4.3 mm (StandUp about 0.48 s). Revision 4: -115 mm.
  - Largest mid-key departure from the key chord: 0.039 m (StandUp 0.15 s, the head's arc as it lifts out of the curl). Revision 4: 0.750 m.
  - While the IK holds a leg, elbows are at least 0.036 m behind the forepaws and knees at least 0.18 m in front of the hind paws.
  - Planted-paw drift is 0.
- Joins are still bit-identical in the glb and through `SkinnedModel`.
- The quarter-turn left on `root` is unchanged.
- Fox_Rest is identical to revision 3. The mesh, materials and joints are unchanged.
- The footprint and covering 16-gon at 2.4x are identical to revision 4 (recomputed).
- Clearance at the new kRuinsFoxSpot (-3.3, -6.6), flat ground, every vertex of every frame: 0.372 m beyond the dirt patch's largest rim (0.96 m), 0.396 m from the paving (Paving.020).

fox_wake.png has six half-key frames added at the former flip points (StandUp 22.5, 26.5, 34.5; LieDown 9.5, 13.5, 22.5).
