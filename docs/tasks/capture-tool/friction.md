## game-developer cycle 1
- instruction-gap: Verification 7 (`start in the background, kill -INT`) cannot trigger a trap from a non-interactive shell (background jobs start with SIGINT ignored; bash can't trap ignored-on-entry signals). Worked around with docs/tasks/capture-tool/scripts/interrupt-test.py (python subprocess, default signal disposition). Future specs for signal-handling checks should say to launch the process that way.
- tool: docs/tasks/capture-tool/scripts/interrupt-test.py starts a command, signals it after its `edit:` line and checks the file is restored; only useful for testing captures.sh.
- doc-gap: none otherwise.
