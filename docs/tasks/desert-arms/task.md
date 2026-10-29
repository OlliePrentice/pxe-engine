# The desert player's arms
Type: feature
Status: queued
Cycle: 0

## Goal
The desert player is only an eye. Give it a pair of arms, seen in first person and themed for a sci-fi desert: the wanderer's wrapped forearms, dark leather gloves, and a brass bracer with a glowing teal gauge on the left forearm. They aren't animated in this task. They're a rigged model held in its rest pose, locked to the view.

Arms hang about 0.45 m in front of the eye, but the eye only keeps 0.3 m from the palm's trunk. So the engine gains a general capability, a **view model**: characters attached to the view and drawn in front of the whole scene, so nothing in the world ever cuts or hides them. It is perspective mode only.

## Decisions
All from the planning interview with the user, 2026-10-29.
- **Concept: wanderer wraps** (Q1, Tatooine scavenger):
  - pale, sun-bleached cloth wrapped round each forearm in stepped bands;
  - dark leather gloves, full gloves with no skin showing (Q8);
  - on the left forearm, a worn brass bracer held by two leather straps;
  - on the bracer, a small gauge glowing teal.
- **Pose and framing** (Q2):
  - Both forearms rise from the bottom corners of the view.
  - The hands are relaxed, fingers slightly curled, as if walking.
  - Each hand is centred about a quarter of the view's width in from its side.
  - The knuckles come about 20% up from the bottom edge, and the forearms run out of the bottom edge.
  - Nothing is above the lower 35% of the view.
- **Rigged now, animated later** (Q3):
  - The full arm skeleton for both arms (joint list under Assets), with smooth weights at the elbows and wrists.
  - The rest (bind) pose is the held pose. The game draws the rest pose. There are no clips.
- **Never clipped by the world** (Q4):
  - New engine capability `FrameParams::viewModel`: characters drawn after the scene and sky, squeezed into the nearest sliver of the depth range, so the world can't cut or hide them.
  - Perspective mode only. It uses the existing actor shader unchanged.
  - `kBodyRadius` and the palm collision are unchanged.
- **Light and shadow** (Q5):
  - The arms are lit by the sun and moon through the toon ramp like any character.
  - They darken in the world's shadows (the palm's shade), since the actor shader reads the shadow map at their real world position.
  - They cast no shadows: they are in no shadow map.
  - They're outlined like other characters. The gauge glows (emissive, unlit).
- **Locked to the view** (Q6). The arms follow every turn and every look up or down exactly. No walking bob, sway or lag in this task.
- **Always drawn** (Q7):
  - The arms show in play, `--capture` and `--window-png`, with no flag to hide them.
  - The desert's `--capture` output changes on purpose. So `tools/perspective-check.sh` fails its two desert cases in this task, and must still pass `example-first-person`.
- **Detail** (Q8):
  - about 2,500 triangles for both arms, flat-shaded;
  - one solid colour per material;
  - four fingers and a thumb as blocky segments.
- **Materials** (Q9). Actor slots 0–4 in the desert, which has no other characters. Colours are sRGB mid tones:

  | Slot | Material | What | sRGB | Crease ink (Q10) |
  |---|---|---|---|---|
  | 0 | `Arms_Wrap` | bleached cloth bands | (206, 194, 168) | yes |
  | 1 | `Arms_Glove` | dark leather gloves | (78, 54, 40) | yes |
  | 2 | `Arms_Strap` | bracer straps and buckles | (118, 84, 56) | no |
  | 3 | `Arms_Brass` | worn brass bracer plate | (176, 132, 64) | yes |
  | 4 | `Arms_Glow` | the gauge's bars, emissive | (90, 230, 210) | no |

- **The gauge** (Q9): on top of the left bracer, turned toward the eye so it reads in the held pose. It's a small rectangular window with three glowing bars.

## Layering
- Engine:
  - `FrameParams::viewModel` (`std::vector<ActorDraw>`): characters attached to the view.
  - The perspective frame (`engine/src/renderer_perspective.cpp`) draws them after the scene, the sky backdrop and the window panes. The depth range is squeezed to `[0, kViewModelDepth]` while they draw, with back faces culled.
  - They use the shared mesh program and `drawActors` unchanged, and go into no shadow map.
  - The orthographic frame never reads the field. No shared shader or shared renderer code changes ("Render modes" in `docs/architecture.md`).
  - Named for what it does: the engine names no arms, gloves or player.
