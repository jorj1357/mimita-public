# Dust2cyberiav4 slope and block collision trace

Date: 2026-10-24

## Scope

Investigated ground walking that catches at block seams and the transition from a slope to a block. Enabled bounded per-contact collision diagnostics, ran the baseline and retest through the dev-loop in mode 1 Sandbox on `dust2cyberiav4`, and compared map triangle IDs, face and response normals, remaining movement, and velocity before and after response. Temporary logger/map launch settings were restored after the run.

## Source change

`src/physics/movement/actor-triangle-solver.cpp` now classifies map support using the oriented map face normal, preventing rounded edge response normals from turning vertical map walls into false floors. Actual walkable slope normals are retained for response; only numerically flat floors canonicalize to world-up. `docs/architecture/collision/collision.md` documents that rule. `docs/regressions/2026-09-29/slope-edge-snag-REG.md` records the live retest and remaining blocker.

## Evidence

- Baseline build 3094 journal: `logs/2026-10-24/20261024_200800/events.jsonl`.
- Retest build 3099 journal: `logs/2026-10-24/20261024_202809/events.jsonl`.
- Four cardinal scenarios each applied 600 ticks. Focused uphill scenario applied 600 movement ticks after settling.
- The baseline had 80 vertical-face-as-walkable/flattened records in the rightward segment; the retest had zero such records in all four segments.
- Slope triangle 1729 retained its face/response normal `(0, 0.2425, 0.9701)`. Uphill movement advanced about 20.6 m and climbed about 5.38 m before stopping at vertical map face triangle 2440, normal `(0, 1, 0)`, opposing uphill movement. Trace cannot say whether this is intended geometry or an authored seam.
- The broad cardinal routes encounter real terrain and walls and did not meet the requested 99.9% straight-line criterion. No overall seam-fix acceptance or human visual/feel acceptance is claimed.
- Retest journal size was 78,485,523 bytes with unrelated performance logging temporarily disabled. Baseline journal exceeded 100 MB. Temporary settings were restored.
- Dev-loop published build 3099 at `.dev/builds/3099/mimita.exe`, SHA256 `12391EE26E38083CFB7CE1A2F7565A8ADD3EB2B3571CB73BDE937908ACBF9FA0`; launched runtime matched this build. No claim is made for later, unlaunched builds.

## Required follow-up

Inspect the authored/rendered map around triangle 2440 and identify a nearby straight path that crosses the suspected slope/block seam without an intended wall. Repeat the fixed 600-tick path with per-contact traces and compare before/after movement and velocity. Keep the remaining-movement and wall response intact until the map face's intent is established.
