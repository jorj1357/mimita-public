# issue-e8e596d81554: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed-real-compile-wait-attribution-gap)

### 2026-11-28T09:12:19.999Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed this wait occurrence against the pinned game-DLL attempt, the dev-loop build journal, and the reviewer wake record. Conclusion: confirmed real over-budget wait caused by a genuine heavy compile, not a hung build. The wait itself and the reviewer wake behaved correctly; the issue still carries the known attribution gaps (wrong build_id/generation, unreadable reason) already tracked elsewhere.

WAIT OCCURRENCE (verified)
- wait_id game-dll-build:2026-11-28T08:41:29.889Z; started 08:41:29.889Z; elapsed 60.405s; budget 60.0s (over by 0.405s); reported stage "game_build".
- Issue evidence build_id 2026-11-27T17:45:19.700118Z is the stale EXE artifact, not this DLL attempt; issue evidence generation is 816 while the actual attempt produced generation 817. Same mis-attribution family already reported in issue-2235461bb31b/REG.
- The issue `reason` is a truncated mid-JSON tail (":58547.0,\"build_attempt_id\":..."). It is the last ~240 chars of the module-identity-provider translation_unit.completed row, so it is unreadable as written. This recurrence is already recorded in issue-6e08d89b55e3; no new issue created.
- Wake record: exactly one request for this issue (wake-bec34940dfde), queued 08:42:32.396Z, attempted twice on the opencode adapter, claimed/launched 09:03:31.295Z/09:03:31.554Z. Acceptance "one reviewer wake per occurrence" is met; queue-to-review was ~21 minutes, which is a separate latency observation, not this issue's defect.

PINNED BUILD ATTEMPT
- Attempt game-dll-20261128T084130319731Z-1d504b; request build-request-b402825fe078; source_generation 29; revision 23c97ffca676aecbd5780505a446110903de8090e83458218fb94844f01fb17e; exit 0; no_op=false; generation 817; total 79.157s (worker_total 59.141s).
- Changed sources: src/vehicles/vehicle-system.cpp, src/vehicles/vehicle-render.cpp. Build note header_fanout 41 (below the 50 threshold).
- Phase timeline from .dev/build-timings/game-dll-20261128T084130319731Z-1d504b.jsonl: build.started 08:41:30.512; manifest_snapshot 08:41:35.342-08:41:36.459 (828ms); manifest_validation 62ms; source_snapshot 610ms; resident_state codegen cache hit; packet_registry codegen 3.484s; compile wall 43.766s; link 13.156s; publish 0.578s; build.completed 08:42:49.374.
- Slowest compiles: vehicle-system.cpp 42.608s (parsing 21.13s, opt/generate 11.79s, template instantiation 7.47s, 576M GGC), vehicle-render.cpp 17.455s, module-identity-provider 0.986s. compile_sum 61.049s across 10 jobs.
- The 60.405s check landed ~08:42:30.294Z, just after vehicle-system.cpp finished at 08:42:27.842Z (wall 57.329s) and module-identity-provider finished 08:42:29.069Z (wall 58.547s). So the check fell in the tail of the compile phase, before the 13.156s link. There was no stall, retry, or cache fault.

ASSESSMENT
- This is not a hung or no-op build. A single 42.6s translation unit plus a 13.2s monolithic link makes the 60s wall budget marginal for any real vehicle change, so the wait trip is expected given the current budget. The overage is 0.405s, i.e. policy noise over a genuinely long build, not a failure.
- Direct resource observation during the attempt: postmortem mean host CPU 93.39%, peak 100%, minimum available memory 976 MB; a live game pair was active (run 20261128_082433, server actual mode vehicle_freeplay, client ~101.8 FPS, 81/102 frames over a 4.17ms budget). Inference only: host pressure plausibly lengthened the compile, but matched prebuild baseline, host commit charge, and independent client-mode confirmation are unavailable, so this review does not claim host pressure caused the overage. Gap key build-resource-sampling:game-dll-20261128T084130319731Z-1d504b is reported once on the host-resource capability issue.
- Ownership of any fix is outside this issue and already tracked: issue-136d4553ff92 (build latency) and issue-6e08d89b55e3 (unreadable wait reason). No source edited, no build requested. Confidence: high (pinned attempt JSONL/JSON and wake record).
