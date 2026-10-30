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
Seen: render-mode-checks (2026-10-29), desert-arms (2026-10-29, developer: a `cp`-restored file wasn't rebuilt, so a capture came from a stale binary), render-mode-split (2026-10-29, orchestrator: an unquoted `$t` holding a command and its argument didn't word-split in zsh, so a check loop ran nothing), desert-arms-walk (2026-10-30, developer: a loop over bad-argument strings passed each as one argument)
Detail: The developer had to work out that BSD `sed -i` needs `sed -i ''` and that zsh's pipe exit codes are `$pipestatus`, for the negative-check edits. The old byte-identical paragraph in CLAUDE.md carried a zsh tip ("pass multi-word arguments through \"$@\" or an array"), and the render-mode-checks spec's replacement text dropped it.
Action: CLAUDE.md "Verifying a change" gains a shell-notes paragraph covering zsh arguments, `$pipestatus` and `sed -i ''` (retro commit); desert-arms retro adds `touch` after restoring a file. desert-arms-walk retro: the shell notes say an unquoted `$var` doesn't split in zsh, and to loop over arrays or use `${=var}`.

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
Seen: render-mode-split (2026-10-29), desert-settings-menu (2026-10-30, developer: the `ImGui` grep over `desert/src` matched comments in the spec's own binding `settings.hpp` and `main.cpp` sketch; reworded to pass)
Detail: The spec's "no output" grep for generic words (perspective, haze) over files the spec required to stay as main's could never be empty: main's own text matched. It took a replan.
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": run every "no output" grep in a spec's Verification against the code the spec keeps (main, or the files restored from it) before queueing it, and grep for the identifiers the change adds rather than generic words (retro workflow commit). 2026-10-30 retro: the greps also run against the spec's own binding code and sketches, comments included.

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
Seen: desert-arms (2026-10-29), desert-arms-detail (2026-10-29, designer: the knurled dial's required facing squeezed its knurl pitch under the 3 px check; the per-part triangle guide couldn't hold the stitching the same section set, 3,120 triangles for 260 dashes), desert-arms-walk (2026-10-30, designer: "the hand trails it at the wrist" named no axis)
Detail: The arms spec required the gauge to read (Decisions, Verification) but didn't list it among the build checks. It also measured "hand centre", "top of the index knuckle" and "wrist turned 30 degrees" without defining the points or the axis, and gave a pose guide its own binding framing checks contradicted (the gauge fell off-screen with the guide's elbows). The designer worked each out and recorded it in assets.md.
Action: .claude/skills/feature/SKILL.md, step 1.7 "Each asset": every visual requirement gets a build check, every measured point and axis is defined, and a guide that binding checks also constrain is checked against them, roughly with numbers, before queueing (retro workflow commit). desert-arms-detail retro: the same step now asks to work a budget guide from the counts and sizes the spec itself sets, and to check each fixed placement or facing against the size checks on the parts placed there. desert-arms-walk retro: the same step now asks for the axis of every rotation in a motion guide too (F-024).

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
Seen: desert-arms (2026-10-29), desert-arms-walk (2026-10-30)
Detail: docs/tasks/desert-arms/scripts/verify_arms_glb.py (build_arms.py also reads its glb back after export). desert-arms-walk: docs/tasks/desert-arms-walk/scripts/reimport_glb.py re-imports a glb and lists its skinned meshes, armatures and actions.
Action: none yet (two sightings). The glb-tool task (F-026) reads glbs directly; it doesn't re-import through Blender's importer. Since glb-tool (0476111), designer.md tells the designer to check an export with `tools/glb.py info`/`compare` and re-import only for what the glb's data can't show, which should make re-imports rarer.

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
Status: actioned
Seen: desert-arms-detail (2026-10-29), desert-arms-walk (2026-10-30, `check_glb_clips` in build_arms.py compares the rest-pose vertices, joint nodes and inverse bind matrices with the glb before the clips)
Detail: docs/tasks/desert-arms-detail/scripts/compare_joints.py compares joint names, node transforms and inverse bind matrices of two glbs, to prove a rebuild kept the rig. Related to F-015 (re-importing a glb to verify it).
Action: folded into the glb-tool tooling task (F-026) as `tools/glb.py compare`. Done in glb-tool (0476111).

## F-020: Temporary-edit verifications without the line to edit
Kind: instruction-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29)
Detail: Verification 8's "spawn pitch 1.2f" didn't name the line (`player.spawn(..., 0.f)` in desert/src/main.cpp). A rebuild after one temporary edit silently gave a capture identical to the previous edit's; the developer caught it only by comparing the two captures.
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": a temporary edit names the file, the exact text to replace and its replacement, and the Verification says to `cmp` captures from different edits against each other (retro workflow commit).

