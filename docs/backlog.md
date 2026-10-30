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

## render-mode-checks: Byte-identical checks per render mode
Status: done
Type: tooling
Spec: docs/tasks/render-mode-checks/task.md
Depends on: none
Touches: tools/ortho-check.sh, tools/perspective-check.sh, tools/README.md, CLAUDE.md
Assets: no
Merged: 02ecc72

## render-mode-split: Orthographic and perspective as separate render modes
Status: done
Type: alignment
Spec: docs/tasks/render-mode-split/task.md
Depends on: render-mode-checks
Touches: engine/include/pxe/renderer.hpp, engine/include/pxe/camera.hpp, engine/include/pxe/first_person_camera.hpp, engine/src/renderer.cpp, engine/src/renderer_orthographic.cpp, engine/src/renderer_perspective.cpp, engine/src/renderer_internal.hpp, engine/src/camera.cpp, engine/src/first_person_camera.cpp, engine/shaders/edge.frag, engine/shaders/edge_perspective.frag, engine/CMakeLists.txt, game/src/main.cpp, desert/src/main.cpp, examples/minimal/main.cpp
Assets: no
Merged: cab03e7

## desert-arms: The desert player's arms
Status: done
Type: feature
Spec: docs/tasks/desert-arms/task.md
Depends on: render-mode-split
Touches: engine/include/pxe/renderer.hpp, engine/src/renderer_perspective.cpp, engine/src/gl.hpp, desert/src/arms.hpp, desert/src/arms.cpp, desert/src/main.cpp, desert/src/player.hpp, desert/CMakeLists.txt, desert/assets/characters/, desert/tools/blender/build_arms.py, desert/docs/art-style.md
Assets: yes
Merged: 89c5126

## view-model-edges: Edges for the view model
Status: done
Type: feature
Spec: docs/tasks/view-model-edges/task.md
Depends on: none
Touches: engine/shaders/edge_perspective.frag, engine/src/renderer_perspective.cpp, docs/tasks/view-model-edges/
Assets: no
Merged: d64d2de

## desert-arms-detail: Detailed arms and gadgets
Status: done
Type: feature
Spec: docs/tasks/desert-arms-detail/task.md
Depends on: view-model-edges
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/src/arms.hpp, desert/src/arms.cpp, desert/docs/art-style.md, docs/tasks/desert-arms-detail/
Assets: yes
Merged: b9c8d7a

## desert-arms-walk: The arms walk with the player
Status: done
Type: feature
Spec: docs/tasks/desert-arms-walk/task.md
Depends on: desert-arms-detail
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/src/arms.hpp, desert/src/arms.cpp, desert/src/player.hpp, desert/src/player.cpp, desert/src/main.cpp, desert/docs/art-style.md, docs/tasks/desert-arms-walk/
Assets: yes
Merged: bc94943

## glb-tool: A shared glb reader and checker
Status: done
Type: tooling
Spec: docs/tasks/glb-tool/task.md
Depends on: none
Touches: tools/glb.py, tools/README.md, docs/tasks/glb-tool/
Assets: no
Merged: 0476111

## gitignore-pycache: Ignore Python bytecode caches
Status: done
Type: fix
Spec: docs/tasks/gitignore-pycache/task.md
Depends on: none
Touches: .gitignore, docs/tasks/gitignore-pycache/
Assets: no
Merged: 8c45110

## desert-arms-revision: Arms: hands held with intent, an LCD gauge, quieter stitching
Status: done
Type: feature
Spec: docs/tasks/desert-arms-revision/task.md
Depends on: none
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/docs/art-style.md, desert/src/arms.hpp, desert/src/arms.cpp, docs/tasks/desert-arms-revision/
Assets: yes
Merged: 451ab12

## desert-settings-menu: The desert's settings menu
Status: done
Type: feature
Spec: docs/tasks/desert-settings-menu/task.md
Depends on: none
Touches: desert/src/settings.hpp, desert/src/settings.cpp, desert/src/main.cpp, desert/src/player.hpp, desert/src/player.cpp, desert/src/arms.hpp, desert/src/arms.cpp, desert/CMakeLists.txt
Assets: no
Merged: 60b20f7

