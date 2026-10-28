# The minimal example from docs/engine-guide.md: a procedural scene on pxe,
# using nothing from game/ or assets/. Built by default, so every build
# compile-checks what the guide shows.
add_executable(pxe_minimal main.cpp)

target_compile_options(pxe_minimal PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(pxe_minimal PRIVATE pxe_engine)
