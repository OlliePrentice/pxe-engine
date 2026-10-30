## game-developer cycle 1
- doc-gap: the spec says `build_ruins.py` has no `main()`; it has one, called unguarded at line 722, so exec'ing it runs the whole build and rewrites assets/scenes/ruins.glb. Found by running it; the tool now checks the AST for a `__name__` guard first.
- tool: none
- instruction-gap: none
