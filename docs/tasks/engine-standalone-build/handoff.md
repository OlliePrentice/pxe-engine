## Cycle 1
Build: pass

### Changes
- CMakeLists.txt: keeps only project settings (C++20, compile commands) and the two add_subdirectory calls; the FetchContent declarations, GLFW options and the imgui target moved to the engine.
- engine/CMakeLists.txt: declares and fetches glfw, glm, imgui and miniaudio (same tags and GLFW options as before) and builds the `imgui` target, now with `target_compile_features(imgui PUBLIC cxx_std_11)`. `pxe_engine` gets `target_compile_features(... PUBLIC cxx_std_20)`. A consumer that sets no C++ standard otherwise compiled imgui under the compiler default and failed, which the consumer test caught. In this repo both are no-ops because CMAKE_CXX_STANDARD is already 20. Include paths now use `${CMAKE_CURRENT_SOURCE_DIR}/third_party/...`. `PXE_SHADER_DIR` is gone. A new `add_custom_command` generates `${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.cpp` from the 22 files listed in `PXE_SHADERS`, with DEPENDS on every shader file and on the generator script. That source is compiled into `pxe_engine`.
- engine/cmake/embed_shaders.cmake (new): the build-time generator (`cmake -P`). It writes each shader as an `unsigned char` array, 16 bytes to a line, which avoids MSVC's 16 KB string-literal limit (edge.frag is 21 KB). It also writes a name table and `pxe::embeddedShader(std::string_view name)`, which throws on an unknown name. It fails the build if a listed shader is missing or empty.
- engine/src/shader.hpp / shader.cpp: `linkProgram` takes shader file names ("mesh.vert") rather than paths and reads them through `embeddedShader`. `readAll`/std::ifstream are removed. `compose` (the `// #include "common.glsl"` splice) and `withDefines` are unchanged. A new `shaderSource` drops a CR before a LF. That matches what text-mode ifstream did on Windows if a checkout has CRLF line ends. It is a no-op here, because the shaders contain no CR bytes.
- engine/src/renderer.cpp: `shaderDir()` removed. `Renderer::init` passes shader names, and calls `grid_.init()`.
- engine/include/pxe/firegrid.hpp, engine/src/firegrid.cpp: `FireGrid::init()` no longer takes a path parameter. Removed the now-unused `<string>` include from the header.
- third_party/ -> engine/third_party/ via `git mv` (cgltf, khronos, stb; history kept as renames).
- README.md: not changed. The build steps (`cmake -S . -B build ...`, `./build/game/pxe`) are the same.

### Issues addressed
- None (cycle 1).

### Layering
- Engine: build setup in engine/CMakeLists.txt; engine/cmake/embed_shaders.cmake; `pxe::embeddedShader` (private, src/shader.hpp); `linkProgram` now takes names; `FireGrid::init()` without parameter; engine/third_party/.
- Game: none. The game never called FireGrid::init. game/CMakeLists.txt is unchanged and still uses `PXE_ASSET_DIR`, which is out of scope.
- Misplaced code touched: none.

### Verification
- Fresh build: `cmake -S <repo> -B <scratch>/fresh-build -DCMAKE_BUILD_TYPE=Debug && cmake --build ... --parallel` configures and builds. The only warning is stb_image_write's `sprintf` deprecation, which is also in the base build.
- Regeneration: `touch engine/shaders/gridglow.frag` and a rebuild run "Embedding pxe shaders", recompile embedded_shaders.cpp and relink. A second build does nothing. The generated arrays were parsed back and compared to the 22 shader files: all byte-identical.
- Captures: the base c9eaadf came from `git archive` into a scratch dir and was built there. Both binaries ran from working directories in the scratchpad, outside the repo. PNGs are byte-identical (cmp) for: default, `--scene hills`, `--hour 21 --foxes 3`, `--hour 21 --candles --shelter-test --extra-fires 6`. Logs are identical apart from the "wrote <path>" line, which names each run's own output path. `--self-test` prints PASS, and its log is identical to the base's.
- GLSL: scratch copies of both trees were patched to dump every string passed to glShaderSource (the patch was not committed). All 48 sources (24 programs, vertex and fragment) are byte-identical between base and new.
- Consumer project outside the repo (scratchpad `consumer/`): configures, builds and runs with its working directory in its own `run/` directory. Output: `pxe: Apple M3 Pro — 4.1 Metal - 89.4`, `pxe: target 964x544 ...`, `consumer: window and renderer ready`, exit 0. `strings` on the consumer binary finds no `engine/shaders` path.

Consumer CMakeLists.txt:
```cmake
cmake_minimum_required(VERSION 3.20)
project(pxe_consumer LANGUAGES CXX)

add_subdirectory(/Users/sampr/Projects/Games/pxe/pxe-engine/engine pxe_engine)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE pxe_engine)
```

Consumer main.cpp:
```cpp
#include <pxe/renderer.hpp>
#include <pxe/window.hpp>

#include <cstdio>
#include <exception>

int main() {
  try {
    pxe::WindowOptions options;
    options.title = "pxe consumer";
    options.visible = false;
    pxe::Window window(options);
    pxe::Renderer renderer;
    renderer.init();
    std::printf("consumer: window and renderer ready\n");
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "consumer: %s\n", e.what());
    return 1;
  }
}
```

### How to playtest
- Build as usual and play: nothing should look or behave differently. Every shader program now comes from data compiled into the library.
- Edit any file in engine/shaders and rebuild: the build regenerates the embedded shaders and the change shows in the game. Without a rebuild, shader edits no longer take effect (they never did at run time either: there is no hot reload).
- Run `build/game/pxe` from any directory. Shaders no longer depend on the source tree. Game assets still come from `PXE_ASSET_DIR`, which is out of scope.

### Known limitations
- A new shader file has to be added to `PXE_SHADERS` in engine/CMakeLists.txt. Otherwise `linkProgram` throws "no embedded shader <name>" at Renderer::init. This is deliberate (an explicit list, like the source list) rather than a glob.
- Not tested on Windows/MSVC or Linux. The byte-array form avoids the MSVC literal limit, and CRLF checkouts are normalised, but neither was built there.
- engine/CMakeLists.txt still FORCE-sets the GLFW cache options, as the root file did before. A consumer can't override them. Unchanged behaviour, noted for the engine-guide task.
- If a consumer already defines a target named `imgui`, it will clash with the engine's. Out of scope (no install/export work in this task).
