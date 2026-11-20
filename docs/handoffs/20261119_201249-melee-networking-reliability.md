# Melee hit reliability and network recovery handoff

**Date:** 2026-11-19 (UTC)
**Checkout:** `C:\mimita-v9`
**Branch:** `checkpoint/one-checkout-condensation-20261111`
**Purpose:** continue the live investigation of predicted melee contact,
snapshot starvation/reconnect failure, and server tick performance from the
last verified checkpoint. **Audience:** the next agent continuing the work.
This is an investigation handoff, not a claim that the requested behavior is
fixed.

## Does not

- authorize a manual or agent-owned build; the shared root dev-loop policy
  below is still binding;
- claim that melee parity, reconnect recovery, NPC presentation, or the 60 Hz
  target is fixed;
- authorize deleting, reverting, or replacing unrelated shared-checkout
  edits.

## User goal and decided scope

The user wants attacks to feel immediate and to hit the actor the client sees,
with the server acting as a referee for plausible results. Server/client
networking must keep snapshots flowing, avoid reconnect loops and frozen NPCs,
and keep simulation on the fixed 60 Hz tick.

For melee, focus on **Mode 1** for **Chainsaw, Mace, and SwordSword**. Treat
physical overlap/contact as the activation and use one reusable contact,
validation, damage, and event path. The client predicts its local contact and
feedback immediately; the server remains the sole writer of authoritative
health/death/score and reconciles only clear impossibilities. Mode 2
(windup/active/recovery/return-to-idle) belongs in the specification only for
this task. SpyKnife and GodBall are explicitly deferred.

The intended target is at least 99.99% agreement between eligible predicted
damaging contacts and accepted server target/total damage. This is a target,
not a result established by the run below. One unexplained mismatch is
actionable. Existing target language is in
[`docs/specs/networking/contact-authority.md`](../specs/networking/contact-authority.md)
and focused Mode 1 behavior is in
[`docs/specs/weapons/melee/melee-weapons.md`](../specs/weapons/melee/melee-weapons.md).

## Active repository and build gates

The current repository policy is the shared checkout and root dev-loop. Read
`AGENTS.md`, `docs/ROUTER.md`, and
[`docs/specs/dev-loop/agent-lanes-build-authority.md`](../specs/dev-loop/agent-lanes-build-authority.md)
before acting. Do not create a lane, checkout, second dev-loop, or manual
build; do not invoke a compiler or request/wait/poll for a build. The root
dev-loop owns automatic builds from saved compiler inputs. Check status once,
continue useful source/path review, and use the exact current published
artifact for later live evidence. Preserve other agents' shared-tree edits.

At the last checkpoint (**2026-11-19 20:12 UTC**):

- `python devscripts/dev-loop.py --status` reported the primary run as
  `NOT RUNNING`, the published EXE as stale, and a game-DLL attempt
  `game-dll-20261119T200909570923Z-22df53` in `manifest_snapshot`, started at
  `20:09:12Z`. The last published `build/game/ready.json` named generation
  375, revision `3f23dc0d...`, source generation 4; treat it as stale while
  the later attempt is active or source keeps changing.
- The published EXE is still
  `2026-11-19T17:45:16.114810Z`, SHA-256
  `54879b91e6623e913d4781fe0824b9ad95aaef00449c20d413db48f24771c74a`,
  source generation 0. Do not present it as current for the broad dirty
  source tree.
- The tree contains many unrelated modified files and ongoing edits (including
  networking, melee, NPC/nav, map/editor, hot-reload, config, and docs).
  Confirm file ownership before editing. Do not revert or overwrite unrelated
  changes.
- The prior isolated run `20261119_190051` is now `stopped`. Its own server
  PID 10884 and client PID 29668 were stopped by the run controller after
  executable, command line, and process-start identity matched the manifest.
  The primary session was not stopped by that action. Another prior run,
  `20261119_185910`, is listed as failed.

## What changed and build evidence

