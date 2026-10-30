#!/usr/bin/env bash
# Verification 2 of captures-edit-stale-build: the back-to-back edit/base/edit
# sequence. Usage: verify2.sh <rounds>   (run from the worktree root)
set -u
cases="$(cd "$(dirname "$0")" && pwd)/x.cases"
anchor='  // Nothing is scattered: no grass on the sand, no leaf sprites on the palm.'
rc_all=0
for ((r = 1; r <= ${1:-1}; r++)); do
  d=/tmp/ces/r$r
  rm -rf "$d"
  echo "=== round $r"
  tools/captures.sh --out $d/a --edit desert/src/world.cpp "$anchor" "  scene.terminator[pxe::kSurfSand] = 0.35f;"$'\n'"$anchor" "$cases" || { echo "ROUND $r STEP a FAILED"; rc_all=1; }
  tools/captures.sh --out $d/b --base HEAD --expect same "$cases" || { echo "ROUND $r STEP b FAILED"; rc_all=1; }
  tools/captures.sh --out $d/c --edit desert/src/world.cpp "$anchor" "  scene.ripples.surface = pxe::kSurfSand;"$'\n'"$anchor" "$cases" || { echo "ROUND $r STEP c FAILED"; rc_all=1; }
  cmp -s $d/a/t.png $d/c/t.png && { echo "ROUND $r: a == c (BAD)"; rc_all=1; } || echo "round $r: a != c ok"
  cmp -s $d/b/t.png $d/c/t.png && { echo "ROUND $r: b == c (BAD)"; rc_all=1; } || echo "round $r: b != c ok"
done
exit $rc_all
