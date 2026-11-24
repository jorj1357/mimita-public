# issue-302ab3824727: Wait over budget: exe-build at linking

## Human report (verbatim, immutable)

> exe-build wait at linking exceeded the wait budget


## Context

### 2026-11-24T06:53:40.648Z — build-wait-reviewer

paths: ['devscripts/dev-loop.py', 'devscripts/build_friction.py', 'devscripts/agent-wake.py', 'build.py', 'build_game_dll.py']
related_build: 2026-11-24T06:22:17.620103Z:a48546ad82b057b8bcc982c992a2cc8350f54e18e5a266d1858504280774e59a
related_run: 20261124_053435
evidence: {'budget_seconds': 60.0, 'build_id': '2026-11-24T03:30:52.372066Z', 'elapsed_seconds': 60.546, 'generation': 26, 'kind': 'exe-build', 'reason': 'linking mimita.exe (object_set_changed)', 'request_ids': ['2026-11-24T06:21:07.582Z-fce07e1f:mode3:cold'], 'source_generation': 26, 'stage': 'linking', 'started_utc': '2026-11-24T06:21:08.234Z', 'wait_id': 'exe-build:26:2026-11-24T06:21:08.234Z', 'review': {'attempt_id': 'exe-build-20261124T012108-f9d70640', 'published_build_id': '2026-11-24T06:22:17.620103Z:a48546ad82b057b8bcc982c992a2cc8350f54e18e5a266d1858504280774e59a', 'dev_loop_run_id': '20261124_053435', 'attempt_wall_seconds': 64.854, 'phase_seconds': {'compile_s': 34.999, 'link_s': 3.24, 'presentation_dll_s': 21.57, 'staging_s': 0.018, 'scheduler_wait_s': 0.004}, 'presentation_dll': {'outcome': 'failed', 'exit_code': 1, 'elapsed_s': 21.57}, 'compiled': 16, 'skipped': 71, 'jobs': 12, 'host_mean_cpu_percent': 95.528, 'host_peak_cpu_percent': 100.0, 'minimum_available_memory_bytes': 535056384, 'resource_samples': 63, 'runtime_alignment': 'unavailable', 'wake': {'request_id': 'wake-e8b7a1914af1', 'status': 'launched', 'attempts': 2, 'duplicate_requests_found': 0}, 'journal_gap': 'logs/2026-11-24/20261124_053435/dev-loop/events.jsonl does not exist; dev-loop.wait.over_budget and reviewer-wake events are not retrievable'}}

## Review (valid; phase-at-detection mislabels cost (not link-bound))

### 2026-11-24T06:54:46.817Z — build-wait-reviewer

Reviewer: build-wait-reviewer

## Verdict

Valid over-budget wait, but the "at linking" label is the phase **at detection**, not the cost driver. The 60.546s overrun came from the whole EXE attempt: 34.999s compile + 21.570s failed optional presentation-DLL sub-build; the actual link was 3.240s.

## Direct observations (evidence)

- Issue record: wait_id `exe-build:26:2026-11-24T06:21:08.234Z`; started 06:21:08.234Z; elapsed 60.546s; stage `linking`; reason `linking mimita.exe (object_set_changed)`; request `2026-11-24T06:21:07.582Z-fce07e1f:mode3:cold`.
- Waited-on attempt: `exe-build-20261124T012108-f9d70640`; published build `2026-11-24T06:22:17.620103Z:a48546ad82b057b8bcc982c992a2cc8350f54e18e5a266d1858504280774e59a`; run `20261124_053435`.
- `build/build-timing.json` and `build/build-metrics.jsonl` (timestamp 2026-11-24T06:22:13Z): compiled 16, skipped 71, jobs 12, total 64.854s; phase_seconds compile_s 34.999, link_s 3.240, presentation_dll_s 21.570, staging_s 0.018, scheduler_wait_s 0.004; presentation_dll outcome `failed`, exit 1.
- `.dev/build-timings/exe-build-20261124T012108-f9d70640.resource.json`: 63 samples, mean host CPU 95.528%, peak 100.0%, minimum available memory 535,056,384 bytes; runtime_alignment `unavailable`.
- Wake: exactly one request exists for this issue (`wake-e8b7a1914af1`, created 06:22:09.312Z, status launched). `attempts: 2` is two launch attempts of the same request, not a duplicate queue entry. No second request for this issue or wait_id.
- Code path: budget check `devscripts/dev-loop.py:3839-3930` uses elapsed-since-build-start with `stage = active_build_phase`; phase detail mapping at `devscripts/dev-loop.py:4080-4124`; `build.py:1290-1291` emits `linking` only immediately before the link; optional presentation-DLL sub-build at `build.py:652-812`.
- Journal gap: `logs/2026-11-24/20261124_053435/dev-loop/events.jsonl` does not exist (Test-Path false). The expected `dev-loop.wait.over_budget` and `dev-loop.wait.reviewer-wake` events are therefore not retrievable; only the newer run `20261124_063130` journal exists.

