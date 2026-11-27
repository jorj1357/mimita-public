# issue-a42b497de2d4: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Context

### 2026-11-26T23:34:09.002Z — build-wait-reviewer

related_build: game-dll-20261126T230530084939Z-7deb99
related_run: 20261126_230415

## Review (legitimate-over-budget)

### 2026-11-26T23:34:49.302Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Verdict: legitimate-over-budget (not a hang, not a build fault).

Evidence inspected:
- Dev-loop journal: logs/2026-11-26/20261126_230415/dev-loop/events.jsonl
- Build timing summary: .dev/build-timings/game-dll-20261126T230530084939Z-7deb99.json
- Resource summary: .dev/build-timings/game-dll-20261126T230530084939Z-7deb99.resource.json
- Wake records: .dev/agent-wake/events.jsonl (wake-497e6d80722f)

Attempt identity: request 2026-11-26T23:05:29.871Z-2d398df5; build attempt game-dll-20261126T230530084939Z-7deb99; run 20261126_230415; generation 734; revision cc5144c6.

Timeline (UTC):
- 23:05:30.106 game-DLL request accepted.
- 23:05:32.915 manifest_snapshot started; 23:05:43.986 completed (10.906s, source_count 1010).
- 23:05:44.297-23:05:45.382 manifest_validation; 23:05:51.843-23:05:53.278 source_snapshot (1.000s).
- 23:05:55.081-23:05:55.436 resident_state_codegen (cache hit); 23:05:56.663-23:06:03.615 packet_registry_codegen (6.953s).
- 23:06:04.093 dev-loop.game-build.started.
- 23:06:07.147 to 23:07:06.097 single slow TU src/map/map-authored-geometry.cpp compiled 58.935s (gcc parse 25.11s, template instantiation 8.54s, preprocessing 14.16s).
- 23:07:09.093 identity provider compiled 2.770s.
- 23:07:25.707 dev-loop.game-build.result status=ok total_seconds=115.437 (compile 61.843, link 15.343).
- 23:07:32.930 dev-loop.game-generation.ready generation 734, duration 121.625s.

Wait occurrence: dev-loop.wait.over_budget at 23:06:42.071, elapsed 62.069s / budget 60.0s. It fired while map-authored-geometry.cpp was about 23s into a 58.9s compile of a single TU. Every stage reached a terminal success, so this is a genuine over-budget duration, not a stall.

Why it overran: about 34.6s of pre-compile pipeline (manifest snapshot 10.9s + codegen 7.0s + validation/snapshot) elapsed before the slow TU was even submitted, and that TU alone cost 58.9s. A healthy one-source game-DLL build of this kind therefore exceeds the 60s budget on the compile phase alone with no fault present.

Resource context (inference, not proven cause): resource sampler for this attempt reports mean host CPU 98.41 percent / peak 100 percent and minimum available memory 770,560,000 B while jobs=10. It also reports status=partial, sampler_thread_status=did_not_stop_before_timeout, and unavailable matched_prebuild_resource_baseline / host_commit_charge_and_limit / client_mode_independent_confirmation. Sustained CPU saturation is consistent with the 10-job compile, but this sample cannot separate compile demand from external load; do not claim host pressure as the cause.

Acceptance check:
- wait_id, stage, reason, elapsed, evidence: present and correct. Reason is readable this time (GAME BUILD revision=... sources=644 jobs=10 ...), so issue-6e08d89b55e3's truncated-tail symptom did not occur.
- Reviewer wake queued exactly once: yes (wake.queued 23:06:36.362Z, wake-497e6d80722f).
- build/run IDs: was partial. The stored evidence.build_id was the previously published artifact 2026-11-26T22:52:33.481041Z (dev-loop self.latest_build), not this attempt, and no run_id existed. I filled related_build=game-dll-20261126T230530084939Z-7deb99 and related_run=20261126_230415. Code gap reported to issue-136d4553ff92 (dev-loop.py:3896 uses latest_build; check_wait_budget never records run_id, while build_friction.py:187-190 acceptance says build/run IDs).
- Wake launched twice, not once: codex adapter launched 23:06:41 (pid 31984), then wake.retry at 23:29:24 switched to opencode which claimed/launched 23:29:26 (pid 2368). This is the known late-dispatch defect in issue-84c584957e65; occurrence recorded there.

First divergence: none in the build pipeline. The only defect in this occurrence is the wait record's build/run identity (prior artifact build_id, missing run_id), a reporting gap, not a build fault.

Confidence: high that the wait was a legitimate over-budget build (full request-to-publish journal reconstructs every stage). Medium that the 60s budget is systematically too tight for single-slow-TU builds (single sample; many prior occurrences and umbrella issue-136d4553ff92 support it, this attempt alone does not prove it).

Recommendation: do not treat as a build failure. Route budget/noise to issue-136d4553ff92 (owner build-coordinator). No source edit or claim by this reviewer.
