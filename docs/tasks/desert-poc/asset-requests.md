# desert-poc asset requests

## REQ-001: date_palm: frond blades break up into speckle
Status: open
Blocking: no

### Problem
In the spawn capture (25 m from the palm, eye 1.7 m, 60° FOV; `docs/tasks/desert-poc/capture_hour16.5.png`, and `capture_hour10.png`), each blade of the 15 per side draws at about one pixel wide or less. Most blades are a 1 px diagonal line broken into dots, with sky pixels between neighbouring blades, so each frond reads as dotted hatching rather than a leaf, and the frond tips (the drooping outer fronds) dissolve into scattered single pixels. The eye is never snapped under the perspective camera, so as the view turns and walks these sub-pixel blades will flicker pixel by pixel (crawl). Only the ribs and the inner blades near the crown hold as solid shapes. At night (`capture_hour19.png`) the crown is a dark mass and the speckle shows less.

This is the case the task's Decisions name ("If the 15 thin blades per side speckle or crawl, the designer cuts them to about 6 broader blades per side"). The user judges the capture; this request is ready if they confirm.

### Required change
Rebuild `Palm_Fronds` with about 6 blades per side on each of the 18 fronds instead of 15, each blade broader in proportion (15/6, about 2.5 times its current width), so the six cover the frond as the fifteen do and each blade spans at least 2 px across at 25 m. Keep the frond count, lengths, droop, the rib, the two-sided blades (a reversed twin per blade), the `Leaf` material, the origin and the trunk, stubs and dates unchanged, and update assets.md's triangle counts and crown extent.

### Designer notes

