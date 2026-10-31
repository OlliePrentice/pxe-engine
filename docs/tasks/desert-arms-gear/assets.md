# desert-arms-gear assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b --python-exit-code 1 -P desert/tools/blender/build_arms.py` (exit 0, every check passing).
Built step by step in the live Blender session first (Arms scene, through the MCP): `check()` and `clip_checks()` returned no problems, 24,208 triangles, 679 pieces, the same as headless.
Export: desert/assets/characters/arms.glb (2,244,820 bytes). Deterministic: two runs into scratch `PXE_DESERT_OUT` dirs gave `cmp`-identical glbs, identical to the committed one.
Preview: beside the glb, all six rewritten: `arms_eye.png`, `arms.png`, `arms_left.png`, `arms_right.png`, `arms_walk.png`, `arms_sprint.png`. Workbench, back faces culled, Standard view transform.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, flat-shaded, no textures. Unchanged.
Units and scale: 1 unit = 1 metre, real adult scale. Unchanged.
Axes: a view model. Engine +Z is the view's forward, +Y is up, +X is the player's left (Blender -Y forward, +Z up). Unchanged.
Origin: the eye, with the `root` joint at the origin. Unchanged.
Triangles: 24,208 (budget 18,000-30,000), up from 16,396.
Materials (slot = glTF index, sRGB, ✦ emissive), read back from the glb exactly:
0 Arms_Wrap (176,160,128), 1 Arms_Glove (104,90,76), 2 Arms_Strap (86,64,48), 3 Arms_Gunmetal (70,74,78), 4 Arms_Glow ✦ (60,230,230), 5 Arms_Steel (120,122,124), 6 Arms_Copper (150,96,64), 7 Arms_Rubber (40,38,36), 8 Arms_Thread (96,80,64), 9 Arms_Worn (130,114,96), 10 Arms_Grime (150,136,110), 11 Arms_Dial (34,40,42), 12 Arms_Rust (122,70,46), 13 Arms_Undersuit (46,44,44), 14 Arms_Screen ✦ (14,32,34). The game inks creases on wrap, glove, gunmetal, worn and grime (`CREASED`).
Nodes and objects: mesh node `Arms`, skin `Arms_Rig`, the same 39 joints in the same order. `tools/glb.py compare` against the cycle base: joints equal, joint transforms and inverse bind matrices identical (largest difference 0); the glove's primitive identical.
Animations: `Arms_Walk`, `Arms_Sprint`, 1.0 s, 117 channels, 31 LINEAR keys each. Regenerated from the unchanged pose: all 7,254 keys equal to the base glb's (largest difference 0).
Revision: 2, 2026-10-31, gear and palette after the concept art (task desert-arms-gear).