In the previous session, a per-player snapshot-send diagnostic was added to
`src/network/server-packets.cpp` in `buildAndSendSnapshot`. At the existing
diagnostic interval, `network.server.snapshot.build` and
`network.server.snapshot.send` now include each player's transport kind,
whether a transport handle is bound, spawned/stale state, sent/failed chunk
counts, and bytes. The snapshot event transport label is `none` for an empty
recipient set and `mixed` for the server's per-player send loop. It is intended
to distinguish a missing recipient, a failed send, and a send accepted by the
transport layer.

The required build note was queued **before** the C++ edit:

- note ID: `a78977b8696f`
- change: `Add per-player snapshot send diagnostics for reconnect investigation`
- file: `src/network/server-packets.cpp`

The root dev-loop consumed that note in attempt
`game-dll-20261119T192034365650Z-ce0575`. Its journal reports successful
generation 371, source generation 2, revision
`c60528a9caeb00d12ddae5152886c622e7f41fb3a9bcaea816f420babafac17c`, and code
hash `77240d9cfd24dd1ef3880d12cbb98eecbfa11442ba8f55d9aaa9b19b049b8d22` at
`2026-11-19T19:27:59Z`. A later `ready.json` reports generation 375, but a
still-later build attempt was active at the checkpoint and the source was
continuing to change. **Compilation/publication is not proof the diagnostic
loaded in a live process.** No run after generation 371 was performed with the
diagnostic verified in its loaded module.

The same previous session changed `devscripts/dev-loop.py` so isolated-run
shutdown obtains a fresh process identity with a 10-second timeout. This fixed
the test controller's `stop_partial` result under compiler load; the retry
stopped exactly PIDs 10884 and 29668. `python -m py_compile
devscripts/dev-loop.py` passed. Changelog:
[`docs/changelog/2026-11-19/20261119_192054-snapshot-peer-send-diagnostics.md`](../changelog/2026-11-19/20261119_192054-snapshot-peer-send-diagnostics.md).

## Reproduced networking and performance symptoms

The isolated run manifest is
[`logs/2026-11-19/20261119_190051/run.json`](../../logs/2026-11-19/20261119_190051/run.json).
It used TDM mode 41 / `atdm`, profile `tdm-validation`, a loopback server on
`127.0.0.1:58948`, one client, EXE source generation 0, and game DLL generation
370 (revision `41de14a34054b55cbc53641a107f027aca2dcecb9f1b0298ed357a498841fbf4`,
hash `5421c31937a4b996d511ec579e4ac3935f8063ae2cd76502f57795b49440f7de`).
Its journals are:

- client: [`client-01/events.jsonl`](../../logs/2026-11-19/20261119_190051/client-01/events.jsonl)
- server: [`server-01/events.jsonl`](../../logs/2026-11-19/20261119_190051/server-01/events.jsonl)

Observed sequence from those journals:

1. Client connected at `19:01:15Z`; client snapshots were received/applied
   through approximately tick 12,381 at `19:04:55Z`.
2. Snapshot age crossed the 2.5 s stale threshold. At `19:05:05Z`, the client
   scheduled reconnect at 10,004 ms snapshot age.
3. The server received and accepted all 10 reconnect requests. Client
   `network.client.reconnect.accepted` events appeared, but no fresh
   `network.client.snapshot.received`/`.applied` followed. The client correctly
   remained in reconnect recovery awaiting authoritative state, then emitted
   `network.client.reconnect.terminal` with `grace_expired` and entered
   `ReconnectFailed` at `19:06:05Z`.
4. Server world-phase windows continued and generally reported all 9 living
   NPCs moving. However, some one-second windows had 46–59 ticks, and the
   server logged long tick stalls. A severe sample at tick 1,200 took about
   6.78 seconds total, with about 6.77 seconds in the NPC phase. Later stalls
   were also observed. This confirms serious server tick overruns, but the
   sequence does **not** prove that those overruns caused the missing
   snapshots.
