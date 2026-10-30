## game-developer cycle 1
- tool: `scripts/tune_phases.py` mirrors `duneHeight` in Python (no numpy on the machine) and grid-searches the four `kRidges` phases against the three `--terrain-report` constraints; `scripts/gradient_check.cpp` compares `duneGradient` with a finite difference. A "phase tuner" is likely to recur for any dune change.
- doc-gap: the spec says `duneGradient` is "file-local" and also "declared in world.hpp"; I declared it in the header (main.cpp needs it).
- doc-gap: `tools/captures.sh` adds `--capture` itself; I first put it in the cases file. `--help` says so, I read it too late.
- doc-gap: "windward" in the spec's palm constraint means the numeric test (gradient dot toward-spawn <= 0), which with wind toward -z is the dune's steep downwind face seen from the spawn, not a windward slope. I followed the numeric test.

## orchestrator cycle 1
- instruction-gap: the spec fixed a capture at `--hour 18` as "golden" and called the palm's face "windward", both contradicted by the code (the engine's phase weights, the wind direction). /feature specs that name an hour's look or a slope's side should check them against the code (`lightingAt`'s phase weights, the ridge heading).
