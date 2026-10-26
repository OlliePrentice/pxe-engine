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
  src/window.cpp
  src/input.cpp
  src/overlay.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

# Third-party implementation: built without the project's warnings.
set_source_files_properties(
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
  PROPERTIES COMPILE_OPTIONS "-w"
)

target_include_directories(pxe_engine
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/khronos
  PRIVATE ${CMAKE_SOURCE_DIR}/third_party/cgltf
  PRIVATE ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(pxe_engine
  PRIVATE PXE_SHADER_DIR="${CMAKE_SOURCE_DIR}/engine/shaders"
)

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
