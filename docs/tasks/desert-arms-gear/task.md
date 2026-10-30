# Arms: gear and palette after the concept art
Type: feature
Status: queued
Cycle: 0

## Goal
Restyle the desert player's arms' gear and palette after `desert/docs/concept/wanderers.png`:
- heavy, worn grey-brown work gloves with gunmetal plates over the backs of the hands and raised knuckle guards;
- a dusty, frayed khaki cloth wrap in layered strips over a dark undersuit;
- a scratched gunmetal vambrace strapped over each forearm;
- worn gunmetal and rusted steel in place of brass, with copper only as a small accent;
- cyan as the only bright colour, on the gauge readout, the compass marks and lamp, and glowing tubes running from the devices along the forearms out of view.

The pose stays as `desert-arms-pose` left it. Update `desert/docs/art-style.md` to the new standing look.

## Decisions
All from the planning interview with the user, 2026-10-30.
- **Materials.** There are still 15, in the same slots, renamed and recoloured as in the table below. ✦ means emissive; colours are sRGB albedo under the smooth lighting (`perspective-smooth-lighting`):

  | Slot | Name | sRGB | Used on | Crease ink |
  |---|---|---|---|---|
  | 0 | Arms_Wrap | (176,160,128) | khaki cloth strips | yes |
  | 1 | Arms_Glove | (104,90,76) | grey-brown leather gloves | yes |
  | 2 | Arms_Strap | (86,64,48) | dark leather straps and keepers | no |
  | 3 | Arms_Gunmetal | (70,74,78) | vambraces, hand plates, gauge bezel, compass housing and lid, canister | yes |
  | 4 | Arms_Glow ✦ | (60,230,230) | readout dots, compass ticks, needle and lamp, tubes | no |
  | 5 | Arms_Steel | (120,122,124) | scratches, worn plate edges, buckles, screws, rivets, toggle bases | no |
  | 6 | Arms_Copper | (150,96,64) | the coil only | no |
  | 7 | Arms_Rubber | (40,38,36) | the dark cable, toggle levers, compass buttons | no |
  | 8 | Arms_Thread | (96,80,64) | stitching | no |
  | 9 | Arms_Worn | (130,114,96) | knuckle guards, worn patches | yes |
  | 10 | Arms_Grime | (150,136,110) | frayed tails, grimy strips | yes |
  | 11 | Arms_Dial | (34,40,42) | the compass's dark face | no |
  | 12 | Arms_Rust | (122,70,46) | rust patches on the plates, toggle tips, the dial's index mark | no |
  | 13 | Arms_Undersuit | (46,44,44) | the dark sleeve under the wrap | no |
  | 14 | Arms_Screen ✦ | (14,32,34) | the gauge's dark teal backlit panel | no |

  The amber lamp is gone: it becomes a cyan `Arms_Glow` lamp. Only `Arms_Glow` and `Arms_Screen` are emissive.
- **Wrap and undersuit.** The arm tube becomes the dark undersuit sleeve (`Arms_Undersuit`). Over the forearm lie **two or three layered strips** of `Arms_Wrap` cloth (one per forearm may be `Arms_Grime`), wound at slight angles. Each strip's edges are frayed: notched outlines with at least 8 notches along each edge, 3–6 mm deep, uneven. Each forearm has 2–3 short hanging tails. The undersuit shows in the gaps between strips and toward the elbow. Today's six stepped bands and their band-edge crease check go.
- **Gloves.** On `desert-arms-pose`'s chunkier shape, in `Arms_Glove`, each hand gets:
  - a **back-of-hand plate** of three overlapping `Arms_Gunmetal` segments, each 1.5–2.5 mm thick, with worn `Arms_Steel` edge bevels;
  - **raised knuckle guards** (`Arms_Worn`) over the four knuckles;
  - stitching (`Arms_Thread`) and worn patches as today.
