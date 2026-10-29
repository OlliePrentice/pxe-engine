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
Seen: render-mode-checks (2026-10-29)
Detail: The developer had to work out that BSD `sed -i` needs `sed -i ''` and that zsh's pipe exit codes are `$pipestatus`, for the negative-check edits. The old byte-identical paragraph in CLAUDE.md carried a zsh tip ("pass multi-word arguments through \"$@\" or an array"), and the render-mode-checks spec's replacement text dropped it.
Action: CLAUDE.md "Verifying a change" gains a shell-notes paragraph covering zsh arguments, `$pipestatus` and `sed -i ''` (retro commit).

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

