# Agent context contract

## Purpose

Any idle agent must be able to pick up any unowned issue without relying on a
previous session's private context. The shared brain is the source of truth;
the issue document and journal are evidence, not a conversation transcript.

## Required issue context

`agent-coord.py issue brief <issue-id>` returns one bounded JSON document with:

- `issue`: title, verbatim user quote, summary, state, severity, priority, kind,
  source, reporter, via agent, owner, reviewers, reproduction, expected behavior,
  acceptance, clarification, defer reason, resolution, related build/run, paths.
- `issue` analysis: append-only observations and proposals with a claim status
  (observed, inferred, hypothesized, disproved, or unknown), evidence links,
  limitations, and the next useful check.
- `quotes`: all preserved user and reviewer quotes in append-only order.
- `occurrences`: every recurrence, with launch/build/run evidence and time.
- `tasks`: ordered steps, owner, dependencies, acceptance, proof status, and
  stale-proof reason.
- `decisions`: decisions that name this issue, with rationale and alternatives.
- `leases`: active edit leases for issue paths and conflicting owners.
- `builds`: linked build requests, status, generation, DLL path/hash, and cause.
- `runs`: linked runtimes, health, journal path, proven events, and failures.
- `regressions`: related confirmed regression records.
- `blockers`: only current hard blockers, each with an owner and a wait budget.
- `next`: the smallest legal next action, or `wait` with a reason.
- `causality`: the last relevant events ordered by global sequence, with explicit
  parent event IDs where available.
- `handoff`: current owner/activity, tested and untested methods, open
  hypotheses, known counterevidence, safe next steps, and any decision that
  requires the user.

## Cold-handoff minimum

An agent taking an issue without the original chat must be able to distinguish
what happened from what is only suspected and reproduce the next useful check.
The issue and linked artifacts should include:

- the exact user report or observation, current expected behavior, steps and
  launch/configuration needed to reproduce it;
- current owner, task dependencies, code owner/function/path, blockers, wait
  reason, and the smallest safe next action;
- append-only claims labeled observed, inferred, hypothesized, disproved, or
  unknown, with timestamp, confidence/limits, counterevidence, and the next
  test that could discriminate between explanations;
- exact build/EXE identity, source revision or hashes where relevant, game-DLL
  generation, mode/map/profile, run/instance/PIDs, and paths to the matching
  dev-loop, server, and client journals;
- exact journal event names/IDs, ticks/timestamps, measurement conditions, and
  relevant screenshot/video/audio sidecars with hashes when visual evidence is
  necessary. A screenshot alone is not runtime behavior proof;
- all attempted reproductions and their outcomes, including failed tools,
  failed builds, missing artifacts, and contrary results;
- measurable acceptance and proof expectations, correctness/privacy risks,
  rollback or recovery path, and decisions that still require the user.

If an artifact or identity is unavailable, say so explicitly and link the
attempt that failed. Never fill a gap from private memory or imply a run proves
a scenario it did not exercise. Do not copy secrets or unrelated private data
into an issue. A cold reviewer should be able to continue from this brief and
the linked evidence without reconstructing hidden conversation context.

## Contract rules

- The document is generated, bounded, and safe to read repeatedly; reading it
  does not mark messages or claims as consumed.
- It contains paths and identifiers, not full source files or chat logs.
- A stale task proof is shown as stale, never silently accepted.
- A closed issue still exposes its quote, resolution, proof, and regression
  links so later agents can audit it.
- A preview build or a synthetic unit proof cannot satisfy integration behavior;
  the document states the allowed proof level.
- If required context is missing, the command reports the missing fields and
  keeps the issue unowned rather than inventing history.
- The user's original words remain verbatim. An agent may add interpretation
  only in a separate summary or comment.
- The issue and linked artifacts must be sufficient for any qualified agent
  to continue without private chat history. Keep source quotes, claims,
  evidence, task outcomes, and unresolved questions distinct and append-only.
- Agents add new evidence and tests to the same issue over time, including
  failed attempts and counterexamples. A claimed improvement includes
  comparable measurements and the conditions needed to reproduce them.

## First use

Before claiming an issue, an agent runs:

```powershell
python devscripts/agent-coord.py --agent <name> issue brief <issue-id>
```

If the document is complete, the agent runs `next --claim`; if it is not, the
agent files the missing context as a blocker with evidence and stops.

## Acceptance

Focused tests prove deterministic ordering, bounded output, stale-proof
visibility, missing-field reporting, and that reading the context does not
mutate message or claim state.
## User-context bus

Every user utterance is one immutable `user_context` record. It stores the exact
quote, content hash, source agent/session, stable source reference, priority,
tags, creation time, one primary issue, and any related issue links. Quotes are
never rewritten or deleted.

The router:

- `agent-coord context add` records and broadcasts the exact quote.
- The OpenCode `prompt` plugin records every user prompt automatically; other
  runtimes must call the same command at activation or through a relay.
- `sync` returns unread and latest user context; `brain_check` includes both.
- `issue brief` shows all linked user context.
- `context move` records a routing correction with from/to issues, agent, and
  reason. The old link is deactivated, not deleted.
- `context lint` reports unlinked context, multiple primary issues, and stale
  canonical links.
- A disputed or ambiguous route creates a `context-routing` issue and notifies
  the affected owners.

Normal context is delivered at the next activation. High-priority context also
queues wake requests for active agents. The user's exact words are the durable
source of truth; summaries and interpretations are separate fields.