- **Vambraces.** One on each forearm:
  - a curved `Arms_Gunmetal` plate over the top of the forearm, 0.12–0.15 m long along it, spanning ±70° round it, 3–4 mm thick;
  - at least 4 scratches, each a thin `Arms_Steel` sliver 0.5 mm proud, 0.6–1.2 mm wide and 15–40 mm long;
  - a worn `Arms_Steel` bevel along its edges and one or two `Arms_Rust` patches;
  - two buckled `Arms_Strap` straps, each buckle an `Arms_Steel` frame and tongue with an `Arms_Strap` keeper.

  The **left** vambrace replaces the brass bracer and carries the gauge at its wrist end, where `desert-arms-pose` placed it, plus the toggles, canister and coil. The **right** is plainer. The compass stays on its strap at the right wrist.
- **Gauge.** A gunmetal bezel with steel screws, keeping today's readout: `47%▮` in cyan `Arms_Glow` dots on the dark teal `Arms_Screen` panel. Every readout check stays.
- **Compass.** A gunmetal housing and lid, and a dark `Arms_Dial` face. The 12 ticks, the needle and the status lamp are `Arms_Glow` cyan. Every compass check stays: wholly seen, at least 30 px across, facing the eye, and small parts at least 3 px.
- **Tubes.** Emissive `Arms_Glow` tubes, 2.5 mm in radius with 8 sides. **Two on the left**, each starting at the gauge's housing, and **one on the right**, starting at the compass housing. Each runs along the forearm, bends round or under the vambrace strap nearest the elbow, and leaves the view through the bottom edge. Today's copper cable becomes one of the left tubes. The rubber cable stays, dark.
- **Copper** is the coil alone.
- **Budget:** 18,000–30,000 triangles. Guide: gloves with plates about 9,000, wraps about 3,000, vambraces about 2,500, gadgets about 6,000, tubes about 1,000, about 21,500 in all.
- **Unchanged:** the pose, the joints, the clips' motion (regenerated from the same pose), the framing rules and their values, the attachment test poses, and the readout's glyphs and dot size.

## Layering
- Engine: none. Two emissive materials and 15 slots are already supported.
- Game (`desert`):
  - `arms.cpp`'s material constants and crease set follow the new names: wrap, glove, gunmetal, worn and grime are creased.
  - `arms.hpp`'s slot comment follows them too.
- Assets: `build_arms.py` rebuilds `arms.glb` and its previews.

## Approach
Designer first, then the developer.

### 1. `desert/tools/blender/build_arms.py` (designer)
- **`MATERIALS`:** exactly the Decisions table, in order, with the emissive flags. `CREASED = ("Arms_Wrap", "Arms_Glove", "Arms_Gunmetal", "Arms_Worn", "Arms_Grime")`. `TRI_BUDGET = (18000, 30000)`.
- **`Builder.arms()` and `wrap_detail()`:** the arm tube in `Arms_Undersuit`. The wrap is 2–3 frayed strips per forearm plus 2–3 tails, replacing `BAND_*` and the stepped bands. Remove `band_crease_problems` and its call. The strips are closed volumes (thin slabs following the forearm, their outline notched), as every piece must be.
- **`gloves()` and `glove_detail()`:** add the back-of-hand plate segments and the knuckle guards, rigid to `hand.<S>`. Stitches and patches stay.
- **`bracer()` becomes `vambrace(S)` for both sides**, with the plate, scratches, edge bevel, rust patches and straps with buckles. `gauge()` and `bracer_detail()` keep their pieces on the left vambrace, in the new materials.
- **`compass()`:** the materials as in Decisions. The lamp's material is `Arms_Glow`.
- **Tubes:** a `tubes()` step using `Builder.tube`, rigid to each forearm (they leave the view, so their far ends need no attachment beyond the forearm). The copper cable's path becomes the first left tube's.
- **New checks** in `check()` and `framing()` (binding):
  - Materials: the colours, names, order and emissive flags of the table. Every piece in `Arms_Copper` is named `*.coil*`.
  - Undersuit seen: at least 8 `Arms_Undersuit` vertices per forearm are `seen` from the eye at rest.
  - Strips: each forearm has 2–3 strip pieces. Each strip edge has at least 8 notches, a notch being a dip of 3–6 mm in the edge's offset along the forearm. Each forearm has 2–3 tails.
  - Vambraces: one `Arms_Gunmetal` plate per forearm, 0.12–0.15 m long along the forearm axis and spanning 140° ± 10° round it. At least 4 `Arms_Steel` scratch pieces per plate, each 15–40 mm long and at most 1.2 mm wide. Two straps per vambrace, each with a buckle.
  - Hand plates: three `Arms_Gunmetal` segments per hand, each touching the next (overlap). Four `Arms_Worn` knuckle guards per hand.
  - Tubes: 3 `Arms_Glow` tube pieces (2 left, 1 right), each with 8 sides and radius 2.5 mm ± 0.2. Each starts within 1 mm of its device's housing. Its last ring is out of view below the bottom edge (NDC y < −1 for every vertex of the last ring, or behind the eye). Every in-view vertex is at least `CLEARANCE` ahead.
  - The existing checks all stay, with their values: framing, readout, compass, small parts, attachment in every pose, covered, closed, clips, and glb read-back.
