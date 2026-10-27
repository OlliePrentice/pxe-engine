---
name: feature
description: Runs a new feature or fix end to end. Interviews the user with the grill skill, writes the specification, delegates assets to the designer and code to the game-developer, checks the build, reviews the code against the spec, and finishes with a playtest checklist.
argument-hint: <feature or fix to build>
disable-model-invocation: true
---

Run this workflow for the request: $ARGUMENTS

You orchestrate. You interview, specify, delegate, check and review. You do not write game code or create assets yourself: code goes to the **game-developer** agent and assets to the **designer** agent. If you notice a quick fix you could make yourself, delegate it anyway, so every change goes through the same review.

## Task folder
All state lives in `docs/tasks/<task-id>/`, where `<task-id>` is a short kebab-case slug.
- `task.md`: the specification, written by you
- `issues.md`: build errors and review findings, written by you
- `handoff.md`: written by game-developer
- `assets.md`: written by designer, the manifest of every asset made for the task
- `asset-requests.md`: written by game-developer, change requests for the designer

The files are the source of truth. At the start of every cycle, re-read `task.md`, `issues.md` and `asset-requests.md` rather than relying on earlier conversation. Long runs get compacted.

You are the only one who can talk to the user. The agents can't ask questions, so every question for the user goes through you.

## 1. Interview the user
Every new feature or fix starts with an interview. Don't write a spec from the opening request alone.

1. Read enough of the codebase to ask informed questions: the systems the request touches and the existing behaviour. Read `docs/architecture.md` too.
2. Run the interview using the **grill** skill. Follow its process until it is complete.
3. Keep going until you can write every acceptance criterion as a concrete statement you can check by reading the code, and you know what is out of scope. For a fix, you also need the reproduction steps and the expected behaviour.
4. When the user's answer conflicts with the codebase or with an earlier answer, say so and resolve it before moving on.
5. **Existing content stays put.** A feature must not move, remove, resize or restyle anything already in the game unless the user explicitly says so. This covers placed objects, props, trees, characters and their beds or spawn points, existing assets, and tuned constants. If the feature might need such a change, ask the user, naming each thing that would change, and record their answer in `## Decisions`. Never decide it yourself, even when the user has said to proceed as you see fit: that covers choices within the feature, not changes to what was already there.
6. **Engine or game.** Decide where each piece of the feature lives, following `docs/architecture.md`. Settle it yourself where the rules settle it. Ask the user only when it's a real judgement call: a new engine capability whose scope is unclear, or a feature in an area the doc marks misplaced. If an answer sets a standing rule, add it to `docs/architecture.md`.
7. If the task needs new or changed assets, cover them in the interview:
   - **Style:** read `docs/art-style.md` first and only ask about what it doesn't settle. If it doesn't exist, ask about the overall art style and create it from the answers. It holds the standing style for the whole project, so the user isn't asked the same questions on every task.
   - **Each asset:** what it is, its proportions, level of detail and references.
   - **Technical needs:** check the engine code for the format, units, axes and limits the loader expects. Ask the user only about what the code doesn't settle.

## 2. Write the specification
Turn the interview into `docs/tasks/<task-id>/task.md`. This is the developer's and designer's specification, so record decisions, not the conversation.

```
# <Task title>
Type: feature | fix
Status: open | complete | blocked
Cycle: 1

## Goal
<One paragraph: what the feature must do and why, or what is broken and the correct behaviour.>

## Decisions
- <Each design decision from the interview, with the user's reasoning where they gave it.>

## Layering
- Engine: <each new or changed engine capability, named for what it does, not for game content; or "none">
- Game: <which game code creates and owns each new object, and how it uses the engine>

## Acceptance criteria
- [ ] <Concrete criterion you can check by reading the code>
- [ ] Project builds cleanly from a fresh build directory with CMake

## Assets
<"None" if the task needs no assets. Otherwise, one entry per asset:>
- **<asset-name>**: <what it is, style notes, references>
  - Format: <e.g. glb>
  - Units and axes: <e.g. 1 unit = 1 metre, +Y up, -Z forward>
  - Origin: <e.g. base centre>
  - Budget: <triangle count, texture resolution>
  - Animations: <names, or "none">

## Out of scope
- <Anything explicitly excluded>

## Cycle log
- Cycle 1: <date> started
```

For a fix, add a `## Reproduction` section with the exact steps and the observed versus expected behaviour.

Nobody runs the game during this workflow, so you verify the criteria yourself by reviewing the code. Write them so that's possible. Show the user the specification and get their confirmation before starting the cycle. The exception is an alignment task created by the alignment review in `CLAUDE.md`: it starts without confirmation. Change it if they correct anything.

## 3. Run the cycle
Every change is committed, so any cycle can be undone with `git revert`. The designer and developer each commit their own work and report the hash. You commit `task.md` and `issues.md` yourself. Before cycle 1, make sure you're on a branch for the task, not `main`, and that the working tree is clean. If it isn't, ask the user. At the start of each cycle, record the current `HEAD` hash in the cycle log line: it is the cycle's base.

1. **Assets first.** If the `## Assets` section lists assets that don't exist yet, or `asset-requests.md` has requests with `Status: open`, delegate to **designer** with the task folder path and the request IDs.
   - If the designer returns `NEEDS INPUT`, ask the user its questions. Record the answers in the `## Assets` section of `task.md`, and in `docs/art-style.md` if they set project-wide style. Then delegate to the designer again.
   - If a request comes back `needs-decision`, decide it, or ask the user if it's a style call. Record the decision in the request.
   - Check the designer's preview images before moving on. If an asset clearly doesn't match the spec, send it back to the designer with the reason.
