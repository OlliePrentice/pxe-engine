# Detailed arms and gadgets
Type: feature
Status: complete
Cycle: 1

## Goal
Raise the fidelity of the desert player's arms (`desert/assets/characters/arms.glb`, task `desert-arms`).

At the hands, one pixel of the 960×540 view is under 1 mm, so fine detail can show. The rebuild:
- details the gloves and wraps: stitching, knuckle pads, worn patches, a cuff strap, grime bands and frayed tails;
- rebuilds the left brass bracer into an intricate hand-built hygrometer round the existing teal gauge;
- adds a new gadget on the back of the right wrist, a sun-compass chronometer with an amber status lamp.

These stay as they are:
- the concept, the held pose and the framing;
- the five shipped materials and the 39 joints.

The detail goes where the player sees it: gloves, cuffs, wrist ends and the bracer. It's still rest-pose only. `desert-arms-walk` adds the clips after this task.

## Decisions
All from the planning interview with the user, 2026-10-29.
- **Gadgets** (Q1): the existing left bracer and gauge, rebuilt as an intricate device, plus a second gadget on the right wrist. The hands stay empty and keep their pose. No held tool.
- **What has shipped stays** (Q2):
  - the wanderer concept: wraps, leather gloves, brass bracer, teal glow;
  - the five materials `Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Brass`, `Arms_Glow`, with their names, colours and slots 0–4;
  - the 39 joint names;
  - the crease flags on Wrap, Glove and Brass.
- **Pose and framing stay put** (Q3):
  - every joint's rest transform is unchanged;
  - the binding framing rules are unchanged;
  - the gauge's place, window and three bars are unchanged.

  Detail goes where it is seen.
- **Art style amended for view models** (Q4): detail down to about 3 px on screen (about 2.5 mm at the hands). Still flat-shaded, one solid colour per material, no textures. Small round parts take as few sides as still read round. A standing rule, written into `desert/docs/art-style.md` here.
- **Budget** (Q5): 12,000–20,000 triangles for both arms.
- **Materials** (Q6, Q13): up to 16 in all. Nine new ones in slots 5–13, binding (sRGB base colours):

  | Slot | Material | On | sRGB | Glow | Crease ink |
  |---|---|---|---|---|---|
  | 0 | `Arms_Wrap` | wrap tubes and bands | (206, 194, 168) | no | yes |
  | 1 | `Arms_Glove` | cuffs, hands, fingers, thumbs | (78, 54, 40) | no | yes |
  | 2 | `Arms_Strap` | bracer straps, keeper loops, cuff straps, the compass's wrist strap | (118, 84, 56) | no | no |
  | 3 | `Arms_Brass` | bracer plate, gauge bezel, coil tube | (176, 132, 64) | no | yes |
  | 4 | `Arms_Glow` | the gauge's three bars | (90, 230, 210) | yes | no |
  | 5 | `Arms_Steel` | screws, canister and its clip, valve wheel, compass housing, post and lid, buckle frames and tongues, snap rivets, toggle bases | (92, 96, 102) | no | no |
  | 6 | `Arms_Copper` | coil, copper cable | (178, 98, 56) | no | no |
  | 7 | `Arms_Rubber` | rubber cable, toggle-switch bodies, compass side buttons | (44, 40, 38) | no | no |
  | 8 | `Arms_Thread` | glove and strap stitching | (196, 170, 124) | no | no |
  | 9 | `Arms_Worn` | worn glove patches, knuckle pads, palm patch | (112, 82, 62) | no | yes |
  | 10 | `Arms_Grime` | grime bands, frayed tails | (160, 146, 120) | no | yes |
  | 11 | `Arms_Dial` | compass face (ivory enamel) | (222, 212, 186) | no | no |
  | 12 | `Arms_Paint` | toggle tips, compass needle and tick marks, dial index mark | (170, 64, 44) | no | no |
  | 13 | `Arms_Amber` | compass status lamp | (255, 170, 60) | yes | no |

  Crease ink stays off on the small-part materials, because a 6- or 8-sided screw would be inked on every side. The inner outlines from `view-model-edges` separate those parts instead. There's no glass: the engine has no transparency.
