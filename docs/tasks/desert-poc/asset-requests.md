# desert-poc asset requests

## REQ-001: date_palm: frond blades break up into speckle
Status: done
Blocking: no

### Problem
In the spawn capture (25 m from the palm, eye 1.7 m, 60° FOV; `docs/tasks/desert-poc/capture_hour16.5.png`, and `capture_hour10.png`), each blade of the 15 per side draws at about one pixel wide or less. Most blades are a 1 px diagonal line broken into dots, with sky pixels between neighbouring blades, so each frond reads as dotted hatching rather than a leaf, and the frond tips (the drooping outer fronds) dissolve into scattered single pixels. The eye is never snapped under the perspective camera, so as the view turns and walks these sub-pixel blades will flicker pixel by pixel (crawl). Only the ribs and the inner blades near the crown hold as solid shapes. At night (`capture_hour19.png`) the crown is a dark mass and the speckle shows less.

This is the case the task's Decisions name ("If the 15 thin blades per side speckle or crawl, the designer cuts them to about 6 broader blades per side"). The user judges the capture; this request is ready if they confirm.

### Required change
Rebuild `Palm_Fronds` with about 6 blades per side on each of the 18 fronds instead of 15, each blade broader in proportion (15/6, about 2.5 times its current width), so the six cover the frond as the fifteen do and each blade spans at least 2 px across at 25 m. Keep the frond count, lengths, droop, the rib, the two-sided blades (a reversed twin per blade), the `Leaf` material, the origin and the trunk, stubs and dates unchanged, and update assets.md's triangle counts and crown extent.

### Designer notes
Rebuilt `Palm_Fronds` in `desert/tools/blender/build_date_palm.py` (revision 2) with `BLADES = 6` per side instead of 15. The blades still span 0.17..0.98 of each frond with each foot covering 0.75 of its slot, so each is about 2.5 times as wide as before: every blade is 0.149..0.216 m across its long axis (median 0.188 m, about 3.1..4.5 px at 25 m; revision 1's median was 0.083 m), and about 0.9 m long. Frond count (18), lengths, tiers, droop, roll, ribs and the reversed twin per blade are unchanged. The blades still draw revision 1's 30 random numbers per frond, so frond placement and the dates don't shift: re-importing both glbs shows Palm_Trunk, Palm_Dates and every rib vertex identical. Triangles 2,728 -> 2,080 (Palm_Fronds 1,692 -> 1,044). The build's triangle check is now 1,900..2,300. Crown extent moved slightly (x -4.73..2.45, y 9.46..14.61, z -3.75..3.55, radius 3.87), recorded in assets.md. Trunk radius 0.40, trunk top and the dates are unchanged. Re-exported `desert/assets/scenes/date_palm.glb` and refreshed the three previews. Not yet checked in the engine capture: the developer's next capture at 25 m should confirm the fronds hold as solid facets.
