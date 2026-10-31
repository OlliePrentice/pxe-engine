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
Status: done
Type: tooling
Spec: docs/tasks/png-diff-tool/task.md
Depends on: none
Touches: tools/png-diff.py, tools/README.md, docs/tasks/png-diff-tool/
Assets: no
Merged: 881493b

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
Status: done
Type: feature
Spec: docs/tasks/desert-arms-pose/task.md
Depends on: desert-lighting
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, docs/tasks/desert-arms-pose/
Assets: yes
Merged: 7ffff71

## desert-arms-gear: Arms: gear and palette after the concept art
Status: done
Type: feature
Spec: docs/tasks/desert-arms-gear/task.md
Depends on: desert-arms-pose
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/docs/art-style.md, desert/src/arms.hpp, desert/src/arms.cpp, docs/tasks/desert-arms-gear/
Assets: yes
Merged: 255a676

## desert-arms-motion: Arms: a natural walk and a pumping sprint
Status: blocked
Type: feature
Spec: docs/tasks/desert-arms-motion/task.md
Depends on: desert-arms-gear
Touches: desert/tools/blender/build_arms.py, desert/assets/characters/, desert/docs/art-style.md, docs/tasks/desert-arms-motion/
Assets: yes
Questions:
- The sprint as specified can't be built. Its forward-peak wrist target (+50 mm forward, +18 mm up) is 46.7 mm beyond what the arm reaches with the elbow closed the specified 26.8°, and no fallback k passes (the fallback doesn't scale the swing). The walk is built and passes at k 1.0 (branch desert-arms-motion, db6e434). Evidence and options are in docs/tasks/desert-arms-motion/replan-1.md. Choose: (a) keep the forward swing and limit the front elbow close to about 13° (a weaker "bent" read); (b) keep a ~27° pump and drive the forward peak up toward the chest instead of out (new sprint numbers, chosen with scripts/sprint_reach.py so every frame keeps 10 mm of reach margin); (c) let the fallback scale the swing and draw-back as well. I'd pick (b).
- Closing both elbows 18° or more pushes the end of the right tube (R.tube.1) out of its sleeve, so any sprint with the elbow past about 17° needs a gear change, which this task rules out. Should the right tube be revised so it stays sleeved up to about 30° closed (the mesh changes, so "rest pose unchanged" and the perspective-identical checks would exclude the tube)? Or should the sprint's elbow stay at 17° or less? I'd revise the tube; (b) needs it.
- The desert-arms-walk check ELBOW_SPAN (elbows closed at most 10° in clips) contradicts the sprint's 15° hold. The designer dropped it for clips, since the per-clip attachment check at 8 phases covers each clip's range. Confirm, or name the limit that replaces it. I'd drop it.

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
