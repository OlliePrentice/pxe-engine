# desert-poc assets

## date_palm
Source: desert/tools/blender/build_date_palm.py (the source of truth; no .blend is kept). Rebuild with
`/Applications/Blender.app/Contents/MacOS/Blender -b -P desert/tools/blender/build_date_palm.py`
(builds, runs its checks, exports, renders the previews; `PXE_SKIP_PREVIEW=1` skips the previews, `PXE_DESERT_OUT=<dir>` writes elsewhere).
Export: desert/assets/scenes/date_palm.glb
Preview: desert/assets/scenes/date_palm.png (three-quarter view), desert/assets/scenes/date_palm_eye.png (eye 1.7 m above the ground, 14 m south, engine -Z, looking at the palm), desert/assets/scenes/date_palm_below.png (standing under the crown looking up). All rendered with back faces culled, as the engine draws.
Format: glb (glTF 2.0 binary), exported +Y up, flat-shaded (per-face normals)
Units and scale: 1 unit = 1 metre, real scale. Place it at scale 1.
Axes: +Y up; Blender -Y is engine +Z. The trunk bows toward engine -X.
Origin: trunk base centre at ground level. Every node has an identity transform, so the origin is shared. The trunk continues 0.5 m below the origin (to y = -0.5) so it stays planted on a sloping dune; place the origin at the ground height at the palm's position.
Triangles: 2,728 (Palm_Trunk 808, Palm_Fronds 1,692, Palm_Dates 228)
Materials: `Bark` (Palm_Trunk, Palm_Dates), `Leaf` (Palm_Fronds). No textures.
Nodes and objects: `Palm_Trunk` (trunk, stepped stub rings and crown knob, one closed volume), `Palm_Fronds` (18 fronds: closed three-sided ribs plus flat blades, each blade a triangle and a reversed copy, so it shows from above and below), `Palm_Dates` (3 stalks with blocky clusters, closed volumes).
Animations: none
Measurements (engine coordinates, metres, origin = trunk base centre at ground):
- Trunk collision radius at the base: 0.40 (circumradius of the 7-sided base; inradius 0.36). The base flare is the widest part of the trunk below 11 m; the stub rings above stay within 0.39 of the trunk's centre line. Vertical cylinder at x = 0, z = 0.
- Overall height: 14.65 (highest frond tip). Trunk top (foot of the crown knob) at (-0.90, 11.00, 0.00); knob top 12.11.
- Whole-asset bounds: x -4.81 .. 2.49, y -0.50 .. 14.65, z -3.85 .. 3.63.
- Crown extent: fronds span x -4.81 .. 2.49, y 9.30 .. 14.65, z -3.85 .. 3.63; radius 3.94 around the vertical through the trunk top (-0.90, *, 0.00). The lowest frond tip is 9.30 above the origin. Dates hang between y 9.54 and 11.33.
Build checks (the script fails the build if any fails): object names exactly Palm_Trunk, Palm_Fronds, Palm_Dates with no duplicates; materials exactly Bark and Leaf, one per object; triangles within 2,600..3,300; identity transforms; the trunk's ground ring centred on the origin at height 0; trunk and dates closed; 18 fronds; every blade has a reversed twin; every rib closed.
Revision: 1, 2026-10-28, first build.
