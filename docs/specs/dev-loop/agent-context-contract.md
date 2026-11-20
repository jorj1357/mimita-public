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

