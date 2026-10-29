---
name: feature
description: Plans a new feature or fix. Interviews the user with the grill skill, settles the architecture, writes one implementation-ready spec per task and queues the tasks in docs/backlog.md for /backlog-run. Also answers blocked tasks' questions. Does not implement.
argument-hint: <feature or fix to plan> | unblock
disable-model-invocation: true
model: claude-opus-5-5
effort: high
---

Run this workflow for the request: $ARGUMENTS

You are the planner and architect. You interview the user, decide the architecture, split the work into tasks, write a spec for each task that a developer can implement without guessing, and queue the tasks in `docs/backlog.md`. You don't implement anything and you don't delegate to the developer or the designer. `/backlog-run` does that later, unattended, usually in a loop (`tools/backlog-loop.sh`). Nobody can answer a question during that run, so every decision has to be made here.

**Model check.** Planning runs on Opus 5.5 at high effort. The `model` and `effort` lines above only hold for the turn that loads this skill, and the interview takes many turns. If the session isn't Opus 5.5 at high effort (`${CLAUDE_EFFORT}` should read `high`), tell the user to restart with `claude --model claude-opus-5-5 --effort high` and stop.

**Branch.** Planning commits to a branch, never to `main`. If the session is on `main`, create a branch named for the run (`git switch -c <slug>`) before the first commit, and tell the user. `/backlog-run` runs on the same branch.

If `$ARGUMENTS` is `unblock`, go to **Unblocking** at the end.

## Task folder
Each task's state lives in `docs/tasks/<task-id>/`, where `<task-id>` is a short kebab-case slug. Planning writes `task.md`. The run adds `issues.md`, `handoff.md`, `assets.md`, `asset-requests.md` and `friction.md` (see `/backlog-run`).

## 1. Interview the user
Every new feature or fix starts with an interview. Don't write a spec from the opening request alone.

1. Read enough of the codebase to ask informed questions: the systems the request touches and the existing behaviour. Read `docs/architecture.md` and `docs/engine-guide.md` too. Check `docs/friction-log.md` for open doc gaps in the area: they are things earlier runs couldn't find written down.
2. Run the interview using the **grill** skill. Follow its process until the frontier is empty: every open question has been put to the user and answered. Skip the grill's closing request to confirm shared understanding: the written specs replace it.
3. Keep going until you can write every acceptance criterion as a concrete statement you can check by reading the code, and you know what is out of scope. For a fix, you also need the reproduction steps and the expected behaviour.
4. When the user's answer conflicts with the codebase or with an earlier answer, say so and resolve it before moving on.
5. **Existing content stays put.** A feature must not move, remove, resize or restyle anything already in the game unless the user explicitly says so. This covers placed objects, props, trees, characters and their beds or spawn points, existing assets, and tuned constants. If the feature might need such a change, ask the user, naming each thing that would change, and record their answer in `## Decisions`. Never decide it yourself, even when the user has said to proceed as you see fit: that covers choices within the feature, not changes to what was already there.
6. **Engine or game.** Decide where each piece of the feature lives, following `docs/architecture.md`. Settle it yourself where the rules settle it. Ask the user only when it's a real judgement call: a new engine capability whose scope is unclear, or a feature in an area the doc marks misplaced. If an answer sets a standing rule, add it to `docs/architecture.md`.
7. If the task needs new or changed assets, cover them in the interview:
   - **Style:** read `docs/art-style.md` first and only ask about what it doesn't settle. It holds the standing style for the whole project, so the user isn't asked the same questions on every task.
   - **Each asset:** what it is, its proportions, level of detail and references. The designer can't ask during the run, so settle everything its `NEEDS INPUT` questions would cover: shading, triangle budget, texture size, which existing nodes must stay byte-identical.
   - **Technical needs:** check the engine code for the format, units, axes and limits the loader expects. Ask the user only about what the code doesn't settle.
8. Think about what you'd ask the user in the middle of the run, and ask it now: a tuning value, a fallback if an approach doesn't work out, how far a fix should reach. A question left open turns into a blocked task.

## 2. Design the architecture
Before you write the specs, work out how the feature will be built, from the code:
- The engine capabilities and game code it needs, their names, and the types, functions and data that cross the boundary between them.
- Each file that changes and what changes in it. Read those files: the spec names real functions, not guesses.
- The order the work has to happen in, and which parts can be built independently.
- How the result is verified: which `--capture` views, which byte-identical cases, which grep.

