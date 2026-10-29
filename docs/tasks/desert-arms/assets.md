# desert-arms assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_arms.py`
(builds, runs every check and stops if one fails, exports, reads the glb back and checks it, renders the previews; `PXE_SKIP_PREVIEW=1` skips the previews, `PXE_DESERT_OUT=<dir>` writes elsewhere). The build is deterministic: two runs give byte-identical glbs. `docs/tasks/desert-arms/scripts/verify_arms_glb.py` re-imports the glb into an empty scene and compares it with a fresh build (rest-pose bounds agree to 1e-7 m).
Export: desert/assets/characters/arms.glb
Preview: desert/assets/characters/arms_eye.png (through the eye camera, 60 degree vertical field of view, 960x540: what the player sees, over a sand plane 1.7 m below and a sky colour), desert/assets/characters/arms.png (three-quarter view from front left and above). Both Workbench, back faces culled.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, exported +Y up, flat-shaded (per-face normals), no textures
Units and scale: 1 unit = 1 metre, real adult scale. Place at scale 1.
Axes: a view model. Engine +Z is the view's forward (Blender -Y), +Y up, +X the player's left (Blender +X); the right arm is on engine -X. Place it with the binding eye transform in task.md (`eye[0] = -view.right`, `eye[1] = view.up`, `eye[2] = view.forward`, `eye[3] = view.eye`).
Origin: the eye. The `root` joint is at the origin; the armature and mesh nodes have identity transforms.
Triangles: 2,236 (budget 2,000-3,000). Arm tubes 952, cuffs 232, hands 104, fingers and thumbs 680, bracer 36, straps 144, buckles 24, gauge bezel 28, gauge bars 36. 45 closed pieces.
Materials (glTF order = actor slots 0-4 from `kFirstMaterial = 0`; base colours set so the engine's `toSrgb` gives these sRGB values back exactly, checked in Blender and in the glb):

| Slot | Material | sRGB | Emissive | Crease (set by the game) | On |
|---|---|---|---|---|---|
| 0 | `Arms_Wrap` | (206, 194, 168) | no | yes | arm tubes, stepped cloth bands |
| 1 | `Arms_Glove` | (78, 54, 40) | no | yes | cuffs, hands, fingers, thumbs |
| 2 | `Arms_Strap` | (118, 84, 56) | no | no | two straps and two buckles (left) |
| 3 | `Arms_Brass` | (176, 132, 64) | no | yes | bracer plate and gauge bezel (left) |
| 4 | `Arms_Glow` | (90, 230, 210) | yes (emissiveFactor = base colour) | no | three gauge bars (left) |

Nodes and objects: mesh node `Arms` (the skinned mesh), armature node `Arms_Rig`. Joints (39, binding names): `root`; per side `.L` / `.R`: `shoulder` > `upper_arm` > `forearm` > `hand` > `thumb.1` > `thumb.2` > `thumb.3`, and `hand` > `index.1..3`, `middle.1..3`, `ring.1..3`, `pinky.1..3`.
Weights: smooth over 4 cm across each elbow (forearm/upper_arm) and each wrist (hand/forearm; the cuff, the wrap tube's end inside it and the back of the hand share the same blend, so they bend together). Every finger and thumb segment rigid to its joint; the bracer, straps, buckles, bezel and bars rigid to `forearm.L`; the wraps to their forearm; the shoulder cap blends shoulder/upper_arm.
Animations: none. The rest (bind) pose is the held pose: `SkinnedModel::restPose` gives it.
Collision footprint: none (a view model; `kBodyRadius` and the palm collision are unchanged).

Held pose (engine coordinates, metres from the eye; the right side mirrors x):
- shoulder joint `upper_arm.L` (0.190, -0.220, -0.050); clavicle head `shoulder.L` (0.030, -0.205, -0.075)
- elbow `forearm.L` (0.339, -0.303, 0.196): interior angle 98.7 degrees, 0.083 below the shoulder, 0.246 ahead of it, 0.149 out from it
- wrist `hand.L` (0.215, -0.155, 0.370): forearm rises 34.8 degrees; the hand bends 54 degrees from the forearm's line (down, and from the forearm's inward run toward straight ahead); hand centre 0.425 ahead of the eye
- index knuckle `index.1.L` (0.165, -0.164, 0.442)
- whole asset bounds: x -0.387..0.387, y -0.350..-0.120, z -0.103..0.509 (z < 0 is the shoulder caps behind the eye)

Framing, measured by the build through the eye camera (Blender: at the origin looking along -Y, +Z up, 60 degree vertical field of view, 16:9; NDC x positive to the screen's right). All binding checks pass:

| Check | Rule | Left | Right |
|---|---|---|---|
| Everything in view in the lower 35% | NDC y <= -0.30 | highest point in view at -0.571 (both arms, vertices and edge midpoints) | |
| Top of the index knuckle (highest point of `index.1` within 12 mm of its joint) | -0.70..-0.50 | -0.593 | -0.593 |
| Hand centre (centre of the screen box of palm, fingers and thumb) | \|x\| 0.40..0.60, left negative | (-0.448, -0.744); box x -0.643..-0.252, y -0.895..-0.593 | (+0.448, -0.744); box x 0.252..0.643, y -0.895..-0.593 |
| Forearm leaves through the bottom edge (axis walked wrist to elbow) | exits at y < -1, \|x\| <= 1 | exits at x -0.725 | exits at x +0.725 |
| Wrist joint on screen | (guide) | (-0.566, -0.726), 0.37 m ahead | (+0.566, -0.726) |

Near clearance: every vertex in view, and every edge's quarter points, is at least 0.281 m ahead of the eye along the view (rule >= 0.15 m; near plane 0.1 m). Read back from the glb's own positions: highest in-view NDC y -0.571, nearest in-view vertex 0.283 m.

Gauge (also checked by the build: wholly in view, facing the eye, at least 3 px wide, and the first thing a ray from the eye meets): on the left bracer's wrist end, 5.85 cm behind the wrist, its face turned 55% of the way from the plate toward the eye. Bars at NDC y -0.970..-0.857, x -0.851..-0.686; 19.0, 18.4 and 17.9 px wide at 960x540; facing the eye at cos 0.97-0.98. The bezel's lower corner reaches NDC y -1.05, so about 14 px of brass is cut by the bottom edge; the window and bars are wholly in view.

Wraps: six bands a forearm from under the cuff (2.6 cm behind the wrist) to 18-19 cm behind it, widths 2.4-3.0 cm, lips 4.3-5.0 mm proud of the band under them, each band edge leaning 1.5-4.5 degrees; every visible band edge turns 59.8 degrees or more (rule > 40, so the crease ink draws it). The tubes are 12-sided (30 degrees between sides, so no ink along their length). Bracer: 12 cm (4 to 16 cm behind the left wrist), four 35 degree facets, two 14 mm straps at 9.0 and 14.8 cm behind the wrist with square buckles on the outside. Cuff: 3.6 cm long, flaring to 4.6 cm radius over the wraps.

Attachment: no gaps or floating pieces (every piece touches the one it hangs from; the wrap tube's end and the back of the hand stay inside the cuff) in the rest pose and in each test pose: each wrist twisted, flexed and bent sideways 30 degrees each way, every finger and thumb joint curled 60 degrees, and each elbow opened 30 degrees (98.7 to 128.7 degrees).

Build checks (the script fails the build if any fails): objects `Arms` and `Arms_Rig` only, identity transforms, `root` at the origin; the 39 joint names; the five material names in slot order, their colours round-tripped through the engine's conversion, only `Arms_Glow` emissive; every vertex weighted; flat shading; triangles 2,000-3,000; every piece a closed, outward-facing volume; band edges turn more than 40 degrees; the framing table above; near clearance; the gauge; attachment in the rest and test poses. After export it reads the glb back: one mesh and one skin, the 39 joints, materials and colours, no animations, NORMAL/JOINTS_0/WEIGHTS_0 present, and the same triangle count.

Notes for the developer:
- The held pose deviates from task.md's guide where the framing settled it, as the spec allows: the elbows are 0.25 m ahead of the shoulders and 0.15 m out from them (not "a little ahead", "near the body"), and the hands bend 54 degrees at the wrist. With real proportions that's what puts the knuckles 20% up, keeps the forearms leaving through the bottom edge and keeps the gauge in view.
- Only a few centimetres of each forearm behind the glove are in view, so the wrap bands show mostly at the bottom corners and in the three-quarter preview.

Revision: 1, 2026-10-29, first build.
