# render-mode-split issues

## ISSUE-001: Python bytecode committed in the task's scripts
Status: fixed
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Commit 0d8a3a5 adds `docs/tasks/render-mode-split/scripts/__pycache__/shaderedit.cpython-314.pyc`, a generated binary. Only the scripts themselves belong in `scripts/`. Remove the `__pycache__` directory from the branch with `git rm -r`. Don't change `.gitignore`: it isn't in this task's Touches.

### Fix notes
Root cause: cycle 1 committed the `__pycache__` that running `shaderedit.py` as an imported module generated. Removed it with `git rm -r docs/tasks/render-mode-split/scripts/__pycache__`. `.gitignore` untouched.