## perspective-smooth-lighting: Smooth lighting in perspective mode
Status: done
Type: feature
Spec: docs/tasks/perspective-smooth-lighting/task.md
Depends on: none
Touches: engine/include/pxe/scene.hpp, engine/src/scene.cpp, engine/include/pxe/lighting.hpp, engine/src/lighting.cpp, engine/include/pxe/renderer.hpp, engine/src/renderer.cpp, engine/src/renderer_internal.hpp, engine/src/renderer_perspective.cpp, engine/include/pxe/first_person_camera.hpp, engine/shaders/lit_perspective.glsl, engine/shaders/mesh_perspective.frag, engine/shaders/backdrop.frag, engine/shaders/edge_perspective.frag, engine/CMakeLists.txt, docs/tasks/perspective-smooth-lighting/
Assets: no
Merged: 597ddce

## desert-lighting: The desert's light and its menu controls
Status: done
Type: feature
Spec: docs/tasks/desert-lighting/task.md
Depends on: perspective-smooth-lighting, desert-settings-menu
Touches: desert/src/world.cpp, desert/src/settings.hpp, desert/src/settings.cpp, desert/src/main.cpp, desert/docs/art-style.md, docs/tasks/desert-lighting/
Assets: no
Merged: a373b26

## perspective-sky-clouds: Sky clouds in perspective mode
Status: done
Type: feature
Spec: docs/tasks/perspective-sky-clouds/task.md
Depends on: perspective-smooth-lighting
Touches: engine/include/pxe/scene.hpp, engine/src/scene.cpp, engine/include/pxe/lighting.hpp, engine/src/lighting.cpp, engine/src/renderer_perspective.cpp, engine/shaders/backdrop.frag, docs/tasks/perspective-sky-clouds/
Assets: no
Merged: 3eaf280

## capture-tool: A capture runner: sets of captures, twice, against a base, under a temporary edit
Status: done
Type: tooling
Spec: docs/tasks/capture-tool/task.md
Depends on: none
Touches: tools/captures.sh, tools/README.md, docs/tasks/capture-tool/
Assets: no
Merged: 96fbfa8

## blender-override-tool: Run a Blender build script with constants overridden
Status: done
Type: tooling
Spec: docs/tasks/blender-override-tool/task.md
Depends on: none
Touches: tools/blender-override.py, tools/README.md, docs/tasks/blender-override-tool/
Assets: no
Merged: 087a0f1

## captures-edit-stale-build: captures.sh rebuilds the edited file after its edit and restore
Status: done
Type: fix
Spec: docs/tasks/captures-edit-stale-build/task.md
Depends on: none
Touches: tools/captures.sh, tools/README.md, docs/tasks/captures-edit-stale-build/
Assets: no
Merged: b36de25

## png-diff-tool: A PNG difference reporter
Status: queued
Type: tooling
Spec: docs/tasks/png-diff-tool/task.md
Depends on: none
Touches: tools/png-diff.py, tools/README.md, docs/tasks/png-diff-tool/
Assets: no

## perspective-dust: Airborne dust in perspective mode
Status: done
Type: feature
Spec: docs/tasks/perspective-dust/task.md
Depends on: perspective-sky-clouds
Touches: engine/include/pxe/scene.hpp, engine/include/pxe/renderer.hpp, engine/src/renderer.cpp, engine/src/renderer_perspective.cpp, engine/shaders/dust.vert, engine/shaders/dust.frag, engine/CMakeLists.txt, docs/tasks/perspective-dust/
Assets: no
Merged: 135a6e3

## desert-sky: The desert's sky clouds and dust, and their menu controls
Status: done
Type: feature
Spec: docs/tasks/desert-sky/task.md
Depends on: perspective-dust, desert-lighting
Touches: desert/src/world.cpp, desert/src/settings.hpp, desert/src/settings.cpp, docs/tasks/desert-sky/
Assets: no
Merged: 73ba6c1

