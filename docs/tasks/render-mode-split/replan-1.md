# render-mode-split replan 1

## What failed
Verification step 5, third grep:
`grep -n -i -E "perspective|PerspectiveFrame|FirstPersonCamera|backdrop|haze|shadowFit|flashFit" engine/src/renderer_orthographic.cpp engine/shaders/edge.frag engine/include/pxe/camera.hpp`
must give no output, but it can't. Code the spec itself requires matches it. Checked by the orchestrator, cycle 1:
- `engine/shaders/edge.frag` is `main`'s file (required: `git diff main` shows only the two surface constants), and `main`'s rain code says "haze" (lines 99, 297, 476, 484, 492, 493, 520).
- `renderer_orthographic.cpp:271`: `updateFires`, moved unchanged as the spec requires, builds the fire shadow cubes with `glm::perspective`.
- `renderer_orthographic.cpp:360`: the binding mode-mismatch message names `kPerspective`.

None of them refers to the perspective camera mode. The grep was written too broadly. The code is right.

## Alternatives
- Change the code to avoid the words: rejected. It would break the byte-identical `edge.frag` requirement, the "moved unchanged" requirement and the binding message.
- Drop the grep: rejected. It is the check for Decision (c), that perspective-mode code doesn't leak into orthographic files.

## Proposal
Narrow the grep to the perspective mode's own identifiers, and check the generic words separately against an allowed list.

## Changes
`## Verification`, step 5, the third bullet only. It changes from the grep above to:
- `grep -n -E "PerspectiveFrame|FirstPersonCamera|PERSPECTIVE|edgePerspective|progBackdrop|backdrop|fitLightAhead|shadowFit|flashFit|distanceHaze|uHaze" engine/src/renderer_orthographic.cpp engine/shaders/edge.frag engine/include/pxe/camera.hpp`: no output. `grep -n -i perspective` over the same files: only `glm::perspective` in `updateFires` and the mode-mismatch message.

No other section changes. The acceptance criteria don't name this grep.