- **The module docstring's piece list** describes the new gear and materials.
- Build step by step in the user's live Blender through the MCP, then rebuild headless and keep the previews.

### 2. `desert/docs/art-style.md` (designer)
Make exactly these replacements. Everything else stays word for word.
- In "Characters", replace
  > with the left bracer's hygrometer and the right wrist's sun compass. About 12,000–20,000 triangles.

  with
  > with the left vambrace's gauge and the right wrist's sun compass. About 18,000–30,000 triangles.
- Replace the whole "Actor slots" bullet
  > - **Actor slots.** The desert's ranges in `pxe::kActorMaterials`: the arms take 0–14 (`Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Brass`, `Arms_Glow`, `Arms_Steel`, `Arms_Copper`, `Arms_Rubber`, `Arms_Thread`, `Arms_Worn`, `Arms_Grime`, `Arms_Dial`, `Arms_Paint`, `Arms_Amber`, `Arms_Screen`, in that glTF order). The game sets crease ink per slot (on the wrap, glove, brass, worn and grime). A new character takes the next free range, from 15.

  with
  > - **Actor slots.** The desert's ranges in `pxe::kActorMaterials`: the arms take 0–14 (`Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Gunmetal`, `Arms_Glow`, `Arms_Steel`, `Arms_Copper`, `Arms_Rubber`, `Arms_Thread`, `Arms_Worn`, `Arms_Grime`, `Arms_Dial`, `Arms_Rust`, `Arms_Undersuit`, `Arms_Screen`, in that glTF order). The game sets crease ink per slot (on the wrap, glove, gunmetal, worn and grime). A new character takes the next free range, from 15.