## Acceptance check

- wait_id, stage, reason, elapsed, evidence: present.
- build/run IDs: originally `evidence.build_id` was the **running** EXE `2026-11-24T03:30:52.372066Z` (generation 20 artifact) and `related_build`/`related_run` were empty. This review now pins the waited-on attempt, published build, and run above.
- Reviewer wake exactly once: pass.
- Proof expectation: partial. Issue record, wake request, and build timings exist, but the dev-loop wait journal was not preserved.
- Duplicate issue: none for this wait_id.

## Cause assessment

- Total attempt 64.854s (69.4s to publish) exceeds the 60s budget.
- The 03:30:44Z attempt compiled only 4 files yet still took 60.996s (compile 34.167 + presentation_dll 21.579 failed + link 3.578). This puts the default 60s budget below the observed non-noop EXE floor.
- In the last 8 `build/build-metrics.jsonl` attempts, the optional presentation-DLL sub-build failed in 6; elapsed 10.5-32.6s, and the 05:49:50Z noop took 344.634s with 335.702s in that failing sub-build. This is recurring avoidable cost in the same build path.
- Host CPU was 95.5% mean and memory had 535MB free (direct samples), but runtime alignment was unavailable and no live-mode workload was matched. "Contention caused the 35s compile wall" is an inference, not proven.

## Follow-ups (not implemented here; no source edited)

1. Report phase-at-detection separately from phase elapsed so "elapsed 60.5s at linking" does not imply link cost.
2. Derive `WAIT_ISSUE_SECONDS` from the measured non-noop EXE floor (or start the clock per phase); otherwise nearly every ~60s+ EXE build files an issue. Related open question: `docs/specs/dev-loop/agent-build-queue.md:314`.
3. Investigate why the failed optional presentation DLL repeatedly costs 10-33s (and 335s once) instead of a cached skip (failure-cache fingerprint invalidation). Candidate owner: build-coordinator / issue-136d4553ff92.
4. Preserve or rotate the dev-loop journal across daemon restarts so wait events remain retrievable.

## Confidence and smallest next check

High confidence on measured phase timings and the single-wake count; medium on the budget-policy recommendation and contention attribution. Smallest next check: one controlled EXE build with no other builds/runtimes, comparing compile_s, presentation_dll outcome, and host CPU; if the optional DLL is `cached_failure_skip`, the attempt should be ~35-40s.

## Agent discussion and suggestions

### 2026-11-24T06:56:14.796Z — build-wait-reviewer

Message: `msg-7be5139e0e80` (question)

Subject: Review exe-build wait evidence; agree on smallest next check

I reviewed exe-build wait issue-302ab3824727 (60.546s at stage linking, attempt exe-build-20261124T012108-f9d70640, run 20261124_053435). Measured: total 64.854s, compile 34.999s, link 3.240s, failed optional presentation DLL 21.570s; host mean CPU 95.5% (63 samples). The 'linking' stage is phase-at-detection, not link cost. I also cross-linked the exe-build numbers to issue-136d4553ff92. Please review the evidence and agree on the smallest next check: controlled EXE build vs wait-budget policy change. No code fix claimed; no source edited.
