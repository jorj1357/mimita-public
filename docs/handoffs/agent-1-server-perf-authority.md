# Agent 1 handoff — server 60 Hz, client 240 FPS, damage/cooldown authority

Snapshot: 2026-11-11
Repository: `C:\mimita-v9`
Role: Agent 1 owns server timing/performance and combat authority.
Agent 2 owns packet/domain splits; see `agent-2-packet-domain-splits.md`.

## Goal

Hold the authoritative fixed 60 Hz tick at or better than 16.667 ms for
99.99% of ticks with a clean 42-NPC Payload match, while the same-PC client
holds 240 FPS for 99.99% of one-second windows at the game's current visual
quality. During the same run, honest live-target damage must agree between the
client and the server, with no false hit misses, hit rejects, or cooldown
disagreements.

Server correctness wins over client frame rate whenever they conflict.

## Acceptance gates

### Server

Payload, funworld3, clean 21v21 = 42 living NPCs, 10 minutes:

- At least 99.99% of fixed ticks have total server work at or below 16.667 ms.
  Over roughly 36,000 ticks, that permits at most three or four late ticks.
- Zero hard snapshot timeouts (`snapshot_timed_out=true`).
- Zero `performance.server-tick-stall` entries above 100 ms.
- The phase budget counts all 42 NPCs and actually defers work; it must not
  report 19 while simulating 42.

### Client

Same run, this PC, current visual quality, low-end hardware assumed:

- At least 99.99% of 600 one-second windows average 240 FPS or better.
  Over 10 minutes that is effectively zero bad windows.
- Keep p99 frame time as a diagnostic, not a substitute for the window gate.
- Do not reduce visual quality to meet the gate. If the hardware cannot hold
  it, report the exact CPU/GPU/present ceiling and the responsible scopes.

### Damage and cooldown

For honest shots with a live target that remains valid through server apply:

- At least 99.99% of client-displayed damage equals server-applied damage.
- Zero false `HIT MISS`, `HIT REJECTED`, or generic cooldown disagreement
  effects.
- Every genuine reject has a structured reason and restores ammo/cooldown
  exactly once.

Exclude only genuinely impossible cases: target died, disconnected, teleported,
or changed spawn generation before the server applied the claim. Do not accept
damage on invalid targets to satisfy the gate.

## Current measured baseline

Server:

- Approximately 9.9–11 Hz effective tick rate, about 95 ms/tick, at 42 NPCs.
- No one-second windows at or below 16.667 ms in the sampled run.
- 42 NPCs are composed of 23 default-roster IDs (100000+) plus 19 Payload IDs
  (1000+); mode 57 expects 19.
- NPC phase is roughly 59–74 ms; physical contact about 18 ms; target scans are
  O(N²) at 42×42.
- `performance.server-phase-budget` reports `npcs=19` while 42 simulate, and
  `defer_npc`/`defer_contacts` are logged but not enforced.

Client:

- About 238 FPS average but p99 around 13.19 ms; only 1 of 482 one-second
  windows met a 4.167 ms p99.
- The perf target is currently 999 FPS, so the 1.001 ms budget makes almost
  every frame report as a spike.
- Observed hitches:
  - config map polling: 2.3 s
  - networking self time: 204 ms
  - snapshot NPC avatar apply: 104 ms
  - `Rendering::Actors::NetworkEntities`: 50.7 ms
  - ragdoll world collision: 15–26 ms
  - swap/present: 66–72 ms
- Client perf reports zero NPCs while many remote NPCs render.

Authority:

- Client cooldown uses local seconds; server uses authoritative ticks.
  When the server is behind wall time, the client fires before
  `nextAllowedFireTick`, producing repeated cooldown rejects.
- Retries continue while the server is stalled; requests time out after 3 s.
- Rejected generic attacks do not refund the client clip.
- Prediction rollback can run when prediction was not applied; NPC damage
  packets are not request-correlated, so mismatch telemetry includes false
  alarms.
- Hitscan claims lack structured accept/reject events and do not reliably
  enforce target spawn generation; melee reach omits claim tolerance.

## Phase 0 — truthful measurement and baseline

1. Commit and promote the current validated lane work before starting parallel
   lanes. Preserve the coordinator's uncommitted work; do not reset it.
2. Create the clean 21-attacker/21-defender Payload stress fixture. Mode 57
   expectation becomes 42.
