## ISSUE-001: Heavy rain lights the whole room
Status: resolved
Severity: major
Source: review
Found in cycle: 1
Reopened: 0

### Problem
Decision "Indoor lighting follows the world clock": the room sits in shade, with daylight only through the windows. But `sunLit` and the related shadow terms in `engine/shaders/common.glsl` (lines ~100–105 and ~141–142) fade every cast shadow toward fully lit as `uRain` rises (`mix(lit, 1.0, smoothstep(0.15, 0.95, uRain))`). The tavern's roof and near-wall shadows are cast shadows, so in a downpour the whole interior shows as lit, as bright as the overcast lawn. The handoff confirms this ("Indoors in a downpour the room is lit evenly").

Required behaviour: indoors (`Scene::indoors`), rain never fades the room's shade toward lit. Outdoors, rain removes direct sunlight, so indoors the window light patches should fade toward shade as the rain gets heavier, and the rest of the room stays in shade at any rain level. Lightning's flash lighting through the windows is unchanged. Outdoor behaviour must be unchanged, so captures with no tavern in view stay byte-identical.

### Fix notes
Root cause: `sunLit` in `engine/shaders/common.glsl` treats every cast shadow as the sun's and fades it toward lit as rain rises (`mix(lit, 1.0, smoothstep(0.15, 0.95, uRain))`), because outdoors rain cloud removes the direct light that casts shadows. Indoors the shade is the room's own shadow-only roof and walls, so the same rule lifted the whole room to lit in a downpour.

Changes:
- `engine/shaders/common.glsl`: new `uniform int uIndoors`. `sunLit` now returns `lit * (1 - fade)` indoors and `mix(lit, 1.0, fade)` outdoors (the old expression, unchanged), with `fade = smoothstep(0.15, 0.95, uRain)`. So indoors the window patches fade toward shade as rain rises and are gone in a downpour, and shaded surfaces stay shaded at every rain level. `flashLit` is untouched, so lightning still lights the room through the windows with hard shadows.
- `engine/src/renderer.cpp`: `bindToon` takes `scene.indoors` and sets `uIndoors` for the mesh, grass and water programs.
- `engine/shaders/edge.frag` and `renderer.cpp`: the same flaw existed in the edge pass's point-light dimming (fires and fireflies are drowned out by direct sun via `(1 - uRain)`). Indoors, uRain is forced to 0 there to suppress rain streaks, so a rained-out window patch still dimmed nearby fire light as if it were sunlit. New `uSunRain` (always the real rain) replaces uRain in that term. Outdoors it equals uRain.

Verified: with the tavern moved out of view, the default, rain 0.5, rain 1, rain 1 + flash, night + rain 0.7, and candles + shelter test + rain 0.6 captures are byte-identical with and without these shader changes. In `--scene tavern --hour 15`, the window patches are full dry, faint at rain 0.5 and gone at rain 1, and the rest of the room is unchanged apart from the overcast dimming (captures listed in handoff.md, Cycle 2).