- Game (`desert/`):
  - `desert::Arms` (new `desert/src/arms.hpp`, `arms.cpp`) loads the arms' glb into actor slots 0–4 and holds the model, its rest pose and its joint matrices.
  - Each frame it places the model at the view and adds it to `params.viewModel` and `params.actorMaterials`, setting the crease flags.
  - `desert/src/main.cpp` owns one `Arms`.
- Assets (`desert/`):
  - `desert/tools/blender/build_arms.py` builds `desert/assets/characters/arms.glb`, with previews beside it.
  - The designer adds the arms to the Characters section of `desert/docs/art-style.md`.

## Approach
This task runs after `render-mode-split`, which creates `renderer_perspective.cpp`, `PerspectiveFrame` and `RenderMode`. Assets first, then code.

### 1. The asset (designer)
Build `desert/tools/blender/build_arms.py` to the Assets section below. It follows `desert/docs/art-style.md`, and the game's Characters, Materials and Rigs rules in `game/docs/art-style.md`, at real scale. Build step by step in the live Blender session, then rebuild and export headless.

The script's checks are binding; the build fails if any fails:
- object, material and joint names exactly as listed, materials in slot order;
- the triangle count;
- closed volumes;
- the framing;
- the near clearance;
- attachment.

Then add a short entry for the arms to the Characters section of `desert/docs/art-style.md`. It covers:
- a view model is modelled round the eye, origin at the eye;
- actor slots 0–4;
- the joint naming;
- the framing rules.

### 2. Engine: `FrameParams::viewModel`
`engine/include/pxe/renderer.hpp`, in `FrameParams` after `actorEffects` (binding):
```cpp
  // Perspective mode only (ignored in orthographic mode): characters
  // attached to the view, such as a first-person view's arms or a held
  // tool, posed in world space in front of the eye. They are drawn after
  // the scene and the sky, in front of everything, so the scene never cuts
  // or hides them. They take the scene's light and its shadows as other
  // characters do, but cast none. Their materials come from actorMaterials,
  // and their joints must last until the frame is drawn, as for actors.
  std::vector<ActorDraw> viewModel;
```
`engine/src/renderer_perspective.cpp`, in the perspective `Renderer::render`:
- In its anonymous namespace (binding):
  ```cpp
  // A view model is drawn into [0, kViewModelDepth] of the depth range, in
  // front of every surface of the scene: a surface's depth reaches
  // kViewModelDepth only about a millimetre past the near plane.
  constexpr double kViewModelDepth = 0.01;
  ```
- The actor tint and material uniform block runs when `!params.actors.empty() || !params.actorEffects.empty() || !params.viewModel.empty()`.
- In the `kMeshes` pass, after the window-pane block and before `endPass()` (sketch):
  ```cpp
  // The view model, in front of everything already drawn.
  if (!params.viewModel.empty()) {
    glDepthRange(0.0, kViewModelDepth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(progMesh_);
    umat(progMesh_, "uViewProj", viewProj);
    umat(progMesh_, "uMirror", identity);
    u1f(progMesh_, "uClipBelow", -1e9f);
    drawActors(progMesh_, params.viewModel);
    glDepthRange(0.0, 1.0);
  }
  ```
  If `render-mode-split` named the identity matrix or these calls differently, use its names: the block must draw with the same program state `drawMeshes` sets.
- The view model is not drawn in the shadow pass, the actors' static-map logic, or the lightning map. Don't add `params.viewModel` to any of them.
- Nothing else in the frame changes. The edge pass reads the squeezed depths as surfaces just past the near plane. That outlines the view model against the scene and gives it no haze.

No other engine file changes. In particular:
- `renderer.cpp`, `renderer_orthographic.cpp` and every shader stay as they are;
- `game/` doesn't change.

