## ISSUE-001: Dunes shade as a sawtooth triangle checker
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`desert/src/world.cpp` `appendGround` gives each triangle its own face normal. In `capture_hour16.5.png` the 1 m facets show as sawtooth band edges and a checker of triangles on the shaded slopes and in the foreground, which reads as noise. The user decided on smooth normals: each grid corner's normal from the dune surface (e.g. the gradient of `duneHeight`, or the average of the faces round it), so the toon ramp bands each slope in clean contours. Silhouettes and `groundHeight` are unchanged. See task.md Decisions (Terrain).

### Fix notes
Root cause: `appendGround` emitted three unshared vertices per triangle, each carrying that triangle's face normal, so the toon ramp chose a band per 1 m triangle and the band edges followed the triangles' sawtooth.

Change (`desert/src/world.cpp`): the ground is now indexed, one vertex per grid corner, and each vertex's normal is `duneNormal(x, z)`, a central difference of `duneHeight` 0.5 m either side (`kNormalStep`), as `examples/minimal` builds its ground's normals. The normal depends only on the world position, so the corners the fine and coarse grids share (ISSUE-002) have one normal and the two grids shade alike where they meet. Positions, the (i, j)-(i+1, j+1) split and `groundHeight` are unchanged. In the new `capture_hour16.5.png` the slopes cut into a few clean bands that follow the dunes, and the triangle checker and sawtooth are gone.

## ISSUE-002: The ground's edge shows as a hard line at the horizon
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
The ground ends at ±150 m (`kGroundHalf`), but the haze starts 180 m from the eye (0.6 × the 300 m far plane), and the ground's edge can be 70 m from the play area's edge. The sand then ends in a crisp line against the sky (the right of `capture_hour16.5.png`). The user decided: extend the ground to ±380 m. Keep 1 m cells within ±120 m and use 4 m cells outside, stitched to the 1 m grid without cracks or T-junction gaps (for example, the inner grid's border vertices shared by fans to the coarse cells, or the coarse ring's inner edge subdivided to match). `groundHeight` must still match the drawn triangles everywhere the player can walk (within ±80 m, all fine grid). Check the triangle count stays near cycle 1's and say what it is. Confirm with a capture from the play-area corner (-80, -80) looking out that the ground's edge is fully hazed.

### Fix notes
Root cause: the ground stopped at ±150 m while the haze only starts 180 m from the eye, so the ground's edge could be 70 m from the player and unhazed.

Change (`desert/src/world.hpp`, `desert/src/world.cpp`): `kGroundHalf` is now 380 m. Within ±120 m (`kFineHalf`) the ground is the 1 m grid (240 x 240 cells). Outside it is a ring of 4 m cells (`kCoarseCell`) out to ±380 m. The coarse corners on the fine grid's border are the fine grid's own vertices (by index, not copies). Each of the 240 coarse cells along the border fans its near side's 5 fine vertices to its two far corners (5 triangles), so no fine edge ends part-way along a coarse edge. The 4 corner cells of the ring touch the fine grid only at a shared vertex and are plain quads. Winding is fixed per triangle by its turn seen from above. `groundHeight` now reads the fine grid from -120 m. Its corners are the same integer points as before, and the play area (±80 m) is all fine grid.

Checked with temporary code, since removed: 180,920 triangles (cycle 1: 180,000) and 90,841 vertices. Every edge is shared by exactly two triangles except the 760 on the outer ±380 m boundary, so there are no cracks or T-junctions. No edge is used more than twice, and every triangle faces up.

`capture_corner.png` is from (-80, -80), the eye 1.7 m up, looking level toward (-1, -1) (a temporary spawn pose, since reverted). The sand runs to the horizon and its last rows are the horizon colour. The same capture with the ground pushed out to ±1000 m is byte-identical, so nothing of the 380 m edge shows.