## F-021: Clipping captures by temporary source edits to the desert's spawn
Kind: tool
Status: actioned
Seen: desert-arms (2026-10-29), desert-arms-detail (2026-10-29)
Detail: Both tasks moved the desert's spawn (`kSpawn`) and pitch by temporary edits to desert/src/main.cpp, rebuilt, captured and reverted. `pxe_desert` has no flag for the spawn pose. A third sighting queues a tooling task (likely `--spawn X,Z` and `--pitch` flags, or a script).
Action: folded into F-038 (the general temporary-edit capture loop): `tools/captures.sh --edit` (task `capture-tool`, queued 2026-10-30) covers spawn-pose edits.

## F-022: Headless Blender exits 0 when the build script fails
Kind: doc-gap
Status: actioned
Seen: desert-arms-detail (2026-10-29, orchestrator)
Detail: `Blender -b -P build_arms.py` raises on a failed check, which stops the export, but Blender still exits 0, so a "build fails" check showed only in the log. The orchestrator confirmed it with a one-line raising script; `--python-exit-code 1` gives exit 1. The rebuild also rewrote the four preview PNGs with different bytes, leaving the tree dirty.
Action: CLAUDE.md, after the shell notes: the `--python-exit-code 1` flag and restoring previews after a verify-only rebuild; README.md's arms rebuild command gains the flag (retro commit).

## F-023: Exact rest keys from Blender's glTF exporter
Kind: instruction-gap
Status: actioned
Seen: desert-arms-walk (2026-10-30, designer)
Detail: The spec required rest-keyed joints and every translation and scale key to equal rest "exactly". Blender 5.2's glTF exporter, with forced sampling, writes keys 1-2 float32 steps off the joint nodes' own TRS (translation 1.3e-7, scale 4.2e-7, rotation 6e-8; docs/tasks/desert-arms-walk/scripts/key_diffs.py). The designer exported the glb without animations and appended the clips itself (`add_clips` in desert/tools/blender/build_arms.py), copying node values for the still keys.
Action: .claude/agents/designer.md, "Building assets": the exporter's key error and the `add_clips` way round it. .claude/skills/feature/SKILL.md, step 1.7: a spec that wants exported values to equal others exactly says it means float32 from the same source, or gives a tolerance (retro workflow commit).

## F-024: A value given for one clip only, and a rotation without its axis
Kind: instruction-gap
Status: actioned
Seen: desert-arms-walk (2026-10-30, designer)
Detail: The spec gave the walk's wrist trail ("up to 6 degrees") but no value for the sprint, and "the hand trails it at the wrist" named no axis. The designer used 6 degrees in both and pitched the hand about the elbow's hinge, recording both in assets.md.
Action: .claude/skills/feature/SKILL.md, step 1.7: every value given for one clip or variant is given for each of the others (or "as <clip>"), and every rotation in a motion guide names its axis (retro workflow commit).

## F-025: A check whose targets are the constants that build the thing checked
Kind: doc-gap
Status: actioned
Seen: desert-arms-walk (2026-10-30, designer)
Detail: The first amplitude check compared the motion against the same `CLIPS` table that drives it, so a wrong number there could never fail it. A negative run (walk bob set to 2 cm) passed. The checks now use a separate `CLIP_TARGETS` table.
Action: .claude/agents/designer.md, "Building assets": a check's targets come from the spec, in their own table, not from the build's constants, and each new check is proved by one negative run (retro workflow commit).

## F-026: Reading a glb's chunks and accessors by hand
Kind: tool
Status: actioned
Seen: desert-arms (2026-10-29, docs/tasks/desert-arms/scripts/verify_arms_glb.py), desert-arms-detail (2026-10-29, docs/tasks/desert-arms-detail/scripts/compare_joints.py), desert-arms-walk (2026-10-30, `read_glb` and `read_accessor` in desert/tools/blender/build_arms.py, and docs/tasks/desert-arms-walk/scripts/key_diffs.py)
Detail: Four copies of the same glb JSON-chunk, binary-chunk and accessor decoding with `struct`, in three tasks. tools/ has no shared glb reader.
Action: tooling task glb-tool queued (`tools/glb.py`: `read`, `accessor`, `node_trs`; `info` and `compare`), with F-019 folded in. glb-tool merged (0476111): `tools/glb.py` is in tools/README.md and README.md, and .claude/agents/designer.md's export check now points to it (retro commit).

