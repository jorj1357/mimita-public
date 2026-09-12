# Live JSONL observability: the evidence engine for AI-assisted development

Date recorded: 2026-09-12
Status: GOLD REFERENCE / TARGET DIRECTION
Scope: live hot-code development, authoritative runtime evidence, and AI
diagnosis while MiMITA.exe remains running

## Why this is important

The live event journal is more than a log file. It is the running game's
explanation of what happened, when it happened, which code was active, and
where observed behavior diverged from the intended contract.

The long-term development loop is:

```text
human or AI asks a question
-> running MiMITA records the relevant evidence
-> hot code is edited and saved
-> candidate is compiled and validated
-> behavior activates without restarting the process
-> JSONL records the result
-> AI explains the first divergence using the evidence
```

This makes a fast model useful as an interpreter of the live repository and
runtime rather than only as a generator of patches.

## Required event shape

Every important event is one flushed JSONL object. It must include, when
available:

```json
{
  "ts_utc": "2026-09-12T17:03:34.207Z",
  "mono_ms": 215354779,
  "simulation_tick": 34544,
  "process": "server",
  "pid": 39616,
  "session_id": 1106304605,
  "generation": 7,
  "code_hash": "...",
  "type": "hot_damage_policy_result",
  "actor_id": "server",
  "projectile_id": "12884902333",
  "attacker_entity": 4294967297,
  "victim_entity": 8589935592,
  "result": "hot",
  "base_damage": 1111,
  "out_damage": 66666
}
```

`ts_utc` is human-facing, ISO-8601 UTC with milliseconds. `mono_ms` preserves
ordering and elapsed-time measurement even if the operating-system wall clock
moves. The pair is required for forensic diagnosis.

## Event categories

The journal should describe the complete input-to-outcome chain:

```text
source_saved
source_hash_changed
compile_started
compile_finished
candidate_loaded
validation_result
code_activation
rollback
state_migration
packet_sent
packet_received
projectile_spawned
projectile_impact
hot_damage_policy_result
damage_applied
prediction_reconciled
ui_effect_applied
notification_emitted
sound_emitted
test_started
test_finished
hot_reload_boundary_violation
```

Each event should identify the first useful owner: source file, module,
function or policy, actor/projectile/entity ID, tick, generation, and result.

## What the AI should be able to answer

Given one test session, the journal should allow an AI to answer:

- Did the save get detected?
- Which source hash changed?
- Did compilation succeed?
- Which generation did the server activate?
- Did the client and server use the same generation?
- Did the packet arrive?
- Did the projectile reach impact?
- Did the hot behavior run?
- What was the base value and what did hot code return?
- Where did the observed result diverge from the intended result?
- Did a later system overwrite the result?
- Did the world, player, NPC, session, or entity IDs survive?

For example, these records prove that the authoritative hot kernel is working
even when the tuned value is not yet the desired one:

```json
{"ts_utc":"2026-09-12T17:03:34.207Z","type":"hot_damage_policy_result","process":"server","base_damage":338,"out_damage":2222,"result":"hot"}
{"ts_utc":"2026-09-12T17:03:36.806Z","type":"hot_damage_policy_result","process":"server","base_damage":1111,"out_damage":2222,"result":"hot"}
```

The next required improvement is attaching the active server generation and
code hash to those damage records. Without them, a server can be proven to be
using hot code but not which hot version produced the result.

## Cold restart is a severe violation

`cold_restart_pending` must not be treated as a normal reminder or harmless
status message. It means the requested edit crossed the current live boundary
and the running process cannot yet accept it.

For this project, a cold restart is equivalent to losing the living experiment:

```text
the process stops
the network session stops
the world stops
the reproduction context is damaged
```

Therefore:

- `cold_restart_pending` is a high-severity development event;
- it must identify the exact cold file and owner;
- it must explain why the boundary exists;
- it must not repeatedly nag without new information;
- the preferred response is to move editable behavior behind the hot boundary;
- only genuine runtime-kernel or ABI changes may remain cold;
- a cold build is allowed only during an intentional no-process installation
  window, never by killing or silently restarting MiMITA.exe.

The invariant is:

> If MiMITA.exe is running, it remains running for the entire live-development
> session.

## Relationship to changelogs

The JSONL journal is immediate machine evidence. It is written during the run
and survives a crash as far as the last flushed line.

The human-readable changelog is the later explanation of the session. It should
summarize the journal rather than invent timestamps or rely on memory.

```text
runtime JSONL evidence
-> session summary
-> changelog
-> regression or specification update when required
```

Every code activation, failed candidate, rollback, boundary violation, and
human acceptance result should be traceable from the changelog back to JSONL.

## Direction to continue

Prioritize improvements that increase AI leverage:

1. Add `process`, `pid`, `session_id`, `generation`, and `code_hash` to every
   authoritative gameplay event.
2. Add a session ID that groups source edits, builds, activations, tests, and
   gameplay outcomes.
3. Add explicit intended-result and observed-result fields to deterministic
   tests.
4. Compute the first missing event in a declared event chain.
5. Add a lightweight query/summary command so an AI can ask for a session,
   actor, projectile, generation, or time interval without scanning raw logs.
6. Preserve raw JSONL as the authority; summaries remain derived views.
7. Add server/client generation agreement records before multiplayer code
   switching is attempted.
8. Keep deterministic replay inputs and random seeds beside the event chain.

The target is not a larger log. The target is a live, queryable explanation of
the running simulation that lets a human or AI shorten investigation, editing,
validation, and confirmation into one continuous loop.

## Related authoritative files

- `docs/features/live-code-development/live-code-development.md`
- `docs/architecture/live-development/live-development.md`
- `docs/architecture/live-development/hot-kernel.md`
- `src/live-code/live-journal.*`
- `src/live-code/live-code-events.*`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/regressions/regressions-v1.md`
