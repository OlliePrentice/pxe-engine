#!/usr/bin/env bash
# Alternate `--bench 600` runs of main's game binary and the worktree's, main first, N times (default 5).
# Prints each run's ms/frame and each side's median. Usage: bench_alt.sh <main-binary> <worktree-binary> [n]
set -euo pipefail
a="$1"; b="$2"; n="${3:-5}"
ms() { "$1" --bench 600 2>&1 | sed -n 's/^bench: wall *\([0-9.]*\) ms.*/\1/p'; }
ra=(); rb=()
for ((i = 1; i <= n; ++i)); do
  x=$(ms "$a"); y=$(ms "$b")
  echo "run $i: main $x  split $y"
  ra+=("$x"); rb+=("$y")
done
median() { printf '%s\n' "$@" | sort -n | awk '{v[NR]=$1} END{print v[int((NR+1)/2)]}'; }
ma=$(median "${ra[@]}"); mb=$(median "${rb[@]}")
echo "median: main $ma  split $mb  ratio $(awk -v a="$ma" -v b="$mb" 'BEGIN{printf "%.4f", b/a}')"