3. Set the client target to 240 FPS (`maxFrames=240`, `target_fps=240`).
4. Add a true server conformance counter using the existing
   `ServerPhaseBudgetPolicyV1` event 709: ticks at or below 16.667 ms divided
   by total ticks, emitted once per second.
5. Fix the client perf NPC count to include remote players and NPCs.
6. Fix snapshot diagnostics to report real interarrival and per-window deltas,
   not the diagnostic cadence and cumulative totals.
7. Record the honest 42-NPC Payload baseline at current quality.

Gate: the baseline reports the true 240/60 numbers and the current composition
of 42 NPCs.

## Phase 1 — single-thread server recovery

Target files are host-owned. Batch these into one cold boundary build; keep
policy thresholds in the existing hot modules.

- Gate or remove the periodic `npc.trace` containment scan. It calls
  `actorRootInsideClosedSolid` for multiple NPCs and has cost hundreds of
  milliseconds. Keep a bounded sample behind a debug toggle.
- Rate-limit the per-tick over-budget and stall diagnostics into per-second
  aggregates.
- Make the phase budget count and defer all 42 NPCs.
- Apply the existing staggered thinking cadence to Payload, not only Fun
  Counter Strike.
- Add a spatial target-candidate index; remove the 42×42 full scans.
- Cache/reuse navigator work; keep one NPC navigator update within budget.
- Build physical-contact weapon shapes only for attackers with an active
  honest attack; reject distant or same-team actors before shape construction.
- Replace per-tick adopt/sync/rebuild nested scans with an `id -> Npc*` index.
- Consume the existing `outDeferNpc` and `outDeferContacts` policy outputs.
  The policy remains hot; the enforcement is host.

Gate: measure the 42-NPC 60 Hz run. If 99.99% is not met, proceed to Phase 1B.

## Phase 1B — deterministic multicore

Single-threaded per-NPC work is about 1.35 ms per update, roughly 57 ms for 42
NPCs before contact. Staggering reduces that but likely not enough for the
16.667 ms gate.

- Run per-NPC sense, decision, navigation, aim, and weapon-choice jobs against
  immutable bounded input snapshots.
- Each NPC owns its RNG stream; workers must not share a random stream.
- Partition physical-contact pair work into deterministic chunks.
- Apply intents, physics, damage, and snapshot results in fixed NPC-ID order.
- Use an adaptive worker count that reserves headroom for the client, network,
  and OS. On four cores, do not saturate all four with server workers.
- Do not move gameplay off the fixed 60 Hz tick and do not introduce
  wall-clock-dependent decisions.
- Do not start bot-host or NPC-as-client work. Hybrid worker processes are a
  later fallback only if in-process threads cannot meet the gate.

Gate: 42 NPCs, at least 99.99% of ticks at or below 16.667 ms. If it still
fails, stop and report the measured ceiling before changing architecture.

## Phase 2 — client 240 FPS at current quality

Do not lower visual quality. Fix work that is redundant, blocking, or
mis-measured.

Current problems:

- The 999 FPS target hides the real 240 FPS budget.
- Config polling performs large filesystem scans on the main thread.
- Remote entity geometry is resolved every render frame for every actor.
- Avatar/model binding and bounds work repeats per frame.
- Snapshot avatar application can block a frame.
- Ragdoll and network entity rendering produce recurring spikes.
- Present/swap can stall independently of CPU work.
- Perf instrumentation serializes many scopes several times per second.

Fixes:

- Move heavy config polling and scanning off the main thread with cached
  results and a staggered reload.
- Cache remote entity geometry; run safety resolution only when the target has
  moved materially, and skip far or offscreen entities with a conservative
  broadphase reject.
- Bind avatars/models only on identity change; stagger avatar finalization
  across frames while applying transforms immediately.
- Remove repeated per-frame allocations, texture binds, and redundant draw
  setup in remote entity rendering.
- Reduce perf instrumentation overhead against the 4.167 ms budget while
  keeping enough evidence to diagnose spikes.
- Diagnose present/swap stalls as a separate CPU/GPU/driver category.

Do not:

- Add VSync or raise the frame cap as a workaround.
- Lower render quality, resolution, shadows, effects, or ragdoll fidelity for
  the acceptance run.
- Move collision, damage, or physics into the render loop.
- Disable diagnostics permanently or hide hitches by changing the test
  population.
- Claim a p99 number as proof of the one-second-window gate.

Gate: 99.99% of one-second windows at 240 FPS or better at current quality. If
the same-PC four-core ceiling is below that, report the exact ceiling and the
largest scopes instead of changing quality.

