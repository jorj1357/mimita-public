# issue-003b412c0836: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (true-positive-non-stall-stale-identity)

### 2026-11-27T20:34:42.483Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Independent bounded review — build-wait-reviewer, 2026-11-27T20:3xZ UTC. Read-only: no source edited, no build/run started, no fix or proof claimed.

OCCURRENCE
- Wait: wait_id game-dll-build:2026-11-27T18:41:02.539Z; stage game_build; budget 60.0s; elapsed 60.845s; source_generation 85; request build-request-1625350243a7.
- In-flight attempt (recovered from build-timings; not in the wait record): game-dll-20261127T184102743581Z-55292b, generation 805, revision fc7d555aa70be7ec4994eb754ae00f4d96b0a68c55cab3cfda4c1968bff51c1c, success=true, no_op=false, total 427.963s, published ready.json 2026-11-27T18:48:09.805Z.
- Artifact: .dev/build-timings/game-dll-20261127T184102743581Z-55292b.json (+ .resource.json/.resource.jsonl).

ACCEPTANCE / AUTOWIRE CHECK
- Exactly one issue opened: .dev/agent-coord/events.jsonl:22564 (coord.issue.opened, 18:42:03.919Z).
- Exactly one reviewer wake queued: .dev/agent-wake/events.jsonl:4032 (wake-3acc6701f327, 18:42:07.426Z). No duplicate issue or wake for this wait_id. Meets "one visible issue + one reviewer wake".
- Wait record carries wait_id, stage, reason, elapsed, evidence — met.
- "Build/run IDs" only partially met (known identity defect, issue-7d3dcfd7433b): evidence.build_id=2026-11-27T17:45:19.700118Z is the preceding EXE artifact (the one pinned in run 20261127_183715) and generation=804 is the last published DLL (attempt game-dll-20261127T183233046312Z-d5be13, completed ~18:35:45Z); run_id is absent. The actual in-flight target was gen 805, attempt ...55292b. Same self.latest_build staleness previously documented for this wait arm.

VERDICT: true positive, not a stall
- The build was progressing and completed successfully; the 60s crossing fired ~48s into real compile work, not a hang.
- Slowest/only changed TU src/npc/recast-navigation.cpp alone took 285.963s (compile_max_ms 285963.2) vs a 60s budget, so the crossing is deterministic for any save to this file.
- Phase split (postmortem.seconds): changed_source_preflight 286.041s, compile wall 290.658s (compile_sum 290.553s), link 119.636s (646 inputs, 506,491,744 object bytes), object_cache_scan 0.052s, publish 0.544s, total 427.963s; 2 compiled / 645 skipped of 646 sources, jobs 10.

CONCURRENCY (correlation; causation not proven)
- A live pinned run 20261127_183715 (tdm-validation, map dust2cyberiav4, EXE gen 804) was active 18:37:15Z–18:52:16Z, overlapping this build. logs/2026-11-27/20261127_183715/dev-loop/events.jsonl:4 (ready 18:38:02Z) and :5 (stopped 18:52:16Z).
- During the build the server logged npc.server-phase-window with ai_ms 238–399ms per 21-tick window at 19 NPCs (logs/2026-11-27/20261127_183715/server-01/events.jsonl); resource sidecar: mean host CPU 99.95%, peak 100%, minimum available memory 508.8MB.
- Base rate: the immediately prior build of the same TU (gen 804, 18:32:33–18:35:45, no live run yet) compiled recast-navigation.cpp in 123.472s with min free memory 2672.2MB. Same TU took 285.963s (2.32x) once the live NPC-heavy run was consuming CPU.
- Inference (not proof): host saturation and the concurrent live game likely inflated this TU's wall time. A matched baseline is not available, so I do not claim host pressure as the cause.

CROSS-LINK / NEXT CHECKS (no new proposal; consistent with existing umbrella asks)
1. This is another instance of the flat-60s-budget class (prior exemplars: vehicle-system.cpp, main-systems.cpp, server-gamemode.cpp). Fold into issue-7d3dcfd7433b (wait-latency tracker) and issue-136d4553ff92 (DLL build stage umbrella); do not open duplicates.
2. Record the in-flight attempt id + target generation + run_id in the wait record instead of self.latest_build (issue-7d3dcfd7433b). That would have made this issue self-identifying.
3. Decide progress/stage-gated budget vs p100 single-TU compile, and dedupe by (kind, stage, active attempt). Not a code change by this reviewer.
4. Latency target (owned elsewhere): src/npc/recast-navigation.cpp is a 1623-line, ~286s TU; splitting or reducing its includes would help materially. Reported, not claimed.

Owner: none claimed here. State left in_review with stale identity + duplicate-volume as the actionable gaps; no runtime proof claimed.

## Agent suggestion

### 2026-11-27T20:36:05.846Z — build-wait-reviewer

Supplement to the 2026-11-27 review (build-wait-reviewer, read-only).

Corrected wait-event location: the over-budget and reviewer-wake rows for wait game-dll-build:2026-11-27T18:41:02.539Z are in log run 20261127_174537, not the isolated game run. See logs/2026-11-27/20261127_174537/dev-loop/events.jsonl:179 (dev-loop.game-build.started, attempt game-dll-20261127T184102743581Z-55292b) and :180 (dev-loop.wait.reviewer-wake, wake-3acc6701f327, adapter=codex, 18:42:10.391Z). The isolated run 20261127_183715 referenced in the review is the concurrent live game pinned to EXE gen 804, not the wait journal.

Spec cross-reference (elevates this from "alarm noise" to a spec-mandated latency defect): docs/specs/dev-loop/build-performance-budget.md:141-150 says no single TU may exceed 10% of the full-build compile budget and that the "Current known outlier: src/npc/recast-navigation.cpp at 95-123 s. It MUST be split into [Recast/Detour build+config; tile/mesh adoption; query/path result adapter]. Each part must compile in a few seconds." This attempt measured that same TU at 285.963s: ~2.3x the documented 123s top end and ~9.5x the 30s hot-leaf hard ceiling, with the inflation coinciding with the concurrent NPC-heavy run. The split is already required by spec; no new design needed, only ownership. Reported here, not claimed.

No source edited; no build/run started by this reviewer.
