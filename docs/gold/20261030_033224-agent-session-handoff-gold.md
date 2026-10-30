# Gold reference: fresh agent sessions and handoffs beat repeated compaction

- Status: `CONFIRMED_OPERATIONAL_PRACTICE` — observed repeatedly in this repository's AI workflow; not a controlled benchmark.
- Audience: people directing an AI coding agent, and agents writing a handoff for the next session.
- Purpose: record the project practice that one long agent session repeatedly compacted becomes cluttered and less reliable, and that starting a fresh agent session with a written handoff is the fix.

## This reference does not

- Claim a scientific measurement of model quality or a universal law for every agent.
- Say that `/compact` is useless. It is useful inside one coherent task.
- Replace `AGENTS.md`, `docs/ROUTER.md`, the specifications, the changelog, or the regression record.
- Excuse a bad handoff. A fresh session without a correct handoff only loses context faster.
- Allow a new agent to trust the handoff over the repository. The repository remains authoritative.

## The observed problem

Working with one agent for a long time and using `/compact` repeatedly can keep the session alive while slowly degrading its behavior:

- old plans and abandoned attempts remain in the summary;
- superseded instructions compete with current ones;
- exact file paths and rejection reasons disappear;
- the agent re-reads the same files, repeats failed fixes, or forgets an invariant;
- summaries summarize earlier summaries, so drift compounds;
- the agent starts treating its own earlier conclusions as facts instead of checking the repository again.

The result is not usually one obvious failure. It is a gradual loss of focus: more repetition, more confident wrong assumptions, and more work that has to be undone.

## Why repeated compaction drifts

Compaction is lossy by design. It keeps a compressed version of the conversation, but the next agent still inherits the shape of the old session: its unfinished ideas, its phrasing, its assumptions, and its unresolved branches. Repeating that process does not reset the state; it preserves a smaller, less precise version of the clutter.

More context is not automatically better context. Once a session contains several unrelated tasks, failed approaches, and stale tool output, the useful signal is buried. A fresh session is a real reset of working memory. The handoff is the deliberate transfer of only the useful state.

## The solution this project adopted

Use a fresh agent session when the current session has become a long multi-task history or has been compacted repeatedly.

1. The old agent writes a short handoff before stopping.
2. The human starts a new agent session.
3. The new agent reads `AGENTS.md`, `docs/ROUTER.md`, the routed documents for the task, and then the handoff.
4. The new agent verifies the repository state, build identity, and runtime evidence before changing anything.
5. Work continues from the current state, not from the old transcript.

This is the project's answer to session clutter: do not keep compressing one increasingly noisy session. Make a clean session and hand off the facts.

## When a fresh session is the right move

Use a fresh session when:

- the session has covered several distinct tasks;
- `/compact` has already been used more than once;
- the agent is repeating work or contradicting its earlier decisions;
- the agent cannot state the current build, run, or evidence path reliably;
- the next task is large enough to deserve a clean plan.

A single coherent task can usually stay in one session. A long debugging journey with several abandoned hypotheses is a strong candidate for a handoff and restart.

## Required handoff contents

Keep the handoff short and factual. The next agent should not need the old transcript.

```text
# Handoff

## Current goal
One sentence. What must be true when this task is done?

## Current state
Done, in progress, blocked, or waiting for human review.

## Exact files changed
Paths only. Note which changes are mine and which were already present.

## Exact evidence
Build number, run id, events.jsonl path, scenario names, and the last known result.

## Decisions and rejected options
What was chosen, why, and what must not be re-tried.

## Constraints and invariants
The rules from AGENTS.md and the routed specs that the next agent must not break.

## Next concrete step
The smallest next action, with the exact file or command.

## Do not redo
Work that is already complete or approaches already proven wrong.

## Open questions
Only real unanswered questions that need a human or a runtime check.
```

## New-session startup checklist

The new agent should:

1. Read `AGENTS.md` and `docs/ROUTER.md`.
2. Read the routed specification and workflow documents for the task.
3. Read the handoff.
4. Run `git status` and inspect the current diff before editing.
5. Confirm the build number, `EVENTS_JSONL_PATH`, and run id from the handoff or the dev loop.
6. Restate the goal and the next step in its own words.
7. Continue from the repository and the evidence, not from memory of the old chat.

If the handoff and the repository disagree, the repository wins and the disagreement is reported.

## What this fixes in practice

A fresh session with a correct handoff:

- removes stale plans and abandoned attempts from working memory;
- keeps the current goal and the exact evidence path;
- makes the agent re-check the repository instead of trusting a long summary;
- reduces repeated fixes and contradictory edits;
- keeps the project rules in the routed documents instead of in a compressed transcript.

## Evidence and limits

This practice is confirmed by repeated use in this project's AI workflow. It is not a controlled experiment, and it does not prove that every model degrades at the same rate or that every fresh session will perform better.

The honest claim is smaller and still useful: when a session has accumulated unrelated work and repeated compaction, a fresh session plus a factual handoff produces cleaner work than continuing to compress the same session.

## Related project documents

- `AGENTS.md` — permanent working rules and evidence gates.
- `docs/ROUTER.md` — required reading routes.
- `docs/operations/task-completion/task-completion.md` — completion and handoff procedure.
- `docs/changelog/` — one final record for each AI work session.
- `docs/regressions/` — confirmed failures and permanent lessons.
