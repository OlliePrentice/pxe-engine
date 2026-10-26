# Issues

## ISSUE-001: Engine header included with angle brackets
Status: resolved
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
game/src/audio.cpp:3 has `#include <pxe/audio_device.hpp>`. Every other engine header include in game/ and engine/ uses quotes (`#include "pxe/..."`, 21 in game/). Match the project's style.

### Fix notes
Root cause: cycle 1 wrote the new engine include in angle-bracket form instead of the project's quoted style. game/src/audio.cpp:3 now reads `#include "pxe/audio_device.hpp"`. No other angle-bracket `pxe/` includes remain in game/ or engine/ (grep).
