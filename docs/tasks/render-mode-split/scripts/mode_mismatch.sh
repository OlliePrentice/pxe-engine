#!/usr/bin/env bash
# Builds and runs mode_mismatch.cpp against the release build (macOS link line). Run from the worktree root
# after tools/build.sh release; writes the binary to a scratch dir given as $1 (default $TMPDIR).
set -euo pipefail
root="$(git rev-parse --show-toplevel)"
out="${1:-${TMPDIR:-/tmp}}"
b="$root/build/release"
c++ -O1 -std=c++20 -I"$root/engine/include" -I"$b/_deps/glm-src" -I"$b/_deps/imgui-src" \
  "$root/docs/tasks/render-mode-split/scripts/mode_mismatch.cpp" -o "$out/mode_mismatch" \
  "$b/engine/libpxe_engine.a" "$b/_deps/glm-build/glm/libglm.a" "$b/engine/libimgui.a" "$b/_deps/glfw-build/src/libglfw3.a" \
  -framework Cocoa -framework IOKit -framework CoreFoundation -framework OpenGL
"$out/mode_mismatch"