- **Static** (Q8): no moving parts, no blinking lights, no clips in this task.
- **Left bracer** (Q9, Q17), a hand-built hygrometer:
  - a brass plate with bevelled edges;
  - the raised bezel held by 4 slotted screws;
  - a knurled adjustment dial on the bezel's **inner side** (toward the body, which faces the eye);
  - a row of 3 toggle switches;
  - a copper coil wound round a short brass tube;
  - two cables, copper and rubber, from the housing back under the wraps;
  - a small steel canister clipped beside the plate, with a valve wheel;
  - stitched strap edges, and buckles with a frame, tongue and keeper loop.

  The gauge, screws, dial and toggles go at the wrist end, where the eye sees them.
- **Right wrist** (Q10): a sun-compass chronometer:
  - a round steel housing and an ivory dial face;
  - a fixed needle and a ring of tick marks;
  - an amber status lamp;
  - two side buttons;
  - a hinged sun-shade lid, propped open.
- **Gloves** (Q11):
  - stitched seams along each finger's sides and round the cuff;
  - raised knuckle pads over each `index`–`pinky` `.1` joint;
  - a reinforced palm patch;
  - worn patches on the fingertips and the back of the hand;
  - a cuff strap with a snap rivet.
- **Wraps** (Q11):
  - each band's overlap edge visible;
  - one loose, frayed tail from each forearm's band nearest the cuff, rigid to the forearm;
  - a few grime bands: the 2nd and 5th band from the wrist on each forearm.
- **Compass placement and readability** (Q14):
  - **Mount:** on a strap over the back end of the right cuff, between the wrist joint and 7 cm behind it, raised on a short post and tilted toward the eye.
  - **Size:** a face about 30 mm across.
  - **Binding checks:**
    - the face (dial disc, ticks, needle, lamp) is wholly in view;
    - its normal is within 30° of the direction to the eye (cos ≥ 0.87);
    - it is at least 30 px across;
    - it is the first thing a ray from the eye meets;
    - the lamp is at least 3 px wide;
    - the open lid stays within the lower 35%.

  The housing's rim may be cut by the bottom edge.
- **Checks** (Q15, Q17):
  - **Unchanged:** framing, near clearance of 0.15 m or more, the gauge, closed volumes, the 39 joints, and attachment in the rest and test poses.
  - **New:**
    - triangles 12,000–20,000;
    - every seen screw head, toggle tip, knurl pitch, lamp, needle, tick and the dial index mark is at least 3 px;
    - every round part in a creased material has at least 10 sides;
    - every gadget part is rigid to its forearm;
    - the knurled dial, the three toggles and at least 2 of the 4 bezel screws are seen.
- **Weights of stitches and patches** (Q15, clarified in planning): a stitch, patch or pad takes the weights of the glove surface under it, which is rigid to that joint wherever the glove is rigid. Where the glove blends (cuff and wrist), the detail blends with it, so it stays attached in the wrist test poses.
- **Captures change on purpose.** The desert's `--capture` output changes, so `tools/perspective-check.sh` fails its two desert cases and must pass `example-first-person`.

## Layering
- Engine: none. The arms use `SkinnedModel`, `FrameParams::viewModel` and `actorMaterials` as they do today.
- Game (`desert/`):
  - `desert::Arms` loads the rebuilt glb into slots 0–13 and sets crease ink on Wrap, Glove, Brass, Worn and Grime.
  - `desert/tools/blender/build_arms.py` builds the asset.
  - `desert/docs/art-style.md` gains the view-model detail rule.

## Approach
This task runs after `view-model-edges`, so its captures show the finished outlines. Assets first, then code.

### 1. The asset (designer)
Rebuild `desert/tools/blender/build_arms.py` in place to the Assets section below, step by step in the live Blender session, then headless. Keep its structure:
- `Builder`, `Side` and the pose constants (`SHOULDER`, `WRIST`, `ELBOW_SWING`, `HAND_YAW`, `HAND_PITCH`, `HAND_ROLL`, the lengths) and `bone_table()` stay as they are, so the rig and held pose don't move;
- the gauge constants (`GAUGE_S`, `GAUGE_TURN`, `WINDOW`, `BEZEL`, `RECESS`, `BAR`, `BAR_GAP`) keep their values;
- `MATERIALS` gains the nine new entries in slot order, and `TRI_BUDGET` becomes `(12000, 20000)`.

The script's checks are binding, and the build fails if any fails. They are the existing checks, plus those in Assets. Reuse its `project`, `in_view`, `framing` and `attachment_problems`. Extend them rather than writing new copies (friction F-013: no third copy of the attachment check).

