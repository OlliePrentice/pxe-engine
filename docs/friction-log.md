# Friction log
What the agents and the orchestrator had to find or build by hand during `/backlog-run`, collected across every run. The retro at the end of each batch adds to it and acts on it (see `.claude/skills/backlog-run/SKILL.md`, step 7):
- `doc-gap`: a fact that wasn't written down where someone looked for it. Fixed on the first sighting.
- `tool`: a script or command sequence written by hand. Built into a proper tool in `tools/` once it has been written three times.
- `instruction-gap`: a skill, agent or spec instruction that left something unclear. Fixed in that file.

Entries the retro may not act on alone (models, permissions, hard rules, review gates, layering) are `needs-user`, with a proposed change.

Entry format:
```
## F-<nnn>: <short title>
Kind: doc-gap | tool | instruction-gap
Status: open | actioned | needs-user
Seen: <task-id> (<date>), <task-id> (<date>), ...
Detail: <what was missing or repeated, and where people looked>
Action: <what changed, with the file and commit, or the tooling task's ID>
```

Number entries sequentially and never reuse IDs.

# Entries

## F-001: Shell differences on macOS (zsh, BSD sed)
Kind: doc-gap
Status: actioned
Seen: render-mode-checks (2026-10-29), desert-arms (2026-10-29, developer: a `cp`-restored file wasn't rebuilt, so a capture came from a stale binary), render-mode-split (2026-10-29, orchestrator: an unquoted `$t` holding a command and its argument didn't word-split in zsh, so a check loop ran nothing)
Detail: The developer had to work out that BSD `sed -i` needs `sed -i ''` and that zsh's pipe exit codes are `$pipestatus`, for the negative-check edits. The old byte-identical paragraph in CLAUDE.md carried a zsh tip ("pass multi-word arguments through \"$@\" or an array"), and the render-mode-checks spec's replacement text dropped it.
Action: CLAUDE.md "Verifying a change" gains a shell-notes paragraph covering zsh arguments, `$pipestatus` and `sed -i ''` (retro commit); desert-arms retro adds `touch` after restoring a file.

## F-002: tools/README.md's build.sh row omitted the desert game
Kind: doc-gap
Status: actioned
Seen: render-mode-checks (2026-10-29)
Detail: `tools/build.sh` takes `game|desert|example`, but its row in tools/README.md listed only `game|example`. Found by the developer while adding the check tools.
Action: row fixed in the render-mode-checks docs commit.

## F-003: A spec replacing doc text didn't quote the whole passage it replaced
Kind: instruction-gap
Status: actioned
Seen: render-mode-checks (2026-10-29)
Detail: The spec told the developer to replace "the paragraph that begins ..." in CLAUDE.md, but the passage spanned two paragraphs. The developer had to decide how to merge them, and a zsh tip in the old text was lost (F-001).
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": a spec that replaces doc text quotes the full passage to replace and the full replacement, and says what in the old text must survive (retro workflow commit).

## F-004: Where a check tool leaves the base's binaries
Kind: doc-gap
Status: actioned
Seen: render-mode-split (2026-10-29)
Detail: To bench against main, the developer needed main's binaries and found their path only by reading `ortho-check.sh --help`.
Action: tools/README.md's ortho-check row says the printed base directory holds runnable binaries, with a `--bench` example (retro commit).

## F-005: Verification greps that the required code itself fails
Kind: instruction-gap
Status: actioned
Seen: render-mode-split (2026-10-29)
Detail: The spec's "no output" grep for generic words (perspective, haze) over files the spec required to stay as main's could never be empty: main's own text matched. It took a replan.
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": run every "no output" grep in a spec's Verification against the code the spec keeps (main, or the files restored from it) before queueing it, and grep for the identifiers the change adds rather than generic words (retro workflow commit).

## F-006: Splitting a source file by line ranges, keeping moved code byte-identical
Kind: tool
Status: open
Seen: render-mode-split (2026-10-29)
Detail: Written by hand: docs/tasks/render-mode-split/scripts/split_renderer.py.
Action: none yet (first sighting).

