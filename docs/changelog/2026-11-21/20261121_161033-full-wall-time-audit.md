# Full DLL attempt wall-clock audit

Date: 2026-11-21 16:10 EST  
Issue: `issue-136d4553ff92`

## Findings

- Reconciled G432's root attempt from `build.started` to `build.completed` at 172.333s and to same-process G432 activation at 173.976s.
- Broke out the previously summarized pre-worker work: resident codegen 0.392s, packet registry codegen 3.642s, manifest snapshot 11.581s, source snapshot 6.901s, plus 3.856s between recorded pre-worker stages.
- Found 4.988s not represented by the sum of named non-overlapping primary phases. About 3.856s is known pre-worker gaps; approximately 1.137s follows publish-phase completion before root build completion, including a 119ms result-output handler and about one second not yet attributed.
- Found custom pre-worker stage timers exclude event/reporting overhead: source snapshot event offsets span 7.078s versus 6.901s recorded duration; manifest snapshot spans 11.702s versus 11.581s. The outer attempt includes those intervals.
- Found worker `publish_seconds`/`total_seconds` are captured immediately after rename/hash, before metadata writes, external dependency manifest collection, and cache-marker writing. G432's worker fields therefore undercount complete publish by about 1.177s and the outer attempt by about 2.293s.
- Recorded a 1.102s repeated-work candidate: publication reparses 642 depfiles even though the scan already enumerated external dependencies. Reuse must refresh any depfiles rewritten by compiled translation units.
- Reaffirmed that source-save-to-first-effect and aligned PC/worker resource usage remain unmeasured for G432.

## Changes

- Expanded `docs/reports/20261121_152900-dll-generation-latency-and-activation.md` with a full attempt wall-clock reconciliation, timer boundary defect, repeated depfile work, and the remaining evidence needed for true save-to-effect timing.
- No build system, compiler, game source, or runtime process was changed or rebuilt in this audit.

## Validation

- Read-only reconciliation used `.dev/build-timings/game-dll-20261121T201417234689Z-2d9df4.jsonl`, its summary, `build/game/progress.json`, and the G432 server activation journal.
- Existing root dev-loop status responded; its server process remained PID 7556. No additional build or game instance was launched.
