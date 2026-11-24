# game-dll-build wait review (issue-6415397775a2)

Reviewed wait `game-dll-build:2026-11-21T02:29:01.212Z` (60.44 s at the 60 s budget check, stage `game_build`) against attempt `game-dll-20261121T022901426487Z-d15759`, generation 417.

Acceptance split: the record half holds — the issue carries wait_id, stage, reason, elapsed, build_id `2026-11-21T02:26:50.306117Z`, request ids, and evidence; exactly one reviewer wake (`wake-fd61691ffd48`) was queued (2026-11-21T02:30:08.058Z). The behavior half failed — the wake was claimed only at 13:50:41.470Z, retried to opencode 15:08:17.043Z, launched 15:20:50.464Z, and ended `wake.failed` "adapter process exited" at 15:53:26.109Z (queue-to-failed 48198.1 s ~= 13h23m18s); no review was appended and the issue stayed `new` for 309562.9 s until this scheduled sweep (wake-3efa68064700). The 03:26:27Z build-wait-reviewer suggestion predates this wake's first claim, so it is not attributable to wake-fd61691ffd48 from surviving artifacts.

This occurrence was a real failure, not a healthy long build: `.dev/build-timings/game-dll-20261121T022901426487Z-d15759.json` records `failed_phase=changed_source_preflight`, `worker_error="changed-source preflight failed: src/network/multiplayer-projectiles.cpp in 1240.728s"`, total 1914.474 s. Contrary scale evidence: 160 issues now share the title `Wait over budget: game-dll-build at game_build` (145 in_review, 14 new, 1 assigned), and the same 60 s check still fired at 2026-11-24T12:22Z (issue-3472692a57c2, 60.733 s) while normal changed-source builds measured gen594 total 99.428 s (worker 86.147 s) and gen593 181.029 s.

Next discriminating check: join the open game-dll-build wait issues to their pinned build-timings attempt, classify healthy-success vs failed/stalled, and compute the share of successful attempts above 60 s; separately check whether any over-budget reviewer wake emits a `wake.completed`/review rather than only `wake.failed`.

Process review: the late/failed wake class is already covered by measurable proposals on issue-84c584957e65 (2026-11-23T07:25, 15:55; 2026-11-24T14:01) and the flat-budget/per-attempt duplication by issue-9a4503f6ed68 (2026-11-22T23:07; 2026-11-23T13:28). No new proposal filed to avoid a duplicate report; this occurrence's exact timings were appended to issue-84c584957e65 as a distinct occurrence.

No source edit, build, runtime change, process control, or runtime/behavior proof. Issue-6415397775a2 moved to `in_review`; no file leases held.
