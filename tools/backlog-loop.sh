#!/usr/bin/env bash
# Run /backlog-run in a loop, one fresh headless Claude session per batch,
# until the backlog has nothing left to run. macOS and Linux.
set -euo pipefail

usage() {
  cat <<'USAGE'
Usage: tools/backlog-loop.sh [--max-batches n] [--budget usd] [--permission-mode mode]

Runs the /backlog-run skill with `claude -p` on Opus 5.5 at medium effort, once
per batch, each in a fresh session, so no batch carries over a compacted
context. The skill picks the next ready tasks from docs/backlog.md, runs them
in parallel worktrees, merges them and runs its retro. It ends with a status
line that decides what happens next:

  BACKLOG-RUN: continue   run another batch
  BACKLOG-RUN: done       nothing left to run (exit 0)
  BACKLOG-RUN: stop       needs the user (exit 1)

Run it from the repository on the run's branch (never main), with a clean
working tree. Tasks with assets need Blender open with the MCP connected.

  --max-batches n         Stop after n batches (default 10).
  --budget usd            Spend limit per batch (claude --max-budget-usd).
  --permission-mode mode  Permission mode for each session (default auto).
  -h, --help              Print this help.

Each batch's report is printed and saved in .backlog-runs/.
USAGE
}

max_batches=10
budget=""
permission_mode=auto
while [[ $# -gt 0 ]]; do
  case "$1" in
    --max-batches) max_batches="$2"; shift 2 ;;
    --budget) budget="$2"; shift 2 ;;
    --permission-mode) permission_mode="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

cd "$(git rev-parse --show-toplevel)"

branch="$(git branch --show-current)"
if [[ -z "$branch" || "$branch" == main ]]; then
  echo "Run the loop on the run's branch, not ${branch:-a detached HEAD}." >&2
  exit 1
fi
if [[ -n "$(git status --porcelain)" ]]; then
  echo "The working tree has uncommitted changes. Commit or stash them first." >&2
  exit 1
fi

# The environment variable would override the effort the agent files set
# (Sonnet high for the developer, Opus medium for the designer).
unset CLAUDE_CODE_EFFORT_LEVEL CLAUDE_CODE_SUBAGENT_MODEL

log_dir=.backlog-runs
mkdir -p "$log_dir"

extra=()
if [[ -n "$budget" ]]; then
  extra+=(--max-budget-usd "$budget")
fi

for ((batch = 1; batch <= max_batches; batch++)); do
  log="$log_dir/$(date +%Y%m%d-%H%M%S)-batch$batch.log"
  echo "== Batch $batch on $branch, log $log"
  set +e
  claude -p "/backlog-run" \
    --model claude-opus-5-5 \
    --effort medium \
    --permission-mode "$permission_mode" \
    ${extra[@]+"${extra[@]}"} | tee "$log"
  claude_exit=${PIPESTATUS[0]}
  set -e
  if [[ $claude_exit -ne 0 ]]; then
    echo "claude exited with $claude_exit in batch $batch. See $log." >&2
    exit 1
  fi

  status="$(grep -Eo 'BACKLOG-RUN: (continue|done|stop)' "$log" | tail -n 1 | cut -d' ' -f2 || true)"
  case "$status" in
    continue) ;;
    done)
      echo "== Nothing left to run after $batch batch(es)."
      if grep -q '^Status: blocked' docs/backlog.md; then
        echo "== Blocked tasks need you. Answer their questions with /feature unblock:"
        grep -B1 '^Status: blocked' docs/backlog.md | grep '^## ' | sed 's/^## /   /'
      fi
      exit 0 ;;
    stop) echo "== Batch $batch stopped and needs you. See $log." >&2; exit 1 ;;
    *) echo "== Batch $batch ended without a status line. See $log." >&2; exit 1 ;;
  esac
done

echo "== Stopped after --max-batches $max_batches with tasks still to run."
