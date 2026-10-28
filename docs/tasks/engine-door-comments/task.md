# Engine comments: no tavern door
Type: fix
Status: in progress
Cycle: 1

## Goal
Found in the alignment review of merging `main` into `add-tavern` (f48dadc). Two engine comments name game content, which CLAUDE.md's hard rules and docs/architecture.md ("No game content in the engine") forbid:
- engine/include/pxe/skinned.hpp:31: `(a moving prop's carved wood: the tavern door's planks and grain)`
- engine/shaders/edge.frag:15: `A character's creased materials (the tavern door's planks and grain) are inked so too`

Reword both so they explain the capability with generic nouns ("a door's planks and grain" is fine), keeping their meaning.

## Decisions
- Comments only. No code, shader logic or output changes.

## Layering
- Engine: engine/include/pxe/skinned.hpp, engine/shaders/edge.frag.
- Game: none.

## Acceptance criteria
- [ ] Neither comment names the tavern or any other game content.
- [ ] `grep -rniE "tavern|sleeping fox|\bhero\b|\bfox\b|ruins" engine/include engine/src engine/shaders` finds nothing new against f48dadc (fireflies aside).
- [ ] No change outside comments: `git diff f48dadc -- engine` touches comment lines only.
- [ ] Project builds cleanly with the `check` preset.

## Assets
None

## Out of scope
- Any other engine or game change.

## Cycle log
- Cycle 1: 2026-10-28 started, base f48dadc
