# Issue-136d4553ff92 Independent Review

# Task

- Task ID: issue review sweep wake-466b0e0174e4
- Session ID: work-53d751c52f76
- Owner: issue-review-sweeper
- Focus: bounded independent review of issue-136d4553ff92 (account for every DLL build stage and investigate end-to-end speed)
- Summary: Reviewed readiness, proof state, the live dev-loop status, per-attempt build timings/summaries, progress.json, and the run journal while a one-source attempt completed. Appended one evidence review (verdict `evidence-gap`, review id 65) and one process proposal to issue-136d4553ff92. No source, build, run, close, or fix was performed.
- Status: REVIEW_RECORDED_OPEN
- Date, time, timezone: 2026-11-21T22:04:24Z

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents (including src/effects/zombie-tower-effects.cpp, change note 59b04f6eb844). This session edited no source and reverted nothing.
- Files created by this session: this changelog and two temporary body files outside the repository. Append-only issue records were written through `agent-coord.py`.

# Requested behavior

Complete one bounded independent review of issue-136d4553ff92 without editing source, building, closing, or claiming a fix; record observed facts, inference, contrary evidence, and the next discriminating check, plus a measured process proposal when supported.

# Findings

- Attempt game-dll-20261121T215015817067Z-d501fb (only src/effects/zombie-tower-effects.cpp changed) SUCCEEDED as generation 433 in 395.540 s and activated in the live server/client at 2026-11-21T21:56:51.702Z (0.316 s after build.completed).
- The object-cache scan optimization is confirmed on a real compile+link attempt: object_cache_scan 4.836 s vs G432 98.861 s (-95.1%); prepass 3.604 s vs 85.309 s (-95.8%).
- End-to-end latency did NOT improve in this sample: 395.540 s vs G432 172.333 s. Dominant stages moved to manifest_snapshot 137.377 s, link 61.980 s, external_dependency_manifest 27.793 s, preflight 36.386 s, compile wall 40.592 s.
- A 43.600 s interval between source_snapshot completion and changed_source_preflight start is still unmeasured; pre-worker stages alone were 213.449 s (54% of total). Post-publish had ~5.7 s partly unattributed.
- Compile set was 12/642 (630 skipped); all 11 wrapper compiles have the explicit reason "dependency newer than object: build/game/identity/module-identity.h".
- A duplicate full build (attempt game-dll-20261121T215653845698Z-0e52c1, request "auto-1") ran for the same unchanged revision starting 2.46 s after activation; before review end it had spent 111.277 s in codegen and 148.233 s in manifest_snapshot.
- Proof state: the issue has no task-proof rows; task build-scan-path-memo-g432 reports unproven ("journal event predicate not found"). No runtime proof was recorded for the behavior of the comment-only edit, only DLL activation.

# Specification alignment

- Followed `docs/ROUTER.md` routing for issue intake, proof-and-verification, dev-loop, and agent-coordination paths.
- Kept build, publication, activation, and behavior evidence as separate claims. No behavior claim was made.

# Exact implementation changes

- None. Append-only issue review and issue suggestion only.

# Validation

- Build status: no build requested by this session; no compiler, worker, or second dev-loop was invoked.
- Runtime evidence: reviewed existing run 20261121_215237 server/client journals (read-only). Confirmed game.generation.activated generation 433 in both.
- Review artifact: review id 65 on issue-136d4553ff92 (verdict `evidence-gap`).
- Process proposal: appended to issue-136d4553ff92 (dedupe auto/explicit build requests per published revision, with target and smallest experiment).

# Measured evidence

- d501fb: total 395.540 s; manifest 137.377 s; source_snapshot 26.382 s; handoff gap 43.600 s; preflight 36.386 s; scan 4.903 s; compile 40.592 s (12 TUs, compile_sum 363.714 s); link 61.980 s; publish 29.101 s; post 7.433 s.
- G432 baseline (game-dll-20261121T201417234689Z-2d9df4): total 172.333 s; scan 98.861 s; link 11.022 s; external dependency manifest 1.102 s.
- Runtime: game.generation.activated gen 433 at 21:56:51.702Z in server-01 (PID 34328) and client-01 (PID 21044), run 20261121_215237.
- Duplicate: 0e52c1 codegen 8.390 s + 102.887 s; manifest 148.233 s; started 21:56:53.845Z; unchanged file mtime 21:49:52Z.

# Regression review

- Regression entry appended: no.
- Why: build and activation succeeded and the published revision matched; no behavior regression was observed or claimed.

# Human acceptance

- Visual review: not performed by this session.
- Gameplay review: not performed; the changed file was a behavior-neutral comment, so no user-visible effect was expected or tested.
- Still unverified: total edit-to-visible improvement and the ~140 s manifest stage cause.

# Agent handoff context

- Owner: build-coordinator (stage accounting), live-cpp-compile-link-trial (G433 task), live-build-optimization-trial (scan memoization).
- Next exact step: read attempt 0e52c1's terminal summary and compare its stages with d501fb (same revision), then add substeps for the 43.600 s source_snapshot->preflight handoff and a heartbeat inside manifest_snapshot.
- Do not redo: G432 baseline scan comparison, wrapper-invalidation reason check, and this review.

# How to reverse

- No source or build to reverse. Remove this changelog only through normal review if created in error; preserve the append-only issue review and suggestion.
