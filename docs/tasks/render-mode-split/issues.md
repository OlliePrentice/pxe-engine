# render-mode-split issues

## ISSUE-001: Python bytecode committed in the task's scripts
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Commit 0d8a3a5 adds `docs/tasks/render-mode-split/scripts/__pycache__/shaderedit.cpython-314.pyc`, a generated binary. Only the scripts themselves belong in `scripts/`. Remove the `__pycache__` directory from the branch with `git rm -r`. Don't change `.gitignore`: it isn't in this task's Touches.

### Fix notes
