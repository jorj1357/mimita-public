# issue-5d8ebe281ce6: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-wait-failed-build-codegen; acceptance-met; duplicate-class-no-new-issue)

### 2026-11-27T01:54:13.244Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded independent review of wait occurrence game-dll-build:2026-11-27T01:23:17.195Z (issue-5d8ebe281ce6). No source edits, no file leases taken.

OBSERVED (journal: .dev/build-timings/game-dll-20261127T012317214621Z-4a732c.json + .jsonl):
- Wait record: kind=game-dll-build, stage=game_build, budget=60.0s, elapsed=60.492s, started_utc=2026-11-27T01:23:17.195Z, source_generation=10, request=2026-11-27T01:23:16.924Z-ca1c886e:mode3:game.
- Exact build attempt game-dll-20261127T012317214621Z-4a732c: started 01:23:17.214Z, completed 01:24:21.897Z, total 64.688s, exit_code=1, success=false, error="codegen check failed: packet_registry_codegen.py". Attached note: agent rocket-league-feel, src/vehicles/vehicle-system.cpp + vehicle-types.h.
- Stage timeline: build.started 01:23:18.771; manifest_snapshot started 01:23:29.890 (+11.1s); enumerate_cpp completed 01:23:45.848 (13.62s); owner_resolver completed 01:23:58.155 (12.30s); source_ownership_and_digest 0.32s; enumerate_headers completed 01:24:05.590 (7.10s); header_digest started 01:24:05.606; last GAME_STAGE source_snapshot started 01:24:10.231 (elapsed 51.468s); failure 01:24:21.897. The 60s budget was reached during pre-compile manifest/source-snapshot, not during compile/link.
- Aligned resource sample (same attempt, .resource.json): mean_host_cpu=97.939%, peak_host_cpu=100%, min_available_memory=230,301,696 B; sampler_thread_cpu_ms=19,640.6 of 64.656s; process_discovery_wall_ms_total=41,088.8. Runtime alignment pinned to run 20261127_011757: server mode=team_deathmatch pid=5900, npc.server-phase-window total=394.50ms/50 ticks @01:24:17.237 (9 NPCs); client pid=34092 performance.fps=6.248, p95_frame=178.06ms, max_frame=193.88ms @01:24:16.989.
- Wake/acceptance: exactly one wake queued for this occurrence (wake-8545c858ed2c @01:24:25.178Z), first claimed 01:24:41 via codex, retried to opencode 01:34:36, launched 01:39:42.752 (~15 min dispatch delay driven by adapter fallback, not a duplicate wake).

INFERENCE:
- Real over-budget wait, but not a slow healthy build: it failed at codegen (exit 1) at 64.7s, only ~4.2s past budget, matching the stale-codegen signature tracked for the same build owner in issue-a6890271d414 (game DLL build 408 timeout + retry stale packet_registry_codegen.py).
- Host was CPU-saturated (98-100%) by the live TDM game while the build ran; manifest enumerate/owner-resolver/header enumeration (~33s combined) plus sampler overhead plausibly lengthened pre-compile time, but without a matched prebuild baseline this is inference, not proof.

EVIDENCE GAP (recorded once): gap_key build-resource-sampling:game-dll-20261127T012317214621Z-4a732c - unavailable matched_prebuild_resource_baseline, host_commit_charge_and_limit, client_mode_independent_confirmation. Host CPU/memory WERE captured, so this review makes no claim that host pressure caused the delay.

UNKNOWN:
- Whether the manifest phase would stay under 60s on an unloaded host (no matched baseline).
- Whether the flat 60s budget is intentional for game-dll builds; many sibling per-occurrence issues exist for the same wait class, so that is a policy/dedupe question owned elsewhere, not a new defect to file here.

VERDICT: Real over-budget wait on a failing build. Acceptance for wait_id/stage/elapsed/build+request IDs/evidence and exactly-one reviewer wake is MET. Underlying build failure is owned by issue-a6890271d414. No source fix applies to this per-occurrence issue; recurring class => occurrence recorded, no duplicate issue filed.