- In "Palette", after the "Palm" bullet, add:
  > - **Gear (the wanderer's kit, after `desert/docs/concept/wanderers.png`):** desaturated dusty browns, greys and khaki: khaki cloth wraps over a near-black undersuit, grey-brown leather, dark leather straps, worn gunmetal plates scratched to bare steel with a little rust, and copper only as a small accent. Cyan is the only bright colour, kept to glowing parts (readouts, tubes, lamps), which are emissive.

### 3. `desert/src/arms.hpp`, `desert/src/arms.cpp` (developer)
- `arms.hpp`'s slot comment lists the new names in order.
- `arms.cpp`'s material constants are renamed to match (`kGunmetal = 3`, `kRust = 12`, `kUndersuit = 13`, the rest as now, with `[[maybe_unused]]` where unused). The crease line is `i == kWrap || i == kGlove || i == kGunmetal || i == kWorn || i == kGrime`.

### For the run's docs step
After the merge, update `README.md`'s arms description and `docs/architecture.md`'s arms row (the crease list) to the new gear.

## Verification
Run in the task's worktree, against the cycle base:
1. Fresh build: `cmake --preset check --fresh && cmake --build --preset check --clean-first`. The only warning is stb_image_write's `sprintf`.
2. The headless build exits 0 with every check passing. Two runs into scratch `PXE_DESERT_OUT` directories give `cmp`-identical glbs.
3. `python3 tools/glb.py info desert/assets/characters/arms.glb`: the 15 materials in the table's order, 39 joints, both clips at 1.0 s with 31 LINEAR keys, and the triangle total within 18,000–30,000.
4. `tools/ortho-check.sh <cycle base>`: 6/6 identical. `tools/perspective-check.sh <cycle base>`: `example-first-person` identical, desert cases differ.
5. `git diff <cycle base> -- engine game examples desert/src/main.cpp desert/src/player.hpp desert/src/player.cpp desert/src/world.hpp desert/src/world.cpp desert/src/settings.hpp desert/src/settings.cpp`: no output.
6. `git grep -n -e Arms_Brass -e Arms_Paint -e Arms_Amber -e kBrass -e kPaint -e kAmber -e band_crease_problems -- desert`: no output.
7. Captures into `docs/tasks/desert-arms-gear/`, each rendered twice and `cmp`-identical:
   - `pxe_desert --capture gear-day.png`
   - `--hour 18 --capture gear-golden.png`
   - `--hour 21 --capture gear-night.png`
   - `--walk 3 --sprint --capture gear-sprint.png`

   Describe each against the concept art in the handoff:
   - grey-brown gloves with gunmetal back plates and knuckle guards;
   - frayed khaki strips with dark undersuit between;
   - scratched gunmetal vambraces with buckled straps;
   - cyan readout, compass marks and lamp, and tubes leaving the bottom of the view;
   - no bright colour but cyan;
   - at night the cyan parts still glow.
8. `git diff <cycle base> -- desert/docs/art-style.md` shows exactly the three changes in §2.

## Acceptance criteria
- [ ] `MATERIALS`, `CREASED` and `TRI_BUDGET` are as binding. The gear is built as Decisions say, and every new check in §1 runs and passes alongside every existing one.
- [ ] `arms.cpp` and `arms.hpp` use the new names, with crease ink on wrap, glove, gunmetal, worn and grime.
- [ ] `desert/docs/art-style.md` has exactly the three changes.
- [ ] Verification 2–8 pass.
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms** (revised): `desert/assets/characters/arms.glb`, rebuilt by `desert/tools/blender/build_arms.py`, with all six previews.
  - Format: glb. Units and axes: 1 unit = 1 m; engine +Z forward, +Y up, +X the player's left (Blender −Y forward, +Z up). Origin: the eye, `root` at the origin. All as today.
  - Budget: 18,000–30,000 triangles (guide in Decisions). No textures.
  - Materials: the 15 in Decisions.
  - Animations: `Arms_Walk`, `Arms_Sprint`, regenerated, values as shipped.

  The binding checks are listed in Approach §1. Notch: a local minimum in the strip edge's distance along the forearm axis, at least 3 mm and at most 6 mm below the neighbouring maxima either side. Strip edge: the boundary of a strip piece's outer face, measured along the forearm axis from the wrist. Bottom edge: NDC y = −1 through the eye camera at 60° vertical field of view, 16:9.

## Out of scope
- The pose and placement (`desert-arms-pose`) and the motion (`desert-arms-motion`).
- Textures; a backpack, rifle or anything else from the concept beyond the arms; animating the readout, the needle or the tubes' glow.
- Engine changes.

## Spec revisions
- 2026-10-30, from the unblock of `desert-arms-pose` (user): each vambrace is 0.12–0.15 m long along the forearm (was 0.11–0.13 m), in Decisions and the check. `desert-arms-pose` extended the left plate's wrist end to s −0.020 to keep the gauge's toggles on it, so the plate is 0.140 m (`BRACER_S` (−0.160, −0.020)).

## Cycle log
