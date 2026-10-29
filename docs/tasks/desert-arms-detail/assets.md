# desert-arms-detail assets

## arms
Source: desert/tools/blender/build_arms.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_arms.py`
(builds, runs every check and stops if one fails, exports, reads the glb back and checks it, renders the four previews; `PXE_SKIP_PREVIEW=1` skips the previews, `PXE_SKIP_EXPORT=1` stops after the checks, `PXE_DESERT_OUT=<dir>` writes elsewhere). The build is deterministic: two runs give byte-identical glbs (checked with `cmp`). The same steps run one at a time in a live Blender (see the script's docstring); the live build gave the same checks and numbers.
Export: desert/assets/characters/arms.glb
Preview: beside the glb, all Workbench with back faces culled, Standard view transform (so the materials show their sRGB values):
- `arms_eye.png`: through the eye camera (60 degree vertical field of view, 960x540), over a sand plane 1.7 m below and a sky colour. This is what the player sees.
- `arms.png`: three-quarter view from front left and above, as before.
- `arms_left.png`: through the eye with a 20 degree vertical field of view, aimed at the gauge's centre (the bezel face's centre), 960x540.
- `arms_right.png`: the same, aimed at the compass face's centre.
Format: glb (glTF 2.0 binary), one skinned mesh on one armature, exported +Y up, flat-shaded (per-face normals), no textures
Units and scale: 1 unit = 1 metre, real adult scale. Place at scale 1.
Axes: a view model. Engine +Z is the view's forward (Blender -Y), +Y up, +X the player's left (Blender +X); the right arm is on engine -X. The placement is unchanged from `desert-arms`.
Origin: the eye. The `root` joint is at the origin; the armature and mesh nodes have identity transforms.
Triangles: 15,844 (budget 12,000-20,000). By part:
- hands, fingers and thumbs 3,080: palms 328; each digit 504 for both hands (12-sided segments); cuffs 232;
- glove detail 5,480: stitch dashes on the gloves 3,120 (260 dashes); worn patches 1,544; knuckle pads 352; cuff straps 288 and their rivets 176;
- wraps 1,288: arm tubes with their grime bands 952; tails 336;
- left bracer and devices 5,100: plate 92; straps 144; strap stitching 1,464 (122 dashes); buckle frames, tongues and keepers 152; gauge bezel 28; bars 36; screws 272; knurled dial and mark 548; toggles 276; coil and tube 1,252; cables 436; canister, clip and valve 400;
- compass 896 (strap 144, post, housing, face, ticks, needle, hub, lamp, buttons, hinge, lid, prop).

The task's guide was hands about 5,000, glove detail about 3,000 and compass about 2,500. The stitching set by the spec (5 mm dashes, 2.5 mm apart, along both sides of every finger and round the cuff) takes 3,120 triangles on its own. I kept the hands and the compass lower rather than go over 20,000 or thin the stitching.

Materials (glTF order = actor slots 0-13 from `kFirstMaterial = 0`). Base colours are set so the engine's `toSrgb` gives these sRGB values back exactly, checked in Blender and in the glb:

| Slot | Material | sRGB | Emissive | Crease (set by the game) | On |
|---|---|---|---|---|---|
| 0 | `Arms_Wrap` | (206, 194, 168) | no | yes | arm tubes, cloth bands |
| 1 | `Arms_Glove` | (78, 54, 40) | no | yes | cuffs, hands, fingers, thumbs |
| 2 | `Arms_Strap` | (118, 84, 56) | no | no | bracer straps, keeper loops, cuff straps, the compass's wrist strap |
| 3 | `Arms_Brass` | (176, 132, 64) | no | yes | bracer plate, gauge bezel, coil tube |
| 4 | `Arms_Glow` | (90, 230, 210) | yes (emissiveFactor = base colour) | no | the gauge's three bars |
| 5 | `Arms_Steel` | (92, 96, 102) | no | no | screws, canister, clip, valve wheel, compass housing, post, hub, hinge, lid and prop, buckle frames and tongues, snap rivets, toggle bases |
| 6 | `Arms_Copper` | (178, 98, 56) | no | no | coil, copper cable |
| 7 | `Arms_Rubber` | (44, 40, 38) | no | no | rubber cable, toggle levers, compass side buttons |
| 8 | `Arms_Thread` | (196, 170, 124) | no | no | glove and strap stitching |
| 9 | `Arms_Worn` | (112, 82, 62) | no | yes | knuckle pads, palm patch, back-of-hand patch, fingertip patches |
| 10 | `Arms_Grime` | (160, 146, 120) | no | yes | the 2nd and 5th band from the wrist, the frayed tails |
| 11 | `Arms_Dial` | (222, 212, 186) | no | no | compass face |
| 12 | `Arms_Paint` | (170, 64, 44) | no | no | toggle tips, compass ticks and needle, the dial's index mark |
| 13 | `Arms_Amber` | (255, 170, 60) | yes (emissiveFactor = base colour) | no | compass status lamp |

Every slot is used by some face (the build checks it, because the exporter drops unused materials and would shift the slots).

Nodes and objects: mesh node `Arms` (the skinned mesh), armature node `Arms_Rig`. Joints: the 39 of `desert-arms`, unchanged: `root`; per side `.L` / `.R`: `shoulder` > `upper_arm` > `forearm` > `hand` > `thumb.1..3`, and `hand` > `index.1..3`, `middle.1..3`, `ring.1..3`, `pinky.1..3`. Rest transforms are identical to the shipped glb at 505b604: the node transforms and inverse bind matrices differ by 0 (`docs/tasks/desert-arms-detail/scripts/compare_joints.py`).
Weights: as in `desert-arms` for the tubes, cuffs, hands and fingers. Every gadget part is rigid to its forearm (189 pieces, checked vertex by vertex): `forearm.L` for the plate, straps, buckles, stitching on the straps, gauge, bars, screws, dial, toggles, coil, cables, canister, clip, valve and the left tail; `forearm.R` for the compass, its strap and post, and the right tail. Stitches, pads and patches take the weights of the glove surface under them: rigid to their finger joint on the fingers, the palm's weights on the palm, and on the cuff the cuff's own blend, interpolated between its rings as its facets are, so they stay on it when the wrist turns.
Animations: none. The rest (bind) pose is the held pose, unchanged.
Collision footprint: none (a view model).

Held pose (engine coordinates, metres from the eye), unchanged: shoulder joint `upper_arm.L` (0.190, -0.220, -0.050); elbow `forearm.L` (0.339, -0.303, 0.196); wrist `hand.L` (0.215, -0.155, 0.370); index knuckle `index.1.L` (0.165, -0.164, 0.442); the right side mirrors x. Whole asset bounds x -0.387..0.387, y -0.350..-0.107, z -0.103..0.509.

### Framing (measured through the eye camera; all binding checks pass)

| Check | Rule | Left | Right |
|---|---|---|---|
| Everything in view in the lower 35% (vertices and edge quarter points) | NDC y <= -0.30 | highest at -0.571 (both arms) | |
| Top of the index knuckle | -0.70..-0.50 | -0.596 | -0.596 |
| Hand centre (centre of the screen box of palm, fingers, thumb) | \|x\| 0.40..0.60, left negative | (-0.448, -0.745); box x -0.643..-0.252, y -0.894..-0.596 | (+0.448, -0.745); box x 0.252..0.643, y -0.894..-0.596 |
| Forearm leaves through the bottom edge | exits at y < -1 | at x -0.725 | at x +0.725 |
| Wrist joint on screen | (guide) | (-0.566, -0.726), 0.37 m ahead | (+0.566, -0.726) |

Near clearance: every vertex in view, and every edge's quarter points, is at least 0.281 m ahead of the eye (rule >= 0.15 m). Read back from the glb by `docs/tasks/desert-arms/scripts/verify_arms_glb.py`: highest in-view NDC y -0.571, nearest in-view vertex 0.283 m.

### Gauge (unchanged; checked)
Bars at NDC y -0.970..-0.857, x -0.851..-0.686, the same as `desert-arms` records. 19.0, 18.4 and 17.9 px wide, facing the eye at cos 0.966, 0.973 and 0.979, each wholly in view and the first thing a ray from the eye meets.

### Compass (right wrist; checked)
- Mount: an `Arms_Strap` strap over the cuff's back end, 3.55 to 2.65 cm behind the wrist, carrying an 8 mm steel post at 27 degrees round from the forearm's back (the angle that faces the eye most). The face centre is 3.13 cm behind the wrist joint; the housing spans 1.34-4.88 cm behind it (rule: between the wrist and 7 cm behind).
- Face: an `Arms_Dial` disc 30 mm across (24 sides) on a 16-sided steel housing 36 mm across, turned halfway from the post's line toward the eye. 12 raised `Arms_Paint` ticks, a fixed `Arms_Paint` needle on a steel hub, an `Arms_Amber` lamp on the face's rim, two `Arms_Rubber` buttons on the housing's side. The steel sun-shade lid is hinged on the face's far edge (toward the fingers), swung open 112 degrees and held by a prop, so it stands beyond the face.
- Face centre on screen at NDC (0.630, -0.840), 0.301 m ahead.
- Normal to the eye: cos 1.000 (rule >= 0.87).
- Face wholly in view: all 216 face vertices (dial, ticks, needle, hub, lamp).
- First hit: nothing lies in front of any face vertex (0 of 216 blocked). Hits within 0.5 mm of the vertex or behind it don't count, the same tolerance as "seen".
- Dial disc 46.7 px wide (rule >= 30). Lamp 5.3 px wide (rule >= 3).
- The lid's top is at NDC y -0.610, within the lower 35%.

### Left bracer devices (checked)
- Seen (a vertex in view whose first ray hit is within 0.5 mm of it): the knurled dial yes; toggles 1, 2, 3 yes; bezel screws 1 and 2 (wrist end) yes, 3 and 4 (elbow end) no. Rule: dial, all three toggles, and at least 2 of the 4 screws.
- Small parts, width on screen (smallest extent of the in-view vertices over 8 directions at 960x540; rule >= 3 px):
  - screw heads 7.9 and 8.3 px;
  - toggle tips 5.8, 5.8 and 5.8 px;
  - dial index mark 4.6 px;
  - compass needle 5.9 px;
  - compass lamp 5.3 px;
  - compass ticks 4.0-5.3 px (each: 4.0, 4.2, 5.0, 5.1, 5.2, 4.7, 4.2, 4.4, 5.2, 5.2, 5.3, 4.5).
- Knurl: 16 ridges, 13 seen; pitch between neighbouring seen ridges 3.73-4.90 px (rule >= 3).
- Screws: 8-sided steel heads 5.0 mm across, slot 1.2 mm wide (the gap between the two halves of the head's top). The two at the wrist end sit on the bezel face's corners, 2.5 mm in from each edge. The two at the elbow end sit on the bezel's elbow-side wall by the corners, facing the elbow. On the face, the bottom edge of the view would cut them: the inner one's centre is at NDC y -1.009, and 1.9 px of its head would show. No position inside the 5 mm border clears that.
- Knurled dial: steel, 13 mm across the ridges, 16 ridges, 3 mm thick, on a short steel boss off the bezel's inner side (toward the body, -X). Its face is turned 75% of the way from that side toward the eye, so the ridges read round the rim. It carries an `Arms_Paint` index mark 2.6 x 4.6 mm.
- Toggles: three, 3 mm past the bezel's wrist edge, 18, 10 and 2 mm toward the outer side of the bezel's centre line, on the plate. Each has a 4.2 mm steel base, a rubber lever 6.2 mm long leaning 30% toward the wrist, and a 3 mm `Arms_Paint` tip.
- Coil: copper wire, 8 turns over 25 mm, on a 7 mm brass tube (12 sides), on the plate at 28 degrees round, between the straps (10.75-13.25 cm behind the wrist).
- Cables: copper (2.6 mm) and rubber (3 mm), from inside the bezel's elbow-side wall, along the plate under both straps, and down into the wrap band. Each ends 18 cm behind the wrist, 4 mm inside the wrap tube; the containment check holds in every pose.
- Canister: steel, 50 mm long (8.7-13.7 cm behind the wrist), 18 mm across, beside the plate's outer edge at -76 degrees round and over the first strap. A steel clip round its middle runs down to the plate's edge facet. A valve wheel (rim, hub, 4 spokes) sits on its wrist end.
- Plate: unchanged extent (4-16 cm behind the wrist, four 35 degree facets); its long edges and ends are bevelled 1.5 mm. Straps: unchanged places (9.0 and 14.8 cm). Each has a row of thread dashes 2.45 mm in from each edge, left out within 24 mm of the buckle. Each buckle is a steel frame 13 x 17 mm with a steel tongue, plus a strap keeper loop gripping the strap 12 mm along.

### Gloves and wraps
- Fingers and thumbs: the same joints and segment lengths, now 12-sided cut-box sections (30 degrees between sides), with a rounded fingertip.
- Stitching: 382 `Arms_Thread` dashes, every one 5.0 mm long and 2.5 mm wide (shortest 5.0 mm, narrowest 2.5 mm, measured on their vertices; rule 4 and 2.5). They stand 0.6 mm proud and sink 0.4 mm. They run along both sides of every finger and thumb segment, round the cuff's flare (3 a facet), and along both edges of the bracer straps.
- Knuckle pads: `Arms_Worn`, 12 x 10 mm, 2.5 mm proud with a 1 mm bevel, over each index-pinky `.1` joint.
- Patches: palm patch, back-of-hand patch, and a patch over each `.3` segment's last 9 mm. All `Arms_Worn`, 0.8 mm proud.
- Cuff strap: `Arms_Strap`, 1.2 cm wide (1.4 to 0.2 cm behind the wrist), with a two-step steel snap rivet on the outer side.
- Wraps: the six bands per forearm as before; the 2nd and 5th from the wrist are `Arms_Grime`. Band boundaries (m from the wrist): left -0.026, -0.0519, -0.0768, -0.1048, -0.1292, -0.1564, -0.1826; right -0.026, -0.0541, -0.0828, -0.1099, -0.137, -0.1633, -0.1933. Lips 4.3-5.0 mm. Every visible band edge turns 59.8 degrees or more (rule > 40).
- Frayed tails: from the band nearest the cuff (4.6 cm behind the wrist) on each forearm's outer side, an `Arms_Grime` knot and three strands 8.8, 7.4 and 6.4 cm long, 5.5 mm wide, with ragged ends. They hang down and out, rigid to the forearm. They hide neither the gauge nor the compass face (both first-hit checks pass).
- Round parts in creased materials: 45 pieces (arm tubes, cuffs, finger segments, fingertip patches, coil tube). The fewest sides is 12 (rule >= 10).

### Attachment
No gaps or floating pieces in the rest pose or any test pose: each wrist twisted, flexed and bent sideways 30 degrees each way, every finger and thumb joint curled 60 degrees, and each elbow opened 30 degrees (98.7 to 128.7 degrees). There are 511 closed, outward-facing pieces, and every piece reaches its arm tube through the attachments. The wrap tube's end, the back of the hand and both cable ends stay inside the pieces that cover them.

### Build checks (the script fails the build if any fails)
- Objects `Arms` and `Arms_Rig` only, identity transforms, `root` at the origin; the 39 joints.
- The 14 materials in slot order, their colours round-tripped through the engine's conversion, only `Arms_Glow` and `Arms_Amber` emissive, and every slot used.
- Every vertex weighted; flat shading; triangles 12,000-20,000.
- Closed outward volumes; band edges turn more than 40 degrees.
- The framing table; near clearance; the gauge; the compass; seen; small parts; knurl pitch.
- Round parts in creased materials at least 10 sides; stitch dashes at least 2.5 x 4 mm; gadget parts rigid to their forearm.
- Attachment in the rest and test poses.

After export the script reads the glb back and checks: one mesh and one skin, the 39 joints, the 14 materials and colours, no animations, NORMAL/JOINTS_0/WEIGHTS_0, and the same triangle count (15,844).

### Notes for the developer
- Load into actor slots 0-13 as task.md's step 2 says. Set crease ink on `Arms_Wrap` (0), `Arms_Glove` (1), `Arms_Brass` (3), `Arms_Worn` (9) and `Arms_Grime` (10) only.
- The glb is 1.4 MB, up from the first build: flat shading splits the vertices per face.
- The canister's valve wheel just reaches into the bottom-left corner of the view. It is the nearest thing in view, at 0.281 m.

Revision: 2, 2026-10-29, desert-arms-detail: glove, wrap and bracer detail, the hygrometer devices, the sun compass, 14 materials, 15,844 triangles. (Revision 1 was `desert-arms`.)