## Phase 3 — damage and cooldown authority

- Client fire gate: do not fire until the observed authoritative server tick has
  reached the weapon's ready tick. Keep the server gate unchanged.
- Convert authoritative cooldown into displayed cooldown using the observed
  server tick rate, not a fixed 60 Hz assumption.
- Make attack retry and timeout backlog-aware. Do not retry while the server is
  behind the request's fire tick. Use bounded backoff.
- Refund one round exactly once per rejected ammo-consuming request.
- Make prediction request-scoped. Roll back only when prediction was applied.
  Do not emit a world disagreement for a normal cooldown or rate-limit reject.
- Fill and enforce hitscan target spawn generation. Do not overwrite the
  claimed generation with the current generation.
- Add structured hitscan claim accept/reject events with explicit reasons.
- Include melee claim tolerance in reach validation.
- Reject a second distinct shot in the same authoritative tick when the weapon
  fire delay requires more than one tick, without rejecting genuinely fast
  weapons.
- Reset per-tick shot counters before packet intake or move gameplay packet
  handling inside the fixed-step loop.
- Use a per-player time-bounded idempotency cache and stable contact dedupe
  keys.
- Defer request-scoped NPC damage correlation (`causeSerial`) until Agent 2 has
  completed the packet split; do not add wire fields concurrently.

Gate: honest live-target shots have at least 99.99% client/server damage
agreement, zero false hit effects, zero cooldown rejects, and exact ammo/
cooldown restoration. Exclude dead, disconnected, and stale-generation targets.

## Phase 4 — soak and evidence

Run the same 42-NPC Payload scenario for at least 10 minutes, preferably 30.

Report separately:

- lane, worktree, branch, commit;
- host EXE identity and stable artifact pointer;
- module generations and module-set hashes;
- exact per-instance `events.jsonl` paths;
- server conformance count and late-tick list;
- client one-second window pass rate and worst scopes;
- claim accept/reject/mismatch counts and reasons;
- snapshot timeouts and ICE events;
- visual/gameplay acceptance, which remains a human claim.

## Hot/cold boundary

Hot first:

- Events 703 (hit claim), 708 (combat validation), 709 (server phase budget),
  800 (navigation), NPC 510/511/512/714/715/716.
- `network-policy`, `npc-behavior`, `navigation-policy`,
  `actor-contact-policy`, `npc-perception` module sources.
- NPC difficulty, weapon, ragdoll, networking, and gamemode config values.

Cold batch:

- fixed-tick loop and defer enforcement;
- NPC roster/budget/spawn lifecycle;
- contact loop and target indexing;
- client remote entity/avatar/config polling;
- cooldown/claim host gates;
- any new public event or payload.

Reserve event IDs 718–729 for Agent 1 only if a new server-only policy is
genuinely required. Use existing events first. Do not add packet wire fields;
that is Agent 2's boundary.

## Files Agent 1 owns

- `src/network/server.cpp`
- `src/network/server-attack.cpp`
- `src/network/server-damage.cpp`
- `src/network/server-players.cpp`
- `src/network/server-npcs.cpp`
- `src/network/server-gamemode.cpp`
- `src/network/server.h`
- `src/npc/npc.cpp`, `npc-combat.cpp/.h`, `npc-spawn.cpp`
- `src/network/network-policy-module.cpp`
- `src/npc/npc-behavior-module.cpp`, `npc-utility.cpp`
- `src/navigation/navigation-policy-module.cpp`
- `src/physics/movement/actor-contact-policy-module.cpp`
- `src/npc/npc-perception-module.cpp`
- own configs under `config/`

Do not edit `src/network/packets.h`, `src/network/wire/*`, packet handlers,
packet dispatch, or Agent 2's domain files.

## Do not do next time

- Do not measure 240 FPS against a 999 FPS target.
- Do not use a misleading p99 or cumulative snapshot counter as acceptance.
- Do not hide server overruns with unbounded per-tick logging.
- Do not keep duplicate NPC populations and budget only one of them.
- Do not leave `defer_npc` as advisory while claiming the budget is enforced.
- Do not optimize by lowering quality, adding VSync, or changing the test
  population.
- Do not fix cooldown by raising grace ticks or using wall-clock server
  cooldown.
- Do not accept impossible claims or add a second damage sink.
- Do not copy a lane module DLL without matching host constants, wrappers, and
  catalog ownership.
