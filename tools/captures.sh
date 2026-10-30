#!/usr/bin/env bash
# Run a set of captures listed in a cases file with the working tree's release
# build, and check them: deterministic, against a base commit's build, pairwise
# different, optionally under a temporary source edit. macOS and Linux.
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/captures.sh [options] <cases-file>
       tools/captures.sh --help

Runs a set of captures with the working tree's release build (built first).

Cases file: one case per line, <name> <binary relative to build/release>
[flags...]; blank lines and # comments skipped. Each case runs as
<binary> [flags...] --capture <name>.png from the output directory.
  e.g.  d21   desert/pxe_desert --hour 21
        fp    examples/minimal/pxe_minimal --first-person

Options:
  --out <dir>          write <name>.png into <dir> (created if missing).
                       Default: a new ${TMPDIR:-/tmp}/pxe-captures.XXXXXX.
  --twice              run each case again from a scratch directory and cmp:
                       the check is that every case is deterministic.
  --base <ref>         also run each case with <ref>'s build (kept in
                       ${TMPDIR:-/tmp}/pxe-render-check/<sha>/, shared with
                       tools/ortho-check.sh and tools/perspective-check.sh)
                       and cmp; the base PNGs go in <out>/base/.
  --expect same|differ with --base: the check is that every case is
                       identical to (same, the default) or differs from
                       (differ) the base.
  --distinct           the check is that the cases' PNGs are pairwise
                       different.
  --edit <file> <old> <new>
                       before building, replace the one occurrence of the
                       exact text <old> in <file> (relative to the repo root)
                       with <new>; restore the file byte for byte and touch it
                       when the tool exits. Exit 2 if <old> occurs zero or
                       several times.
  -h, --help           print this help.

Exit status: 0 when every case ran and every check held, 1 when a case or a
build failed or a check failed, 2 on bad usage.
EOF
}

bad_usage() {
  echo "captures.sh: $*" >&2
  usage >&2
  exit 2
}

out=""
twice=0
base_ref=""
expect=""
distinct=0
edit_file=""
edit_old=""
edit_new=""
cases_file=""

