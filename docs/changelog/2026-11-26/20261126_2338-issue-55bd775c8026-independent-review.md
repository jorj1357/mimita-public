# Independent bounded review of issue-55bd775c8026

- Task ID: wake-79ea1b90306d
- Agent: issue-review-sweeper
- Focus: one read-only review of the client-connect/crash/reconnect issue, its readiness and proof state, the retained 2026-11-26 run journals, and the newly created 2026-11-26 crash artifacts
- Summary: Appended review id 496 (verdict `inconclusive`) plus a correction addendum id 498 to `issue-55bd775c8026`, and one evidence-backed process proposal to `issue-cf9cd77eb056`. No source edit, build request, test run, close, owner change, or fix claim.
- Status: complete (review only)
- Date, time, timezone: 2026-11-26T23:40Z, ISO 8601

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents and humans (config files, `devscripts/dev-loop.py`, `devscripts/dev_loop/game_build.py`, `devscripts/dev_loop/gui.py`, `docs/ROUTER.md`, `docs/issues/README.md`, many changelogs). This session edited no source, reverted nothing, and touched only the issue records through `agent-coord.py` and this changelog. No compiler, `build.py`, module worker, second dev-loop, or runtime test run was invoked.

# Requested behavior

Run one bounded independent review of `issue-55bd775c8026`: inspect issue brief/readiness, proof status, linked artifacts, and the latest relevant journals; append one evidence-grounded review with observed facts, inference/uncertainty, contrary evidence, and a next discriminating check; then a short development-process review with one specific proposal to the best existing improvement issue (or none if unsupported). Do not edit source, build, close, or claim a fix.

# Specification alignment

- AGENTS.md gates 0b/0c/0d, 2, 3, 4: coordination begin/sync/observe; observations separated from inference; no compiler/build/second dev-loop in a review wake; preserve prior append-only text.
- `docs/specs/dev-loop/agent-proof-and-verification.md`: a build or live process is not behavior proof; acceptance needs a pinned run journal with the required event.
- `docs/specs/networking/network-event-contract.md` expectations referenced by the issue: connect -> validate -> join accepted -> map sync -> spawn -> authoritative snapshot -> health, and terminal-only Disconnected.

# Exact implementation changes

None. No source, specification, or configuration change.

# Changed files and reverse steps

| File | Before | After | Why | Reverse |
|---|---|---|---|---|
| `docs/issues/bug/20261120-client-crashes-while-connecting-to-a-new-dev-loo-issue-55bd775c8026.md` | 5 reviews (ids 50/196/291/381/440) | + review id 496 + addendum review id 498 | required independent review record | remove only the appended review/addendum text |
| `docs/issues/improvement/20261120-end-to-end-mimita-development-and-networking-con-issue-cf9cd77eb056.md` | no 11-26 snapshot-publish proposal | + one process proposal | required process-improvement record | remove only the appended proposal text |
| `.dev/agent-coord/brain.db` | no review id 496/498 | review ids 496 and 498 | coordination record | restore from backup if withdrawn |
| `docs/changelog/2026-11-26/20261126_2338-issue-55bd775c8026-independent-review.md` | absent | new changelog | one changelog per repository-touching session | delete file |

# Contract / ABI changes

None.

# State / schema changes

- `issue_review` added review id 496 (`issue-review-sweeper`, 2026-11-26T23:37:07.640Z) to `issue-55bd775c8026`; state stays `in_review`, owner unchanged.
- `issue_review` added addendum review id 498 (2026-11-26T23:37:54.948Z) correcting the "NEW" label in id 496.
- `issue_suggest` appended one process proposal to `issue-cf9cd77eb056` (its doc line 1971 onward, `updated_utc` 2026-11-26T23:37:08.165Z).

# Diagnostics

- Readiness: `ready=true`, score 8/8, missing=[], state `in_review`, 3 open tasks (`new-mode-connect-crash-trace`, `welcome-delivery-trace`, `welcome-transport-label`).
- Proof: `agent-coord proof list --issue issue-55bd775c8026` returns `[]` again; no proof record is linked to any task. No completion claim is supported.
- Primary new evidence, run `20261126_225234` (build `2026-11-26T22:52:33.481041Z:cb72ee40...d781c`, vehicle_arena/arena2, room C5V7MXD, server PID 15808, client PID 28676): client first-packet 22:52:40.278Z and snapshots every ~1s; last `network.client.snapshot.applied` 22:52:57.757Z (snapshot_tick 1220); `health_lost` reconnect scheduled 22:53:08.613Z with `snapshot_age_ms=10002`, `last_heard_age_ms=5`; 10 reconnect attempts accepted by the server (`token_found`/`current_token`/`slot_state=held`); terminal `ReconnectFailed` 22:54:08.619Z with `connected=true`, `packet_received_after_disconnect=true`, `last_heard_age_ms=41`.
- Server side: `network.server.snapshot.build`/`snapshot.send` and `network.metrics.window` gap 22:52:57.755Z -> 22:55:18.618Z (140.863s) while `performance.server-loop-window` stayed ~60 Hz and gameplay/reconnect-accept events continued; the stream resumed in the same 5ms as `network.server.player.removed` `reconnect_grace_expired` (silent_ms=70005) at 22:55:18.613Z. Three `performance.server-publish-breakdown` rows in the gap tail show `publish_us` 105581-133046 and `snapshot_build_send_us` 97-122503 (sub-second, not the 141s span).
- The dev-loop journal for run 225234 emitted no connection-failure/first-missing-event record for the client; `run.json` shows client `offline`.
- Adjacent crash family: `.dev/crashes/20261126_{061338,145453,145549}/crash-*.txt` are all arena1 vehicle_freeplay server-start ACCESS_VIOLATION (0xc0000005, stack through `GetMimitaGameServerPolicyV1+0xdc6f`); run `145549` is already documented on `issue-cf9cd77eb056` review id 477, which also records that its client never reached Connected. Runs `061338`/`145453` lack retained journals.
- No `3221225477` client access-violation exit and no `3221226356`/`0xC0000409` exit was found in retained 2026-11-26 evidence; several `dev-loop.instance.exited` rows carry `exit_code=4294967295` with `crash_artifacts=[]` (distinct lifecycle observation, not a connect failure).

