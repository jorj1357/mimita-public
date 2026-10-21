# Countdown reliability investigation

Status: PASS_WITH_HUMAN_REVIEW

Time: 2026-10-20T23:36:19Z (2026-10-20 19:36:19 EDT)
Branch: `20261011zt`
HEAD: `0b5145b2`

## Scope and working tree

Investigated the user-reported countdown delay and reversal without changing gameplay code or the already-modified networking specification. The working tree was already dirty before this investigation, including `docs/specs/networking/networking.md` and numerous unrelated files. Those edits were preserved. This changelog is the only file added by this session.

## Findings

1. `docs/specs/networking/networking.md` lines 3-9 contain the user's current request in informal form. The active lifecycle contract in `docs/specs/gamemodes/gamemodes.md` lines 45-46 is `WAITING → INTERMISSION → COUNTDOWN 3 → COUNTDOWN 2 → COUNTDOWN 1 → GO → ACTIVE`. The intent is clear, while the NPC pause scope and exact timing/acceptance criteria remain undecided.
2. Server countdown timing is based on fixed simulation ticks. `startObjectiveRound()` sets `matchStartTick = currentTick + countdownSeconds * 60` and emits `round.start` in `src/network/server-gamemode.cpp` around lines 2296-2317. The generic match start path also sets a tick-based `matchStartTick` around line 3299.
3. The shared server NPC update path runs from the fixed tick in `src/network/server.cpp` around lines 897 and 1450. `simulateSharedNpcs()` has a countdown gate that skips NPC target selection around `src/network/server-npcs.cpp` lines 1280-1293, but later still calls `npcSystem.updateOneWithTarget(..., SERVER_DT)` for each NPC around line 1497. Thus countdown freeze currently does not remove the per-NPC simulation cost.
4. The existing Oct 20 journal gives direct evidence that NPC work can stretch a countdown. In `logs/2026-10-20/20261020_221119/events.jsonl`, round 1 starts at 2026-10-20T22:12:21.948Z, tick 900, with `countdown_seconds=3.0`. Near tick 903, a server tick window with 135 NPCs reports 26.4 Hz; NPC simulation reached a 1261.726 ms maximum and the window's total tick maximum was 1268.336 ms. `round.go` is recorded at 22:12:26.109Z, tick 1080: the three simulation seconds took about 4.16 seconds of wall time. The journal records one `round.start`, then `round.go`, then `round.active` for that round, so it does not show the authoritative server restarting that countdown.
5. The same journal shows workload sensitivity: a later countdown with 135 NPCs around 22:16:36Z remained near 60 Hz, with 0.8 ms average NPC simulation and 1.794 ms maximum in the reported window. NPC count alone is not a sufficient predictor; an individual NPC update can become pathological.
6. A client-side reversal is also plausible. `CommunityMatchClient::onState()` rejects a lower `serverTick` only when `stateVersion` is equal (`src/network/community-match-client.cpp:324-325`), then unconditionally re-anchors `mServerTick` from the accepted packet (`:404`). A higher-version packet carrying an older tick can therefore move the countdown clock backward. The HUD derives the numeral from `matchStartTick - match.serverTick()` (`src/engine/engine-tick-ui-overlays.cpp:781-791`). The server journal does not capture the exact text drawn on the client, so this remains a source-level risk, not a confirmed cause of the observed reversal.

## Recommended staged direction

- First make the client countdown timeline monotonic within one match/session: do not accept a server tick older than the latest applied tick just because the packet has a higher state version. Reset the watermark only at a genuine new match/session boundary. Keep phase/state version handling separate from the time anchor.
- At the existing shared NPC simulation owner, honor the countdown pause for the expensive per-NPC update as well as target selection. Keep server networking and match lifecycle ticks running. Resume NPC simulation at the existing authoritative release point. This directly addresses the measured stall without changing the 60 Hz simulation contract.
- Add bounded owner-level structured evidence for countdown start tick, phase transitions, applied client tick anchor, NPC count, and NPC simulation duration/max offender. Then compare the exact client-visible sequence against the server journal in a real run before claiming the reversal is fixed.

## Information still needed for a final implementation contract

- Does “other NPCs” mean every NPC in the world, or only NPCs outside the active match roster? The request currently leans toward pausing NPC simulation broadly during countdown, but does not explicitly define it.
- Should 3-2-1 be exactly three seconds of server simulation time, or three seconds of wall time even while the server is stalled? Current implementation specifies simulation ticks. A wall-clock guarantee would be a separate timing decision.
- Should a countdown restart only for a new round/session, or can roster/map readiness intentionally begin it again? The server journal showed no restart in the investigated round.
- Define acceptance for “reliable”: a single monotonic 3, 2, 1, GO sequence per start is evident; allowable wall-time stretch and whether GO duration is included need a product decision.

## Documents and focused review

Read `docs/ROUTER.md`, the networking and gamemode specifications, `docs/specs/performance/performance.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/task-completion/task-completion.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/architecture/code-ownership/code-ownership.md`, and `docs/architecture/player-npc-systems/npc-movement.md`.

Applied `docs/skills/spec-behavior-review-v1.md`, `docs/skills/efficiency-checker-v1.md`, and `docs/skills/logging-checker-v1.md` as an investigation review. Findings are source and journal based; no code or specification edits, tests, build, or new runtime run were performed. No regression record was created because the exact client-visible reversal and its cause were not independently confirmed in this session.

Human review still needed: confirm whether to pause all NPC simulation during countdown and select the timing acceptance target before implementation. A future fix still needs runtime verification through the canonical dev loop and a fresh journal, including human confirmation of the visible countdown.
