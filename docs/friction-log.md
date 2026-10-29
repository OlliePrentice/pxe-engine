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
