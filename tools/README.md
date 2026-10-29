# Tools
Scripts for building, running and checking pxe, and for running the agent workflow. Check here before writing a script or a long command sequence by hand. If nothing fits, write it in the task's `scripts/` folder and record it in the task's `friction.md`: once the same job has been done by hand three times, the retro turns it into a tool here.

| Tool | What it does |
| :- | :- |
| `tools/build.sh [debug\|release] [game\|desert\|example] [-- args]` | Configures if needed, builds the engine, both games and the example with the CMake presets, and optionally runs one of them. `--help` for details. |
| `tools/ortho-check.sh <base-ref>` | Compares the orthographic mode's standard cases (the game's captures and self-test, the example's `--capture`) byte for byte between a base commit and the working tree. Builds the base once per commit in `${TMPDIR:-/tmp}/pxe-render-check/<sha>/` and prints that directory; outputs go to `${TMPDIR:-/tmp}/pxe-render-check-run.XXXXXX`. Exit 0 all identical, 1 any differs, 2 bad usage. `--help` lists the cases. |
| `tools/perspective-check.sh <base-ref>` | The same for the perspective mode's standard cases (the desert's `--capture` and `--hour 21 --capture`, the example's `--first-person --capture`), with the same base build and output directories. |
| `tools/backlog-loop.sh [--max-batches n] [--budget usd]` | Runs `/backlog-run` in a loop, one fresh headless Claude session per batch, until the backlog has nothing left to run. Logs go to `.backlog-runs/`. `--help` for details. |
| `tools/blender/build_<asset>.py` | Rebuilds each asset in Blender. Written and run by the designer. |
