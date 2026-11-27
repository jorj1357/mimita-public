# issue-a9f9f82c92f7: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-27T01:37:46.493Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewer: build-wait-reviewer. Independent bounded read-only review of issue-a9f9f82c92f7 (wake wake-fc7e64af5b1c). No source edit, no build trigger, no fix.

OBSERVED FACTS
- Wait event dev-loop.wait.over_budget in logs/2026-11-27/20261127_001718/dev-loop/events.jsonl (wall 2026-11-27T00:52:49Z) for wait_id game-dll-build:2026-11-27T00:51:42.586Z, kind game-dll-build, stage game_build, elapsed 60.149s, budget 60.0s, started_utc 2026-11-27T00:51:42.586Z, issue issue-a9f9f82c92f7, created true.
- Same file contains exactly one dev-loop.wait.over_budget and one dev-loop.wait.reviewer-wake for this wait_id (2 total matches = one of each). Reviewer wake queued once: disable=no; .dev/agent-wake/requests/wake-fc7e64af5b1c.json shows one request (attempts=2: codex then opencode adapter fallback, launched 2026-11-27T01:24:03.277Z).
- Waited-on attempt: game-dll-20261127T005142888202Z-0200fc, request 2026-11-27T00:51:42.171Z-50a993ff:mode3:game, source_generation 18, target generation 747, revision 632747992f1e0f2f087649d6c3a76c7d4dee8b2278b78453ace925c2bebdc692. Timeline .dev/build-timings/game-dll-20261127T005142888202Z-0200fc.jsonl: build.started 00:51:43.685Z; object_cache_scan 2.734s; compile 33.297s (only module-identity-provider.cpp recompiled, 33.205s); link started 00:52:40.293Z; link.lld_linker failed 00:53:08.927Z (28.563s, exit 1); build.failed 00:53:13.456Z, total_seconds 90.89, generation 742.
- Therefore the 60.149s budget crossing (~00:52:42.7) occurred while the build was actively linking, not stalled. The attempt later succeeded in producing nothing: it FAILED at link.
- Failure is the same stale-object-cache defect already recorded: three gen-742 link failures (20aea5 105.9s, f583bc 56.3s, 0200fc 90.9s) with ld.lld undefined symbols for PlayerAiController::{active,meleeWeapon,buildFrame,setMeleeWeapon,setActive} and ClientCollisionWorldView ctor+vtable; forcing those two TUs to recompile yielded successful generation 748 (43dc27, 173.781s). Evidence and links: docs/issues/build-friction/20261120-game-dll-build-408-times-out-after-rebuilding-64-issue-a6890271d414.md and issue-51b425637ea9. No behavior code change.

ACCEPTANCE CHECK (satisfied)
- wait_id present: game-dll-build:2026-11-27T00:51:42.586Z. stage present: game_build. elapsed present: 60.149. reason present (build_attempt_id 0200fc, source_generation 18, generation 747, revision 6327...). build/request IDs present: build_id 2026-11-27T00:44:04.497616Z, request_ids [2026-11-27T00:51:42.171Z-50a993ff:mode3:game]. Evidence JSON present. Reviewer wake queued exactly once. All acceptance criteria met.

CAUSE SEPARATION
- Over-budget wait: valid, but a false-positive under the current flat 60s policy for a normal full game-DLL rebuild; the successful gen-748 build took 173.8s and the failed 0200fc took 90.9s. This is build bookkeeping, not a stall or hang.
- Build failure: a real object-cache-freshness defect (cached objects for player-ai.cpp and client-collision-world-view.cpp lacked their definitions), already recovered by forcing recompilation. Not caused by this wait. Tracked in issue-a6890271d414.
- Host load observed, not attributed: 0200fc resource capture (69 samples) mean_host_cpu_percent 98.503, peak 100.0, minimum_available_memory_bytes 440193024 (memory_used_percent ~96), 12 logical CPUs, with non-MiMITA load (RobloxPlayerBeta ~14%, FL64 ~9%, dwm, MsMpEng) plus two mimita.exe. unavailable_fields include matched_prebuild_resource_baseline, host_commit_charge_and_limit, journal_aligned_live_mode_workload_and_frame_tick_metrics, and runtime_alignment status unavailable. No matched baseline, so host pressure is NOT claimed as the cause.

RESOURCE GAP
- gap_key build-resource-sampling:game-dll-20261127T005142888202Z-0200fc is already recorded in capability issue-b9edd402e658 (docs/issues/capability/20261124-automatically-show-host-resource-use-and-reasons-issue-b9edd402e658.md) which explicitly names this attempt. No duplicate report created; no cause inferred.

WAKE DISPATCH CAVEAT
- Queue is correct (one request), but dispatch was late: codex adapter launched 00:52:49 then exited, opencode fallback launched 01:24:03 (~31 min). That dispatch latency belongs to issue-84c584957e65 (over-budget reviewer wakes dispatch late), not this wait's acceptance.

OWNER / NEXT STEP / UNKNOWNS
- No fix claimed here; no file edited. Issue state remains new (not closed).
- Proposed owner for the wait-policy noise: umbrella issue-e044a931c54b / build-runtime-subagents; flat-budget scaling already covered by issue-136d4553ff92 and issue-9a4503f6ed68.
- Unknown: exact object-cache freshness predicate that allowed the two TUs to be skipped while their cached objects were stale; not investigated in this bounded review.
