# issue-8fdc520a07c5: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Review (confirmed)

### 2026-11-23T17:33:04.803Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Build-wait review — issue-8fdc520a07c5 (game-dll-build at game_build). Agent: build-wait-reviewer. Evidence gathered 2026-11-23T17:32Z. No source edited.

WHAT HAPPENED (direct observation)
- `dev-loop.wait.over_budget` wait_id `game-dll-build:2026-11-23T17:10:57.269Z`, stage `game_build`, reason `[PACKET CODEGEN] ok packets=78`, elapsed 60.153s vs 60.0s budget, `created=true`, issue issue-8fdc520a07c5, fired 17:12:03.196Z. Journal `logs/2026-11-23/20261123_144429/dev-loop/events.jsonl`.
- The waited-on build did NOT hang: attempt `game-dll-20261123T171100081510Z-c4f29b` reached `dev-loop.game-build.result status=ok` at 17:15:02 and `dev-loop.game-generation.ready generation=566 success=true duration_seconds=243.34` at 17:15:03. Summary `.dev/build-timings/game-dll-20261123T171100081510Z-c4f29b.json`: total 243.303s, worker 173.258s, compiled 2 / skipped 641, compile 135.906s (server-gamemode.cpp 98.422s + module-identity-provider.cpp 37.364s), link 32.315s over 484.7MB of objects, object-cache scan 2.494s.
- 60s is far below this normal incremental rebuild: changing one TU (server-gamemode.cpp) plus the regenerated identity provider already costs ~136s compile + 32s link. Same compile/link/header-fanout surface as issue-136d4553ff92, not a stalled worker.
- Reviewer wake was queued exactly once for this occurrence: `wake.queued wake-2114901f17f9` issue issue-8fdc520a07c5 at 17:12:02.166Z, and a single `dev-loop.wait.reviewer-wake` (17:12:03.135Z) carries this wait_id. Acceptance "queued exactly once" holds.

DEFECTS IN THE WAIT RECORD (follow-up needed; not fixed here)
1. Stale build identity: evidence records build_id 2026-11-23T13:26:39.822193Z and generation 565 while the active attempt is `game-dll-...c4f29b` generation 566. Root: dev-loop.py:3878-3879 sets build_id=self.latest_build and generation=self.game_build_generation (last published). The true attempt id is absent from the structured evidence.
2. `reason` is a pre-build codegen progress line, not the build stage/TU: `[PACKET CODEGEN] ok packets=78` came from packet_registry_codegen completion at 17:11:57, before game_build.started 17:12:00, and was still the copied game_build_detail (dev-loop.py:3874) when the wait fired ~1 min later. It does not identify the attempt, compiled/total, or current TU.
3. Reviewer wake dispatched late and launched twice for one queue: codex claimed+launched at 17:12:02.77/17:12:02.837Z, `wake.retry` "stale launched request recovered by scheduler" at 17:14:47.715Z, then opencode claimed+launched at 17:24:09.931/17:24:09.991Z (~12 min after queue, and this session). One queued wake but two adapter launches. Tracked canonically in issue-84c584957e65; not duplicated here.

RECURRENCE
Dedup keys on wait_id only (build_friction.py:131-169); every attempt has a unique started_utc, so each >60s rebuild creates a NEW high-priority issue + reviewer wake instead of appending an occurrence. The shared list (limit 100) already holds >=79 `Wait over budget: game-dll-build at game_build` issues (70 in_review, 8 new, 1 assigned), including today's issue-8a2e0ac7236b (15:57), issue-991c632f1ca0 (16:28), issue-3d02ab0de320 (16:54) and this 17:10 occurrence. With a 60s budget against a 243s single-TU rebuild the noise is guaranteed, not incidental.

RECOMMENDATION (not applied)
- Dedup/aggregate recurring game-dll-build waits into ONE issue by (kind, stage) within a window and append an occurrence, instead of one issue+wake per build. The occurrence path already exists (build_friction.py:164-169) but only triggers for an identical wait_id.
- Or raise WAIT_ISSUE_SECONDS above the observed normal build time for `game-dll-build` (243s here) while keeping it tight for genuinely stuck waits.
- Record the active attempt id/generation on the wait (dev-loop.py:3878-3879), not the last published generation.
- Build a curated reason (stage + attempt id + compiled/total + current TU) instead of copying the raw progress tail (dev-loop.py:3874).
Cross-link: issue-136d4553ff92 (build stage accounting/speed, owner build-coordinator), issue-e044a931c54b `wait-instrument` (owner build-runtime-subagents), issue-84c584957e65 (wake adapter late/double dispatch), sibling issue-3d02ab0de320.

CONFIDENCE
High on build outcome and stale IDs/reason (direct journal + summary JSON). High on wake-queued-once. Medium on wake double-launch mechanism (event sequence direct; owner of issue-84c584957e65).

No proof claim: this is a tooling/observability review; the wait evidence is journal-tier, not behavior proof. No source edited, no new code issue opened because the defects already have canonical owners above.