### 3. Game: `desert::Arms`
`desert/src/arms.hpp` (binding interface):
```cpp
#pragma once

#include "pxe/first_person_camera.hpp"
#include "pxe/renderer.hpp"
#include "pxe/skinned.hpp"

#include <glm/mat4x4.hpp>

#include <vector>

namespace desert {

// The player's arms (desert/assets/characters/arms.glb): a rigged model
// held in its rest pose, which is the held pose, drawn as the view model
// in front of everything. Modelled round the eye, so the view's own
// transform places it. The game owns it.
class Arms {
 public:
  // Actor-material slots 0-4: Arms_Wrap, Arms_Glove, Arms_Strap, Arms_Brass,
  // Arms_Glow, in the glb's material order.
  static constexpr int kFirstMaterial = 0;

  // Needs the GL context. Throws std::runtime_error if the glb can't be read.
  void load();
  // Places the arms at `view` and adds them to the frame: params.viewModel
  // and their actor materials. Their joints stay valid until the next
  // pose().
  void pose(const pxe::PerspectiveFrame& view, pxe::FrameParams& params);

 private:
  pxe::SkinnedModel model_;
  pxe::SkinnedModel::Pose rest_;
  std::vector<glm::mat4> joints_;
};

}  // namespace desert
```
`desert/src/arms.cpp`:
- `load()`: `model_ = pxe::SkinnedModel::load(std::string(PXE_ASSET_DIR) + "/characters/arms.glb", kFirstMaterial);` then `model_.restPose(rest_);`.
- `pose()`:
  - Build the eye's transform from the view. Binding: the model faces +Z, with +X on its left and +Y up, the way every character faces at rest (the asset contract), so its axes map onto the view as:
    ```cpp
    glm::mat4 eye{1.f};
    eye[0] = glm::vec4(-view.right, 0.f);
    eye[1] = glm::vec4(view.up, 0.f);
    eye[2] = glm::vec4(view.forward, 0.f);
    eye[3] = glm::vec4(view.eye, 1.f);
    ```
  - `model_.skin(rest_, eye, joints_);`
  - `params.viewModel.push_back({&model_.mesh(), joints_});`
  - Copy `model_.materials()` into `params.actorMaterials[kFirstMaterial + i]`, as the engine guide's snippet does.
  - Set `crease = true` on the Wrap, Glove and Brass slots (indices 0, 1, 3 from `kFirstMaterial`), as `game/src/tavern.cpp` does for the door. Name the five indices as constants in `arms.cpp`'s anonymous namespace.
- `desert/CMakeLists.txt`: add `src/arms.cpp` to `pxe_desert`.

`desert/src/main.cpp`:
- After `renderer.upload(world.scene)`: `desert::Arms arms; arms.load();`.
- In both the capture loop and the play loop, replace the render call with (sketch):
  ```cpp
  pxe::FrameParams params = frameParams(world, framebuffer, time, opt.hour);
  const pxe::PerspectiveFrame view = camera.frame(renderer.rtW(), renderer.rtH());
  arms.pose(view, params);
  renderer.render(world.scene, view, params);
  ```
  `params` stops being `const`, and `frameParams` is unchanged.
- The file's header comment says the view is from the eye with the player's arms, no tools or HUD. The `frameParams` comment says the arms are added on top.

`desert/src/player.hpp`: the comment "The player is only an eye: no body, arms or tools are drawn." becomes "The player is an eye. Its arms are drawn attached to the view (arms.hpp); it has no body or tools." No constant changes.

## Verification
Run in the task's worktree:
1. Fresh build, `cmake --preset check --fresh && cmake --build --preset check --clean-first`: only the stb_image_write `sprintf` warning.
2. Run `tools/ortho-check.sh <cycle base>`: every case passes.
3. Run `tools/perspective-check.sh <cycle base>`:
   - `example-first-person` passes;
   - `desert-default` and `desert-night` fail with `png differs`, the intended change.

   Record the summary in the handoff.
4. `git diff <cycle base> -- engine/src/renderer.cpp engine/src/renderer_orthographic.cpp engine/shaders game/`: no output.
5. Captures, saved into the task folder with these names:
   - `build/release/desert/pxe_desert --capture docs/tasks/desert-arms/arms-day.png` (16.5 h);
   - `... --hour 10 --capture docs/tasks/desert-arms/arms-morning.png`;
   - `... --hour 18.5 --capture docs/tasks/desert-arms/arms-golden.png`;
   - `... --hour 21 --capture docs/tasks/desert-arms/arms-night.png`.

   Each should show:
   - both arms in the lower corners, hands about a quarter of the width in from each side;
   - nothing above the lower 35% of the view;
   - the outline round the arms and ink at the wrap bands and finger segments;
   - the gauge's three teal bars glowing, at night too.

   Two runs of the same capture are identical (`cmp`).