while (($# > 0)); do
  case "$1" in
    -h | --help)
      usage
      exit 0
      ;;
    --out)
      (($# >= 2)) || bad_usage "--out needs a directory"
      out="$2"
      shift 2
      ;;
    --twice)
      twice=1
      shift
      ;;
    --base)
      (($# >= 2)) || bad_usage "--base needs a ref"
      base_ref="$2"
      shift 2
      ;;
    --expect)
      (($# >= 2)) || bad_usage "--expect needs same or differ"
      expect="$2"
      [[ "$expect" == "same" || "$expect" == "differ" ]] || bad_usage "--expect takes same or differ, not: $expect"
      shift 2
      ;;
    --distinct)
      distinct=1
      shift
      ;;
    --edit)
      (($# >= 4)) || bad_usage "--edit needs <file> <old> <new>"
      [[ -z "$edit_file" ]] || bad_usage "--edit given more than once"
      edit_file="$2"
      edit_old="$3"
      edit_new="$4"
      shift 4
      ;;
    -*)
      bad_usage "unknown option: $1"
      ;;
    *)
      [[ -z "$cases_file" ]] || bad_usage "more than one cases file: $cases_file, $1"
      cases_file="$1"
      shift
      ;;
  esac
done

[[ -n "$cases_file" ]] || bad_usage "missing <cases-file>"
[[ -f "$cases_file" && -r "$cases_file" ]] || bad_usage "cases file not readable: $cases_file"
if [[ -n "$expect" && -z "$base_ref" ]]; then
  bad_usage "--expect needs --base"
fi
[[ -n "$expect" ]] || expect="same"

# Parse the cases file: <name> <binary> [flags...], whitespace-separated.
case_name=()
case_bin=()
case_flags=()
lineno=0
while IFS= read -r line || [[ -n "$line" ]]; do
  lineno=$((lineno + 1))
  words=()
  read -ra words <<<"$line" || true
  ((${#words[@]} > 0)) || continue
  [[ "${words[0]}" != \#* ]] || continue
  if ((${#words[@]} < 2)); then
    bad_usage "$cases_file:$lineno: expected <name> <binary> [flags...]"
  fi
  if [[ ! "${words[0]}" =~ ^[A-Za-z0-9._-]+$ ]]; then
    bad_usage "$cases_file:$lineno: case name must be letters, digits, . _ -: ${words[0]}"
  fi
  for prev in ${case_name[@]+"${case_name[@]}"}; do
    [[ "$prev" != "${words[0]}" ]] || bad_usage "$cases_file:$lineno: duplicate case name: ${words[0]}"
  done
  case_name+=("${words[0]}")
  case_bin+=("${words[1]}")
  flags=""
  if ((${#words[@]} > 2)); then
    flags="${words[*]:2}"
  fi
  case_flags+=("$flags")
done <"$cases_file"
ncases=${#case_name[@]}
((ncases > 0)) || bad_usage "no cases in $cases_file"

root="$(git rev-parse --show-toplevel)"

sha=""
if [[ -n "$base_ref" ]]; then
  if ! sha="$(git -C "$root" rev-parse --verify --quiet "$base_ref^{commit}")"; then
    bad_usage "not a commit: $base_ref"
  fi
fi

# The edit must apply exactly once. Checked now so bad usage exits 2 before any
# build; applied later.
edit_py='
import os, sys
mode, path, old, new = sys.argv[1:5]
old = os.fsencode(old)
new = os.fsencode(new)
with open(path, "rb") as f:
    data = f.read()
n = data.count(old)
if mode == "count":
    print(n)
    sys.exit(0)
if n != 1:
    sys.exit(3)
with open(path, "wb") as f:
    f.write(data.replace(old, new))
'
if [[ -n "$edit_file" ]]; then
  command -v python3 >/dev/null || bad_usage "--edit needs python3"
  [[ -n "$edit_old" ]] || bad_usage "--edit: <old> is empty"
  [[ -f "$root/$edit_file" ]] || bad_usage "--edit: no such file in the repo: $edit_file"
  count="$(python3 -I -c "$edit_py" count "$root/$edit_file" "$edit_old" "$edit_new")"
  if ((count != 1)); then
    bad_usage "--edit: the text occurs $count times in $edit_file, not once"
  fi
fi

# Output directory.
if [[ -z "$out" ]]; then
  out="$(mktemp -d "${TMPDIR:-/tmp}/pxe-captures.XXXXXX")"
else
  mkdir -p "$out"
  out="$(cd "$out" && pwd)"
fi

# Restore the edited file (byte for byte, then touch it so the next build
# recompiles it) and remove scratch files, whatever way the tool exits.
edit_backup=""
scratch=""
cleanup() {
  local rc=$?
  trap - EXIT INT TERM
  if [[ -n "$edit_backup" ]]; then
    if cp "$edit_backup" "$root/$edit_file" && touch "$root/$edit_file"; then
      rm -f "$edit_backup"
    else
      echo "captures.sh: FAILED to restore $edit_file; the original is in $edit_backup" >&2
      rc=1
    fi
  fi
  if [[ -n "$scratch" ]]; then
    rm -rf "$scratch"
  fi
  exit "$rc"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

echo "outputs: $out"

# Base build, kept per commit. A missing binary means the base was never built
# (or its build failed part-way, or it predates the binary), so rebuild it.
base_bin=""
if [[ -n "$sha" ]]; then
  base="${TMPDIR:-/tmp}/pxe-render-check/$sha"
  if [[ ! -x "$base/build/release/desert/pxe_desert" ]]; then
    rm -rf "$base"
    mkdir -p "$base"
    git -C "$root" archive "$sha" | tar -x -C "$base"
    if ! cmake --preset release -S "$base" ||
      ! (cd "$base" && cmake --build --preset release); then
      echo "captures.sh: base build failed in $base" >&2
      exit 1
    fi
  fi
  echo "base: $sha in $base"
  base_bin="$base/build/release"
  mkdir -p "$out/base"
fi

# Temporary edit, applied to the working tree only.
if [[ -n "$edit_file" ]]; then
  edit_backup="$(mktemp "${TMPDIR:-/tmp}/pxe-captures-edit.XXXXXX")"
  cp "$root/$edit_file" "$edit_backup"
  python3 -I -c "$edit_py" apply "$root/$edit_file" "$edit_old" "$edit_new"
  touch "$root/$edit_file"
  echo "edit: $edit_file (restored on exit)"
fi

# Current build: the working tree as it stands (edit included).
if ! (cd "$root" && cmake --preset release && cmake --build --preset release); then
  echo "captures.sh: build of the working tree failed" >&2
  exit 1
fi
head_bin="$root/build/release"

if ((twice)); then
  scratch="$(mktemp -d "${TMPDIR:-/tmp}/pxe-captures-scratch.XXXXXX")"
fi

failed=0
run_ok=0
run_msg=""

# run_in <dir> <build/release dir> <case index>
# Runs the case from <dir>; sets run_ok (1: exit 0 and the PNG written) and
# run_msg (the reason when not). The output goes to <dir>/<name>.log.
run_in() {
  local dir="$1" binroot="$2" i="$3" name rc=0 flags=()
  name="${case_name[$i]}"
  read -ra flags <<<"${case_flags[$i]}" || true
  mkdir -p "$dir"
  rm -f "$dir/$name.png"
  (cd "$dir" && "$binroot/${case_bin[$i]}" ${flags[@]+"${flags[@]}"} --capture "$name.png" >"$name.log" 2>&1) || rc=$?
  run_ok=0
  if ((rc != 0)); then
    run_msg="exit $rc"
  elif [[ ! -f "$dir/$name.png" ]]; then
    run_msg="exit 0, no $name.png written"
  else
    run_ok=1
    run_msg=""
  fi
}

case_written=()
for ((i = 0; i < ncases; i++)); do
  name="${case_name[$i]}"
  run_in "$out" "$head_bin" "$i"
  if ((!run_ok)); then
    echo "$name: failed ($run_msg)"
    failed=$((failed + 1))
    case_written+=(0)
    continue
  fi
  echo "$name: written $name.png"
  case_written+=(1)

  if ((twice)); then
    run_in "$scratch" "$head_bin" "$i"
    if ((!run_ok)); then
      echo "$name: failed on the second run ($run_msg)"
      failed=$((failed + 1))
    elif cmp -s "$out/$name.png" "$scratch/$name.png"; then
      echo "$name: deterministic"
    else
      echo "$name: NOT deterministic"
      failed=$((failed + 1))
    fi
  fi

  if [[ -n "$sha" ]]; then
    run_in "$out/base" "$base_bin" "$i"
    if ((!run_ok)); then
      echo "$name: base failed ($run_msg)"
      failed=$((failed + 1))
    elif cmp -s "$out/$name.png" "$out/base/$name.png"; then
      echo "$name: identical to base"
      if [[ "$expect" == "differ" ]]; then
        failed=$((failed + 1))
      fi
    else
      echo "$name: differs from base"
      if [[ "$expect" == "same" ]]; then
        failed=$((failed + 1))
      fi
    fi
  fi
done

if ((distinct)); then
  clashes=0
  for ((i = 0; i < ncases; i++)); do
    ((case_written[i])) || continue
    for ((j = i + 1; j < ncases; j++)); do
      ((case_written[j])) || continue
      if cmp -s "$out/${case_name[$i]}.png" "$out/${case_name[$j]}.png"; then
        echo "${case_name[$i]} and ${case_name[$j]}: identical"
        clashes=$((clashes + 1))
        failed=$((failed + 1))
      fi
    done
  done
  if ((clashes == 0)); then
    echo "distinct: every pair of PNGs differs"
  fi
fi

if ((failed == 0)); then
  echo "captures: $ncases cases, all checks held"
else
  echo "captures: $ncases cases, $failed checks failed"
  exit 1
fi
