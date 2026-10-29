## Cycle 1
Build: fail as the spec is written (renderer_perspective.cpp does not compile); pass with one extra engine line the spec does not name. Nothing code-side is committed.

### Spec infeasible
The spec's `## Approach` step 2 draws the view model between `glDepthRange(0.0, kViewModelDepth)` and `glDepthRange(0.0, 1.0)`, and says "No other engine file changes". The engine loads its GL entry points itself (`engine/src/gl.hpp`, the `PXE_GL_EACH` table) and `glDepthRange` is not in it. With the spec's code exactly as written:

    engine/src/renderer_perspective.cpp:254:5: error: use of undeclared identifier 'glDepthRange'
    engine/src/renderer_perspective.cpp:265:5: error: use of undeclared identifier 'glDepthRange'

No other call works either: `glDepthRangef` is not loaded either, and neither is declared without a table entry. The binding block can't compile without `engine/src/gl.hpp` changing, which the spec doesn't name, so I stopped as the rules require and did not add it.

What I tried: I added one line to the `PXE_GL_EACH` table, `M(PFNGLDEPTHRANGEPROC, glDepthRange)`, after the `glDepthMask` entry, and implemented all of steps 2 and 3 as written. Everything verified (below). Then I discarded all of it so the branch stays as the cycle left it. The full work is saved outside the repo, in `/private/tmp/claude-501/-Users-sampr-Projects-Games-pxe-pxe-engine/1c2863cb-0adb-420a-85d6-394fa258a7e4/scratchpad/desert-arms-cycle1/`: `tracked-changes.patch` (every tracked change including the gl.hpp line), `arms.hpp`, `arms.cpp`, and the six captures.

Evidence with that one line added (build: only the stb sprintf warning, no new warnings):
- `tools/ortho-check.sh db97273`: 6/6 identical.
- `tools/perspective-check.sh db97273`: `example-first-person` PASS; `desert-default` and `desert-night` FAIL "png differs" (the intended change); 1/3 identical.
- `git diff db97273 -- engine/src/renderer.cpp engine/src/renderer_orthographic.cpp engine/shaders game/`: no output.
- Captures: day (16.5 h) twice byte-identical; morning, golden, night and the two clipping captures made.
- arms-day / morning / golden / night: both gloved hands in the lower corners, centres at about 450 px and 510 px of 960 (a quarter in from each side), knuckles about 20% up from the bottom, forearms leaving through the bottom edge, nothing above the lower 35% (the highest point is at about y 425 of 540, which is 21% up from the bottom; the limit is 35%), outline round the arms, light creases on the glove backs and wrap bands, left bracer with three teal bars at the bottom left, still bright teal at night (emissive). The right wrap is pale in day and bluish at night; in all four the arms darken with the light as the sand does.
- arms-at-trunk (kSpawn = {0, -0.71}): both hands and forearms whole in front of the bark, not cut. The trunk fills the middle of the view and the hands overlap its base.
- arms-looking-down (pitch 1.2): both arms whole, unchanged in the view, in front of the plain sand filling the rest of the view.
- Both temporary edits were reverted with the rest.

Alternatives, not built: (a) add `glDepthRange` to `gl.hpp`'s table, name it in the spec (a one-line, output-neutral change that the orthographic check also covers); (b) get the same squeeze without it by scaling clip z in the view model's projection, which departs from the binding block.

### Issues addressed
- none (cycle 1)

### Layering
- Engine: none committed.
- Game: none committed.
- Misplaced code touched: none

### How to playtest
- Nothing to playtest this cycle.

### Docs
- None this cycle.

### Known limitations
- The cycle's code is not on the branch; see Spec infeasible.