6. **No clipping, checked by a temporary edit.** Never commit it.
   - Set `kSpawn` in `desert/src/main.cpp` to `{0.f, -0.71f}`. That's the trunk's 0.40 m radius, plus `kBodyRadius`, plus 1 cm, south of the palm, still facing it.
   - Capture `docs/tasks/desert-arms/arms-at-trunk.png`. The hands and forearms must show whole in front of the bark, not cut by it.
   - Revert the edit, and confirm `git diff desert/src/main.cpp` shows only this task's changes.

   In the same way, set the spawn pitch argument to `1.2f` (about 69° down), capture `arms-looking-down.png`, and revert. The arms stay whole and in front of the sand.
7. Designer previews present (Assets), and the build script's checks pass on a headless rebuild.

## Acceptance criteria
- [ ] `desert/tools/blender/build_arms.py` builds `desert/assets/characters/arms.glb`. A headless rebuild passes all its checks: names, material order, triangles 2,000–3,000, closed volumes, framing, near clearance and attachment
- [ ] The glb has one skinned mesh with exactly the 39 joints listed, materials in the order `Arms_Wrap`, `Arms_Glove`, `Arms_Strap`, `Arms_Brass`, `Arms_Glow` with the listed colours, `Arms_Glow` emissive, and no clips
- [ ] `assets.md` documents the asset as `desert/docs/art-style.md`'s Delivery section asks, plus the measured framing and near clearance. The previews `arms.png` and `arms_eye.png` are beside the glb
- [ ] `desert/docs/art-style.md`'s Characters section describes the arms as the desert's first character: view-model origin at the eye, actor slots 0–4, joint names
- [ ] `FrameParams::viewModel` exists with the comment above. The perspective frame draws it after the scene, sky and window panes, with the depth range `[0, 0.01]` and back faces culled, and draws it into no shadow map. The actor uniform block also runs for a view model alone
- [ ] `renderer.cpp`, `renderer_orthographic.cpp`, every shader and `game/` are unchanged from the cycle base (Verification 4)
- [ ] `desert::Arms` matches the interface above. It loads into slots 0–4, places the model with the binding eye transform, adds it to `params.viewModel`, and sets crease on Wrap, Glove and Brass only
- [ ] `desert/src/main.cpp` poses the arms in the capture and play loops from the same `PerspectiveFrame` it renders
- [ ] `tools/ortho-check.sh <cycle base>` passes. `tools/perspective-check.sh <cycle base>` passes `example-first-person` and fails only the two desert cases
- [ ] The day, morning, golden and night captures and the two clipping captures are in the task folder and show what Verification 5 and 6 describe. Captures are deterministic
- [ ] The engine names no game content (no arms, gloves, bracer or player in engine identifiers or comments beyond the generic "a first-person view's arms or a held tool")
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
- **arms**: the player's two arms, seen in first person. Wanderer style, as in Decisions: wrapped forearms, gloves, and a left-forearm bracer with a glowing gauge. Low-poly, every face flat-shaded, one solid colour per material, no textures (`desert/docs/art-style.md`).
  - Format: glb, one skinned mesh with its armature, exported as the asset contract says.
  - Source: `desert/tools/blender/build_arms.py`, exported to `desert/assets/characters/arms.glb`, with previews beside it.
  - Units and axes: 1 unit = 1 metre, real adult scale, +Y up. The model faces -Y in Blender, which is +Z in the engine: the view's forward. Blender +X is engine +X, the player's left. The player's right arm is on Blender -X.
  - Origin: the eye. The armature's `root` joint sits at the origin, and the game places the origin at the eye with the view's axes, so the model's frame is the view's frame.
  - Proportions (real adult):
    - forearm about 0.26 m, wrist about 0.06 m across;
    - hand about 0.19 m from wrist to fingertip, palm about 0.09 m wide;
    - shoulders about 0.38 m apart, about 0.22 m below the eye and 0.05 m behind it;
    - upper arm about 0.30 m.
  - The held pose (the rest pose), a guide that the framing check below settles:
    - elbows bent about 100°, held in near the body, below and a little ahead of the shoulders;
    - forearms angled forward, up and slightly inward;
    - hands about 0.40–0.45 m ahead of the eye, palms facing inward and down;
    - fingers relaxed and slightly curled, thumbs resting along the index fingers.
  - Framing, checked by the build and binding. Through a camera at the eye looking along the view (Blender: at the origin, looking along -Y, +Z up, 60° vertical field of view, 16:9):
    - every vertex inside the view is in its lower 35% (NDC y ≤ -0.30);
    - the top of each hand's index knuckle is at NDC y between -0.70 and -0.50 (about 20% up from the bottom edge);
    - each hand's centre is at |NDC x| between 0.40 and 0.60, which is about a quarter of the view's width in from its side. NDC x is positive to the screen's right, so the left hand is at negative x and the right hand at positive x;
    - each forearm leaves through the bottom edge.
  - Near clearance, checked by the build and binding: every vertex inside the view is at least 0.15 m ahead of the eye along the view (the near plane is 0.1 m).
  - Wraps:
    - 5–6 stepped cloth bands round each forearm, from just behind the glove cuff to past the bottom of the view;
    - each band a ring standing 4–6 mm proud of the one under it, with bevelled edges that turn more than 40° so the crease ink draws each band's edge;
    - the bands slightly uneven in width and angle, so they read as wrapped by hand.
  - Gloves:
    - full gloves with a flared cuff overlapping the wraps;
    - fingers as blocky segments meeting at knuckles that overlap a little, so no gap shows in a curl;
    - a few broad facets on the back of the hand.
  - Bracer, left forearm only:
    - a brass plate about 0.12 m long, curved over the top of the forearm in 3–4 flat facets;
    - held by two `Arms_Strap` straps with small square buckles.
  - Gauge:
    - a rectangular window about 0.045 × 0.022 m, recessed about 3 mm into the plate, turned toward the eye in the held pose;
    - inside, three `Arms_Glow` bars side by side, each at least 4 mm wide, so each is at least 3 px wide on screen.
  - Closed volumes throughout: the engine culls back faces. Cap the meshes at the shoulders.
  - Joints (39), binding names, per the game's rig naming:
    - `root`;
    - per side, `.L` and `.R`: `shoulder`, `upper_arm`, `forearm`, `hand`, `thumb.1`–`thumb.3`, `index.1`–`index.3`, `middle.1`–`middle.3`, `ring.1`–`ring.3` and `pinky.1`–`pinky.3`.
  - Weights:
    - smooth across the elbows and wrists, over about 4 cm;
    - each finger segment rigid to its joint;
    - the bracer and straps rigid to `forearm.L`, the wraps to their forearm.
  - Attachment, checked by the build: no gaps or floating pieces in the rest pose, nor in test poses with each wrist turned 30° each way, all fingers curled 60°, and each elbow opened 30°. The game shows only the rest pose; the test poses keep later animation safe.
  - Budget: 2,000–3,000 triangles for both arms together. No textures.
  - Materials (binding order, names and colours): `Arms_Wrap` (206, 194, 168), `Arms_Glove` (78, 54, 40), `Arms_Strap` (118, 84, 56), `Arms_Brass` (176, 132, 64), `Arms_Glow` (90, 230, 210), all sRGB base colours. `Arms_Glow` is emissive, so the engine draws it unlit. Set each base colour so the engine's sRGB conversion gives these values back, and print them in the build log.
  - Animations: none. The rest pose is the held pose.
  - Previews, beside the glb:
    - `arms_eye.png`, through the framing camera above at 960×540: what the player sees;
    - `arms.png`, a three-quarter view from outside.

    Both rendered with back faces culled.

## Out of scope
- Animation of any kind: clips, walking bob, sway, lag on turns, grabbing, using the gauge
- A body, legs or shadows of the arms
- Tools or held items, and a HUD
- A flag to hide the arms
- View-model support in orthographic mode
- Changing `kBodyRadius`, the palm or its collision, the spawn, or any other existing desert content
- A separate field of view for the view model

## Spec revisions
None.

## Cycle log