## F-027: Where the view model is in a capture
Kind: tool
Status: actioned
Seen: desert-arms-detail (2026-10-29, a pixel diff against arms-day.png, by hand), desert-arms-walk (2026-10-30, docs/tasks/desert-arms-walk/scripts/arm_rows.py), view-model-edges (2026-10-29, docs/tasks/view-model-edges/scripts/diff_box.py, found at the retro of 2026-10-30), desert-lighting (2026-10-30, docs/tasks/desert-lighting/scripts/png-diff.py: mean per-channel change over the ground), perspective-dust (2026-10-30, developer: inline Pillow count of changed pixels, their neighbours and rows)
Detail: Both tasks found the arms' top row and extent by diffing one capture against another. arm_rows.py needs a render without the arms, which takes a temporary edit of `arms.pose` in desert/src/main.cpp, because pxe_desert has no flag to hide them. Related to F-021 (temporary edits for the spawn pose). A third sighting queues a tooling task (likely a capture flag that hides the arms, and a tool that reports the difference's box).
Action: four copies of "how do two captures differ": queued tooling task `png-diff-tool` (`tools/png-diff.py`: count, box, halves, mean change, region, mask), 2026-10-30. A flag to hide the arms stays out of scope until a task needs it again.

## F-028: A batch cut short after a task's merge
Kind: instruction-gap
Status: actioned
Seen: desert-arms-walk (2026-10-30, orchestrator)
Detail: The batch merged desert-arms-walk (bc94943) and stopped before the post-merge build, the docs, the backlog's `done` and the worktree's removal. The next batch found the task `in-progress` in the backlog with its branch already merged, and the preflight only said an `in-progress` task resumes its cycles.
Action: .claude/skills/backlog-run/SKILL.md, preflight: an `in-progress` task whose branch is already merged into the run's branch resumes at step 6's post-merge steps (retro workflow commit).

## F-029: The docs step left CLAUDE.md's flag list stale
Kind: instruction-gap
Status: actioned
Seen: desert-arms-walk (2026-10-30, orchestrator)
Detail: CLAUDE.md's "Verifying a change" lists pxe_desert's flags ("has only `--help`, `--capture` ..."). The task added `--walk` and `--sprint`, and neither the developer's docs notes nor the merge's docs step named that list. Fixed in the desert-arms-walk docs commit.
Action: .claude/skills/backlog-run/SKILL.md, step 6 "Docs": when a task changes a binary's flags, update CLAUDE.md's flag lists under "Verifying a change" too (retro workflow commit).

## F-030: Importing a module from tools/ leaves `__pycache__/` untracked
Kind: doc-gap
Status: actioned
Seen: glb-tool (2026-10-30, orchestrator: `tools/__pycache__/` showed in `git status` after Verification 7 and 8)
Detail: `.gitignore` has no `__pycache__/` entry, so importing `tools/glb.py` leaves the tree dirty, which stops `/backlog-run` at preflight. The fix lay outside glb-tool's allowed files (ISSUE-001, wont-fix there).
Action: fix task gitignore-pycache queued (adds `__pycache__/` to `.gitignore`). Fixed: `.gitignore` ignores `__pycache__/` everywhere since gitignore-pycache merged (8c45110, 2026-10-30).

