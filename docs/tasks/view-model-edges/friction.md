## game-developer cycle 1
- doc-gap: the base binary directory for the Verification 6 captures is only printed in the `base:` line of `tools/perspective-check.sh` output (and its help says `${TMPDIR}/pxe-render-check/<full sha>/build/release`); I took it from there. The spec could name the pattern.
- tool: `docs/tasks/view-model-edges/scripts/diff_box.py` (spec-named): differing-pixel count and bounding box of two PNGs. I also made zoomed crops of the arms with an inline PIL snippet (not saved as a tool) to inspect the captures.
## game-developer cycle 2
- none
## run (orchestrator)
- doc-gap: I had to read `edge_perspective.frag` to see that the inner outlines the spec asked for were already drawn in the base by the actor crease ink (same `min(mix(col, uInk, 0.65), col * 0.8)`), so the captures' pixel diff shows almost only the outer silhouette. Nothing tells a spec writer to diff the expected look against the base capture before promising a change.
- tool: built zoomed crops and a differing-pixel mask of base vs new captures with an inline PIL snippet (the developer did the same for crops).

