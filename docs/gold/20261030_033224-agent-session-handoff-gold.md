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

## Long-running goals survive turns and session resets

- Status: `GOLD REFERENCE / RECOMMENDED_PRACTICE` — requested by the project owner and supported by this latency investigation; not yet a controlled measure of agent completion quality.
- Audience: agents pursuing a user-authorized goal across multiple turns or work activations.
- Purpose: keep one durable objective moving for as long as needed, including when the user says only “continue,” without requiring them to repeat the whole request.

Treat a follow-up such as “continue” as steering for the active goal unless the user clearly cancels it or asks for an incompatible objective. Carry forward the original target, later clarifications, safety constraints, decisions, completed work, verified evidence, and unresolved gaps. At each activation:

1. Read the current goal and check the coordination brain, user context, build/run status, active leases, and proofs.
2. Continue from the newest verified repository state. Do not redo completed work, trust stale memory over current files, or treat a prior attempt as proof of success.
3. Make the next concrete action advance the goal. While a dependency or build is waiting, do useful independent investigation, measurement, or issue updates; do not wait silently.
4. Record a short checkpoint with exact artifacts, measurements, failed attempts, uncertainty, and the next discriminating action. A fresh session may take over from that checkpoint without making the user restate the goal.
5. Keep the goal active while required outcomes remain. A failed optional tool, an elapsed-time boundary, a context reset, or one successful partial optimization does not complete the goal. Mark it complete only when its stated acceptance is met and the required evidence exists; record the remaining blocker if progress needs user input or an external change.

This practice supports long work while retaining the fresh-session handoff above: continuity belongs in the explicit goal, issue, and checkpoint, not in an indefinitely growing chat transcript.

The project owner explicitly praised this behavior during the ongoing C++ edit-to-live latency goal: “add gold behaivor of pursuing goal mode i think thisi s great and should domore like this letting ai work for long long times bc thats what id end up doing just sending same thign anyway continue”. Treat repeated “continue” messages as authorization to keep advancing that same active goal. Do not interpret them as a request to restart the investigation, close a partially completed goal, or ask the owner to repeat already-recorded requirements. This example is evidence of the requested workflow, not evidence that the latency target has been met.

## Optional tools and accelerators fail open

- Status: `RECOMMENDED_PRACTICE` — based on the WPR permission failure and the optional presentation-DLL failure observed in the MiMITA latency investigation.
- Audience: agents and tools adding optional profilers, CPU samplers, tracing, or accelerated build/reload paths to a workflow.
- Purpose: keep failure of an optional capability from failing an otherwise valid primary build, while making degraded or missing live-reload behavior explicit.

Keep each optional capability separate from the primary build result:

- If an optional profiler cannot start, record its exact error and status as unavailable, continue the build, and use a safe evidence fallback such as existing stage timings or process-level sampling.
- If an optional fast build or reload artifact fails, let the required EXE or DLL build continue when it remains independently valid. Record the failed artifact, error, duration, and attempt ID; suppress unchanged repeated failures for a bounded period, while allowing changed inputs or an explicit retry to run again.
- Do not claim a requested live behavior is active when the failed optional artifact was the only path that could activate it. Report the primary build result, optional-path result, and live activation result separately; use the supported cold or existing-runtime fallback only when that fallback is valid for the requested change.
- A profiler or optional fast-path failure must not fail an otherwise valid required build or report the goal blocked when safe fallback evidence or execution remains available.
- Preserve failures in required correctness gates. Compiler, linker, generated-source validation, source-coherence, and cache-freshness failures still fail the build; fail-open applies only to optional capabilities whose failure leaves a valid primary result.
- Report build, optional-capability, diagnostic, and activation statuses separately so “EXE succeeded, presentation DLL unavailable, live reload not activated” is unambiguous.

For a required live-reload candidate, fail the candidate transaction without taking down a healthy running game: never publish partial objects or advance `current.json` after a required compile, link, coherence, or freshness failure. Keep the last-good generation available, report the exact failed attempt and source revision, and accept a corrected or newer revision as a fresh candidate. Do not mark the requested edit active until the runtime journal proves that exact generation was loaded and committed. This is failure containment, not fail-open compilation: invalid code must remain rejected, while the longer user-authorized goal continues through diagnosis and a corrected attempt.

After the runtime reports activation, keep the proof state separate from sustained health: observe the owned server/client through a defined post-swap window and report any process exit, crash, or frozen peer. A successful build and `game.generation.activated` event prove publication and loading, not that the game stayed healthy afterward. Continue the goal through crash attribution or a verified healthy run; do not restart a peer while another task owns it without coordinating first.

In this investigation, `wpr -start CPU -filemode` returned `0xc5585011` because Windows would not enable system-performance profiling policy. WPR did not start recording and no game build was attempted as part of that call. The investigation continued using the dev-loop timing JSONL, runtime journal, and a Python/psutil process sample. This was a failed optional measurement tool, not a failed build.

## Related project documents

- `AGENTS.md` — permanent working rules and evidence gates.
- `docs/ROUTER.md` — required reading routes.
- `docs/operations/task-completion/task-completion.md` — completion and handoff procedure.
- `docs/changelog/` — one final record for each AI work session.
- `docs/regressions/` — confirmed failures and permanent lessons.