## F-007: Pruning a shader by exact markers, with an identifier-usage counter
Kind: tool
Status: open
Seen: render-mode-split (2026-10-29)
Detail: Written by hand: docs/tasks/render-mode-split/scripts/shaderedit.py.
Action: none yet (first sighting).

## F-008: Building a throwaway test program against the engine
Kind: tool
Status: open
Seen: render-mode-split (2026-10-29)
Detail: Written by hand, with the macOS link line copied from the example's link.txt: docs/tasks/render-mode-split/scripts/mode_mismatch.sh.
Action: none yet (first sighting).

## F-009: Alternating --bench runs against a base, with medians
Kind: tool
Status: open
Seen: render-mode-split (2026-10-29)
Detail: Written by hand: docs/tasks/render-mode-split/scripts/bench_alt.sh.
Action: none yet (first sighting).

## F-010: Asset requirements without a matching build check, and measurement points left undefined
Kind: instruction-gap
Status: actioned
Seen: desert-arms (2026-10-29), desert-arms-detail (2026-10-29, designer: the knurled dial's required facing squeezed its knurl pitch under the 3 px check; the per-part triangle guide couldn't hold the stitching the same section set, 3,120 triangles for 260 dashes)
Detail: The arms spec required the gauge to read (Decisions, Verification) but didn't list it among the build checks. It also measured "hand centre", "top of the index knuckle" and "wrist turned 30 degrees" without defining the points or the axis, and gave a pose guide its own binding framing checks contradicted (the gauge fell off-screen with the guide's elbows). The designer worked each out and recorded it in assets.md.
Action: .claude/skills/feature/SKILL.md, step 1.7 "Each asset": every visual requirement gets a build check, every measured point and axis is defined, and a guide that binding checks also constrain is checked against them, roughly with numbers, before queueing (retro workflow commit). desert-arms-detail retro: the same step now asks to work a budget guide from the counts and sizes the spec itself sets, and to check each fixed placement or facing against the size checks on the parts placed there.

## F-011: A spec's engine code used a GL call the engine's loader doesn't load
Kind: instruction-gap
Status: actioned
Seen: desert-arms (2026-10-29)
Detail: The binding view-model block called `glDepthRange`, which isn't in `PXE_GL_EACH` (engine/src/gl.hpp), while the spec forbade other engine file changes. It took a replan.
Action: .claude/skills/feature/SKILL.md, step 2: grep every GL call a spec's engine code adds in engine/src/gl.hpp, and name the table entry in the spec if it's missing (retro workflow commit).

## F-012: Working in the live Blender session through the MCP
Kind: doc-gap
Status: actioned
Seen: desert-arms (2026-10-29)
Detail: The designer had to work out that a builder object survives between `execute_blender_code` calls only in `bpy.app.driver_namespace`, and that Blender's glTF importer adds a bone-shape `Icosphere` mesh (in `glTF_not_exported`), so a re-import counts two meshes for a one-mesh glb.
Action: .claude/agents/designer.md, "Building assets": both facts (retro workflow commit).

## F-013: Attachment check for a character's pieces (BVH overlap, ray-parity containment)
Kind: tool
Status: open
Seen: fox (build_fox.py, earlier run), desert-arms (2026-10-29, build_arms.py `attachment_problems`)
Detail: Written by hand twice, with different interfaces (the fox finds pieces from the mesh, the arms take named pieces and an attach list). The designer also suggested sharing the perspective eye-camera projection (`project`/`framing` in build_arms.py). A third copy triggers a shared tools/blender/ helper.
Action: none yet (two sightings). desert-arms-detail extended build_arms.py's `attachment_problems` in place (per-piece BVH trees, for 511 pieces) and added `screen_width`, `first_hit` and `seen` beside `project`; not a third copy. The designer suggests a shared tools/blender/ module for view-model checks when the next view-model character comes.

## F-014: Searching a pose numerically against framing checks
Kind: tool
Status: open
Seen: desert-arms (2026-10-29)
Detail: docs/tasks/desert-arms/scripts/pose_search.py.
Action: none yet (first sighting).