Then split the work into tasks. A task is one reviewable change that builds on its own and leaves the game working. Prefer tasks that touch different files, because those can run in parallel. Split a large feature by layer or by system when the pieces are independent, but don't split one change across tasks just to make them smaller: a task that only makes sense together with another adds a dependency and a merge for nothing.

## 3. Write the specs
Write `docs/tasks/<task-id>/task.md` for each task. This is the specification for the game-developer (Sonnet) and the designer, and the reviewer checks against it. Record decisions, not the conversation.

**Make it implementation-ready.** The developer should not have to make a design decision. Name the files, types and functions. Where prose doesn't pin the design down, show it in code: a new struct or class declaration, a function signature, the shape of a shader change, the call sequence in the frame loop, a tricky formula. Keep code examples to interfaces and the hard parts. Don't write out the whole implementation. If the developer would write it the obvious way, leave it in prose. Mark each example as binding (implement this exactly) or a sketch (follow its shape).

```
# <Task title>
Type: feature | fix | alignment | tooling
Status: queued
Cycle: 0

## Goal
<One paragraph: what the task must do and why, or what is broken and the correct behaviour.>

## Decisions
- <Each design decision from the interview, with the user's reasoning where they gave it.>

## Layering
- Engine: <each new or changed engine capability, named for what it does, not for game content; or "none">
- Game: <which game code creates and owns each new object, and how it uses the engine>

## Approach
<How to build it, file by file: what changes in each file and why, in the order it should be done. Name existing functions and types. Code examples where prose isn't enough, each marked binding or sketch.>

## Verification
<The exact commands the developer and reviewer run: --capture views with their flags, byte-identical cases against the cycle base, greps. What each should show.>

## Acceptance criteria
- [ ] <Concrete criterion you can check by reading the code or from a command's output>
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

## Spec revisions
<None. Only a replan adds entries here.>

## Cycle log
```

For a fix, add a `## Reproduction` section with the exact steps and the observed versus expected behaviour.

**Specs are fixed once queued.** The developer and designer implement the spec exactly and never change it. When it can't be implemented as written, the run replans it on Opus (`/backlog-run`, **Replanning**). That replan may change only `## Approach` and `## Verification` on its own. Anything else comes back to the user. So put everything the user agreed in the Goal, Decisions, Layering, acceptance criteria, Assets and Out of scope, and keep `## Approach` to how it's built.

Nobody runs the game during the run, so the reviewer verifies the criteria by reading the code and running the commands in `## Verification`. Write them so that's possible.

## 4. Queue the tasks
Add each task to `docs/backlog.md`, in the order it should run, using the entry format at the top of that file. For each task:
- **Depends on:** the task IDs that must be `done` first, or `none`.
- **Touches:** the files or directories it is expected to change. `/backlog-run` runs two tasks in parallel only if neither depends on the other and their `Touches` don't overlap, so be specific. Shared files such as `CMakeLists.txt` or `engine/CMakeLists.txt` count. Leave out `docs/architecture.md`, `docs/engine-guide.md` and `README.md`: the run updates those itself after each merge.
- **Assets:** `yes` if the designer has work, otherwise `no`. Asset tasks need the user's Blender open with the MCP connected while the run goes.

Commit the specs and the backlog together: `plan: <feature> (<task-ids>)`. Then tell the user:
- the tasks, one line each, and which can run in parallel;
- where the specs are;
- how to run them: `tools/backlog-loop.sh` for the unattended loop, or `/backlog-run` for one batch in this session;
- whether Blender needs to be open.

Don't start implementing. If the user corrects a spec later, change it and commit before the run.

## Unblocking
`/backlog-run` sets a task to `blocked` when it needs a decision nobody could make during the run, and writes the questions under the task's backlog entry.
1. Read every `blocked` entry in `docs/backlog.md`, with its task folder (`task.md`, `issues.md`, the latest `handoff.md` entry).
2. Put the questions to the user, each labelled with its task ID. Grill where the answer opens new questions. For a task blocked on a replan, present its latest `replan-<n>.md`: what failed, the proposal and each section it would change. The user approves it, picks another alternative, or redirects the task. Record the agreed change in `## Spec revisions`.
3. Record the answers in each `task.md` (`## Decisions`, `## Approach`, `## Assets`) and add a cycle log line `Unblocked: <date>, <one line>`. If the task has a worktree (`git worktree list`), its `task.md` is the one in the worktree: edit it there, set `Status: open`, and commit on the task branch. Where the answer changes the plan, revise the specs of the later tasks that depend on it too.
4. In the backlog, set the task to `in-progress` if it has a worktree (the run resumes it there) or `queued` if not, remove the answered questions, and commit: `plan: unblock <task-ids>`.
