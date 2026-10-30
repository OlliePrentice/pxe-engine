## game-developer cycle 1
- instruction-gap: the spec's `"Sun##c"` label with "the IDs stay the same" can't both hold in ImGui (only `###` resets the hash; checked in imgui.cpp's ImHashStr). I used `###` ids and noted it in the handoff.
- tool: `docs/tasks/desert-lighting/scripts/png-diff.py`: mean per-channel RGB difference over the lower ground area of two PNGs, to describe small capture changes in numbers. Needs PIL.
- doc-gap: `tools/README.md`/the check scripts don't say that the base build kept under `$TMPDIR/pxe-render-check/<sha>/build/release/` can be reused to run the base binary by hand; I found it from perspective-check.sh's source (line 62).

## orchestrator cycle 1
- instruction-gap (planning): a spec that names ImGui widget IDs and says an ID survives a label change must use `###`, not `##`. For /feature's spec-writing guidance.