## F-015: Re-importing a glb to verify it against its build
Kind: tool
Status: open
Seen: desert-arms (2026-10-29)
Detail: docs/tasks/desert-arms/scripts/verify_arms_glb.py (build_arms.py also reads its glb back after export).
Action: none yet (first sighting).

## F-016: Checks on parts at the screen edge, and first-hit checks without a tolerance
Kind: instruction-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29)
Detail: "Every seen screw head at least 3 px", with "seen" meaning any vertex in view, failed for a screw cut to a 1.9 px sliver by the bottom edge, and the spec didn't say whether a part cut by the edge counts. "The face is the first thing a ray toward each face vertex meets" gave no tolerance, so rays grazing the rim and hitting the housing behind counted as blocked. The designer moved the two edge screws out of view (2 of 4 seen, as allowed) and used the 0.5 mm "seen" tolerance.
Action: .claude/skills/feature/SKILL.md, step 1.7 "Each asset": say whether a part cut by the screen edge counts in a size check, and give every ray or first-hit check a distance tolerance (retro workflow commit).

## F-017: Weights for detail on a blended skin surface
Kind: doc-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29)
Detail: Stitches on the cuff, where the glove blends between forearm and hand, drifted off it in the wrist test poses (60 dashes failed attachment). Skinning interpolates a facet linearly between its ring vertices' weights; evaluating the blend function at the detail's own position gives different weights. Interpolating the rings' weights fixed it (`cuff_weights` in desert/tools/blender/build_arms.py).
Action: .claude/agents/designer.md, "Building assets" (retro workflow commit).

## F-018: Workbench previews washed out by the default view transform
Kind: doc-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29)
Detail: Blender 5.2's default AgX view transform washed the grime bands (160, 146, 120) out to the wrap colour (206, 194, 168) in the previews. The Standard view transform shows the materials' colours.
Action: .claude/agents/designer.md, "Building assets" (retro workflow commit).

## F-019: Comparing two glbs' rigs
Kind: tool
Status: open
Seen: desert-arms-detail (2026-10-29)
Detail: docs/tasks/desert-arms-detail/scripts/compare_joints.py compares joint names, node transforms and inverse bind matrices of two glbs, to prove a rebuild kept the rig. Related to F-015 (re-importing a glb to verify it).
Action: none yet (first sighting).

## F-020: Temporary-edit verifications without the line to edit
Kind: instruction-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29)
Detail: Verification 8's "spawn pitch 1.2f" didn't name the line (`player.spawn(..., 0.f)` in desert/src/main.cpp). A rebuild after one temporary edit silently gave a capture identical to the previous edit's; the developer caught it only by comparing the two captures.
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": a temporary edit names the file, the exact text to replace and its replacement, and the Verification says to `cmp` captures from different edits against each other (retro workflow commit).

## F-021: Clipping captures by temporary source edits to the desert's spawn
Kind: tool
Status: open
Seen: desert-arms (2026-10-29), desert-arms-detail (2026-10-29)
Detail: Both tasks moved the desert's spawn (`kSpawn`) and pitch by temporary edits to desert/src/main.cpp, rebuilt, captured and reverted. `pxe_desert` has no flag for the spawn pose. A third sighting queues a tooling task (likely `--spawn X,Z` and `--pitch` flags, or a script).
Action: none yet (two sightings).

## F-022: Headless Blender exits 0 when the build script fails
Kind: doc-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29, orchestrator)
Detail: `Blender -b -P build_arms.py` raises on a failed check, which stops the export, but Blender still exits 0, so a "build fails" check showed only in the log. The orchestrator confirmed it with a one-line raising script; `--python-exit-code 1` gives exit 1. The rebuild also rewrote the four preview PNGs with different bytes, leaving the tree dirty.
Action: CLAUDE.md, after the shell notes: the `--python-exit-code 1` flag and restoring previews after a verify-only rebuild; README.md's arms rebuild command gains the flag (retro commit).
