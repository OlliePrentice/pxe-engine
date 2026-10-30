---
name: backlog-run
description: Implements one batch of queued tasks from docs/backlog.md, unattended. Runs independent tasks in parallel git worktrees, delegates assets to the designer and code to the game-developer, checks the build, reviews, merges, then runs a retro that fixes missing docs and repeated tools. Usually run by tools/backlog-loop.sh.
argument-hint: "[task-id ...]"
disable-model-invocation: true
model: claude-opus-5-5
effort: medium
---

Run one batch of the backlog. Arguments: $ARGUMENTS (task IDs to restrict the batch to; empty means pick from the backlog).

You orchestrate, review and keep the workflow improving. You don't write game or engine code and you don't make assets: code goes to the **game-developer** agent (Sonnet 5.5, high effort) and assets to the **designer** agent (Opus 5.5, medium effort). Their models are set in their agent files, so never pass `model` to the Agent tool. If you notice a quick fix you could make yourself, delegate it anyway, so every change goes through the same review.

## Running unattended
This skill usually runs headless from `tools/backlog-loop.sh`: `claude -p`, one fresh session per batch, nobody watching. So:
- **Never ask the user anything.** Nobody would answer. A task that needs a decision outside its spec becomes `blocked`, with the questions written under its backlog entry (see **Blocking**). The other tasks carry on.
- **The files are the source of truth.** `docs/backlog.md`, the task folders and git hold every bit of state. A batch can be cut short at any point, and the next one has to pick up from the files alone. Re-read a task's `task.md` and `issues.md` at the start of every cycle.
- **Finish with the status line** (see **Report**). The loop reads it to decide whether to run another batch.

The same rules hold when the user runs `/backlog-run` in a session. Report the questions at the end rather than asking mid-batch.

## Files
- `docs/backlog.md`: the queue, with each task's status, dependencies and the files it touches. Only you edit it, and only in the main working tree on the run's branch.
- `docs/tasks/<task-id>/`, in the task's worktree:
  - `task.md`: the spec, written by `/feature`. You tick criteria, keep the cycle log and set the status.
  - `issues.md`: build errors and review findings, written by you.
  - `handoff.md`: written by game-developer.
  - `assets.md`: written by designer, the manifest of every asset made for the task.
  - `asset-requests.md`: written by game-developer, change requests for the designer.
  - `friction.md`: written by every agent and by you. Missing docs, tools written by hand, unclear instructions. The retro reads it.
  - `scripts/`: one-off scripts the agents wrote for the task, kept so a later tooling task can build on them.
- `docs/friction-log.md`: the ledger of friction across every run. Only the retro edits it.

## 1. Preflight
1. The run's branch is the branch the main working tree is on. If it is `main`, stop with status `stop`: runs never commit to `main`.
2. If the main working tree has uncommitted changes, stop with status `stop` and name the files. Don't stash or discard them.
3. Read `docs/backlog.md`, `docs/architecture.md` and `docs/engine-guide.md`.
4. List existing worktrees (`git worktree list`). A task that is `in-progress` was cut short by an earlier batch. Its worktree and branch hold its work. It resumes from its `task.md` cycle log and `issues.md`.

## 2. Pick the batch
1. Candidates are, in backlog order: every `in-progress` task, then every `queued` task whose `Depends on` tasks are all `done`. If `$ARGUMENTS` names tasks, only those.
2. Take candidates in order into the batch while each new one is independent of every task already in it: neither depends on the other, and their `Touches` lists share no file or directory. A task with `Touches` you can't trust (vague, or plainly incomplete against its `## Approach`) runs alone.
3. Run at most 3 tasks in a batch. Each one does its own clean builds.
4. If there are no candidates, skip to the retro (step 7) if any task finished since the last retro, and otherwise report status `done`.

Set each batch task to `in-progress` in the backlog and commit: `run: start <task-ids>`.

## 3. Worktrees
Every task runs in its own git worktree on its own branch, even when the batch has one task. The agents then never share a working tree, an index or a build directory, and the main working tree stays free for the backlog and merges. Create it from the run's branch, unless it exists from an earlier batch:
```
git worktree add ../<repo-dir>-<task-id> -b <task-id>
```
Don't use the Agent tool's own worktree isolation: it makes a new temporary worktree per delegation, and a task needs the same worktree for every cycle.