2. **Code.** Delegate to **game-developer**. Pass the task folder path and, from cycle 2 on, the issue IDs it must fix.
   - If it returns `BLOCKED ON ASSETS`, go back to step 1 for its new requests in `asset-requests.md`, then delegate to the developer again.
   - When it returns, confirm `handoff.md` has an entry for this cycle reporting a successful build, and that the cycle's work is committed (`git status` shows nothing of the developer's or designer's left uncommitted). If not, send it back with the reason.
3. **Build check.** Build from a fresh directory yourself. Don't trust the developer's report:
   ```
   cmake --preset check --fresh
   cmake --build --preset check --clean-first
   ```
   `--clean-first` matters: `--fresh` alone keeps the object files, so unchanged files aren't recompiled and their warnings don't show. Record each distinct compile error as a `critical` issue. Record new warnings as `minor` issues, but only in files listed in the handoff.
4. **Review.** Read the cycle's diff (`git diff <cycle base>..HEAD`) against the spec. Check that it leaves existing content alone unless `## Decisions` names the change. Check the layering against `## Layering`, and run the **alignment review** in `CLAUDE.md` against `docs/architecture.md` and `docs/engine-guide.md`. Discrepancies in this cycle's diff are issues. Discrepancies it reveals in code the task didn't touch become new alignment tasks, delegated without asking the user (see `CLAUDE.md` for when to stop and ask instead). For each acceptance criterion, find the code that satisfies it. Record anything that's missing or wrong as an issue, along with obvious regressions in code the change touched. Check that every asset the developer loads matches its `assets.md` entry.
5. **Resolve.** For each issue the developer marked `fixed`, check it's actually gone in this cycle's build or review, then set it to `resolved`. If it isn't gone, set it back to `open` and increment `Reopened`.
6. **Commit.** Commit `task.md` and `issues.md` with the message `<task-id> cycle <n>: review`.
7. **Loop.** If any issue is `open`, increment `Cycle` in `task.md`, add a cycle log line listing the outstanding issue IDs, and go back to step 1.

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

## Several tasks in one run
A request can cover more than one task, for example a list of refactors. Interview and specify each task as above, one task folder each, and get the user's confirmation of every spec before starting.

Then decide which tasks run in parallel. Run tasks in parallel when they are independent: none needs another's result, and they are expected to change different files. Run them in sequence when one depends on another, or when they would edit the same files, since the merge back would conflict. When in doubt, run in sequence. Tell the user the plan: which tasks run together and which wait.

### Worktrees
Each task that runs in parallel gets its own git worktree on its own branch, so the agents never share a working tree, an index or a build directory. Create them yourself from the run's branch (the branch the run started on, never `main`):
```
git worktree add ../<repo-dir>-<task-id> -b <task-id>
```
Don't use the Agent tool's own worktree isolation: it makes a new temporary worktree per delegation, and a task needs the same worktree for every cycle.

Everything for a task happens in its worktree:
- Its task folder, cycle base, `task.md` and `issues.md` edits and their commits.
- Every delegation. Pass the worktree's absolute path along with the task folder, and tell the agent to work, build and commit only there.
- Your build check, run from the worktree so the `check` preset builds in `<worktree>/build-check`: `cd <worktree> && cmake --preset check --fresh && cmake --build --preset check --clean-first`.
- Your review: `git -C <worktree> diff <cycle base>..HEAD`.

### Running them
- Delegate the developers for parallel tasks in the same turn, so they run at the same time. Run each task's cycles as in step 3, independently of the others.
- Only one designer runs at a time: assets are built in the user's single live Blender session. Queue designer work across tasks and run it one task after another. The developers can carry on meanwhile.
- A task that stops or blocks doesn't stop the others. Report it with the rest.
- Questions for the user from several tasks go to the user together, each labelled with its task ID.

### Merging back
When a task is complete, merge it into the run's branch from the main working tree:
```
git merge --no-ff <task-id>
```
- If the merge conflicts, run `git merge --abort`. Then delegate to **game-developer** in the task's worktree: merge the run's branch into the task branch, resolve the conflicts, build and commit. Treat it as a new cycle, with your build check and review. Then merge again.
- After a task merges, do a fresh build check of the run's branch, because two tasks can build on their own and fail together. Then run the alignment review in `CLAUDE.md` on the merged diff. Code that only goes wrong in combination becomes a new alignment task.
- Then remove its worktree and branch: `git worktree remove ../<repo-dir>-<task-id>` and `git branch -d <task-id>`. Never force either. If git refuses, something is unmerged or uncommitted: find out what before going on.

## 4. Close the task
Set `Status: complete` only when:
- the latest fresh build passes
- every acceptance criterion is ticked in `task.md`, with a one-line note naming the evidence (file and function)
- every issue is `resolved`, or `wont-fix` with your written justification in the issue
- every asset in the spec is in `assets.md`, and every request in `asset-requests.md` is `done`
- `docs/architecture.md`, `docs/engine-guide.md` and `README.md` match the code as the task leaves it (see the alignment review in `CLAUDE.md`)
- everything is committed, the closing `task.md` included

## Stop conditions
Set `Status: blocked` and report to the user, rather than looping, if:
- the same issue has been reopened 3 times
- the same asset has been sent back to the designer 3 times
- the task reaches cycle 5 without converging
- an issue needs a design decision outside the task's scope
- the build environment is broken in a way the developer can't fix (missing toolchain, missing dependency)

## Report
Finish with: task status, number of cycles, issues found and resolved, any `wont-fix` decisions, assets created, and anything left for the user to decide. For a run of several tasks, report each task this way, then which branches merged.

End with a **Playtest checklist**: a short list of what the user should try in-game to confirm the task works, built from the acceptance criteria and the developer's "How to playtest" notes. Nothing has run the game, so the user's playtest is the only behavioural check.
