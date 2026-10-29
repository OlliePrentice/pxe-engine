#!/usr/bin/env bash
# Build a base commit and the working tree, run the orthographic render mode's
# standard cases on both and compare the outputs byte for byte. macOS and Linux.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/ortho-check.sh <base-ref>
       tools/ortho-check.sh --help

Checks that the orthographic render mode's output is byte-identical between
a base commit and the working tree (uncommitted changes included).

  <base-ref>   Anything git rev-parse accepts: main, a hash, HEAD~1.
  -h, --help   Print this help.

The base commit is extracted with git archive into
${TMPDIR:-/tmp}/pxe-render-check/<full sha>/ and built there with the
release preset. It is built once and reused by later runs against the same
commit. The working tree is built with the release preset in its own
build/release. Both builds cover everything, so the base also serves
tools/perspective-check.sh.

Each case runs on both sides with the same flags, from a per-side output
directory (<run dir>/base and <run dir>/head), and passes a relative PNG
name. The run directory, ${TMPDIR:-/tmp}/pxe-render-check-run.XXXXXX, is
left in place so the PNGs of a failing case can be looked at.

Cases (name: command, as <binary> <flags>):
  game-default       game/pxe --capture game-default.png
  game-hills         game/pxe --scene hills --capture game-hills.png
  game-night-foxes   game/pxe --hour 21 --foxes 3 --capture game-night-foxes.png
  game-night-fires   game/pxe --hour 21 --candles --shelter-test --extra-fires 6
                     --capture game-night-fires.png
  game-self-test     game/pxe --self-test (combined output compared)
  example-ortho      examples/minimal/pxe_minimal --capture example-ortho.png

A capture case passes when both runs exit 0 and the PNGs are identical.

Exit status: 0 when every case matches, 1 when any differs or fails to run
(or a build fails), 2 on bad usage.
EOF
}

if (($# == 1)) && [[ "$1" == "-h" || "$1" == "--help" ]]; then
  usage
  exit 0
fi
if (($# != 1)); then
  echo "ortho-check.sh: expected exactly one argument: <base-ref>" >&2
  usage >&2
  exit 2
fi
base_ref="$1"

root="$(git rev-parse --show-toplevel)"
if ! sha="$(git -C "$root" rev-parse --verify --quiet "$base_ref^{commit}")"; then
  echo "ortho-check.sh: not a commit: $base_ref" >&2
  usage >&2
  exit 2
fi

# Base build, kept per commit. A missing binary means the base was never built
# (or its build failed part-way, or it predates the binary), so rebuild it.
base="${TMPDIR:-/tmp}/pxe-render-check/$sha"
if [[ ! -x "$base/build/release/game/pxe" ]]; then
  rm -rf "$base"
  mkdir -p "$base"
  git -C "$root" archive "$sha" | tar -x -C "$base"
  if ! cmake --preset release -S "$base" ||
    ! (cd "$base" && cmake --build --preset release); then
    echo "ortho-check.sh: base build failed in $base" >&2
    exit 1
  fi
fi
echo "base: $sha in $base"

# Current build: the working tree as it stands.
if ! (cd "$root" && cmake --preset release && cmake --build --preset release); then
  echo "ortho-check.sh: build of the working tree failed" >&2
  exit 1
fi

out="$(mktemp -d "${TMPDIR:-/tmp}/pxe-render-check-run.XXXXXX")"
mkdir "$out/base" "$out/head"
echo "outputs: $out"

base_bin="$base/build/release"
head_bin="$root/build/release"
passed=0
total=0

# run_case <name> <png|log> <binary relative to build/release> [flags...]
# png: both runs must exit 0 and write identical PNGs (<name>.png, which the
# flags name). log: both runs must exit 0 and print identical output.
run_case() {
  local name="$1" kind="$2" bin_rel="$3" base_rc=0 head_rc=0 reason=""
  shift 3
  total=$((total + 1))

  (cd "$out/base" && "$base_bin/$bin_rel" "$@" >"$name.log" 2>&1) || base_rc=$?
  (cd "$out/head" && "$head_bin/$bin_rel" "$@" >"$name.log" 2>&1) || head_rc=$?

  if ((base_rc != 0)); then
    reason="base exited $base_rc"
  elif ((head_rc != 0)); then
    reason="head exited $head_rc"
  elif [[ "$kind" == "png" ]]; then
    cmp -s "$out/base/$name.png" "$out/head/$name.png" || reason="png differs"
  else
    cmp -s "$out/base/$name.log" "$out/head/$name.log" || reason="log differs"
  fi

  if [[ -z "$reason" ]]; then
    echo "PASS $name"
    passed=$((passed + 1))
  else
    echo "FAIL $name: $reason"
  fi
}

run_case game-default png game/pxe --capture game-default.png
run_case game-hills png game/pxe --scene hills --capture game-hills.png
run_case game-night-foxes png game/pxe --hour 21 --foxes 3 --capture game-night-foxes.png
run_case game-night-fires png game/pxe --hour 21 --candles --shelter-test --extra-fires 6 --capture game-night-fires.png
run_case game-self-test log game/pxe --self-test
run_case example-ortho png examples/minimal/pxe_minimal --capture example-ortho.png

echo "ortho-check: $passed/$total cases identical against ${sha:0:7}"
((passed == total))