## F-031: Where a glb's binary chunk starts
Kind: doc-gap
Status: actioned
Seen: glb-tool (2026-10-30, developer: Verification 7's byte patch)
Detail: The hand-written copies found the bin chunk's data at `20 + json length + 8`, which is right only when the JSON chunk's length is already a multiple of 4. Chunks are 4-byte aligned, so it starts at `20 + pad4(json length) + 8`.
Action: `glb.read` in `tools/glb.py` walks the chunks with the padding, and designer.md now points build scripts to it instead of decoding by hand (retro commit). Only a script that patches raw bytes still needs the offset: `docs/tasks/glb-tool/scripts/perturb_position.py` shows it.

## F-032: A spec's sketch that disagrees with its text
Kind: instruction-gap
Status: actioned
Seen: glb-tool (2026-10-30, developer and orchestrator: the `info` example listed the animations in a different order from the text's "in file order"), perspective-smooth-lighting (2026-10-30, developer and orchestrator: the binding comment on `Lighting::sunDir` said it follows the moon while §2 and Decisions set it to the sun only, costing a cycle; the binding `bindLit` signature had no way to reach `params.actorMaterials`, which its text named)
Detail: The developer kept file order, rightly, since the sketch wasn't binding, but had to decide which one wins.
Action: .claude/skills/feature/SKILL.md: a sketch must agree with the rules written around it (retro workflow commit). 2026-10-30 retro: binding examples too, comments included; a binding signature takes every value its text says the function uses; say whether a binding block's comments may be reworded.

## F-033: `\b` in `git grep -E` matches nothing on this Mac
Kind: doc-gap
Status: actioned
Seen: desert-arms-revision (2026-10-30, planning)
Detail: A spec's "no output" grep used `\bBAR\b` with `git grep -nE`. On this Mac's git it matched nothing, even with `BAR = (...)` in the file, so the check would have passed with the identifier still there. Caught by running the grep against the base before queueing, as `/feature` requires; `git grep -w` works.
Action: CLAUDE.md, shell notes: `git grep -E` doesn't support `\b`; use `-w` or explicit context (retro commit).

## F-034: Running a build script with a constant overridden
Kind: tool
Status: actioned
Seen: desert-arms-revision (2026-10-30, planning: a trial of `HAND_ROLL` 90 by `sed` on a scratch copy of build_arms.py, with `HERE` pinned to the repo), desert-arms-revision (2026-10-30, designer: negative runs of the new checks the same way, with `PXE_SKIP_EXPORT=1`), desert-arms-pose (2026-10-30, designer: docs/tasks/desert-arms-pose/scripts/pose_trials.py on branch desert-arms-pose, runpy with constants set in the functions' globals over pose variants)
Detail: Trying a value, or proving a check fails, means copying `desert/tools/blender/build_arms.py` to scratch, `sed`-editing a constant (and `HERE`, so the copy still finds the repo's paths), and running it headless. Two sightings so far, both in one task; a third queues a tooling task (likely a `PXE_OVERRIDE=NAME=value` hook in the build scripts, or a wrapper in `tools/`).
Action: third sighting: queued tooling task `blender-override-tool` (`tools/blender-override.py -- <script> NAME=expr ...`), 2026-10-30.

## F-035: The orchestrator named the wrong assets.md revision number
Kind: instruction-gap
Status: actioned
Seen: desert-arms-revision (2026-10-30, designer)
Detail: The orchestrator's delegation asked for "Revision: 3", but desert-arms-walk had already recorded Revision 3 for the arms. The designer followed the earlier assets.md files and wrote Revision 4.
Action: none needed: the earlier assets.md files are the record, and the designer read them. Delegations shouldn't restate numbers the task folders already hold.

## F-036: Running a set of captures and `cmp`-ing them, against a base build or twice for determinism
Kind: tool
Status: actioned
Seen: desert-settings-menu (2026-10-30, docs/tasks/desert-settings-menu/scripts/walk-compare.sh: three `--walk` captures, base build from perspective-check.sh's scratch directory vs the worktree), perspective-smooth-lighting (2026-10-30, docs/tasks/perspective-smooth-lighting/scripts/capture-all.sh: seven captures into the task folder, each run twice and `cmp`-ed), desert-lighting (2026-10-30, developer: three captures each twice by hand, and a no-bounce capture `cmp`-ed against the base's from the check script's scratch directory), perspective-sky-clouds (2026-10-30, developer: three captures pairwise `cmp`-ed by hand), perspective-dust (2026-10-30, docs/tasks/perspective-dust/scripts/dust-captures.sh, written before capture-tool merged)
Detail: The check tools cover only their fixed cases. Specs ask for extra cases (walks, hours) compared against base, or captures shown deterministic, and each task writes its own loop. walk-compare.sh also needs the full base sha to find the base build.
Action: queued tooling task `capture-tool` (`tools/captures.sh`: a cases file, `--twice`, `--base` with `--expect`, `--distinct`, `--edit`), 2026-10-30. A separate tool rather than flags on the check scripts, so their fixed cases stay as they are. capture-tool merged 96fbfa8; CLAUDE.md "Verifying a change" points to it (commit ee16c7f).

## F-037: The sort order of `PXE_SHADERS`
Kind: doc-gap
Status: actioned
Seen: perspective-smooth-lighting (2026-10-30, developer)
Detail: The spec said "in alphabetical order"; whether `mesh.vert` or `mesh_perspective.frag` comes first wasn't written anywhere, and the developer worked it out by sorting.
Action: CLAUDE.md "Build": `PXE_SHADERS` is sorted in plain byte order (`.` before `_`) (retro commit).

## F-038: Temporary source edit, rebuild, capture, restore
Kind: tool
Status: actioned
Seen: desert-arms (2026-10-29), desert-arms-detail (2026-10-29) (both as F-021), perspective-sky-clouds (2026-10-30, developer: three cover edits to examples/minimal/main.cpp by `sed`; one rebuild after an in-place edit didn't take, so two captures came out identical until redone), desert-lighting (2026-10-30, developer: the no-bounce edit to desert/src/world.cpp, rebuilt, captured, restored and touched), perspective-dust (2026-10-30, same script as F-036)
Detail: Verification steps prove a change by a temporary edit, and each task applies it, rebuilds, captures, restores and touches by hand. A stale build after an edit is easy to miss (F-020).
Action: queued tooling task `capture-tool` (`tools/captures.sh --edit <file> <old> <new>`: exact-text edit, touched, restored and touched on exit or interrupt), 2026-10-30.

## F-039: Running the check scripts' base build by hand
Kind: doc-gap
Status: actioned
Seen: desert-lighting (2026-10-30, developer: found the base binaries' path in perspective-check.sh's source to compare a no-bounce capture with the base)
Detail: tools/README.md said so for `ortho-check.sh` only; the `perspective-check.sh` row said "the same base build" without the path.
Action: tools/README.md's `perspective-check.sh` row names the base binaries' directory and that the base's captures stay in the run directory's `base/` (retro commit).

## F-040: An ImGui widget ID that must survive a label change
Kind: instruction-gap
Status: actioned
Seen: desert-lighting (2026-10-30, developer and orchestrator: the spec gave `"Sun##c"`/`"Moon##c"` and said the IDs stay the same; ImGui hashes the whole label when it has `##`, and only the part after `###` when it has `###`)
Detail: The developer had to read imgui.cpp to resolve a contradiction inside the spec; it chose `###`, which meets the spec's stated intent.
Action: .claude/skills/feature/SKILL.md, "Make it implementation-ready": when a spec states how a library call behaves (an ID, an order, a flag's effect), check the library; for ImGui, a label whose ID must stay fixed while its text changes uses `###<id>` (retro workflow commit).

## F-041: A planning trial that measured only some of the build's checks
Kind: instruction-gap
Status: actioned
Seen: desert-arms-pose (2026-10-30, designer: the spec's pose failed four binding checks: the compass leaves the view even in the trial's own pose, the bulkier palm leaves the cuff, the 1.10 cuff hides the gauge readout at every search step, and the sprint breaks the clip limit at every allowed wrist z; the trial had measured a few framing numbers only)
Detail: The spec's expected values came from a trial that ran a subset of `check()` and none of `clip_checks()`, and its fallbacks (gauge search, wrist steps) were never run. The spec also said the bracer moves "only as far as it must" without saying how. The task blocked (replan-1.md).
Action: .claude/skills/feature/SKILL.md, assets in the interview: a planning trial runs the build's whole checks at every value the fallbacks allow, and the spec says how dependent parts follow a moved part (retro workflow commit).

## F-042: Testing a signal trap from a non-interactive shell
Kind: instruction-gap
Status: actioned
Seen: capture-tool (2026-10-30, developer and orchestrator: Verification 7's "start it in the background, kill -INT it" can't reach the trap, since background jobs start with SIGINT ignored; the developer wrote scripts/interrupt-test.py)
Detail: A spec's verification step assumed shell job control.
Action: .claude/skills/feature/SKILL.md: a step that signals a process starts it with the default disposition, e.g. from Python's subprocess (retro workflow commit).

## F-043: Parallel tasks racing on the check scripts' shared base build
Kind: instruction-gap
Status: actioned
Seen: capture-tool, perspective-dust, desert-arms-pose (2026-10-30, orchestrator: three tasks would have run the check scripts against the same unbuilt base at once; each `rm -rf`s and rebuilds `${TMPDIR}/pxe-render-check/<sha>` when its binary is missing. Pre-built it before delegating)
Detail: Nothing in the run skill said to build the shared base first.
Action: .claude/skills/backlog-run/SKILL.md, "Run the cycles": build the cycle base's base once before the first delegation and tell agents not to delete it (retro workflow commit).

## F-044: runpy.run_path returns a copy of a build script's globals
Kind: doc-gap
Status: actioned
Seen: desert-arms-pose (2026-10-30, designer: the first live trial gave the same numbers for every candidate, because constants were set in the returned copy)
Detail: Setting `am["GAUGE_S"]` on the dict from `runpy.run_path` doesn't reach the script's functions.
Action: desert/docs/art-style.md, next to "Never hand-edit the glb": set the functions' own globals, or use `tools/blender-override.py` (retro commit).
