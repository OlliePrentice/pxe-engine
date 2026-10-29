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
Seen: desert-arms (2026-10-29)
Detail: The arms spec required the gauge to read (Decisions, Verification) but didn't list it among the build checks. It also measured "hand centre", "top of the index knuckle" and "wrist turned 30 degrees" without defining the points or the axis, and gave a pose guide its own binding framing checks contradicted (the gauge fell off-screen with the guide's elbows). The designer worked each out and recorded it in assets.md.
Action: .claude/skills/feature/SKILL.md, step 1.7 "Each asset": every visual requirement gets a build check, every measured point and axis is defined, and a guide that binding checks also constrain is checked against them, roughly with numbers, before queueing (retro workflow commit).

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
Action: none yet (two sightings).

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