Everything for a task happens in its worktree:
- Its task folder, cycle base, `task.md`, `issues.md` and `friction.md` edits and their commits.
- Every delegation. Pass the worktree's absolute path along with the task folder, and tell the agent to work, build and commit only there.
- Your build check, run from the worktree so the `check` preset builds in `<worktree>/build-check`: `cd <worktree> && cmake --preset check --fresh && cmake --build --preset check --clean-first`.
- Your review: `git -C <worktree> diff <cycle base>..HEAD`.

## 4. Run the cycles
Run every batch task's cycles at the same time:
- Delegate the developers for all tasks in the same turn, so they run in parallel.
- Only one designer runs at a time: assets are built in the user's single live Blender session. Queue designer work across tasks and run it one task after another. The developers of tasks without asset work carry on meanwhile.
- A task that blocks doesn't stop the others.

For each task, at the start of every cycle, increment `Cycle` in `task.md`, set `Status: open`, and add a cycle log line with the date and the current `HEAD` hash: it is the cycle's base. From cycle 2 on, also list the outstanding issue IDs. Every change is committed, so any cycle can be undone with `git revert`. The designer and developer each commit their own work and report the hash. You commit `task.md`, `issues.md` and `friction.md` yourself.

1. **Assets first.** If the `## Assets` section lists assets that don't exist yet, or `asset-requests.md` has requests with `Status: open`, delegate to **designer** with the worktree path, the task folder path and the request IDs.
   - If the designer returns `NEEDS INPUT`, answer what `task.md`, `docs/art-style.md` and the existing assets settle, and send it back with the answers. If it says an asset can't be built as specified, go to **Replanning**. Block the task (see **Blocking**) on anything that is a style call or would change existing content.
   - If the designer reports that Blender or the MCP isn't available, or that the open file has unsaved changes, block the task with that reason.
   - If a request comes back `needs-decision`, decide it if the spec and the art style settle it, and record the decision in the request. Otherwise go to **Replanning**.
   - Check the designer's preview images before moving on. If an asset clearly doesn't match the spec, send it back to the designer with the reason.
2. **Code.** Delegate to **game-developer**. Pass the worktree path, the task folder path and, from cycle 2 on, the issue IDs it must fix.
   - If it returns `BLOCKED ON ASSETS`, go back to step 1 for its new requests in `asset-requests.md`, then delegate to the developer again.
   - If it returns `SPEC INFEASIBLE`, go to **Replanning**.
   - When it returns, confirm `handoff.md` has an entry for this cycle reporting a successful build, and that the cycle's work is committed (`git -C <worktree> status` shows nothing of the developer's or designer's left uncommitted). If not, send it back with the reason.
3. **Build check.** Build from a fresh directory yourself. Don't trust the developer's report:
   ```
   cd <worktree> && cmake --preset check --fresh && cmake --build --preset check --clean-first
   ```
   `--clean-first` matters: `--fresh` alone keeps the object files, so unchanged files aren't recompiled and their warnings don't show. Record each distinct compile error as a `critical` issue. Record new warnings as `minor` issues, but only in files listed in the handoff. Then run the commands in the spec's `## Verification`.
4. **Review.** Read the cycle's diff against the spec. Check that it leaves existing content alone unless `## Decisions` names the change. Check the layering against `## Layering`, that code examples marked binding in `## Approach` were followed, and run the **alignment review** in `CLAUDE.md` against `docs/architecture.md` and `docs/engine-guide.md`. Discrepancies in this cycle's diff are issues. For each acceptance criterion, find the code that satisfies it. Record anything that's missing or wrong as an issue, along with obvious regressions in code the change touched. Check that every asset the developer loads matches its `assets.md` entry.
   - Discrepancies the review reveals in code the task didn't touch become new **alignment tasks**: write the spec with the `/feature` template (`Type: alignment`) and add it to the backlog as `queued`. CLAUDE.md lets these go ahead without the user's confirmation of the spec. When a fix would move, remove or restyle existing game content, change the game's output, or add an engine capability whose scope is unclear, add the task as `blocked` instead, with the question.
5. **Resolve.** For each issue the developer marked `fixed`, check it's actually gone in this cycle's build or review, then set it to `resolved`. If it isn't gone, set it back to `open` and increment `Reopened`.
6. **Commit.** Commit `task.md`, `issues.md` and `friction.md` in the worktree with the message `<task-id> cycle <n>: review`.
7. **Loop.** If any issue is `open`, start the next cycle.

