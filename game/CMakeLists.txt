add_executable(pxe
  src/main.cpp
  src/scene.cpp
  src/settings.cpp
  src/fireball.cpp
  src/hero.cpp
  src/fox.cpp
  src/gamepad.cpp
  src/audio.cpp
  src/miniaudio.cpp
)

# Third-party implementation: built without the project's warnings.
set_source_files_properties(src/miniaudio.cpp PROPERTIES COMPILE_OPTIONS "-w")

target_include_directories(pxe PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party/stb
  ${miniaudio_SOURCE_DIR}
)

target_compile_options(pxe PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_compile_definitions(pxe PRIVATE PXE_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(pxe PRIVATE pxe_engine)

# Keep the binary name stable across generators (pxe, pxe.exe).
set_target_properties(pxe PROPERTIES OUTPUT_NAME pxe)
