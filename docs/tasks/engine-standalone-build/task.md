# Engine builds and runs on its own
Type: fix
Status: open
Cycle: 1

## Goal
Another project can't use pxe today. The engine's dependencies (glfw, glm, imgui, miniaudio) are fetched in the root CMakeLists.txt, engine/CMakeLists.txt reaches into `${CMAKE_SOURCE_DIR}/third_party`, and shaders are read at run time from `PXE_SHADER_DIR`, an absolute path into this source tree baked in at compile time. Make `engine/` a self-contained directory: a project that adds it with `add_subdirectory` (or FetchContent with `SOURCE_SUBDIR engine`) gets a working `pxe_engine` target, and a built program runs without the source tree present.

## Decisions
- Shaders are embedded in the library at build time, generated from engine/shaders/*.glsl|vert|frag by CMake. No shader file is read at run time; `shaderDir()` and `PXE_SHADER_DIR` go. Editing a shader file must still trigger a rebuild. Recommended over copying shaders next to the executable: nothing to install, no working-directory or path lookups. Embed as byte arrays or split literals: edge.frag is 21 KB, over MSVC's 16 KB string-literal limit.
- The `"// #include \"common.glsl\""` splice and the define injection keep working exactly as now, so the GLSL each program compiles is byte-identical to today's.
- `FireGrid::init(const std::string& shaderDir)` loses its path parameter (it is the only public API that names a shader location).
- third_party/ moves to engine/third_party/ (git mv): only the engine uses cgltf, stb and the Khronos headers.
- engine/CMakeLists.txt declares and fetches its own dependencies and builds the imgui target. The root CMakeLists.txt keeps project settings (C++ standard, compile commands) and the two add_subdirectory calls.
- engine/CMakeLists.txt uses only `CMAKE_CURRENT_SOURCE_DIR`/`CMAKE_CURRENT_BINARY_DIR`-relative paths.
- Rendered output and the game's behaviour are unchanged.

## Layering
- Engine: build setup (engine/CMakeLists.txt), shader embedding (engine/src/shader.*, renderer.cpp, firegrid.*), third_party move.
- Game: none, except the FireGrid::init call site if the game makes it (it doesn't today: Renderer does).

## Acceptance criteria
- [ ] engine/CMakeLists.txt contains no `CMAKE_SOURCE_DIR`, and no file under engine/ refers to a path outside engine/.
- [ ] glfw, glm, imgui and miniaudio are declared and made available in engine/CMakeLists.txt; the root CMakeLists.txt no longer declares them.
- [ ] third_party/ no longer exists at the repo root; its files are in engine/third_party/, history kept via git mv.
- [ ] No runtime shader file I/O: no `PXE_SHADER_DIR`, no `shaderDir()`, no std::ifstream of shader paths in engine/src.
- [ ] Shaders are generated into the build directory from engine/shaders at build time, and touching a shader file causes the generated source to rebuild.
- [ ] The GLSL source handed to glShaderSource for every program is unchanged (same splice, same defines).
- [ ] A consumer project outside the repo (in a scratch directory, not committed) with its own CMakeLists.txt that does `add_subdirectory(<repo>/engine pxe_engine)` and links `pxe_engine` configures, builds, and runs a program that creates a pxe::Window and calls pxe::Renderer::init without error. Record the CMakeLists and main.cpp used in handoff.md.
- [ ] The built `pxe` runs `--capture` from a working directory outside the repo, and its captures are byte-identical to the cycle base build for: default scene, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`. `--self-test` passes.
- [ ] README.md build instructions still correct (update if a step changed).
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
None

## Out of scope
- The game's `PXE_ASSET_DIR` (game asset location is the game's concern).
- Install/export rules (`install()`, package config). Not needed until a project consumes a prebuilt engine.
- Shader hot reload.
- The integration guide and example (task engine-guide).

## Cycle log
- Cycle 1: 2026-10-27 started, base c9eaadf
