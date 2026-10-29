# Issues: view-model-edges

## ISSUE-001: View-model uniform block splits the camera uniforms
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`engine/shaders/edge_perspective.frag` lines 110-116: the `uViewModelDepth` uniform, its comment and `kViewModelStep` are inserted between `uniform float uNear;` and `uniform float uFar;`. That separates `uFar`'s comment "(uNear is the near one)" from `uNear`, and breaks up the run of camera uniforms (`uEye` .. `uHazeSpan`) that each carry a trailing comment. The spec puts the block after that group ("after `uniform float uNear;`'s group, next to the other camera uniforms"). Move the block, unchanged, to directly after `uniform vec2 uHazeSpan;` (before `out vec4 oColor;`), so `uNear` and `uFar` sit together again. Keep `viewModelAt` where it is. No output may change: confirm the desert captures byte-match this cycle's base `edges-day.png` and `edges-night.png`, and rerun `tools/ortho-check.sh` and `tools/perspective-check.sh` against 37c2178 with the same results as cycle 1.

### Fix notes
