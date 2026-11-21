# issue-9b5be6c88de0: Let AI inspect existing live game journals as proof

## Human report (verbatim, immutable)

> AI shoudl be able to watch the live events.jsonl to see if their changes show up in a live game, like , looking at the events.jsonl of a game that im running already, so that AI can test it themselves if they need to but also proof frmo the gamesthat im doing if im just already playing should count


## Deferred

### 2026-11-21T15:28:46.591Z — payload-arcade-collision-fix

Reason: Queued follow-up after the active Payload Arcade collision repairs and runtime proof attempt; retain as a claimable issue for subsequent work.
Until: (not set)

## User quote

### 2026-11-21T16:06:46.503Z — codex-autonomous-melee-runtime-investigation

Via codex-autonomous-melee-runtime-investigation; supports unspecified:

> can u make ti an issue that ai shoudl bea bel to inveistaget and launch the .exe and test this like autonomsly and figure out waht is going wrong with the live .exe running and look at eevnts.jsonl from taht exe
adn then can u do that as much as u can

## Agent suggestion

### 2026-11-21T16:06:48.882Z — codex-autonomous-melee-runtime-investigation

Expand acceptance from read-only observation alone to a bounded autonomous test workflow: first discover and inspect a matching user-owned run read-only; if none exists, launch an isolated test run through the root dev-loop with pinned EXE and DLL identities, drive a real scenario, then correlate client contact/claim with server receipt/rejection and authoritative damage in that run's events.jsonl. Identify the first missing data-path step and cite exact event names/timestamps and artifact IDs. Never replace or stop a healthy shared run. Acceptance: one reproducible live invocation proves discovery/tailing and one isolated invocation proves launch, scenario input, journal correlation, and correct refusal to attribute stale artifacts; report build, runtime, and human acceptance separately.

## Triage

### 2026-11-21T16:23:17.888Z — codex-autonomous-melee-runtime-investigation

Summary: Enable AI to inspect the exact live user-owned executable and journals read-only, or launch a pinned isolated executable through the root dev-loop, exercise the requested gameplay path, and identify the first data-path divergence. Correlate every journal to PID/start time, run, EXE artifact, and DLL generation; never perturb a healthy user run or count stale/mismatched evidence as proof.
Owner: (unchanged)
Priority: high

## Agent suggestion

### 2026-11-21T16:23:18.719Z — codex-autonomous-melee-runtime-investigation

The current tooling can launch isolated runs, but the evidence path needs automatic process identity recovery and rotated-journal aggregation. In 20261121_144350, the run manifest still reports server PID 7556 and client PID 11492 offline, while the same run-tagged client journal records server rejections from PID 29352; the process scan shows PID 29352 started later than the original client. Require the autonomous investigator to reconcile this replacement server identity, include rotated events-*.jsonl, correlate client claim to server reject and health application, then produce a concise evidence report. When an existing run lacks a human client, start a bounded isolated run pinned to the chosen EXE and DLL and execute a real input scenario. Pass criteria should include the exact first divergent event and explicit stale/mismatched identity refusal.

## Agent suggestion

### 2026-11-21T20:46:37.830Z — issue-review-sweeper

Process finding from independent review of issue-87d8c9c7ffb1 (2026-11-21): the fix was compiled into published DLL generations 428-432 (source hashes matched snapshots; generation 432 published 20:17:09Z), yet the only live run (20261121_144350, server PID 7556) still had generation-420 loaded and players=0. dev-loop --status already reported build=published_stale and latest_source_stale=True, but neither the board nor the proof record told the reviewer 'no post-fix run exists'; task-proof-7d953b610754 stayed result=unproven with empty run_id/generation/journal. That is an evidence-to-activation gap, not a source gap: ~5h after publish there was still zero collision.arcade_entity.sweep evidence anywhere under logs/. Proposal (safe, no compiler work): on proof record/review, compare the proof's expected source hash or generation with (a) the newest published generation and (b) the generation actually loaded by the running server (from game.host.probe.started), then emit one explicit classification - no_post_fix_run, running_generation_behind_published(<running> < <published>), or post_fix_run_present - with the exact IDs, and surface it in dev-loop --status and issue brief. Reject or flag any proof record whose status says runtime_proven but whose run_id/generation/journal_path is empty. Measurable target: after a fix publishes, an agent can decide in <=10s and at most 2 commands whether the existing user run can count or a pinned post-fix run is required; zero 'published but unproven for >1h while a stale server runs' incidents in a 24h sample; zero proof records with runtime_proven-status and empty run/generation/journal. Smallest experiment: replay this case (published gen 432, running gen 420) through the coordinator CLI and show the classification plus required next action, then show a control case with a pinned post-fix run classified post_fix_run_present. This directly supports the issue's 'correct refusal to attribute stale artifacts' acceptance.
