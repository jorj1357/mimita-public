# issue-b396205aaecf: Runtime registry and build-to-proof links

## Human report (verbatim, immutable)

> 


## Assignment

### 2026-11-20T03:17:29.580Z — build-runtime-subagents

Owner: build-runtime-subagents

## Agent suggestion

### 2026-11-21T17:11:23.673Z — issue-review-sweeper

Proposal from independent review of issue-503f3e410c01 (2026-11-21): auto-correlate a finished pinned run journal to proof-task predicates. Observed: run 20261121_140624 (gen420) already contained 285 accepted Mace claims, 53 active-pose 10x claims, and 56 NPC-to-player melee contacts by 14:37, but issue-503 three proof tasks were still unproven at 17:01; proof correlation happened only when a reviewer manually joined client and server events (~2.4h avoidable delay). Measurable target: for a pinned run whose journal contains a task required events, create the proof record (or an explicit not-observed note) within 5 minutes of run end; reduce median run-end-to-proof latency for such tasks from ~144 min to under 5 min. Smallest experiment: add one read-only CLI verb, proof capture --run <run_id> --task <task_id> --event <event> --field <predicate>, that resolves the run pinned build/generation/DLL hash and journals, scans for the predicate, writes the existing proof record, and reports missing identity fields. Trial it on issue-503 three tasks using run 20261121_140624 and measure elapsed. No gameplay behavior change.

## Agent suggestion

### 2026-11-21T20:12:04.237Z — issue-review-sweeper

Process/evidence improvement found during independent review of issue-8e48cd0bc786 (2026-11-21T20:11Z). Read-only; no code, build, or run performed.

Evidence: the only pinned run on generation 430, `20261121_195955`, used the `tdm-ai-exploration` profile (launch_mode 41, TDM on arena1). Its server journal has zero projectile/rocket/explosion/impact events and its client journal has only `impact_decals.config_read` + `performance.client-projectile-window`, so it cannot satisfy task `rocket-impact-placement-proof` (event `impact_decal.render_submitted`, predicate `decal_id`). `devscripts/dev-profiles/` has 7 profiles and none selects `rocket_duel`; the only rocket scenario is launch mode 43 "rocket launcher duel" in `devscripts/dev-launch-modes.json`, which the profile-based `--start-test-run` path does not use. The issue's earlier run `20261121_000922` did use mode 43 on generation 416, but only `run.json` survives (both journals were not preserved), and its proof record `task-proof-9d05ae9ca4ff` claims `runtime_proven` with `game_generation=0` and a journal path that does not exist. So a run and a proof can both cite the issue while exercising (or retaining) the wrong scenario.

Proposal: bind each proof task to the scenario it requires and make the pinned test-run path able to launch that scenario. Add a bounded `rocket-decal-proof` dev-profile (mode/gamemode `rocket_duel`, map `funworld3`, 1-2 clients, fixed 0-5 NPCs) so `--start-test-run` exercises the rocket decal path directly, and extend the proof-record path to reject or mark unproven a cited run whose profile/generation does not match the task's declared scenario, whose journals are absent, or whose generation is 0.

Measurable target: on the next 3 rocket-decal proof attempts, 100 percent cite a pinned run whose profile is `rocket_duel` on a non-zero `game_generation` with both journals present, and each attempt yields at least one `rocket.impact-mark.resolved` + `impact_decal.spawn` + `impact_decal.render_submitted` `decal_id` for a world hit, or an explicit not-observed note; zero proof records with `status=runtime_proven` while `result!=proven`/`verified_utc` is empty. Median time from proof attempt to a scorable run/journal under 10 minutes.

Smallest experiment: add the one dev-profile JSON and a read-only validation in the proof-record path that rejects a cited run/journal that is absent, generation 0, or profile-mismatched; then run one rocket_duel pinned test on generation >= 430 and inspect both journals. No gameplay change. Cross-ref issue-8e48cd0bc786; this complements the existing proof-integrity proposal on issue-cf9cd77eb056 (2026-11-21T19:39Z).

## Agent suggestion

### 2026-11-21T21:09:55.806Z — issue-review-sweeper

Process improvement from independent review of issue-1d1b9c62ac08 (2026-11-21T21:09Z). Read-only; no code, build, or run performed.

Evidence: one surviving journal (run 20261121_144350) contains three conflicting generation identities. 329 game.hotness.summary events (20:23:24-20:29:59) all report generation=432 and server_path="dll" (HostRuntime active whole-game). 375 npc.phase-window events in the same window all report module_generation=420 (ModuleHost.activeGeneration("npc-behavior")). run.json pins build 2026-11-21T02:26:50... with build_match=false. A reviewer therefore cannot tell whether 6,147 npc.collision.fall-sample events (6,105 with world_contact=false) were produced by the candidate generation or a stale one. Proof task-proof-25a21cfca3c4 is unproven with no journal, and the only generation>=427 run that enabled NPCs (20261121_192618) lost both journals. The existing read-only scanner devscripts/event-proof-scan.py counts events but emits no generation identity, so the conflict is invisible unless a reviewer hand-greps multiple event types.

Proposal: extend the read-only scanner to emit a generation identity block per run: authoritative journal_active_generation (latest game.hotness.summary), per_module_generations (npc-behavior, map-geometry, actor-rendering), pinned_generation from run.json/build_identity, and explicit match/conflict flags. Make proof attach refuse to mark a task proven when the authoritative generation differs from the pinned generation, and retain the exact journal for any run cited by a proof.

Measurable target: within 3 proof attempts, 100 percent of attached evidence includes the generation identity block; zero accepted proofs with match=false or unknown; zero cited proof runs with missing journals; median reviewer time to resolve a generation conflict under 2 minutes.

Smallest experiment: run the extended scanner on run 20261121_144350 (must flag active 432 vs module 420 vs build_match=false) and on run 20261121_201749 (client generation 432; must not false-flag), then wire the identity block into one proof-attach path. No gameplay change.

## Agent suggestion

### 2026-11-22T01:02:22.415Z — issue-review-sweeper

Process improvement from independent review of issue-04079f42c6f9 (2026-11-22T00:55Z). Read-only; no code/build/run performed.

Evidence: task_proofs ids 16 (arcade_npc_melee_hit) and 17 (mace_active_contact), both from run 20261120_072010 / build game-dll-20261120T071159488678Z-9a31ba / generation 398, have status=runtime_proven while result=unproven, verified_utc='', and journal_path=''. The run manifest says status=failed with "test server 33684 did not report logger.started" and lists only a server-01 instance. The melee proof record that a reviewer would first find is therefore unusable but displayed as runtime_proven; discovering the failure requires hand-reading run.json. This is the same defect class as the prior two suggestions on this issue (proof status accepted without retained verified evidence), now confirmed on a second task family and with a concrete failed-run example.

Proposal: add a read-only audit verb (for example `proof audit --bad-status`) that lists every task_proofs row where status='runtime_proven' AND (result!='proven' OR verified_utc='' OR journal_path='' OR the cited journal is absent), and add a guard in proof-status derivation that refuses or downgrades that combination to stale/unproven. Run the audit once over the current brain.db, record the count and ids, and add one unit test with a synthetic bad record plus a good record.

Measurable target: zero rows with status=runtime_proven and result!=proven or empty verified_utc/journal_path (or every such row explicitly marked stale) within 3 days; the audit completes in under 5 minutes and is repeatable. Smallest experiment: implement the read-only query as a CLI, run it on .dev/agent-coord/brain.db, and compare its count against a hand count of failed run.json files cited by proofs. No gameplay change.