## desert-arms-pose: Arms: pointing forward, closer to the body, chunkier gloves
Status: blocked
Type: feature
Spec: docs/tasks/desert-arms-pose/task.md
Depends on: desert-lighting
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, docs/tasks/desert-arms-pose/
Assets: yes
Questions:
All four come from replan-1.md (docs/tasks/desert-arms-pose/replan-1.md, on branch desert-arms-pose). The designer implemented the spec (scripts/build_arms-pose.patch) and ran the build's full checks (scripts/pose_trials.py; reproduced by the orchestrator): four binding checks fail at the agreed values, and the spec's fallbacks can't fix them. Each answer changes `## Decisions` (and Approach, Assets, criteria, Out of scope as replan-1.md lists).
- Compass: at the new wrist, 66–72 of the compass face's 216 vertices are below the bottom edge, even in the planning trial's own pose. Options: (a) slide it toward the hand along the cuff, (b) a different mount angle, (c) keep the wrist higher than −0.330. I'd pick (a).
- Sprint: Arms_Sprint's top reaches NDC y −0.175 to −0.238 over the allowed wrist z (limit −0.30), so it needs a wrist z of about −0.185. Options: (a) lower the wrist that far (undoes "a little higher"), (b) lower the sprint's lift/bob now, ahead of desert-arms-motion, (c) relax the limit for the sprint until desert-arms-motion. I'd pick (b).
- Palm vs cuff: the 15% bulkier palm's wrist ring pokes out of the cuff at every cuff bulk up to 1.20. Options: (a) spare the palm's wrist ring from the bulk, so the glove tapers into the cuff, (b) lengthen the cuff forward. I'd pick (a).
- Gauge vs cuff: the 1.10 cuff hides readout dots at every gauge position in the search. With the cuff at 1.0 the gauge passes at GAUGE_S −0.0385, GAUGE_TURN 0.55. Options: (a) CUFF_BULK 1.0 (fine if the palm answer is (a)), (b) more GAUGE_TURN steps at −0.0485, (c) another placement. Also, past −0.0535 the toggles fall off the bracer plate: extend the plate's wrist end, or shift the whole bracer? I'd pick (a), and extend the plate. Also accept the 12 extra triangles from the stitches following the bulk (the spec says none)? I'd accept them.
- Whatever the answers, rerun pose_trials.py against them and require every check to pass before the spec is revised.

## desert-arms-gear: Arms: gear and palette after the concept art
Status: queued
Type: feature
Spec: docs/tasks/desert-arms-gear/task.md
Depends on: desert-arms-pose
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/docs/art-style.md, desert/src/arms.hpp, desert/src/arms.cpp, docs/tasks/desert-arms-gear/
Assets: yes

## desert-arms-motion: Arms: a natural walk and a pumping sprint
Status: queued
Type: feature
Spec: docs/tasks/desert-arms-motion/task.md
Depends on: desert-arms-gear
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/docs/art-style.md, docs/tasks/desert-arms-motion/
Assets: yes

## perspective-surface-detail: Terminator sharpness and surface ripples in perspective mode
Status: done
Type: feature
Spec: docs/tasks/perspective-surface-detail/task.md
Depends on: perspective-dust
Touches: engine/include/pxe/scene.hpp, engine/shaders/lit_perspective.glsl, engine/shaders/mesh_perspective.frag, engine/src/renderer_perspective.cpp, docs/tasks/perspective-surface-detail/
Assets: no
Merged: 240cb52

## desert-ground: The desert's ground after the concept art
Status: done
Type: feature
Spec: docs/tasks/desert-ground/task.md
Depends on: perspective-surface-detail, desert-sky
Touches: desert/src/world.hpp, desert/src/world.cpp, desert/src/main.cpp, desert/src/settings.cpp, desert/docs/art-style.md, docs/tasks/desert-ground/
Assets: no
Merged: f7fc8c9
