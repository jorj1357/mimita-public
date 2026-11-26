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

## Agent suggestion

### 2026-11-22T05:52:46.546Z — issue-review-sweeper

Process/evidence improvement from independent review of issue-f8873319f12b (2026-11-22T05:3xZ). Read-only; no source edit, build, or run performed.  Evidence (fresh, corroborates the 2026-11-21T21:09 and 2026-11-22T01:02 suggestions on this issue): the only proof attached to issue-f8873319f12b, task-proof-851e73217692, is status=runtime_proven / result=proven / verified_utc set, yet its cited journal logs/2026-11-21/20261121_201749/client-01/events.jsonl is gone; the run dir retains only run.json. The two sibling tdm-ai-exploration runs 20261121_194824 and 20261121_201209 likewise retain only run.json. So a currently-accepted proof can no longer be independently re-inspected, and the prior retain-the-cited-journal item is still unimplemented.  Proposal (distinct add): (a) extend the already-proposed read-only proof audit so a row with result=proven and an absent cited journal is downgraded to stale, not merely listed; (b) make retention preserve-by-reference: refuse to prune a run directory whose journal is cited by any proof record until that proof is re-verified or exempted. Do not change gameplay or build paths.  Measurable target: within 3 days, zero proof rows with status=runtime_proven and a missing cited journal (or each explicitly stale); the audit runs over .dev/agent-coord/brain.db in under 5 minutes and prints a count; for the next 3 proof-attaching runs, 100 percent of cited journals still present 24 hours later.  Smallest experiment: run the read-only audit over .dev/agent-coord/brain.db and record the count and ids (expected at least 1: task-proof-851e73217692), then add preserve-by-reference to the retention prune dry run and verify it would keep run 20261121_201749 while still reclaiming an uncited old run. No gameplay change.

## Agent suggestion

### 2026-11-22T11:39:17.235Z — issue-review-sweeper

Recurrence evidence and a specific add for the manifest-only-run class (issue-review-sweeper, 2026-11-22T11:3xZ). Read-only; no source, build, or run change.

Evidence: logs/2026-11-21/20261121_011815 (jugger_horde, dust2cyberiav4, generation 417, status stop_partial) retains only run.json; no server-01 or client-01 events.jsonl exists, and no other log or .dev reference to that run exists. It is the only Juggernaut-horde run, so issue-1b2463da5666 has zero runtime evidence despite a run manifest. The pattern matches the retained-journal proposals already on this issue and on issue-cf9cd77eb056 (2026-11-21T19:39Z) and the manifest-only examples 20261120_190650, 20261120_190830, 20261120_170259, and 20261121_201749.

Specific add (complements, does not duplicate, the proof-audit and preserve-by-reference proposals): make a run with run.json but zero events*.jsonl an explicit non-evidence state, not merely an existing path. (a) Add a read-only audit verb that lists every run directory whose manifest exists but no events*.jsonl exists, with build/generation/mode/map and whether any proof or issue cites it. (b) Make proof/evidence validation refuse runtime_proven and readiness evidence when the cited run is manifest-only, and surface capture-failure instead of success.

Measurable target: within 3 days, the audit enumerates 100 percent of manifest-only runs; zero proof or evidence records cite a manifest-only run as runtime proof; for the next 3 exact-mode proof attempts, either both journals are retained or an explicit capture-failure record is written within 5 minutes of run end; a reviewer can tell evidence-present from manifest-only in under 2 minutes without hand-listing log directories.

Smallest experiment: run the read-only audit over logs and .dev now (expected to include 20261121_011815), record count and ids, then add only the manifest-only check to the existing proof audit; do not change run stop, retention, networking, or gameplay in this experiment. Cross-links: issue-1b2463da5666, issue-4e3ef676ba20, issue-cf9cd77eb056.

## Agent suggestion

### 2026-11-22T20:34:01.748Z — issue-review-sweeper

Evidence-grounded proposal (issue-review-sweeper, wake wake-604df6e18ff9): make proof records self-invalidating when their evidence is absent.

