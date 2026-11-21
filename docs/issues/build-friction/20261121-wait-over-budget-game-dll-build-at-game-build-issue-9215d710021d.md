# issue-9215d710021d: Wait over budget: game-dll-build at game_build

## Human report (verbatim, immutable)

> game-dll-build wait at game_build exceeded the wait budget


## Agent suggestion

### 2026-11-21T16:10:30.903Z — visual-access-and-issue-review

Occurrence resolution (2026-11-21, ctx-f9ce8d89a650 "why build stuck at 18 min?"): the wait was real but the build was not hung. Attempt game-dll-20261121T150703129448Z-23d2a4 progressed through every phase; ~9.2 min was pre-compile accounting (packet_registry_codegen 56.3 s, manifest_snapshot 77.4 s, source_snapshot 110.2 s, changed_source_preflight 128.0 s, object_cache_scan 180.8 s) and compile then failed at 12.5 min with 4 cc1plus out-of-memory errors at jobs=12. It auto-retried as gen 423, which hit object_cache_scan 469.9 s and 280-363 s per TU. Recommendation lives in issue-136d4553ff92 and issue-2a5c5da53565. This over-budget issue should link that evidence rather than duplicate it.