Then update `desert/docs/art-style.md` as follows (binding text):
- **In "Look",** after the bullet that ends "so a shape reads at both distances.", add:
  > - **View models are the exception.** A view model (the player's arms and what they carry) is seen from 0.15–0.6 m, where one pixel is under 1 mm, so it carries fine detail down to about 3 px on screen (about 2.5 mm at the hands). It is still flat-shaded, with one solid colour per material and no textures. Small round parts (screws, wires, dials) take as few sides as still read round at their size on screen, and a round part in a material with crease ink takes at least 10 sides (36° between them, under the 40° ink turn), so no ink runs along it. Stacked parts read through the edge pass's view-model outlines and through colour, so give neighbouring parts different materials.
- **In "Characters",** replace the sentence "The first character is the player's arms (`build_arms.py`, `arms.glb`, task `desert-arms`): a rigged model held in its rest pose and drawn as the view model, in front of the whole scene." with:
  > The first character is the player's arms (`build_arms.py`, `arms.glb`, tasks `desert-arms` and `desert-arms-detail`): a rigged model drawn as the view model, in front of the whole scene, with the left bracer's hygrometer and the right wrist's sun compass. About 12,000–20,000 triangles.
- **Replace the "Actor slots" bullet in full** with:
  > - **Actor slots.** The desert's ranges in `pxe::kActorMaterials`: the arms take 0–13 (`Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Brass`, `Arms_Glow`, `Arms_Steel`, `Arms_Copper`, `Arms_Rubber`, `Arms_Thread`, `Arms_Worn`, `Arms_Grime`, `Arms_Dial`, `Arms_Paint`, `Arms_Amber`, in that glTF order). The game sets crease ink per slot (on the wrap, glove, brass, worn and grime). A new character takes the next free range, from 14.
- **In the "Framing" bullet,** "anything the player must read (the arms' gauge)" becomes "anything the player must read (the arms' gauge and compass face)".

Everything else in the doc stays.

### 2. Game: `desert::Arms`
`desert/src/arms.cpp`:
- The anonymous namespace names all fourteen material offsets, `kWrap` = 0 through `kAmber` = 13, in the table's order. Mark the ones the file never reads `[[maybe_unused]]`.
- In `pose()`, crease is set on `kWrap`, `kGlove`, `kBrass`, `kWorn` and `kGrime` only.

`desert/src/arms.hpp`: the `kFirstMaterial` comment becomes:
```cpp
  // Actor-material slots 0-13: Arms_Wrap, Arms_Glove, Arms_Strap, Arms_Brass,
  // Arms_Glow, Arms_Steel, Arms_Copper, Arms_Rubber, Arms_Thread, Arms_Worn,
  // Arms_Grime, Arms_Dial, Arms_Paint, Arms_Amber, in the glb's material order.
```
No other code changes. `main.cpp`, `player.*` and `world.*` stay as they are.

## Verification
Run in the task's worktree:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. Only the stb_image_write `sprintf` warning.
2. A headless rebuild, `/Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_arms.py`, passes every check and prints the measured values in Assets. Two runs give byte-identical glbs.
3. `tools/ortho-check.sh <cycle base>`: all 6 identical.
4. `tools/perspective-check.sh <cycle base>`: `example-first-person` identical; `desert-default` and `desert-night` differ.
5. `git diff <cycle base> -- engine game examples desert/src/main.cpp desert/src/player.hpp desert/src/player.cpp desert/src/world.hpp desert/src/world.cpp`: no output.
6. `grep -n "crease" desert/src/arms.cpp`: the only slots given crease are `kWrap`, `kGlove`, `kBrass`, `kWorn` and `kGrime`.
7. Captures, into `docs/tasks/desert-arms-detail/`:
   - `build/release/desert/pxe_desert --capture docs/tasks/desert-arms-detail/detail-day.png` (16.5 h);
   - `... --hour 10 --capture .../detail-morning.png`;
   - `... --hour 18.5 --capture .../detail-golden.png`;
   - `... --hour 21 --capture .../detail-night.png`.

   Each shows:
   - the hands in the same place as `docs/tasks/desert-arms/arms-day.png`, and nothing above row 351;
   - the gauge's three teal bars where they were;
   - at the left bracer's wrist end, the bezel screws, the knurled dial and the three toggles;
   - at the bottom right, the compass face with its ticks and needle, and the amber lamp;
   - stitching and knuckle pads on the gloves.

   The teal bars and the amber lamp glow at night. Two runs are identical (`cmp`).
8. **Clipping, by temporary edits** (never committed), as in `desert-arms`:
   - `kSpawn` `{0.f, -0.71f}` → `detail-at-trunk.png`;
   - spawn pitch `1.2f` → `detail-looking-down.png`.

   The arms are whole and in front of the bark and the sand. Revert, `touch desert/src/main.cpp`, rebuild, and confirm a fresh `--capture` byte-matches `detail-day.png`.
9. Designer previews present (Assets).

## Acceptance criteria
- [x] `build_arms.py` builds `arms.glb`, and a headless rebuild passes every check: the existing ones and the new ones in Assets — Headless `Blender -b -P desert/tools/blender/build_arms.py` run twice by the orchestrator: no check raised (`main()` raises before export on any problem), both glbs `cmp`-identical and equal to the committed one.
- [x] The glb has one skinned mesh with the same 39 joints at the same rest transforms (to 1e-6 m), the 14 materials in slot order with the listed colours, `Arms_Glow` and `Arms_Amber` emissive and no others, no clips, and 12,000–20,000 triangles — `check_glb` read-back: 15,844 triangles, 39 joints, 14 materials with the listed sRGB, glow only on Glow and Amber; joints and inverse binds identical to 505b604 (scripts/compare_joints.py, assets.md).
- [x] The gauge's bars measure where `docs/tasks/desert-arms/assets.md` records them (NDC y -0.970..-0.857, x -0.851..-0.686, ±0.005), and the gauge check passes — Build log: NDC y -0.970..-0.857, x -0.851..-0.686, 17.9–19.0 px (assets.md).
- [x] The compass checks pass (Assets), and the dial, the three toggles and at least 2 bezel screws are seen — Build log: face cos 1.000, 216/216 vertices in view and unblocked, 46.7 px, lamp 5.3 px, lid top NDC -0.610; dial, 3 toggles and screws 1–2 seen.
- [x] Every seen small part listed in Assets is at least 3 px. Every round part in a creased material has at least 10 sides — Build log: screws 7.9/8.3 px, toggle tips 5.8, index 4.6, needle 5.9, ticks 4.0–5.3, knurl pitch 3.73–4.90; 45 creased round parts, fewest 12 sides.
- [x] `assets.md` documents the asset as `desert/docs/art-style.md`'s Delivery section asks, with the measured framing, near clearance, gauge, compass and small-part widths. The four previews are beside the glb — docs/tasks/desert-arms-detail/assets.md; arms_eye/arms/arms_left/arms_right.png in desert/assets/characters/.
- [x] `desert/docs/art-style.md` has the binding changes above, and nothing else in it changed — `git diff 505b604 -- desert/docs/art-style.md`: the four binding edits only, word for word.
- [x] `arms.cpp` names the 14 offsets and sets crease on Wrap, Glove, Brass, Worn and Grime only. The `arms.hpp` comment lists slots 0–13 — desert/src/arms.cpp anonymous namespace kWrap..kAmber, `Arms::pose` line 49; arms.hpp kFirstMaterial comment.
- [x] Verification 3–6 pass. The captures are in the task folder and show what Verification 7 and 8 describe — Orchestrator: ortho-check 6/6 identical; perspective-check example-first-person identical, desert-default/night differ; diff of listed paths empty; crease grep line 49 only. Six captures reviewed; day and night recaptures `cmp`-identical.
- [x] Project builds cleanly from a fresh build directory with CMake — Orchestrator's `check` preset fresh, clean-first build: only the stb_image_write sprintf warning.

## Assets
- **arms** (rebuilt): the player's two arms, seen in first person, as in `desert-arms`, with the detail in Decisions. Low-poly and flat-shaded, one solid colour per material, no textures, under the view-model detail rule (`desert/docs/art-style.md`, as amended here).
  - Format: glb, one skinned mesh with its armature, as the asset contract says. Objects `Arms` and `Arms_Rig` only.
  - Source: `desert/tools/blender/build_arms.py`, exported to `desert/assets/characters/arms.glb`, with previews beside it.
  - Units and axes: as `desert-arms`. 1 unit = 1 metre, real scale. Engine +Z is the view's forward (Blender -Y), +Y up, +X the player's left.
  - Origin: the eye, with `root` there.
  - Rig: the 39 joints of `desert-arms`, unchanged in name and rest transform.
  - Materials: the 14 in Decisions, in that glTF order. Set each base colour so the engine's sRGB conversion gives the listed values back, and print them in the build log.
  - Budget: 12,000–20,000 triangles. A guide:
    - hands, fingers and thumbs about 5,000;
    - glove detail (stitches, pads, patches, cuff straps) about 3,000;
    - wraps, grime, tails about 2,000;
    - left bracer and its devices about 4,500;
    - the compass and its strap about 2,500.
  - Animations: none.

  **Measured points and terms**, binding, for the checks below:
  - **In view:** inside the eye camera's frustum: Blender at the origin looking along -Y, +Z up, 60° vertical field of view, 16:9, near 0.1.
  - **Width on screen:** a part's width is the smallest extent of its in-view vertices, projected at 960×540, over 8 screen directions 22.5° apart.
  - **Seen:** a part is seen when at least one of its vertices is in view and the first surface a ray from the eye toward that vertex meets is within 0.5 mm of it.
  - **Knurl pitch:** the on-screen distance between the centres of neighbouring seen ridges.

  **Gloves:**
  - **Fingers:** blocky segments as today. The palm and finger shapes may be refined, but each joint stays where it is.
  - **Stitching:** `Arms_Thread` dashes about 5 mm long, at least 2.5 mm wide, about 2.5 mm apart, standing about 0.6 mm proud. They run along both sides of each finger and the thumb, and round the cuff's flare.
  - **Knuckle pads:** an `Arms_Worn` pad over each `index`–`pinky` `.1` joint, standing 2–3 mm proud, with bevelled edges.
  - **Patches:** an `Arms_Worn` palm patch, a worn patch on each fingertip (the `.3` segments' ends) and one on the back of the hand, each 0.6–1 mm proud.
  - **Cuff strap:** an `Arms_Strap` strap round each cuff, closed by an `Arms_Steel` snap rivet on its outer side.

  **Wraps:**
  - **Bands:** the six bands per forearm as in `desert-arms`, with their overlap lips (the existing band-edge check stays). The 2nd and 5th band from the wrist on each forearm are `Arms_Grime`.
  - **Frayed tail:** from the band nearest the cuff, on the forearm's outer side, an `Arms_Grime` tail 6–9 cm long, rigid to the forearm. It splits into 3 strands with uneven ends. It must not cover the gauge or the compass face (their first-hit checks).

  **Left bracer:**
  - **Plate:** the plate keeps its extent (4–16 cm behind the wrist, four 35° facets), and its edges are bevelled.
  - **Bezel:** the gauge, its window and its three bars stay as in `desert-arms`. The bezel takes 4 `Arms_Steel` slotted screws, one at each corner. Each head is at least 4 mm across and its slot at least 1 mm wide.
  - **Knurled dial:** an `Arms_Steel` dial on the bezel's inner side (toward the body, engine -X from the left bracer), about 12 mm across. It has 16 or more knurl ridges and an `Arms_Paint` index mark.
  - **Toggles:** a row of 3 toggle switches at the wrist end of the plate, beside the bezel. Each has an `Arms_Steel` base, an `Arms_Rubber` lever body and an `Arms_Paint` tip.
  - **Coil:** an `Arms_Copper` coil of at least 6 turns round a short `Arms_Brass` tube on the plate behind the bezel.
  - **Cables:** an `Arms_Copper` cable and an `Arms_Rubber` cable from the housing, back along the plate. Each ends tucked under the wrap band just past the plate's elbow end. The end is inside the band (the containment check).
  - **Canister:** an `Arms_Steel` canister, about 5 cm long and 1.8 cm across, held to the plate's outer side by a steel clip, with a valve wheel of 4–5 spokes on its wrist end.
  - **Straps:** the two `Arms_Strap` straps keep their places (9.0 and 14.8 cm behind the wrist). They gain `Arms_Thread` stitched edges. Each buckle has an `Arms_Steel` frame and tongue, and an `Arms_Strap` keeper loop.

  **Right wrist, the sun compass:**
  - **Mount:** an `Arms_Strap` wrist strap over the back end of the cuff, carrying a short `Arms_Steel` post. The compass sits on the post, between the wrist joint and 7 cm behind it, tilted toward the eye.
  - **Housing and face:** a round `Arms_Steel` housing (at least 10 sides) and an `Arms_Dial` face about 30 mm across.
  - **Ticks:** 12 raised `Arms_Paint` tick marks round the face.
  - **Needle:** a fixed `Arms_Paint` needle.
  - **Lamp:** an `Arms_Amber` lamp on the face's rim.
  - **Buttons:** two `Arms_Rubber` buttons on the housing's side.
  - **Lid:** an `Arms_Steel` sun-shade lid, hinged on the face's far edge (toward the fingers) and propped open, so it stands beyond the face and never covers it from the eye.

  **Rigging:** every gadget part is rigid to its forearm (`forearm.L` for the bracer and everything on it, `forearm.R` for the compass and its mount). Stitches, pads and patches take the weights of the glove surface under them.

  **Checks, binding, in the build:**
  - **Existing:**
    - names and objects; the joints and their rest transforms; flat shading; every vertex weighted;
    - closed, outward-facing volumes;
    - the band edges' crease turn;
    - the framing table of `desert-arms`: lower 35%, knuckles, hand centres, forearms leaving through the bottom edge;
    - near clearance of 0.15 m or more;
    - the gauge;
    - attachment in the rest pose and the test poses.
  - **Materials:** the 14 names in slot order, the colours round-tripped, and only `Arms_Glow` and `Arms_Amber` emissive.
  - **Triangles:** 12,000–20,000.
  - **Compass:**
    - the face wholly in view;
    - the face normal within 30° of the direction from its centre to the eye (cos ≥ 0.87);
    - the dial disc at least 30 px wide on screen;
    - the face the first thing a ray from the eye toward each face vertex meets;
    - the lamp at least 3 px wide.
  - **Seen:** the knurled dial, the three toggles and at least 2 of the 4 bezel screws.
  - **Small parts:** every seen screw head, toggle tip, the lamp, the needle, every seen tick and the dial index mark at least 3 px wide on screen. The knurl pitch at least 3 px.
  - **Round parts:** every part modelled round (a cylinder, ring, disc or loft of rings) in `Arms_Wrap`, `Arms_Glove`, `Arms_Brass`, `Arms_Worn` or `Arms_Grime` has at least 10 sides.
  - **Stitching:** every stitch dash at least 2.5 mm wide and 4 mm long.
  - **Read-back:** after export, the glb has one mesh and one skin, the 39 joints, the 14 materials and their colours, no animations, NORMAL/JOINTS_0/WEIGHTS_0, and the same triangle count.

  **Previews**, beside the glb, Workbench, with back faces culled:
  - `arms_eye.png`, through the eye camera at 960×540;
  - `arms.png`, the three-quarter view as before;
  - `arms_left.png`, through the eye with a 20° vertical field of view, aimed at the gauge's centre, at 960×540;
  - `arms_right.png`, the same aimed at the compass face's centre.

## Out of scope
- Animation, clips, moving parts or blinking lights (`desert-arms-walk` adds the walk next)
- A held tool, a body, shadows of the arms, a HUD
- Changing the held pose, the framing rules, the gauge's place, or the existing five materials
- Engine changes
- A separate field of view for the view model

## Spec revisions
None.

## Cycle log
- Cycle 1, 2026-10-29, base 505b604.
- Cycle 1 review: designer d7178a7 (asset, art style), developer cdfaabe (code, captures). No issues. Accepted the designer's two calls as within the spec: the elbow-side bezel screws sit on the bezel's wall at its corners, out of view (the spec asks for 2 of 4 seen); the per-part triangle guide was not met but the binding 12,000–20,000 total was (15,844).

## Playtest checklist
- `tools/build.sh release desert`. Look at the hands: stitched dashes along the fingers, thumbs and cuffs, knuckle pads, worn patches, and the grime bands on the wraps.
- Left wrist: the three teal bars where they were, the steel bezel screws, the knurled dial with its red index mark, the three red-tipped toggles, the coil, the canister and its valve wheel.
- Right wrist: the sun compass tilted toward you, ivory face, 12 red ticks, the needle, the amber lamp, the two side buttons and the propped lid.
- Turn and look about: the arms stay fixed in the view with no gaps or floating detail. Walk into the palm trunk and look down at the sand: the arms stay whole and in front.
- `--hour 21` and `--hour 18.5`: the teal bars and the amber lamp keep glowing.
- The arms are now about 15,800 triangles (was about 2,500): check the frame rate still feels smooth.