The spec's content sections (Goal, Decisions, Layering, Approach, Verification, acceptance criteria, Assets, Out of scope) are fixed during a cycle, for you as much as for the agents. You keep the cycle log, the status and the criteria's ticks. Never edit a content section to make a change pass review or to close a task. The only way a spec changes is **Replanning**.

Record issues in `issues.md` in this format. Number them sequentially and never reuse IDs.

```
## ISSUE-<nnn>: <short title>
Status: open | fixed | resolved | wont-fix
Severity: critical | major | minor
Source: build | review
Found in cycle: <n>
Reopened: <count>

### Problem
<For build issues, the compiler output trimmed to the relevant lines, with file and line. For review issues, the criterion or behaviour that isn't met, and where in the code.>

### Fix notes
<left blank for the developer>
```

### Stop conditions
Block the task, rather than looping, if:
- the same issue has been reopened 3 times
- the same asset has been sent back to the designer 3 times
- the task reaches 5 cycles without converging, counted from its latest spec revision (a replan starts the count again)
- the task has been replanned twice and the new approach fails too
- the build environment is broken in a way the developer can't fix (missing toolchain, missing dependency). Report status `stop` too: later tasks would hit the same wall.

### Replanning
When the developer returns `SPEC INFEASIBLE`, the designer says an asset can't be built as specified, or your review finds that the spec itself is wrong, the task goes back to planning. You are on Opus at medium effort, as `/feature` is, so you do the planning, as `/feature` would.
1. **Check the claim.** Read the code and the developer's evidence. Run what it ran. If the spec does work and the developer missed how, record an issue explaining how, and send it back as the next cycle. A spec that does work doesn't get changed.
2. **Design an alternative.** Follow `/feature`'s design step (`.claude/skills/feature/SKILL.md`, step 2): work from the code, read the files involved, and keep to `docs/architecture.md`. Prefer the alternative that keeps the most of the spec. Write `docs/tasks/<task-id>/replan-<n>.md`: what failed, with the evidence; the alternatives you considered and why you rejected them; the proposal; and the exact changes to each spec section, with code examples where the spec has them.
3. **Apply it or block.**
   - If the proposal only changes `## Approach` and `## Verification` (how it's built and checked), apply it yourself. The Goal, Decisions, Layering, acceptance criteria, Assets and Out of scope stay as they were, and no existing content changes. Edit those two sections, add an entry to `## Spec revisions` (`Revision <n>, <date>: <what changed and why>, see replan-<n>.md`), update the task's `Touches` in the backlog if they changed, commit `<task-id>: replan <n>`, and start the next cycle. If the new `Touches` overlap a task still running in this batch, hold this task until that one merges or blocks. If it merged, have the developer merge the run's branch into the task branch at the start of the next cycle.
   - Otherwise the user decides, because the change reaches what they agreed in planning: block the task (see **Blocking**). The question points to `replan-<n>.md` and names each section that would change. `/feature unblock` takes them through it.

## 5. Close the task
A task is complete when:
- the latest fresh build passes and the `## Verification` commands show what the spec says
- every acceptance criterion is ticked in `task.md`, with a one-line note naming the evidence (file and function, or command output)
- every issue is `resolved`, or `wont-fix` with your written justification in the issue
- every asset in the spec is in `assets.md`, and every request in `asset-requests.md` is `done`
- everything is committed

Then set `Status: complete` in `task.md`, add a `## Playtest checklist`: what the user should try in-game to confirm the task works, built from the acceptance criteria and the developer's "How to playtest" notes. Nothing has run the game, so the user's playtest is the only behavioural check. Commit: `<task-id>: complete`.

## 6. Merge back
Merge each completed task into the run's branch from the main working tree, one at a time:
```
git merge --no-ff <task-id>
```
- If the merge conflicts, run `git merge --abort`. Then delegate to **game-developer** in the task's worktree: merge the run's branch into the task branch, resolve the conflicts, build and commit. Treat it as a new cycle, with your build check and review. Then merge again.
- After each merge, do a fresh build check of the run's branch in the main working tree, because two tasks can build on their own and fail together. Then run the alignment review in `CLAUDE.md` on the merged diff (`git diff <branch before merge>..HEAD`). Code that only goes wrong in combination becomes a new alignment task.
- **Docs.** Bring `docs/architecture.md` (the "Where things live" table, and any new standing rule the spec records), `docs/engine-guide.md` (APIs, contracts, snippets, which must still match `examples/minimal/main.cpp` word for word) and `README.md` (features, build, flags) up to date with the merged code. You make these edits, after the merge, so parallel tasks never conflict on them. Commit: `<task-id>: docs`.
- Set the task to `done` in the backlog with the merge commit hash, and commit.
- Remove its worktree and branch: `git worktree remove ../<repo-dir>-<task-id>` and `git branch -d <task-id>`. Never force either. If git refuses, something is unmerged or uncommitted: find out what before going on.

A blocked task isn't merged. Its worktree and branch stay for when it's unblocked.

## Blocking
To block a task:
1. In its worktree, set `Status: blocked` in `task.md`, add a cycle log line with the reason, and commit.
2. In the backlog, set it to `blocked` and add a `Questions:` list under its entry. Write each question so the user can answer it cold: what was tried, what the choice is, the options and what you'd pick. Commit.

Tasks that depend on a blocked task wait. `/feature unblock` takes the user through the questions.

## 7. Retro
Every batch ends with a retro, so the next run doesn't hit the same gaps. It covers every task that finished or blocked in this batch.

1. **Gather.** Read each task's `friction.md`, then its `issues.md` and the developer's handoffs for friction nobody wrote down: an issue caused by something the docs don't say, an instruction the agent misread, the same verification commands typed out again. Add your own: anything you had to look up or work out by hand during the batch, and any command sequence you ran more than once.
2. **Log.** Record each item in `docs/friction-log.md`. If an entry already covers it, add this sighting (task ID and date) to its `Seen` line. Otherwise add a new entry.
3. **Act** on each open entry:
   - **Missing documentation** (`doc-gap`): fix it now, on the first sighting. Write the missing fact where the agent looked for it first: `docs/engine-guide.md` for how to use the engine, `docs/architecture.md` for where code goes (only facts the code already shows; new standing rules come from the user through `/feature`), `CLAUDE.md` for building and verifying, `tools/README.md` for tools, `docs/art-style.md` for style the user already settled. Keep it short and next to related text.
   - **Repeated tooling** (`tool`): a script or command sequence written by hand, rather than taken from `tools/`. When the same tool has been written more than twice in all (three sightings, across tasks and runs), turn it into a proper tool: write a `Type: tooling` spec for a script in `tools/` (or a flag on an existing one), naming the copies in the task folders' `scripts/` to build on, and queue it at the top of the backlog, ahead of the tasks that would use it. The developer builds it. When it's done, document it in `tools/README.md` and wherever the agents were told to do that job by hand.
   - **Unclear instructions** (`instruction-gap`): when an agent or you went wrong because a skill or agent file left something unclear, add the missing instruction to that file (`.claude/agents/*.md`, `.claude/skills/*/SKILL.md`). Write it as a general instruction, not a story about the task.
   - Set each entry you acted on to `actioned`, with what changed and the commit.
4. **Limits.** The retro clarifies and adds. It never changes these, even to make a run go smoother: the agents' and skills' models and effort, permissions and settings, the hard rules and the alignment review in `CLAUDE.md`, the stop conditions, the review and build gates, the existing-content rule, and anything about what belongs in the engine or the game. An entry that needs one of these changed is set to `needs-user` with a proposed change, and goes in the report.
5. **Commit** in the main working tree: `retro: <one line>`, listing the friction entries actioned. Keep workflow changes (`.claude/`) in their own commit, `retro: workflow: <one line>`, so the user can review or revert them on their own.

## Report
End with a short report:
- each batch task: status, number of cycles, issues found and resolved, `wont-fix` decisions, assets created, merge commit;
- tasks blocked, with their questions;
- alignment and tooling tasks added to the backlog;
- what the retro changed, and any `needs-user` entries;
- the playtest checklist of each task merged this batch.

The last line of the report is the status line, exactly one of:
- `BACKLOG-RUN: continue`: the batch finished and at least one queued task is now ready to run.
- `BACKLOG-RUN: done`: nothing is left to run. Every task is done, blocked, or waiting on a blocked task.
- `BACKLOG-RUN: stop`: the run can't go on without the user (the run's branch is `main`, the tree is dirty, the build environment is broken). Say why above it.
