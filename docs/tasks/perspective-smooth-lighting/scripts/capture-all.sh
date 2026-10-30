#!/bin/zsh
# Renders the task's verification captures into the task folder, each twice
# (second run from a scratch directory) and cmp's them.
# Usage: scripts/capture-all.sh   (from anywhere; paths are derived)
here=${0:A:h}
task=${here:h}
root=${task:h:h:h}
B=$root/build/release
scratch=$(mktemp -d)
run() {
  n=$1; shift
  (cd $task && "$@" --capture $n.png >/dev/null 2>&1; echo "$n exit $?")
  (cd $scratch && "$@" --capture $n.png >/dev/null 2>&1)
  cmp $task/$n.png $scratch/$n.png && echo "$n deterministic"
}
run d07 $B/desert/pxe_desert --hour 7
run d12 $B/desert/pxe_desert --hour 12
run d1630 $B/desert/pxe_desert
run d1818 $B/desert/pxe_desert --hour 18.3
run d21 $B/desert/pxe_desert --hour 21
run walk $B/desert/pxe_desert --walk 2
run example-fp $B/examples/minimal/pxe_minimal --first-person
