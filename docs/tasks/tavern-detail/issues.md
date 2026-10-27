# Issues

## ISSUE-001: Crease ink restyles the Wood floor's plank grooves
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`engine/shaders/edge.frag` `edges()`: `creased()` returns true for both `kSurfOak` and `kSurfWood`. The floor's V-groove bevels are about 50° from the plank tops, so every groove and plank end now draws an ink line indoors (developer's handoff, known limitations). This restyles the floor, which the user excluded from this task's changes. The spec is revised (Decisions, "Crease lines indoors"): crease ink is drawn on **Oak pixels only**. An Oak pixel creasing against a Wood, Plaster or Stone neighbour still inks on its own side. Wood pixels never take crease ink. Keep the one-pixel tie-break for Oak against Oak. Update the shader comments and the README line to match.

### Fix notes
Root cause: `creased()` in `engine/shaders/edge.frag` returned true for `kSurfWood` as well as `kSurfOak`, so every Wood pixel indoors (the floor's V-groove bevels and plank ends, about 50 degrees from the plank tops) took crease ink. Changed `creased()` to `surface == kSurfOak` only. The loop's existing rule now gives exactly the revised spec: only Oak pixels enter the crease test; an Oak pixel whose neighbour is anything not Oak (Wood, Plaster, Stone) inks on its own side (`!creased(sq)`); Oak against Oak keeps the one-pixel tie-break (the side facing the camera less, or the larger normal x when equal). Outdoors `creases` is still false, so nothing changes there. Shader comments (header, `creased()`, the tie-break comment) and the README sentence were updated to say Oak only. Checked in a fresh build with `--scene tavern --time 16 --capture`: against the cycle-1 shader, 26,882 px changed, all floor groove ink removed; against the same shader with crease ink switched off, the 4,522 px of crease ink that remain are all on the oak furniture, bar, shelves, barrels, mantel and stage edge, none on the floor.

## ISSUE-002: Settings menu counts the brazier as burning by day
Status: wont-fix
Severity: minor
Source: review
Found in cycle: 3
Reopened: 0

### Problem
Developer's cycle-3 known limitations: the night-only brazier is scaled out by `WorldClock::Fire::drawnLevel()` when drawn, but its `level` stays up, so the settings menu's fire count still includes it by day. From cycle 4 the same applies to the four night-only wall sconces.

### Fix notes
Wont-fix (orchestrator): it is a debug readout only. What the player sees and the light it gives are both correct, and indoors the brushes are inert (out of scope in the tavern task). Revisit if the menu's count is ever shown to players.