# Validation

- `agent-coord.py begin/sync/observe`, `issue brief/show/readiness`, `proof list`, journal scans with Python JSON parsing, crash `.txt` reads, and `git status`.
- `issue review` appended ids 496 and 498; `issue suggest` appended one proposal to `issue-cf9cd77eb056`.
- No compiler, build request, module worker, second dev-loop, or runtime test run was invoked.

# Findings

1. The strongest current failure is a server-side snapshot-publication stall: the server stopped emitting snapshot build/send and metrics for 140.863s while its simulation loop, gameplay, and reconnect-accept path stayed healthy, then resumed exactly when the expired player slot was removed.
2. The resulting client `ReconnectFailed`/Disconnected was terminal-but-misleading: the client was still receiving packets and the server kept accepting reconnects and holding the slot. This matches the user's "don't show disconnected if not actually disconnected" complaint more closely than any prior retained run.
3. The arena1 vehicle_freeplay server-start crash family is already recorded on `issue-cf9cd77eb056` review 477 (and is not a new defect from this review); for run 145549 it explains "start a mode and never connect". Runs 061338/145453 need retained journals before attribution.
4. The dev-loop did not automatically surface either the 141s server snapshot stall or the client terminal reconnect failure, and three of four crash artifacts lack retained run journals.

# Confidence

High on the run 225234 timeline, gap duration, reconnect acceptance, and the pre-existing review-477 crash link (all direct journal/crash reads and DB queries). Moderate on the inference that the publish path is coupled to the held player slot: the gap starts before the client notices and ends exactly at slot removal, but the causal lock/queue/branch is not proven. Unknown: why the publish stream first stopped at 22:52:57.755Z, and whether 061338/145453 represent the same client-connect failure.

# Proof

No proof claim. Read-only review; evidence is journal-tier, crash-artifact-tier, and runtime-state-tier. No behavior change introduced.

# Regression review

- Regression entry appended: no
- Why: no confirmed code change or fix; observations recorded on the issues.

# Human acceptance

- Visual, gameplay, multiplayer review: none captured by this reviewer. The HUD "Disconnected vs Loading/Reconnecting" visual claim remains unproven; no screenshots exist for run 225234.

# Agent handoff context

- Owner: `juggernaut-npc-stuck-shooting` (issue-55bd775c8026); `root-connection-reliability` owns the umbrella issue-cf9cd77eb056.
- Next exact step: on a pinned root-dev-loop reproduction of run 20261126_225234 (vehicle_arena/arena2, one client), capture per-tick `performance.server-publish-breakdown`, the branch decision that skips `network.server.snapshot.build/send` while a player slot exists, and the snapshot-send lock/queue state at stall start and end. Acceptance: exactly one of (i) publish early-returns because the player connection is considered not-ready/held, or (ii) publish is blocked on a named lock/queue released by player removal. Then verify the HUD shows Loading/Reconnecting during a non-terminal reconnect.
- What another agent should not redo: do not re-cite the Nov-21 player-slot-erase or token-loss hypothesis (reconnects were accepted today); do not file the arena1 vehicle_freeplay startup crash as new (already on issue-cf9cd77eb056 review 477); do not treat the run 225234 Disconnected as a client or datagram-delivery defect.

# Automatic evidence

- Git status: shared dirty tree preserved; this session added two issue reviews, one issue suggestion, coordination DB changes, and this changelog
- Build identity inspected: run 225234 build `2026-11-26T22:52:33.481041Z:cb72ee40...d781c`; crash family build `2026-11-26T06:10:41.962780Z:31b8690d...2efe7`; no build requested
- Journal paths checked: `logs/2026-11-26/20261126_225234/{client-01,server-01,dev-loop}/events.jsonl`, `logs/2026-11-26/20261126_225234/run.json`, `logs/2026-11-26/20261126_{061338,145453,145549}/run.json`, `.dev/crashes/20261126_{061338,145453,145549}/crash-*.txt`

# How to reverse

- Remove only the appended review (id 496) and addendum (id 498) text from the `issue-55bd775c8026` doc; do not rewrite prior text.
- Remove only the appended process proposal text from the `issue-cf9cd77eb056` doc.
- Restore `.dev/agent-coord/brain.db` from backup if the reviews/suggestion are withdrawn.
- Delete this changelog. No commit to revert.
