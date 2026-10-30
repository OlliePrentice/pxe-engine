# Replan 1: Verification 4's capture hour

## What failed
Verification 4 captures the temporary edits at `--hour 18`. At that hour `sunDirection` (`engine/src/lighting.cpp`) gives `h = (18 - 12) / 12 * pi = pi/2`, so `cos(h) = 0` and the sun's y is 0: the sun sits exactly on the horizon. `litColour`'s direct term is guarded by `uSunDir.y > 0.0`, so it is skipped, and the terminator (which only acts on the direct term) has nothing to act on. Ripples only move the sky-fill term, by about 0.003, under one 8-bit step. The developer measured edit A, edit B and the base at `--hour 18`: all three captures are byte-identical (md5 1f916c80...). The orchestrator confirmed the arithmetic from the code. The shaders work: with edit A, captures at hours 12, 15, 16.5, 17 and 17.5 differ from the base.

## Alternatives
- Change the engine's sun so hour 18 is above the horizon: changes every perspective and orthographic capture and the agreed day arc. Rejected: out of scope, changes existing output.
- Drop the `uSunDir.y > 0` guard: changes output at and after sunset. Rejected for the same reason.
- Capture at `--hour 17.5`, the latest half hour with the sun up and in the golden phase. Keeps every decision and criterion; only the verification hour changes.

## Proposal
Verification 4 uses `--hour 17.5` for `ripples.png`, `terminator.png`, `off.png` and the base's capture. Nothing else changes.

## Spec changes
- `## Verification` §4: `pxe_desert --hour 18 --capture ripples.png` becomes `pxe_desert --hour 17.5 --capture ripples.png`; "`off.png` equals the base's `pxe_desert --hour 18 --capture`" becomes `--hour 17.5`. `terminator.png` and `off.png` are captured at the same hour.
- `## Approach`: unchanged.
