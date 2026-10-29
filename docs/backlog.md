# Backlog
The queue of planned tasks. `/feature` adds tasks with their specs. `/backlog-run` (usually through `tools/backlog-loop.sh`) runs them in order, in parallel where they're independent, and keeps each entry's status current. Only those two skills edit this file.

Statuses:
- `queued`: specced, waiting for its turn or its dependencies.
- `in-progress`: running in its worktree `../<repo-dir>-<task-id>` on branch `<task-id>`. A batch that was cut short resumes it.
- `blocked`: needs the user. Its questions are listed under it. Answer them with `/feature unblock`.
- `done`: merged into the run's branch.

Entry format:
```
## <task-id>: <title>
Status: queued | in-progress | blocked | done
Type: feature | fix | alignment | tooling
Spec: docs/tasks/<task-id>/task.md
Depends on: <task-ids> | none
Touches: <files and directories it is expected to change>
Assets: yes | no
Merged: <merge commit, once done>
Questions:
- <only while blocked>
```

Tasks run in the order listed. Add new ones at the bottom, except tooling tasks from a retro, which go at the top of the queued tasks.

# Tasks

## render-mode-checks: Byte-identical checks per render mode
Status: queued
Type: tooling
Spec: docs/tasks/render-mode-checks/task.md
Depends on: none
Touches: tools/ortho-check.sh, tools/perspective-check.sh, tools/README.md, CLAUDE.md
Assets: no

## render-mode-split: Orthographic and perspective as separate render modes
Status: queued
Type: alignment
Spec: docs/tasks/render-mode-split/task.md
Depends on: render-mode-checks
Touches: engine/include/pxe/renderer.hpp, engine/include/pxe/camera.hpp, engine/include/pxe/first_person_camera.hpp, engine/src/renderer.cpp, engine/src/renderer_orthographic.cpp, engine/src/renderer_perspective.cpp, engine/src/renderer_internal.hpp, engine/src/camera.cpp, engine/src/first_person_camera.cpp, engine/shaders/edge.frag, engine/shaders/edge_perspective.frag, engine/CMakeLists.txt, game/src/main.cpp, desert/src/main.cpp, examples/minimal/main.cpp
Assets: no
