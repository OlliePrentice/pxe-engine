# The pxe engine. Self-contained: a project adds this directory with
# add_subdirectory (or FetchContent with SOURCE_SUBDIR engine) and links
# pxe_engine. Every path here is relative to this directory or its build
# directory.

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.8
  GIT_SHALLOW TRUE
)
# miniaudio is a single header; populate it without its own CMake, and
# compile the implementation in one source file of the engine.
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG 0.11.21
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR no-cmake
)
FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG 1.0.1
  GIT_SHALLOW TRUE
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
# X11 is the portable Linux backend. Wayland needs wayland-scanner, which a
# plain CMake build should not require.
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw glm imgui miniaudio)

# Dear ImGui has no CMake of its own; build the core here. Its GLFW and
# OpenGL 3 backends are compiled into the engine, which owns the window.
add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR})
# Dear ImGui needs C++11; say so, as a consumer may not set a standard.
target_compile_features(imgui PUBLIC cxx_std_11)

# Shaders are compiled into the library: a C++ source generated from
# shaders/ at build time, regenerated whenever a shader changes. Nothing is
# read from disk at run time.
set(PXE_SHADERS
  common.glsl
  edge.frag
  edge.vert
  fire.frag
  fire.vert
  fireshadow.frag
  fireshadow.vert
  grass.frag
  grass.vert
  gridburn.frag
  gridflame.vert
  gridglow.frag
  gridstep.frag
  gridsum.frag
  mesh.frag
  mesh.vert
  present.frag
  present.vert
  shadow.frag
  shadow.vert
  water.frag
  water.vert
)
list(TRANSFORM PXE_SHADERS PREPEND ${CMAKE_CURRENT_SOURCE_DIR}/shaders/ OUTPUT_VARIABLE pxe_shader_paths)
list(JOIN PXE_SHADERS "," pxe_shader_names)
set(pxe_embedded_shaders ${CMAKE_CURRENT_BINARY_DIR}/generated/embedded_shaders.cpp)
add_custom_command(
  OUTPUT ${pxe_embedded_shaders}
  COMMAND ${CMAKE_COMMAND}
    -DSHADER_DIR=${CMAKE_CURRENT_SOURCE_DIR}/shaders
    -DSHADERS=${pxe_shader_names}
    -DOUTPUT=${pxe_embedded_shaders}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
  DEPENDS ${pxe_shader_paths} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_shaders.cmake
  COMMENT "Embedding pxe shaders"
  VERBATIM
)

add_library(pxe_engine
  src/gl.cpp
  src/shader.cpp
  src/mesh.cpp
  src/camera.cpp
  src/renderer.cpp
  src/gltf.cpp
  src/skinned.cpp
  src/lighting.cpp
  src/fire.cpp
  src/firegrid.cpp
  src/fireflies.cpp
  src/shelter.cpp
  src/heightmap.cpp
  src/scene.cpp
  src/scene_geometry.cpp
  src/triangle_grid.cpp
  src/scatter.cpp
  src/image.cpp
  src/audio_file.cpp
  src/window.cpp
  src/input.cpp
  src/overlay.cpp
  src/audio_device.cpp
  src/miniaudio.cpp
  ${pxe_embedded_shaders}
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Third-party implementation: built without the project's warnings.
set_source_files_properties(
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  src/miniaudio.cpp
  PROPERTIES COMPILE_OPTIONS "-w"
)

target_include_directories(pxe_engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/khronos
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cgltf
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
  PRIVATE ${imgui_SOURCE_DIR}/backends
  PRIVATE ${miniaudio_SOURCE_DIR}
)

# The engine's public headers need C++20; a consumer gets it with the target.
target_compile_features(pxe_engine PUBLIC cxx_std_20)

target_compile_options(pxe_engine PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(pxe_engine PUBLIC glm::glm imgui PRIVATE glfw)

if(APPLE)
  target_link_libraries(pxe_engine PUBLIC "-framework OpenGL")
elseif(WIN32)
  target_link_libraries(pxe_engine PUBLIC opengl32)
else()
  find_package(OpenGL REQUIRED)
  target_link_libraries(pxe_engine PUBLIC OpenGL::GL)
endif()