### What changed (Approach §1)
- **Sleeve:** the arm tube is `Arms_Undersuit`, with rings every 10 mm on the forearm and every 2.5 mm just behind the cuff. The six stepped bands, their grime colouring, `BAND_*` and `band_crease_problems` are gone.
- **Strips (`{S}.strip.1-3`):** three per forearm (`STRIPS`), one in `Arms_Grime`. Each is a closed ring following the sleeve's facets, 1.8-2.4 mm proud, sunk 0.6 mm in, leaning 1.5-2.5 degrees, 24 sections, 12 notches per edge, 3.3-4.3 mm deep (measured 3.30-4.28 mm). Strips take the sleeve's ring weights.
- **Tails:** `{S}.tail_root` knotted on a strip, and `{S}.tail.1-3` (3 per forearm), rigid to the forearm.
- **Hand plates (`{S}.hand_plate.1-3`):** three overlapping gunmetal segments, 2.0 mm thick, over the back of the hand. Their top rim (0.5 mm bevel) and walls are `Arms_Steel` faces. Rigid to `hand.<S>`.
- **Knuckle guards (`{S}.guard_index/middle/ring/pinky`):** `Arms_Worn`, 3.5 mm proud, rigid to `hand.<S>`, standing on the plate's last segment. The old knuckle pads are gone. Patches and stitches are unchanged.
- **Vambraces (`{S}.vambrace`):** gunmetal plates, 4 mm thick, spanning 140.0 degrees about `back`, bevelled edges and end walls in `Arms_Steel` faces. The left is unchanged in place, s -0.160 to -0.020 (0.140 m), so the gauge doesn't move. The right is s -0.190 to -0.062 (0.128 m), with its wrist end inside the preferred -0.045 to -0.066 range, so part of it stays in view. Each plate has 5 scratches (`{S}.scratch.1-5`, 0.9 mm wide, 18-34 mm long, 0.5 mm proud), 2 rust patches (`{S}.rust.1-2`) and 2 straps (`{S}.strap.1-2`) with buckle frame, tongue and keeper, and stitched edges. Plates stand off the strips. In the attachment chain the straps hold them, and the straps sink into the strips.
- **Gauge, devices, compass:** same geometry, new materials. Bezel, coil tube, canister, compass housing and lid are `Arms_Gunmetal`. Toggle tips and knurl mark are `Arms_Rust`. The compass ticks, needle and lamp are `Arms_Glow`. The copper cable is gone and the rubber cable (`L.cable`) stays.
- **Tubes (`L.tube.1`, `L.tube.2`, `R.tube.1`):** `Arms_Glow`, 2.5 mm radius, 8 sides, rigid to the forearm.
  - The left two start inside the gauge (the first on the old copper cable's path). The right one starts inside the compass housing, comes out of its bottom and runs over the cuff and the strip gap onto the plate.
  - Each runs along its plate, dips under both straps, and goes into the sleeve past the plate's elbow end. Their last rings are at NDC y -2.03, -2.11 and -2.44.
- **Undersuit seen (right forearm):** 17 sleeve vertices seen at rest (needs 8), s -0.0625 to -0.040.

### New checks (`gear_problems()` in `check()`; undersuit and tubes in `framing()`), targets in `SPEC_MATERIALS` / `GEAR_TARGETS`
Each was proved with one negative run through `tools/blender-override.py` (list in `scripts/negative_runs.txt`). Every run exited 1 on the intended check:

| Check | Override | Failure |
|---|---|---|
| Materials | Glow (61,230,230) | "Arms_Glow converts back to (61, 230, 230), not (60, 230, 230)" |
| Copper only on the coil | `M` Gunmetal→6 | "Arms_Copper on ['L.canister', ...]" |
| Undersuit seen | R plate (-0.170, -0.040) | "only 2 undersuit vertices on the R forearm" |
| Notches | `FRAY_DIP` 1.5-2.5 mm | "0 notches" |
| Strips | 4 strips on R | "R forearm has 4 strips" |
| Tails | 4 strands | "4 tails" |
| Plate length | R plate 0.153 m | "0.1530 m long" |
| Plate span | `BRACER_SPAN` 80 | "spans 160.0 degrees" |
| Scratches | width 1.5 mm / 3 scratches | each fails |
| Straps | one strap on R | "wants 2, each with a buckle" |
| Hand plates | non-overlapping segments | "touching [False, False]" |
| Tube radius / sides / start / last ring | `TUBE_R` 0.003 / `TUBE_SIDES` 6 / `TUBES_L` y 0.040 / `TUBE_R_END` -0.036 | each fails |
| Tube clearance | `CLEARANCE` 0.27 | "L.tube.1 comes 0.266 m" |

Two runs are weaker than the rest:
- No constant sets the knuckle-guard count, so its negative is `scripts/negative_guard.py` (one guard dropped, "L hand has 3 knuckle guards, wants 4").
- The tube clearance negative raises the target rather than moving a built value.

All existing checks keep their values and pass. Framing is unchanged: highest in view NDC y -0.410, nearest 0.258 m, knuckles -0.469. The readout (38 dots, 4.56 x 4.70 px min, facing 0.954) and the compass (dial 50.9 px, cos 0.999, 0 face vertices blocked) are also unchanged.

### Notes for the developer
- `arms.cpp`/`arms.hpp`: rename to the slots above (`kGunmetal = 3`, `kRust = 12`, `kUndersuit = 13`). Crease ink goes on 0, 1, 3, 9 and 10. Only slots 4 and 14 are emissive.
- The hand plates and knuckle guards are rigid to `hand.<S>` as specified, but the glove's pinky side under them blends toward the forearm (wrist weights 0.16-0.98). In the ±30 degree wrist test poses the plate therefore slides over the glove, by up to about 6 mm at the pinky guard. The guards stand on the plate's last segment, so they stay attached. In the walk and sprint clips the wrist turns far less.
