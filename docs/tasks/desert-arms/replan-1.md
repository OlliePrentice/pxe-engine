# desert-arms replan 1

## What failed
Approach step 2's binding view-model block calls `glDepthRange`, and the step ends "No other engine file changes". The engine loads its own GL entry points through the `PXE_GL_EACH` table in `engine/src/gl.hpp`, which has no `glDepthRange` (`grep -c DepthRange engine/src/gl.hpp` gives 0, checked by the orchestrator). So the block doesn't compile:
`renderer_perspective.cpp:254:5: error: use of undeclared identifier 'glDepthRange'` (developer, cycle 1).

With the one table entry added, the developer's cycle 1 build passed every Verification step:
- ortho-check 6/6;
- perspective-check: example-first-person passes, and only the two desert cases differ;
- Verification 4 empty;
- the captures as specified, the trunk and looking-down captures included.

## Alternatives
- Add `glDepthRange` to the loader table: one line, entry-point loading only, with no effect on either mode's output (the ortho and perspective checks prove it). **Chosen.**
- Squeeze the view model's depth through its projection matrix (scale clip z) instead of the depth range: rejected. It departs from the binding block, needs its own view-projection, and changes nothing the user agreed that the table entry doesn't already give.

## Proposal and changes
`## Approach` only:
- Step 2 gains a first bullet, before the `FrameParams` change: "`engine/src/gl.hpp`: add `M(PFNGLDEPTHRANGEPROC, glDepthRange)` to `PXE_GL_EACH`, after the `glDepthMask` entry. The loader has no `glDepthRange` yet."
- Step 2's closing line "No other engine file changes. In particular:" becomes "No other engine file changes besides `gl.hpp`'s entry. In particular:".

No other section changes. The acceptance criteria, Decisions and Layering stand: `gl.hpp` isn't among the files Verification 4 and the criteria require to stay unchanged. Backlog Touches gain `engine/src/gl.hpp`.
