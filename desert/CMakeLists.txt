# The desert game: first person on procedural sand dunes, with one date
# palm. It uses only the engine; its assets are its own, under
# desert/assets.
add_executable(pxe_desert
  src/arms.cpp
  src/main.cpp
  src/player.cpp
  src/world.cpp
)

target_compile_options(pxe_desert PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_compile_definitions(pxe_desert PRIVATE PXE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

target_link_libraries(pxe_desert PRIVATE pxe_engine)

# Keep the binary name stable across generators (pxe_desert, pxe_desert.exe).
set_target_properties(pxe_desert PROPERTIES OUTPUT_NAME pxe_desert)
