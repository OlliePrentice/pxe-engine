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
Triangles: 2,080 (Palm_Trunk 808, Palm_Fronds 1,044, Palm_Dates 228)
Materials: `Bark` (Palm_Trunk, Palm_Dates), `Leaf` (Palm_Fronds). No textures.
Nodes and objects: `Palm_Trunk` (trunk, stepped stub rings and crown knob, one closed volume), `Palm_Fronds` (18 fronds: closed three-sided ribs plus 6 broad flat blades a side, each blade a triangle and a reversed copy, so it shows from above and below; every blade is 0.15..0.22 m wide across its long axis, median 0.19 m, about 0.9 m long), `Palm_Dates` (3 stalks with blocky clusters, closed volumes).
Animations: none
Measurements (engine coordinates, metres, origin = trunk base centre at ground):
- Trunk collision radius at the base: 0.40 (circumradius of the 7-sided base; inradius 0.36). The base flare is the widest part of the trunk below 11 m; the stub rings above stay within 0.39 of the trunk's centre line. Vertical cylinder at x = 0, z = 0.
- Overall height: 14.61 (highest frond tip). Trunk top (foot of the crown knob) at (-0.90, 11.00, 0.00); knob top 12.11.
- Whole-asset bounds: x -4.73 .. 2.45, y -0.50 .. 14.61, z -3.75 .. 3.55.
- Crown extent: fronds span x -4.73 .. 2.45, y 9.46 .. 14.61, z -3.75 .. 3.55; radius 3.87 around the vertical through the trunk top (-0.90, *, 0.00). The lowest frond point is 9.46 above the origin. Dates hang between y 9.54 and 11.33.
Build checks (the script fails the build if any fails): object names exactly Palm_Trunk, Palm_Fronds, Palm_Dates with no duplicates; materials exactly Bark and Leaf, one per object; triangles within 1,900..2,300; identity transforms; the trunk's ground ring centred on the origin at height 0; trunk and dates closed; 18 fronds; every blade has a reversed twin; every rib closed.
Revision: 1, 2026-10-28, first build.
Revision: 2, 2026-10-28, REQ-001: fronds cut from 15 thin blades a side to 6 broad ones (each about 2.5 times as wide, at least 3 px across at 25 m), so they stop speckling. Palm_Fronds 1,692 -> 1,044 triangles; the build's triangle check is now 1,900..2,300. Frond count, lengths, tiers, droop, ribs, two-sided blades, trunk, stubs, dates, materials, origin and node names are unchanged: trunk, dates and every rib vertex match revision 1 exactly (the blades still draw revision 1's random numbers, so the fronds' placement does not shift). Trunk collision radius 0.40, trunk top (-0.90, 11.00, 0.00), knob top 12.11 and the dates' 9.54..11.33 are unchanged; only the crown extent moved slightly (listed above).