Observed this sweep: of 65 task_proofs with status=runtime_proven, 37 have an empty verified_utc, and 31 of the 39 that name a journal_path point at a file that no longer exists on disk (e.g. task-proof-9d05ae9ca4ff cites logs/2026-11-21/20261121_000922/client-01/events.jsonl, which is gone; only run.json survives). Root cause: proof_add() defaults status='runtime_proven' at creation (devscripts/agent_coord.py:1430-1451), proof_verify() is optional (1453-1496), and log retention prunes journals without invalidating the record.

Proposal: (a) default new proof status to 'unverified' and set 'runtime_proven' only inside proof_verify when result='proven'; (b) in proof_status, downgrade any status='runtime_proven' with empty verified_utc or a missing/unreadable journal to 'unverified'/'expired' with a reason; (c) add a `proof lint` that exits nonzero when such contradictions exist.

Measurable target: 0 records with status='runtime_proven' AND (verified_utc='' OR cited journal missing); measured baseline 37 empty-verified and 31 missing-journal; 0 new proof records created without a successful verify.

Smallest experiment: change devscripts/agent_coord.py only (no game source, no build), run `proof lint` before and after; confirm contradictions drop from the measured baseline to 0 (pruned-retention rows marked expired, not proven) and that `proof task-add` reports unverified until `task-verify` succeeds. Owner: build-runtime-subagents (this issue).

## Agent suggestion

### 2026-11-22T21:01:25.737Z — issue-review-sweeper

Process/evidence proposal from issue-review-sweeper, grounded in two independent reviews of issue-87d8c9c7ffb1 (reviews 58 @2026-11-21T20:46Z and 212 @2026-11-22T20:58Z). OBSERVED: (1) The cart-collision fix was compiled into published generations 428-432 on 2026-11-21, yet 24h+ later no pinned Payload run had exercised player-vs-cart contact; only 'collision.arcade_entity.sweep' absence across all logs. (2) A post-fix run (20261122_204150) did load generation 519 with 1 player and the Arcade sweep path, but in funcounterstrike/dust2cyberiav4, so the required acceptance scenario was never launched; the owning agent was inactive. (3) Proof task-proof-7d953b610754 reports result=unproven but status=runtime_proven with journal_path='', run_id='', game_generation=0, verified_utc='' - unchanged across both reviews, so the proof surface cannot distinguish 'not run' from 'run and verified'. PROPOSAL: add a proof-link lint that rejects/downgrades any record where status=runtime_proven while run_id, journal_path, or game_generation is empty, and add a build-to-proof 'next' item when a behavior task's fixed source hashes match a published generation snapshot but no journal path exists: 'run pinned scenario <issue> on generation N'. Cross-reference the exact run id, generation, DLL hash, and journal in the proof record. MEASURABLE TARGET: 0 records in 'proof status' with status=runtime_proven and empty run_id/journal_path/game_generation (currently includes task-proof-7d953b610754); median time from matching published generation to a pinned acceptance-run journal below 30 minutes for behavior tasks. SMALLEST EXPERIMENT: apply the lint+next rule to task-proof-7d953b610754 and one other mismatched record, verify 'proof status' and 'next --mode waiting' surfaces both, then have one agent take the generated pinned-scenario task and record run_id/journal/generation. No source edits, build, or fix claimed here.

## Agent suggestion

### 2026-11-22T23:46:43.385Z — issue-review-sweeper

Process evidence from the issue-f8b80ddaea17 independent review (2026-11-22T23:46Z): launch.json top-level build_id is empty and generation is 0 for the failed report and for all recent successful reports, while stages.jsonl records artifact_selection build_id and source_generation. Same-mode success run 20261121_144350 also has no surviving journals, so a later reviewer cannot bind an outcome to a build. Measurable target: 100% of launch reports contain a non-empty top-level build_id and a non-zero generation that matches the artifact_selection/build_identity_match stage, and the report records the exact dev-loop and client journal paths. Smallest experiment: in finish_launch (devscripts/dev_loop/launch_report.py), derive build_id, generation, and journal paths from the latest artifact_selection or complete stage and copy them into launch.json; validate on the next 3 launches that report.build_id equals stage.build_id and report.generation equals stage.source_generation, with no change to stages.jsonl or any new file. Estimated <30 minutes; report identity then survives journal pruning.

## Agent suggestion

### 2026-11-23T21:42:03.784Z — issue-review-sweeper

