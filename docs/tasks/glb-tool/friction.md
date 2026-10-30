## game-developer cycle 1
- doc-gap: needed to know how many bytes the `.glb` JSON chunk is padded by when locating the bin chunk's data in a raw file (Verification 7). Worked out from the glTF spec: chunks are 4-byte aligned, so the bin data starts at 20 + pad4(json length) + 8; the existing copies used `20 + clen + 8` without padding, which only works when the exporter's chunk length is already padded.
- instruction-gap: the `info` sketch lists animations alphabetically while `info` is told to print "in file order"; I kept file order (the sketch is non-binding).
- tool: docs/tasks/glb-tool/scripts/synthetic_accessors.py: checks `glb.accessor` on hand-made MAT/stride data no current asset exercises.

## orchestrator cycle 1
- doc-gap: importing a Python module from `tools/` writes `tools/__pycache__/`, which `.gitignore` doesn't cover, so the tree goes dirty (ISSUE-001; `gitignore-pycache` queued).
- instruction-gap: the spec's `info` output sketch listed the animations in a different order from the text's "in file order". A sketch in a spec should match the rule it illustrates.
