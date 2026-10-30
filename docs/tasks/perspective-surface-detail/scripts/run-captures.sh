#!/bin/zsh
# Verification 4: ripples.png (edit A), terminator.png (edit B), off.png
# (unedited, vs the base), then the pairwise-different check.
# Usage: run-captures.sh <out-dir> [hour, default 18]   (from the worktree root)
set -e
out=${1:?out dir}; hour=${2:-18}
mkdir -p $out/a $out/b $out/off
anchor='  // Nothing is scattered: no grass on the sand, no leaf sprites on the palm.'
echo "x desert/pxe_desert --hour $hour" > $out/case.txt
sed -i '' 's/^x /ripples /' $out/case.txt
tools/captures.sh --out $out/a --edit desert/src/world.cpp "$anchor" "  scene.ripples.surface = pxe::kSurfSand; scene.ripples.headingDegrees = 200.f;
$anchor" $out/case.txt
sed -i '' 's/^ripples /terminator /' $out/case.txt
sleep 2  # see friction.md: a build right after a restore can miss the next edit
tools/captures.sh --out $out/b --edit desert/src/world.cpp "$anchor" "  scene.terminator[pxe::kSurfSand] = 0.35f;
$anchor" $out/case.txt
sed -i '' 's/^terminator /off /' $out/case.txt
sleep 2
tools/captures.sh --out $out/off --base c2c6add --expect same $out/case.txt
cmp $out/a/ripples.png $out/b/terminator.png && echo SAME || echo "ripples != terminator"
cmp $out/a/ripples.png $out/off/off.png && echo SAME || echo "ripples != off"
cmp $out/b/terminator.png $out/off/off.png && echo SAME || echo "terminator != off"