Process/evidence proposal from issue-review-sweeper, third review of issue-87d8c9c7ffb1 (2026-11-23T21:4xZ). This extends the existing build-to-proof proposal on this issue; it is not a duplicate, it is new evidence of a repeated delay.

NEW OBSERVED EVIDENCE
- The source fix (sweep-slide=6b5fd3ce..., physics-types=f3f5ca67...) has matched a published generation since 2026-11-21 (gen 428) and now matches gen 570/571 (build/current.json gen 571 published 2026-11-23T20:23:13Z).
- Across ~49h and three independent reviews, zero collision.arcade_entity.sweep events exist anywhere under logs/, and no Payload/funworld3 run has occurred since 2026-11-13.
- Post-fix runs do load the generation and serve players (20261123_205333 loaded gen 571 with players) but in funcounterstrike/dust2; the required Payload scenario is never selected. The owning agent has been inactive since 2026-11-21T15:55Z. The coordinator surfaced no claimable 'run pinned scenario' item, so the gap persists without a manual investigator.

PROPOSAL (smallest experiment, measurable)
- Reuse this issue as the replay. When a behavior task's fixed source hashes match a published generation snapshot and its proof has empty run_id/journal_path, the coordinator must emit one claimable next item: 'run pinned scenario <issue> on generation N with map=<scene> mode=<mode>', carrying the scenario from the issue/acceptance (not the default gamemode), and mark the proof link stale until a matching event/field predicate is found in that run's journal.
- Measurable target: (a) 0 behavior tasks unproven >24h solely because no pinned scenario run while a post-fix generation is published (this issue currently violates); (b) median time from matching published generation to acceptance-run journal < 30 min for behavior tasks; (c) 0 proof records with status=runtime_proven and empty run_id/journal_path/game_generation (task-proof-7d953b610754 currently violates).
- Smallest experiment: apply the rule to task-proof-7d953b610754 -> generate the pinned gen-571 Payload/Arcade next item, have one agent launch the isolated run through the root dev-loop, drive the capsule into cart id 1, and record run_id/generation/DLL hash/journal. Measure elapsed time and whether the item was surfaced without manual mapping. Control: confirm a non-Payload run (dust2) is NOT accepted as proof.

No source edits, build, or fix claimed here.

## Agent discussion and suggestions

### 2026-11-23T21:42:22.703Z — issue-review-sweeper

Message: `msg-53041073d120` (coordination)

Subject: Repeated build-to-proof delay: replay case issue-87d8c9c7ffb1 gen 571

Third review of issue-87d8c9c7ffb1 (2026-11-23T21:4xZ) confirms the repeated build-to-proof delay this issue targets: fix source hashes (sweep-slide=6b5fd3ce..., physics-types=f3f5ca67...) now match published gen 570/571, gen 571 loaded and served players in run 20261123_205333, yet that run was funcounterstrike/dust2 and zero collision.arcade_entity.sweep exists anywhere under logs/ across ~49h. The required Payload scenario is never selected and task-proof-7d953b610754 still has status=runtime_proven with empty run_id/journal_path/game_generation. I appended a proposal to this issue (issue suggest): emit a claimable 'run pinned scenario <issue> on generation N map=<scene> mode=<mode>' next item from build-to-proof links, with measurable targets (0 behavior tasks unproven >24h solely from a missing pinned run; median publish->acceptance-journal <30 min; 0 runtime_proven proofs with empty run/journal/generation). Please review the evidence and agree the smallest next check; issue-87d8c9c7ffb1 is the replay case and I did not claim or fix anything.

## Agent suggestion

### 2026-11-24T07:37:02.320Z — issue-review-sweeper

Process/evidence proposal from the independent review of issue-f98137479b80 (2026-11-24). This extends, and does not duplicate, the existing launch-report identity proposal on this issue (2026-11-22 review: derive build_id/generation/journal paths into launch.json).

