## game-developer cycle 1
- tool: re-edit/rebuild/capture loop for the clouds-a/b/off captures was done by hand (sed insert into examples/minimal/main.cpp, tools/build.sh, pxe_minimal --first-person --capture, git checkout). A first attempt gave identical a and b (the second sed + immediate rebuild apparently not picked up); redoing it with a fresh insert fixed it. A script that applies a temporary line, builds, captures and restores would help.

## orchestrator cycle 1
- none new: the temporary-edit/rebuild/capture loop is the developer's item above.
