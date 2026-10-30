## game-developer cycle 1
- tool: scripts/walk-compare.sh: compares the three --walk captures between the base build perspective-check.sh keeps in $TMPDIR/pxe-render-check/<sha> and the worktree release build. Needs the full sha. Wrote it by hand because the spec's step 3 has no tool.
- instruction-gap: the spec's grep for "ImGui" in desert/src would also match comments in the binding settings.hpp ("Dear ImGui panel") and the main.cpp sketch comment; I reworded the comments so the grep passes.
