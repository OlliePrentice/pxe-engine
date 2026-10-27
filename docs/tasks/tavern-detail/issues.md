# Issues

## ISSUE-001: Crease ink restyles the Wood floor's plank grooves
Status: open
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`engine/shaders/edge.frag` `edges()`: `creased()` returns true for both `kSurfOak` and `kSurfWood`. The floor's V-groove bevels are about 50° from the plank tops, so every groove and plank end now draws an ink line indoors (developer's handoff, known limitations). This restyles the floor, which the user excluded from this task's changes. The spec is revised (Decisions, "Crease lines indoors"): crease ink is drawn on **Oak pixels only**. An Oak pixel creasing against a Wood, Plaster or Stone neighbour still inks on its own side. Wood pixels never take crease ink. Keep the one-pixel tie-break for Oak against Oak. Update the shader comments and the README line to match.

### Fix notes
