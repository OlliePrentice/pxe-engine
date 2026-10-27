# Engine builds and runs on its own
Type: fix
Status: complete
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
- [x] engine/CMakeLists.txt contains no `CMAKE_SOURCE_DIR`, and no file under engine/ refers to a path outside engine/. (engine/CMakeLists.txt uses CMAKE_CURRENT_SOURCE_DIR/BINARY_DIR only; grep for CMAKE_SOURCE_DIR and ../ in engine/CMakeLists.txt, engine/cmake finds nothing)
- [x] glfw, glm, imgui and miniaudio are declared and made available in engine/CMakeLists.txt; the root CMakeLists.txt no longer declares them. (engine/CMakeLists.txt FetchContent_Declare x4 + MakeAvailable + imgui target; root CMakeLists.txt is settings + add_subdirectory)
- [x] third_party/ no longer exists at the repo root; its files are in engine/third_party/, history kept via git mv. (1292c97 renames third_party/* -> engine/third_party/*; root third_party/ gone)
- [x] No runtime shader file I/O: no `PXE_SHADER_DIR`, no `shaderDir()`, no std::ifstream of shader paths in engine/src. (grep PXE_SHADER_DIR|shaderDir|ifstream in engine/src, engine/include: none; shader.cpp shaderSource reads embeddedShader)
- [x] Shaders are generated into the build directory from engine/shaders at build time, and touching a shader file causes the generated source to rebuild. (engine/cmake/embed_shaders.cmake via add_custom_command, DEPENDS every shader + the script; developer verified touch -> regenerate)
- [x] The GLSL source handed to glShaderSource for every program is unchanged (same splice, same defines). (shader.cpp compose/withDefines unchanged; developer dumped all 48 sources, byte-identical; my captures identical)
- [x] A consumer project outside the repo (in a scratch directory, not committed) with its own CMakeLists.txt that does `add_subdirectory(<repo>/engine pxe_engine)` and links `pxe_engine` configures, builds, and runs a program that creates a pxe::Window and calls pxe::Renderer::init without error. Record the CMakeLists and main.cpp used in handoff.md. (scratchpad/consumer: I rebuilt it fresh and ran it: "consumer: window and renderer ready", exit 0; files in handoff.md)
- [x] The built `pxe` runs `--capture` from a working directory outside the repo, and its captures are byte-identical to the cycle base build for: default scene, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`. `--self-test` passes. (my run from scratchpad cwd vs base c9eaadf build: 4 PNGs cmp-identical, logs identical apart from the 'wrote' line; --self-test PASS)
- [x] README.md build instructions still correct (update if a step changed). (build steps unchanged; README not edited)
- [x] Project builds cleanly from a fresh build directory with CMake (fresh build-check exit 0; only warning is pre-existing stb sprintf deprecation)

## Assets
None

## Out of scope
- The game's `PXE_ASSET_DIR` (game asset location is the game's concern).
- Install/export rules (`install()`, package config). Not needed until a project consumes a prebuilt engine.
- Shader hot reload.
- The integration guide and example (task engine-guide).

## Cycle log
- Cycle 1: 2026-10-27 started, base c9eaadf
- Cycle 1: review passed, no issues; complete. Noted, out of scope: GLFW cache options are forced; a consumer with its own imgui target would clash; new shaders must be added to PXE_SHADERS. Not built on MSVC or Linux.
