# Issues

## ISSUE-001: Engine header included with angle brackets
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
game/src/audio.cpp:3 has `#include <pxe/audio_device.hpp>`. Every other engine header include in game/ and engine/ uses quotes (`#include "pxe/..."`, 21 in game/). Match the project's style.

### Fix notes
