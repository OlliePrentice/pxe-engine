# Issues

## ISSUE-001: Importing the module leaves `tools/__pycache__/` untracked in the tree
Status: wont-fix
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Every `import glb` from `tools/` (Verification 7's snippet, Verification 8 in Blender, any build script that uses the module) writes `tools/__pycache__/glb.cpython-*.pyc`. `.gitignore` has no `__pycache__/` entry, so `git status --porcelain` shows `?? tools/__pycache__/` afterwards. A dirty main working tree stops `/backlog-run` at preflight, and an agent could commit the bytecode by mistake.

### Fix notes
Won't fix in this task: the fix is a `.gitignore` entry, and this task's spec allows changes only to `tools/glb.py`, `tools/README.md` and its task folder (acceptance criterion and Verification 9). The module can't stop its own import from writing bytecode. Queued as `gitignore-pycache` (Type: fix), which adds `__pycache__/` to `.gitignore`. The stray directory from this review was deleted by hand.