5. After the client gave up, the server eventually showed zero players and
   continued building snapshots with zero bytes/chunks sent. The old aggregate
   snapshot event could not establish the per-player send path during the
   critical reconnect interval. That is the purpose of the new `peer_sends`
   field.

Treat the root cause of the reconnect/snapshot failure as **unresolved**. Use
the new per-player diagnostics first. If the server reports no peer entry,
trace player slot lifetime and reconnect binding. If a peer has failed chunks,
trace `serverSendToPlayer` and transport-handle ownership. If sends succeed
over ICE but the client has no receives, trace the ICE session/receive route
and compare server/client run, connection, player, tick, and timestamps. A
reconnect acknowledgement alone is not recovery; require a new authoritative
snapshot and state application.

## Reproduced melee findings

In the same old run, the client emitted 227 `melee.client.contact_claim`
events for Mace. The server logged 13 accepted claims and 128 rejected claims;
among those rejections, 111 were `out_of_reach`, 6 `outside-hit-volume`, 5
`invalid_contact_bounds`, 5 `friendly_fire`, and 1 `outside_claim_radius`.
The run also contains server `damage.applied` events for attacker 1 against
NPCs, and several accepted Mace claims correlate with authoritative health
deltas. This proves that some contacts reached the shared server damage path;
it does not prove that each eligible client-predicted contact matched.

The client claim events carried `feedback:"claim_only"`, so the run did not
prove immediate client-predicted damage feedback. Do not equate terminal
command completion, a local contact effect, or a claim send with server
damage. Correlate the same attack/contact ID through client prediction/send,
server receive/decision, `applyActorDamage`, replicated outcome, and client
reconciliation. Record every rejection and the first divergent stage.

`equipweapon mace` was ineffective in the prior test. `equipslot 1` resolved
to Mace; verify the actual server `weapon.equip.accepted` event and active
weapon before attacking. The test run used a high-NPC TDM population and had
uncontrolled combat, so reproduce contact deliberately and keep runs bounded.

## Code/spec owners to trace before editing

- Mode 1 definitions, active-tick contact, intended cadence, shared query, and
  config behavior: `docs/specs/weapons/melee/melee-weapons.md`; current client
  producer `src/combat/melee-client.cpp` (`isClaimMeleeWeapon`,
  `tickClientMeleeContactClaims`).
- Server claim route and acceptance/rejection: `handleGenericMeleeContactBatch`
  in `src/network/server-packet-handlers.cpp`; event names include
  `melee.contact.batch.received`, `melee.contact.claim.accepted`, and
  `melee.contact.claim.rejected`.
- Single authoritative health/damage sink: `applyActorDamage` in
  `src/network/server-damage.cpp`.
- Player/NPC physical contacts and events: inspect
  `src/network/server-physical-contact-actors.cpp` and the shared contact
  authority map in `contact-authority.md`. Do not add per-weapon damage sinks
  or a second hit-validation pipeline.
- Snapshot generation/send: `buildAndSendSnapshot` in
  `src/network/server-packets.cpp`; per-player transport send is
  `serverSendToPlayer` in `src/network/server-ice.cpp`.
- Client snapshot receive/apply and reconnect acknowledgement: packet handling
  in `src/network/multiplayer-tick.cpp`; health/reconnect owner in
  `src/network/multiplayer-packets.cpp`; ICE reconnect owner in
  `src/network/ice/ice-server.cpp` and the host transport adapter.
- Server fixed-tick/stall measurement: `src/network/server.cpp` and hot
  `src/game/server-runtime-module.cpp`. Use phase and NPC subphase data; do
  not infer 60 Hz from a single average or console status line.

The working tree already contains broad pre-existing edits in these areas.
Read the current file diffs and source ownership map before editing, and ask
the other active contributor to yield a file if ownership overlaps.

## Next steps for the receiving agent

1. Read `AGENTS.md`, `docs/ROUTER.md`, the build-authority document linked
   above, and the routed runtime, networking, melee, performance, logging,
   regression, and task-completion documents. Read the latest changelog and
   inspect current `git status`, build note/attempt, and exact source owners.
