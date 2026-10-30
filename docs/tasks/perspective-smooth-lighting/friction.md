## game-developer cycle 1
- tool: scripts/capture-all.sh renders the task's seven captures into the task folder, each twice (second from a scratch dir), and cmp's them. At docs/tasks/perspective-smooth-lighting/scripts/capture-all.sh. A "capture each case twice and cmp" tool would serve any shader task.
- instruction-gap: Spec §2's `sunDir = sun` contradicts the binding field comment ("whichever of sun and moon lights the scene"). I followed the code and reported it in the handoff.
- instruction-gap: Spec §4's `bindLit(prog, lighting, scene, light, flash, normalBias, testColours)` has no way to reach `params.actorMaterials`, which it also names. I added a `params` argument.
- doc-gap: the sorted order of PXE_SHADERS (`mesh.vert` before `mesh_perspective.frag`, because '.' sorts before '_') was not stated. Worked out by sorting.
