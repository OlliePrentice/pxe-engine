## game-developer cycle 1
- instruction-gap: Verification 4 names `--hour 18`, where the desert's sun is exactly on the horizon (lighting.cpp `sunDirection`, y = 0), so neither feature can show. Found by comparing md5s and then testing other hours. Reported as spec infeasible for that step only.
- tool: tools/captures.sh --edit run twice back to back (edit A, restore, edit B) built edit B's capture from a stale world.cpp.o once (terminator.png equalled ripples.png). A `sleep 2` between runs fixed it (cause not confirmed, probably timestamp granularity after the restore's touch). Script wrapping the three runs: docs/tasks/perspective-surface-detail/scripts/run-captures.sh.
- doc-gap: `bindLit` had no access to the camera eye; I added a parameter. Found by reading renderer_perspective.cpp; the spec said "camera.eye" without noting the signature change.

## orchestrator cycle 2
- tool: confirmed the stale build after `tools/captures.sh --edit`: the build uses Unix Makefiles (macOS make, 1-second mtime resolution) and a run finishes in under a second, so the restore's touch lands in the same second as the object file and the next build skips the file. The developer's `sleep 2` between runs does not cover it (my off.png came out identical to terminator.png); a 1 s sleep before the touch did. Queued as fix task captures-edit-stale-build.
- instruction-gap: `--hour 18` is the exact sunset in `sunDirection` (y = 0), so no direct light at all; specs using it as "golden hour" show no sun-lit effect.
