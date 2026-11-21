# issue-054ea1bcda9b: Wait over budget: exe-build at starting

## Human report (verbatim, immutable)

> exe-build wait at starting exceeded the wait budget


## Review (confirmed)

### 2026-11-21T14:04:17.172Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Bounded wake review (wake-1c2023057acd, agent build-wait-reviewer). Read-only; no source edits, no file claims, no build started, no fix claimed.

Occurrence is real and single. Wait id exe-build:0:2026-11-21T01:38:16.191Z, kind exe-build, stage starting, reason "starting compiler", elapsed 60.369s vs 60s budget, generation 0, source_generation 0, build_id 2026-11-20T20:20:51.788061Z.

Correlated live context: agent root was active on issue-80dfb9ab9c5c in run 20261121_013815 (game_generation 416) and recorded wait_reason "dev-loop active EXE build setup has exceeded budget; friction tracked in issue-054ea1bcda9b." at 2026-11-21T01:55:43.573Z (coord.activity.set, pid 17924). This ties the friction record to the session that was actually waiting.

Reviewer wake: queued exactly once (wake-1c2023057acd, manual, created 2026-11-21T01:40:03.777Z; one wake.queued event). Delivery was very late: codex claimed 03:11:34.552Z and launched 03:11:35.717Z but exited; fallback retried to opencode at 03:48:06.180Z; opencode claimed/launched 13:50:40Z. Net ~2h10m to first claim and ~12h10m to delivered review, so the "wake once" acceptance is met but the wait went unreviewed for ~12h. This latency belongs to the wake-delivery path (see issue-62ab3daa615d for truncation and the same late-delivery pattern), not to this wait record.

Duplicate check: only this issue carries wait_id exe-build:0:2026-11-21T01:38:16.191Z. Sibling exe-build waits are distinct occurrences/stages (issue-693e63059a7c compiling, issue-723326f11472 setup, issue-7d013b4a6c77 setup). No duplicate issue was created.

Fields complete: readiness ready=true, possible=8, missing=[]; reproduction, expected behavior, acceptance, paths, and proof expectation are all present. Nothing to add.

Root cause/ownership: stage "starting" is EXE-build setup latency, a slice of the broader avoidable-wait program. Cross-link issue-7d3dcfd7433b (Measure every dev-loop wait stage and reduce avoidable latency, owner launch-latency-investigation) and issue-e044a931c54b (Waiting is the enemy). No source fix is claimed in this bounded activation; a fix owner should take the EXE-build "starting" phase measurement separately.

Caveats for the wait-metering owner: (1) the evidence build_id is self.latest_build (last completed build), so it lags the build that was actually waiting; the waiting build is identified by started_utc + generation, not build_id. (2) run 20261121_013815 has no dev-loop/events.jsonl at review time, so this review relies on the issue evidence, the coord activity record, and the agent-wake events rather than the original dev-loop journal; exact per-stage timing needs journal retention if desired.

## Agent suggestion

### 2026-11-21T14:44:53.852Z — root

Measured outcome for the delayed EXE build (2026-11-21): success, compiled 17 / skipped 70, elapsed 2746.712s (~45m47s) in build/build-timing.json. Named phases: compile 1111.074s (~18m31s), link 118.849s (~1m59s), scheduler wait 0.005s, staging 0.267s, PCH 0s. Thus the visible ~18 min aligns with compile phase, but only ~20m10s is attributed; roughly 25m37s remains unaccounted in reported phases. The earlier wait record was stage=starting/reason=starting compiler; exact setup/preparation delay is not measured, so root cause remains unknown. Add phase timing/progress for setup, dependency discovery, queue/permit, compiler startup, each compile batch, and publication, with heartbeat and stale-progress detection. Build succeeded; this timing evidence does not identify a single culprit.
