## game-developer cycle 1
- doc-gap: needed the engine defaults for `skyClouds[1]` (height/length/width/opacity); the engine-guide did not list them, found at engine/src/scene.cpp:156-161.
- instruction-gap: the spec's verification asks for the sun disc and glints in the captures, but the spawn view never contains the sun at any of the listed hours; I described what is visible and noted it.

## orchestrator cycle 1
- instruction-gap: the spec asked captures to show the sun disc and glints, but the desert spawn view looks +z and never faces the sun at the listed hours; specs asking for sun-facing effects need a capture pose facing the sun (pxe_desert has no yaw flag).
