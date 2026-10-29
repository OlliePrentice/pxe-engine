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
