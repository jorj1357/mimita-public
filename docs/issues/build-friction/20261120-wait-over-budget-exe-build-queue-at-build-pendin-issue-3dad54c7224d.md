# issue-3dad54c7224d: Wait over budget: exe-build-queue at build_pending

## Human report (verbatim, immutable)

> exe-build-queue wait at build_pending exceeded the wait budget


## Review (confirmed)

### 2026-11-20T17:52:15.050Z — build-wait-reviewer

Reviewer: build-wait-reviewer

Reviewed issue-3dad54c7224d against the dev-loop wait record and wake journal.

Evidence
- Wait: exe-build-queue:95529.460 at stage build_pending, elapsed 60.935s vs budget 60.0s. source_generation=23, build_id=2026-11-20T07:47:09.981634Z; request_ids 2026-11-20T17:01:09.699Z-5c9e4334, 2026-11-20T17:01:13.433Z-8b36b669, 2026-11-20T17:01:26.240Z-a328b20c. All three later show status success in dev-loop --status --json.
- Issue completeness: `agent-coord issue show issue-3dad54c7224d` reports readiness.ready=true, missing=[], with reproduction, expected_behavior, acceptance, paths, risk, evidence, and proof_expectation populated. No fields needed completion.
- Deduplication: querying brain.db for evidence containing wait_id 95529 returns exactly 1 issue (issue-3dad54c7224d). No duplicate occurrence was created.
- Reviewer wake: `.dev/agent-wake/events.jsonl` has exactly 1 `wake.queued` for wake-8627cef525a5 / issue-3dad54c7224d at 2026-11-20T17:02:22.528Z (build-wait-reviewer). Later claims/retries are attempts on the same wake id, not new wakes.

Gap (evidence, not a fix request)
- The dev-loop journal path in status/state (`logs/2026-11-20/20261120_170259/dev-loop/events.jsonl`) does not exist on disk; run directory 20261120_170259 contains only client-01, server-01, and run.json. The wait event was generated at 17:02:20Z before run 20261120_170259 was created (run.json created_utc 17:02:59Z), so the on-disk journal for the wait event cannot be located from the current status path. Related retained-server identity paths also point into the new empty run dir while the live server journal remains under runtime_run_id 20261120_161114. Recorded here as evidence for the existing run/journal identity work; no code change claimed by this review.
- The reviewer wake prompt on this activation reached the opencode adapter as only its first line; the full prompt (including issue id and requested work) is present in `.dev/agent-wake/requests/wake-8627cef525a5.json`. This is consistent with issue-62ab3daa615d.

Verdict: wait record and single reviewer wake are correct; issue was ready and did not need field completion. No new duplicate issue created.
