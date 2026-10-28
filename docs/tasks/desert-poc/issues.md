## ISSUE-001: Dunes shade as a sawtooth triangle checker
Status: open
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`desert/src/world.cpp` `appendGround` gives each triangle its own face normal. In `capture_hour16.5.png` the 1 m facets show as sawtooth band edges and a checker of triangles on the shaded slopes and in the foreground, which reads as noise. The user decided on smooth normals: each grid corner's normal from the dune surface (e.g. the gradient of `duneHeight`, or the average of the faces round it), so the toon ramp bands each slope in clean contours. Silhouettes and `groundHeight` are unchanged. See task.md Decisions (Terrain).

### Fix notes

## ISSUE-002: The ground's edge shows as a hard line at the horizon
Status: open
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
The ground ends at ±150 m (`kGroundHalf`), but the haze starts 180 m from the eye (0.6 × the 300 m far plane), and the ground's edge can be 70 m from the play area's edge. The sand then ends in a crisp line against the sky (the right of `capture_hour16.5.png`). The user decided: extend the ground to ±380 m. Keep 1 m cells within ±120 m and use 4 m cells outside, stitched to the 1 m grid without cracks or T-junction gaps (for example, the inner grid's border vertices shared by fans to the coarse cells, or the coarse ring's inner edge subdivided to match). `groundHeight` must still match the drawn triangles everywhere the player can walk (within ±80 m, all fine grid). Check the triangle count stays near cycle 1's and say what it is. Confirm with a capture from the play-area corner (-80, -80) looking out that the ground's edge is fully hazed.

### Fix notes
