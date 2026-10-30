## designer 2026-10-30
- instruction-gap: the orchestrator asked for a "Revision: 3" line, but `docs/tasks/desert-arms-walk/assets.md` already records Revision 3 for the arms. I numbered this one 4 and noted it in the report.
- instruction-gap: the binding framing check says "facing ≥ FACE_COS" and "at least LCD_MIN_PX"; the build fails strictly below each (`facing < FACE_COS`, `px < LCD_MIN_PX`), which is what the wording says. No other ambiguity.
- tool: the negative runs for the new checks were done by `sed`-editing a scratch copy of build_arms.py and running it with `PXE_SKIP_EXPORT=1`. Nothing shared exists for "run a build script with one constant overridden"; it's a one-liner each time, so no helper written.
## game-developer cycle 1
- none
