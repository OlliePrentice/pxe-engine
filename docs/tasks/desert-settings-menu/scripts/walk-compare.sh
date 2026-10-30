#!/bin/bash
# Compares pxe_desert's --walk captures between a base build kept by
# perspective-check.sh and the working tree's release build.
# Usage: walk-compare.sh <full base sha> (run from the worktree root)
set -u
sha="$1"
root="$(pwd)"
base="${TMPDIR:-/tmp}/pxe-render-check/$sha/build/release/desert/pxe_desert"
new="$root/build/release/desert/pxe_desert"
out="$(mktemp -d)"
fail=0
run() {  # name, args...
  local name="$1"; shift
  mkdir -p "$out/base" "$out/new"
  (cd "$out/base" && "$base" "$@" --capture "$name.png" 2>/dev/null)
  (cd "$out/new" && "$new" "$@" --capture "$name.png" 2>/dev/null)
  if cmp "$out/base/$name.png" "$out/new/$name.png"; then echo "IDENTICAL $name"; else echo "DIFFERENT $name"; fail=1; fi
}
run w --walk 2
run s --walk 3 --sprint
run n --walk 2 --hour 21
exit $fail
