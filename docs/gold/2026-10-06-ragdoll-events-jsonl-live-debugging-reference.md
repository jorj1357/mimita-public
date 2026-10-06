# Gold reference: ragdoll debugging through live `events.jsonl`

- Date recorded: `2026-10-06`
- Status: `GOLD / REFERENCE` — evidence-first live ragdoll investigation
- Scope: Counter-Strike deaths, network death presentation, local-player
  deaths, NPC respawn state, corpse spawning, and AI/runtime observability
- Related regression:
  `docs/regressions/2026-10-06/ragdoll-per-death-presentation-ATTEMPT-1-REG.md`
- Related logging specifications:
  `docs/specs/debug-logging/debug-logging.md`,
  `docs/specs/debug-logging/canonical-jsonl.md`

## What happened this session

Ragdolls appeared inconsistently. Sometimes every death in a Counter-Strike
round produced a ragdoll; sometimes teammates, enemies, or the local player
produced only blood and the walking effect. The investigation began with a
symptom and ended with a concrete, runtime-supported state-lifecycle cause.

The important evidence came from the actual running executable's canonical
JSONL journal, not from a synthetic test or from assuming that a successful
build meant the behavior was fixed.

The decisive journal was:

`C:\mimita-v9\logs\10-06-2026\20261006_145207\events.jsonl`

It contained multiple processes and restarted runs, so each conclusion had to
be correlated by `run_id`, process role, PID, executable path, actor ID, and
server tick. Its approximate 14.3 MB size was not a problem: append-only JSONL
remained searchable and could be inspected live and after the scenario.

The journal showed:

- server damage and death transitions were occurring;
- corpse attempts were succeeding when they were requested;
- there were no corpse rejections or corpse-cap evictions;
- later NPC deaths sometimes arrived with
  `networkDeathPresented=true` from an earlier life;
- those deaths were skipped as duplicates without a reset event between lives;
- the local-player authoritative death path changed health/dead state without
  requesting a corpse.

That ruled out several attractive but unsupported explanations: a global
ragdoll disable, a renderer rejection, a corpse-count limit, and a missing
server death for the affected examples. The first missing stage was per-life
death-presentation state, with a second independent gap in the local-player
death path.

## The working investigation loop

The reusable loop is:

```text
human-visible failure
    -> read the relevant specification and prior regression
    -> trace the complete owner-to-output path
    -> add bounded StructuredLogger events at the existing owners
    -> build a newly named executable
    -> run --versioninfo and save EVENTS_JSONL_PATH
    -> exercise the real game scenario
    -> inspect the live canonical events.jsonl
    -> find the first missing or incorrect stage
    -> make the smallest owner-correct fix
    -> rebuild and repeat the same live scenario
    -> compare runtime evidence and human acceptance separately
```

The executable identity step is essential:

```text
mimita-<new-build>.exe --versioninfo
```

The output identifies the executable, PID, process role, run ID, and exact
`EVENTS_JSONL_PATH`. Without that step, an AI can easily read an older client
journal, a server journal, or a different run and draw the wrong conclusion.

## How the logging was used

The investigation progressively added owner-level events to the one shared
`StructuredLogger`. The logger wrote to the active run's canonical:

```text
logs/<yyyy-mm-dd>/<run>/events.jsonl
```

The event chain was intentionally split into stages so each missing stage
could eliminate a different cause:

| Stage | Events used | Owner question |
|---|---|---|
| Authoritative damage | `server.damage.applied` | Did the server apply lethal damage? |
| Authoritative death | `server.death.transition` | Did the server change alive to dead? |
| Client delivery | `client.death.received`, `client.npc.death.received` | Did the client receive the death? |
| Prediction | `client.death.prediction.begin`, skip/state events | Did prediction claim or suppress the death? |
| Presentation gate | `client.death.applied`, `client.npc.death.presentation_skipped` | Was presentation eligible or skipped, and why? |
| Corpse creation | `ragdoll.corpse.spawn.attempt`, `spawned`, `rejected` | Was a corpse requested and accepted? |
| Corpse visibility | render-submission, first-update, position samples | Did the corpse enter the renderer and remain alive? |
| Respawn reset | `client.death.presentation.reset` and completion | Was the per-life latch cleared? |

Every useful death record included, where available:

- actor/entity ID and team;
- process role, PID, run ID, executable identity;
- server/client tick;
- health before and after;
- world position;
- spawn generation or life identity;
- presentation flags before and after;
- expected eligibility;
- actual decision; and
- a specific skip or rejection reason.

