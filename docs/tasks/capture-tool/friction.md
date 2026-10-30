## game-developer cycle 1
- instruction-gap: Verification 7 (`start in the background, kill -INT`) cannot trigger a trap from a non-interactive shell (background jobs start with SIGINT ignored; bash can't trap ignored-on-entry signals). Worked around with docs/tasks/capture-tool/scripts/interrupt-test.py (python subprocess, default signal disposition). Future specs for signal-handling checks should say to launch the process that way.
- tool: docs/tasks/capture-tool/scripts/interrupt-test.py starts a command, signals it after its `edit:` line and checks the file is restored; only useful for testing captures.sh.
- doc-gap: none otherwise.

## orchestrator cycle 1
- instruction-gap: Verification 7 ("start in the background, kill -INT it") can't work as written from a non-interactive shell: background jobs start with SIGINT ignored and bash can't trap a signal ignored on entry. Specs that test signal handling should send the signal to a child started with the default disposition (e.g. python's subprocess), as scripts/interrupt-test.py does.
- tool: parallel tasks running tools/*-check.sh against the same base commit race on ${TMPDIR}/pxe-render-check/<sha> (each rm -rf's and rebuilds it if the binary is missing). The orchestrator pre-built the base before delegating.
