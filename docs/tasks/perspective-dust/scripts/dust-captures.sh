#!/bin/zsh
# Verification 4 of perspective-dust: temporarily turn the desert's dust on,
# capture dust-a/dust-b (twice each), restore world.cpp, capture dust-off,
# and compare with the base's capture. Run from the worktree root:
#   docs/tasks/perspective-dust/scripts/dust-captures.sh <base pxe_desert binary>
set -eu
base="$1"
out=docs/tasks/perspective-dust
bin=build/release/desert/pxe_desert
edit() {  # $1 = lines to insert before the final return
  git checkout -- desert/src/world.cpp
  python3 - "$1" <<'PY'
import sys
p='desert/src/world.cpp'
s=open(p).read()
k="  // Nothing is scattered: no grass on the sand, no leaf sprites on the palm.\n  return world;"
assert s.count(k)==1
open(p,'w').write(s.replace(k, k.replace("  return world;", sys.argv[1]+"\n  return world;")))
PY
  touch desert/src/world.cpp
  tools/build.sh release >/dev/null
}
edit "  world.scene.dustDensity = 0.4f;"
$bin --hour 18 --capture $out/dust-a.png; $bin --hour 18 --capture /tmp/dust-a2.png
edit "  world.scene.dustDensity = 1.0f;
  world.scene.dustSize = 2.f;"
$bin --hour 18 --capture $out/dust-b.png; $bin --hour 18 --capture /tmp/dust-b2.png
git checkout -- desert/src/world.cpp; touch desert/src/world.cpp
tools/build.sh release >/dev/null
$bin --hour 18 --capture $out/dust-off.png
"$base" --hour 18 --capture /tmp/base-dust-off.png
cmp $out/dust-a.png /tmp/dust-a2.png && echo "a repeatable"
cmp $out/dust-b.png /tmp/dust-b2.png && echo "b repeatable"
cmp $out/dust-off.png /tmp/base-dust-off.png && echo "off == base"
for p in "a b" "a off" "b off"; do set -- ${=p}; cmp -s $out/dust-$1.png $out/dust-$2.png && echo "SAME $1 $2" || echo "differ $1 $2"; done
git status --short desert