2. Do not assume the build attempt active at handoff has finished. The
   dev-loop status was `source_changed`, primary run `NOT RUNNING`, and game
   attempt `game-dll-20261119T200909570923Z-22df53` in `manifest_snapshot`.
   Continue useful trace/review work while the root authority acts; do not
   issue another build or poll/wait for it. Before any live claim, verify a
   single current status and the published DLL/executable generation against
   the sources used by that run.
3. Once an artifact containing `peer_sends` is current, start a new isolated
   TDM test run via the root CLI so other sessions remain separate:

   ```powershell
   python devscripts/dev-loop.py --start-test-run --test-profile tdm-validation --test-launch-mode 41
   python devscripts/dev-loop.py --list-test-runs
   ```

   Preserve the returned run ID, pinned build IDs, game generation, server and
   client PIDs, and both exact `events.jsonl` paths. Stop only that manifest's
   run with `--stop-test-run <run-id>` after its decisive evidence is captured.
4. First verify healthy join and continuous authoritative snapshots. Then
   exercise a short controlled disconnect/reconnect or configured bad-connection
   case. Read server `network.server.snapshot.send.peer_sends`, reconnect
   received/accepted events, client snapshot received/applied events, session
   health, and stall windows together. Identify the first missing stage before
   changing reconnect code.
5. Test Mace Mode 1 first, then Chainsaw and SwordSword using the same shared
   contact path. Equip through a verified slot, get close to a target, and
   issue ordinary gameplay input through the run's client. Compare client
   predicted contact with server accepted/rejected claims and authoritative
   damage by contact ID, target generation, tick, and weapon config revision.
   Fix the first divergence; do not widen server validation by a guessed
   radius or declare `claim_only` to be prediction.
6. Profile tick cost from a matched current run. Keep simulation/collision and
   damage on the fixed 60 Hz owner. Determine which measured phase/NPC subphase
   exceeds budget and reduce that owner without skipping gameplay ticks.
   Confirm NPC movement updates in client snapshots as well as server-side
   movement windows; a moving server actor with stale client snapshots still
   appears frozen.
7. Repeat the live run on the edited source generation, and report source,
   build, loaded-module activation, per-process runtime logs, and human feel
   acceptance separately. The 99.99% target needs a large eligible-contact
   sample and must not be claimed from this one short test. Add one final
   changelog for the receiving session and record any confirmed regression
   append-only.

## Required validation references

- [`docs/workflows/runtime-scenario-validation.md`](../workflows/runtime-scenario-validation.md)
- [`docs/workflows/multi-client-dev-testing.md`](../workflows/multi-client-dev-testing.md)
- [`docs/specs/networking/contact-authority.md`](../specs/networking/contact-authority.md)
- [`docs/specs/networking/networking.md`](../specs/networking/networking.md)
- [`docs/specs/weapons/melee/melee-weapons.md`](../specs/weapons/melee/melee-weapons.md)
- [`docs/specs/performance/performance.md`](../specs/performance/performance.md)
- [`docs/specs/debug-logging/debug-logging.md`](../specs/debug-logging/debug-logging.md)
- [`docs/specs/dev-loop/multi-instance.md`](../specs/dev-loop/multi-instance.md)
- [`docs/specs/dev-loop/dev-loop.md`](../specs/dev-loop/dev-loop.md)
- [`docs/specs/hotreload/hotreload.md`](../specs/hotreload/hotreload.md)
- [`docs/handoffs/hot-reload-agent-focus.md`](hot-reload-agent-focus.md)
- [`docs/regressions/networking/README.md`](../regressions/networking/README.md)
- [`docs/skills/logging-checker-v1.md`](../skills/logging-checker-v1.md)
- [`docs/skills/efficiency-checker-v1.md`](../skills/efficiency-checker-v1.md)
- [`docs/operations/task-completion/task-completion.md`](../operations/task-completion/task-completion.md)
