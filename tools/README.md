# Tools
Scripts for building, running and checking pxe, and for running the agent workflow. Check here before writing a script or a long command sequence by hand. If nothing fits, write it in the task's `scripts/` folder and record it in the task's `friction.md`: once the same job has been done by hand three times, the retro turns it into a tool here.

| Tool | What it does |
| :- | :- |
| `tools/build.sh [debug\|release] [game\|example] [-- args]` | Configures if needed, builds the engine, game and example with the CMake presets, and optionally runs one of them. `--help` for details. |
| `tools/backlog-loop.sh [--max-batches n] [--budget usd]` | Runs `/backlog-run` in a loop, one fresh headless Claude session per batch, until the backlog has nothing left to run. Logs go to `.backlog-runs/`. `--help` for details. |
| `tools/blender/build_<asset>.py` | Rebuilds each asset in Blender. Written and run by the designer. |