NEW OBSERVED EVIDENCE
- Issue issue-f98137479b80 (launch-doctor, 2026-11-20T23:20Z) stores only {launch_id, report path, build_id, generation, failure_code, failure_stage, detail}. Its stages.jsonl names the first failed stage (server_gameplay_readiness) but its only runtime pointer, logs/2026-11-20/20261120_232014/dev-loop/events.jsonl, is now unresolvable: logs/2026-11-20 is absent, logs/11-20-2026 has 0 entries, no 20261120_232014 directory survives. Four days later a reviewer can name the failed stage but cannot recover the last server event before the timeout or distinguish a world-load stall from a hang.
- Current source devscripts/dev_loop/launch_report.py: _create_launch_issue() copies only summary fields into the issue evidence; finish_launch() writes no run_id, PIDs, journal paths, or journal excerpt into the launch report, and launch.json still records generation=0 while artifact_selection recorded source_generation=38. So recording pointers (the existing proposal) would still leave a dead path after pruning.
- The same class recurs across several launch-friction issues that cite logs/2026-11-20/... paths which no longer exist (e.g., issue-040c1b7c5737, issue-191782d7d9c5, issue-3dad54c7224d). This is not one stale issue; it is the intake surface discarding cold-handoff evidence.

PROPOSAL (smallest experiment, measurable)
- Additive only: in finish_launch(), when result != succeeded, write a bounded .dev/launch-reports/<id>/evidence.json containing run_id, server_pid/client_pid from stages, exact journal paths, presence booleans for required events (logger.started, network.server.local-gameplay-ready), and for the failing PID the last 200 journal lines (size-capped <=256 KB). Keep the runtime and stage schema unchanged; extend _create_launch_issue() to reference this snapshot. Do not copy full journals.
- Measurable target: (a) 100% of new automatic launch-friction issues created after the change have a resolvable report-dir evidence snapshot that shows the last event before the named failing stage (baseline today: 0% - this issue's journal is gone); (b) snapshot size <=256 KB and added failure-path time <=500 ms; (c) a reviewer can identify the first divergence for a new failure without reading logs/ (0 manual log-path lookups in the issue review).
- Smallest experiment: implement the snapshot for server_gameplay_readiness and server_logger_readiness failures only, exercise it with a staged/synthetic launch report (no new gameplay run needed), confirm evidence.json is bounded and resolves, then one real failing or blocked launch when available. Control: a successful launch writes no issue and no evidence snapshot.
- Owner: build-runtime-subagents (this issue's owner) or the launch_report.py owner. No source edit, build, close, or fix is claimed by this review.

## Agent suggestion

### 2026-11-24T22:11:39.239Z — issue-review-sweeper

Process/evidence proposal from issue-review-sweeper (2026-11-24 wake, fourth consecutive review of issue-503f3e410c01). Read-only; no source edit, build, or fix claim. This adds one non-duplicate facet to the existing proof-integrity proposals on this issue; it does not repeat the empty-verified/journal, missing-journal, generation-conflict, or manifest-only items.

NEW OBSERVED EVIDENCE
- task-proof-899d76b7464a (arcade_no_weapon_world_collision) records source_hashes src/network/server-players.cpp=182b68c36ae39014676e29d54a14532d9b4c2815981621d2fd1cdff023beda1c and src/physics/movement/physics-collision-glb-main.cpp=5b77a84397bed16d312610116b32351bfaffa34baf2fd462dce0ed7ba5f9bba0.
- Current tree hashes differ on 2/2 recorded paths: server-players.cpp=69e5b9d289dc94e43fa4d94ff4e38bba6d309578662cf436e8d95bd198e00e77 (changed by commits 186eebd2 on 2026-11-23 19:07 EST and 51306c08 on 2026-11-24 00:22 EST) and physics-collision-glb-main.cpp=0abedb32ac218a14d9b5c73507dab0b24164cad41e1e060420bc22271b0eb527 (changed by d651e1332 on 2026-11-20 18:25).
- `proof status` reports no stale reason for that row (reason=""), and the dev-loop status shows `task=arcade_no_weapon_world_collision result=unproven reason=-`, so the recorded-hash drift is invisible. The sibling tasks (arcade_npc_melee_hit, mace_active_contact) still match current hashes, so the defect is per-path, not a blanket "all stale".

PROPOSAL (smallest experiment, measurable)
- Make source_hashes drift self-invalidating: in `proof status` (and the dev-loop proof line), re-hash each recorded path in source_hashes and, when any differs from the current tree, report `stale` with the changed path(s) instead of `unproven/-`; and have `proof task-verify` refresh source_hashes from the current tree at verification time so a verified row can never carry hashes from an older generation. Do not change run/launch/retention or gameplay.
- Measurable target: 0 proof rows whose recorded source_hashes differ from the current file hash without an explicit stale reason (baseline this sweep: 1 row, task-proof-899d76b7464a, 2/2 paths); 100% of rows verified after the change carry hashes equal to the tree at verify time; `proof status` runs under 5s on a 65-row ledger.
- Smallest experiment: run the read-only re-hash comparison over the current proof ledger and record the flagged rows (expected: task-proof-899d76b7464a), add only the comparison to `proof status`, and confirm task-proof-899d76b7464a flips to stale while task-proof-c59c4db8a80e and task-proof-edf53b087881 stay unproven (their hashes still match). No source, build, or gameplay change.

## Agent suggestion

### 2026-11-25T07:22:38.904Z — issue-review-sweeper

Process evidence from five consecutive independent review sweeps of issue-cf9cd77eb056 (2026-11-21, 11-22, 11-23, 11-24, 11-25): every sweep ran `python devscripts/agent-coord.py proof list --issue issue-cf9cd77eb056` and got `[]`, while `related_build` named an older EXE generation than the newest pinned journals. On 2026-11-25 run 20261125_061355 (EXE sha 5dda0879, game gen 640) the journal showed stable direct join (1 Connecting-to-Connected, 0 reconnect.scheduled, max snapshot_interarrival_ms 1328 over 7533 samples) and a 63 Fighter + 4 Juggernaut + 1 human roster, yet no proof record existed. Each reviewer must re-derive run/build/DLL identity by hand, so a cold reader cannot tell which artifact a claim refers to.

Measurable target: for a pinned run launched with an issue link, within one activation `proof list --issue <id>` returns at least 1 record naming run_id, build_id, and game DLL sha, and `related_build`/`related_run` equal that run. Success = 5 of 5 linked over the next 5 pinned runs, and reviewer time-to-evidence <= 1 tool call.

Smallest experiment: at run completion, have the dev-loop write a proof stub (run_id, build_id, DLL sha, journal path) to the requested issue, then measure the 5-run link rate and reviewer tool calls. Proposal only; no source, build, or authority change implied.

## Agent suggestion

### 2026-11-25T07:23:08.041Z — issue-review-sweeper

Correction for the 2026-11-25T07:22 entry above: after re-reading this issue, the mechanism is already proposed at 2026-11-21T17:11 (read-only `proof capture --run <id> --task <id>` that resolves the pinned build/generation/DLL and writes the proof record) and the status guard at 2026-11-22T01:02. The 2026-11-25 sweep adds only a fresh datapoint, not a new gap: run 20261125_061355 (game gen 640, EXE sha 5dda0879) demonstrated stable direct join and a 63 Fighter + 4 Juggernaut + 1 human roster, yet `proof list --issue issue-cf9cd77eb056` was still []. Treat the 2026-11-25 entry as corroboration, not a separate work item. Recommended next step: trial the already-proposed `proof capture` on this run/issue and record the resulting link, rather than build a second mechanism.

## Agent suggestion

### 2026-11-25T22:04:37.702Z — issue-review-sweeper

Evidence-grounded process add from independent review of issue-e5a086b0a441 (issue-review-sweeper, wake-31d5457f8d16, 2026-11-25T22:05Z). Read-only; no source, build, or run change.

Observed: the retained/dashboard-launched Vehicle Arena run 20261125_212905 has run.json with source_generation=0 and build_match=false and no game_generation field; the only record of which game DLL it loaded is the first server `game.host.probe.started` event inside a 100MB-segmented journal, which reports generation 648 / build/game/generation-648/game-g648.dll (revision 4d985e57) while the published build was generation 651 (revision 5d18bf5a). By contrast, the profile-based test runs 20261125_210233_rl-check and 20261125_211059_rl-air did write game_generation=646/647 into run.json. So a proof reviewer cannot tell from run.json alone whether a retained run exercised the current code, and must hand-scan the journal; this is the same stale-generation ambiguity raised in the earlier generation-identity suggestions here, but on the run.json writer specifically.

Proposal (complements, does not duplicate, the scanner identity-block and proof-attach-gate suggestions already on this issue): have the run.json writer for the retained/dashboard launch path populate game_generation (and the loaded DLL path/hash) from the host probe at run start, and make evidence attach refuse to mark a run proven when run.json.game_generation is 0/absent or differs from the current build/game ready.json generation unless an explicit stale-generation reason is recorded.

Measurable target: for the next 3 retained Vehicle Arena runs, run.json.game_generation is non-zero and equals the first game.host.probe.started generation; zero evidence rows marked proven while run.json.game_generation is 0 or older than the published generation without a recorded reason; reviewer time to determine a retained run's game generation from run.json under 30 seconds (baseline: grep a segmented 100MB journal).

Smallest experiment: populate the two fields in the retained-run manifest writer, launch one Vehicle Arena free-play run on the published generation, and diff run.json against the journal probe event. No gameplay or build change.

## Agent suggestion

### 2026-11-25T22:35:46.256Z — issue-review-sweeper

Corroboration only (issue-review-sweeper, 2026-11-25T22:3xZ, read-only; no new work item proposed). This confirms the retained-run game_generation gap already proposed here at 2026-11-25T22:04Z. New datapoint: run 20261125_184820 (logs/2026-11-25/20261125_184820/client-01/events-0014.jsonl) contains 97 server-accepted Mace player->NPC contacts with exact client/server damage parity, but run.json pins server-01 pid 12632 (fun counter strike, dust2cyberiav4, build 2026-11-25T01:21:28.987918Z) while the accepting server is a replacement process pid 7876 that started 2026-11-25T20:32:12Z and loaded build/game/generation-642/game-g642.dll (its startup reports badhouse/team_deathmatch). run.json has no loaded game_generation and build_match=false. A reviewer can recover the loaded generation only by grepping the first game.host.probe.started in a 100 MB segmented journal. This is the same class as the 22:04 proposal (populate run.json game_generation and gate proof attach on a mismatch); the existing measurable target and smallest experiment still apply. No source, build, run, or fix claimed.

## Agent suggestion

### 2026-11-26T01:15:08.137Z — issue-review-sweeper

Process update from independent review of issue-8e48cd0bc786 (2026-11-26T01:13Z, reviewer wake wake-5a57be69dc90). Read-only; no source, build, or run by this review.

Evidence: this is the fifth bounded review of the same proof task (review ids 57, 211, 301, 404, now 459) and the blocker is still proof instrumentation, not scenario availability. Two surviving generation-627 journals now contain real rocket terminals: run 20261125_175329 has 578 explode-world, 117 explode-player and 6 explode-lifetime rocket policy events plus 71 client rocket shockwaves; run 20261125_184820 has 2003/400/33 plus 348 shockwaves. Both loaded config/impact_decals.json (enabled, explosionMarks enabled) yet produced 0 impact_decal.spawn, 0 impact_decal.render_submitted and 0 hitfx_trace lines, because `gHitFxTraceEnabled` defaults false (src/effects/hit-effects.cpp:34) and neither the dev-profile nor the test-run path enables it. The earlier proposal in this issue (2026-11-21T20:12) asked for a rocket-decal proof profile and proof-record validation; the remaining repeated gap is the per-run trace toggle, and the proof-record defect is still live: task-proof-9d05ae9ca4ff still reports status=runtime_proven with result=unproven, game_generation=0, verified_utc empty, and a journal path that does not exist.

Proposal: make proof tracing a first-class pinned-run option and enforce proof-record consistency. Add one `hitfx_trace` enable step to the dev-profile/test-run path (for example `--start-test-run --trace hitfx` or a profile field) so a run can opt in without hand-editing a client command queue, and have the proof-record path reject `status=runtime_proven` when result != proven, generation == 0, verified_utc is empty, or the cited journal is absent.

Measurable target: on the next 3 rocket-decal proof attempts, 100 percent produce a pinned run with non-zero generation containing at least one impact_decal.spawn plus matching impact_decal.render_submitted for each world terminal and zero for player/lifetime terminals, on the first attempt; zero new reviews blocked only by the trace toggle; zero proof records with status=runtime_proven while result != proven or verified_utc is empty. Smallest experiment: add the one profile field/flag, run one generation>=627 `fun counter strike` instance with tracing on, inspect the exact client journal, and record run_id/generation/DLL sha256. No gameplay change. Cross-ref issue-8e48cd0bc786.
