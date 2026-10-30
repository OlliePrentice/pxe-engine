## game-developer cycle 1
- instruction-gap: Verification 2 writes the replacement as `"...;\n<anchor>"`; in bash that passes a literal backslash-n, which would not compile. I used real newlines (`$'\n'`) in scripts/verify2.sh.
- tool: scripts/verify2.sh runs the back-to-back edit/base/edit sequence for N rounds and checks the cmps (task-specific; not worth a shared tool).
- doc-gap: no way to know the failure is timing-dependent until trying; the first run (which builds everything) always passes, so the repro needs the base build to exist first. Worked out by running more rounds.
