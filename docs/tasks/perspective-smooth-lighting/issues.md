# Issues

## ISSUE-001: Lighting::sunDir's comment says it follows the moon
Status: open
Severity: minor
Source: review
Found in cycle: 1
Reopened: 0

### Problem
`engine/include/pxe/lighting.hpp`, the comment above `sunDir` says "Toward the sun, whichever of sun and moon lights the scene", but `lightingAt` (`engine/src/lighting.cpp`) sets `out.sunDir = sun`, as spec §2 and Decisions ("The moon gets no disc") require. The code is right; the public comment is wrong and will mislead later perspective tasks (perspective-sky-clouds, desert-lighting). Reword the comment to say what the field is: toward the sun, even when it is below the horizon (the moon gets no disc or glow; the glow fades as the sun sets), with its display colour blended by phase. Keep the field, its type and its default as they are. No other change.

### Fix notes