This turned “I saw blood but no ragdoll” into a searchable causal question:

```text
Did damage happen?
  -> did death happen?
    -> did the client receive it?
      -> was presentation eligible?
        -> was spawnCorpse called?
          -> was it accepted?
            -> was it submitted and updated for rendering?
```

The log is evidence of execution and state. It is not a replacement for the
human seeing the game. A `ragdoll.corpse.spawned` record proves creation, not
necessarily that the final pixels were visible.

## The fix applied

The first fix was deliberately small and matched the first proven divergence.

### Remote players and NPCs

In `src/network/multiplayer-interpolation.cpp`, the fallback dead-to-alive
health-reset path now clears `networkDeathPresented`. It also records before
and after reset events. This prevents an earlier life from permanently making
the next life look like a duplicate death.

### Local player

In `src/network/multiplayer-projectiles.cpp`, a lethal authoritative local
damage confirmation now requests one corpse when the local per-life
presentation latch is clear. Duplicate confirmations are logged and skipped.

The successful build was:

`C:\mimita-v9\.dev\builds\1561\mimita.exe`

Its identity probe produced:

`C:\mimita-v9\logs\10-06-2026\20261006_150036\events.jsonl`

After that build, the user reported repeatedly killing the Counter-Strike
teams without seeing another ragdoll failure. That is strong human gameplay
acceptance for this scenario, while the exact fresh-journal event chain remains
the technical evidence to preserve when the issue is revisited.

## Full ideal-behavior specification

For every actor life, the system should satisfy all of these requirements:

1. Every authoritative alive-to-dead transition produces one corpse
   presentation for that actor life.
2. The local player, remote players, and NPCs use the same semantic rule even
   if their transport paths differ.
3. Prediction, reliable packets, and snapshots may race, but deduplication
   must be keyed to the actor's current life, not to a boolean that survives a
   respawn.
4. A new spawn generation/life clears all death-presentation state before the
   actor can die again.
5. Missing replica, generation mismatch, missing model, disabled presentation,
   renderer rejection, cap eviction, and duplicate suppression each produce a
   distinct searchable reason.
6. Corpse creation, renderer submission, first update, bounded position
   sampling, removal, rejection, and eviction remain distinguishable.
7. Dead actors do not continue normal walking/idle visual effects at their
   death position unless that effect is explicitly part of the death design.
8. One corpse per actor life is the default invariant; duplicate network
   deliveries must not create two visible corpses.
9. The implementation remains fixed-tick for gameplay state and uses bounded
   logging rather than unbounded per-frame output.
10. Configuration remains owned by `config/debuglogger.json` and the central
    logger; gameplay files do not create private debug files.

### Success conditions

For a live Counter-Strike scenario containing repeated rounds and repeated
deaths after respawns:

- every server lethal transition has a client death/presentation chain;
- every eligible actor life has exactly one corpse attempt and one successful
  creation, unless a clearly logged intentional presentation policy says
  otherwise;
- no death is skipped because of a stale prior-life latch;
- local-player deaths are included;
- corpse rejections and evictions remain zero unless intentionally induced;
- dead actors produce no ordinary walking effect after their death transition;
- no new unexplained logger, network, or renderer warnings appear; and
- the human can repeatedly observe the corpse in the live executable.

## Rules for future AI work

- Start with the current specification, regression record, and ownership map.
- Do not add a second file logger. Use `StructuredLogger` and the active
  `events.jsonl`.
- Instrument before guessing, but keep records bounded and purposeful.
- Build a new executable after source changes.
- Run `--versioninfo` and record the exact journal path before gameplay.
- Verify that the journal belongs to the executable being tested.
- Use process role and PID so server and client records are not mixed up.
- Follow the first missing golden-path stage rather than adding logs randomly.
- Record both expected and actual state, including the reason for every skip.
- Treat build success, runtime path execution, visual acceptance, and
  multiplayer acceptance as separate claims.
- Preserve the full specification before implementation so a fix has a clear
  target and measurable success conditions.
- When a fix works, preserve the evidence and convert the useful diagnostic
  into a permanent bounded event or remove it if it no longer has value.

## What “gold” means here

This document is gold as a reusable investigation method and specification.
The user's repeated successful live play is recorded as human acceptance for
the current Counter-Strike scenario. It does not claim that every possible
game mode, network condition, renderer path, or future ragdoll change is
proven forever. A later regression should restart at the same observable
golden path instead of returning to guesswork.
