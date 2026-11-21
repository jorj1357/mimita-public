# One-source compile, cache, and link trial

## Result

A behavior-neutral comment was added to the clean, 158-byte `src/effects/zombie-tower-effects.cpp` to force a real one-source DLL build. The root dev-loop built and published generation 433 successfully:

- Request: `2026-11-21T21:50:15.046Z-84e61847`
- Attempt: `game-dll-20261121T215015817067Z-d501fb`
- Source revision: `d2ca2333f75538a8f795e19cdc4f493e4ba76323db39d1aab1114fd8f89a5e83`
- Result: exit 0, `no_op=false`, generation 433, `build/game/generation-433/game-g433.dll`
- Outer attempt wall time: 395.540 s (from `.dev/build-timings/game-dll-20261121T215015817067Z-d501fb.jsonl`, `build.completed`)

The source change had no runtime behavior effect. No EXE build or explicit launch/start-client request was made. The existing dev-loop server was left running.

## Baseline versus the trial

| Stage | G432 edit attempt | G433 one-source trial | Comparison |
|---|---:|---:|---|
| Attempt total (outer clock) | 172.333 s | 395.540 s | Trial 2.30× slower; different source edit and severe resource contention during the trial |
| Pre-worker interval | 26.383 s | 213.176 s | Trial 8.08× slower |
| Changed-source preflight | 14.230 s | 36.386 s | One tiny source compile in the trial took 36.304 s |
| Object-cache scan | 98.861 s | 4.903 s | 20.2× faster than G432; 630 objects reused and 11 wrappers invalidated |
| Remaining compile batch | 18.425 s | 40.592 s | 11 wrappers compiled; trial roughly 2.20× slower |
| Total compiler wall (preflight + batch) | 32.655 s | 76.978 s | Trial 2.36× slower under a different load state |
| GNU linker | 11.022 s | 61.718 s | Trial 5.60× slower; link succeeded |
| Publish phase | 1.674 s | 29.101 s | Mostly a repeated dependency-manifest scan |

The scan optimization persisted in a real compile/link attempt. It completed in 4.903 s versus 98.861 s on G432. Its dependency prepass took 3.604 s; stat prewarm took 0.391 s; object freshness checks took 0.781 s. The changed-source preflight compiled the edited source. The later scan reported 641 checked entries, 630 skipped, and 11 stale wrappers; the already-preflighted source is not counted in that 641.

All 12 objects compiled in this build were `src/effects/zombie-tower-effects.cpp` plus the 11 generation-identity wrappers:

```text
src/map/map-geometry-module.cpp
src/mapeditor/map-editor-module.cpp
src/navigation/navigation-policy-module.cpp
src/network/network-policy-module.cpp
src/npc/npc-behavior-module.cpp
src/npc/npc-perception-module.cpp
src/physics/movement/actor-contact-policy-module.cpp
src/render/actor-rendering-module.cpp
src/render/world-rendering-module.cpp
src/world/chunk-collision-module.cpp
src/worldgen/infinite-worldgen-module.cpp
```

Each wrapper compiled in about 10.5–34.6 s (10 of 11 were about 30–34.6 s). The changed 158-byte source compiled in 36.304 s. This is direct evidence that source byte/line count alone does not predict compile wall time. G432’s wrapper compilation was around 12.4–12.8 s each, and its edited `player-ai.cpp` took 14.177 s; those G432 times are not a like-for-like source comparison.

## Resource evidence and limits

During G433’s manifest/source-snapshot stage, a five-second host sample observed:

- Average CPU use: 98.4% across 12 logical CPUs; each core was about 96.6–100% busy.
- Memory: 14.38 GB used of 15.65 GB; 1.27 GB available.
- System-wide I/O during the sample: 283.4 MB read and 35.31 MB written.
- The dev-loop Python process used 37.8% CPU and the existing game server 8.7%; the sample did not attribute the remaining CPU to individual processes.

This sample was taken during pre-worker snapshot work, not during the compiler or linker interval. It confirms that the machine was saturated at one point in this build sequence, but does not prove which process caused the 36-second compile or 62-second link. The next compile/link comparison needs aligned process-tree CPU/RSS and disk sampling during those exact phases.

## Newly exposed costs and timer defects

1. **Publish reparses the dependency files.** `publish.external_dependency_manifest` took 27.793 s to parse 642 depfiles. The object scan already collected the external dependency set. Reusing the set from the scan should avoid repeating the work; this was identified but not changed in this trial.
2. **Displayed worker totals stop before publish work ends.** `publish_seconds` was 1.152 s while the full publish phase was 29.101 s. `total_seconds` was 359.611 s while outer attempt wall time was 395.540 s. The difference is about 35.9 s, mostly the dependency-manifest work and finalization after the worker calculates its subfields. Use the dev-loop outer clock as the authoritative total until this timer boundary is corrected.
3. **Manual and automatic requests did not coalesce.** After the explicit request completed, the normal watcher ran `auto-1`, source generation 1, against the same revision. It completed successfully as a no-op on generation 433 but took 377.503 s end-to-end. That run repeated codegen (8.390 s and 102.887 s), manifest snapshot (148.233 s), and source snapshot (66.961 s) before discovering the current generation already matched. The likely cause is that the manual request started before the watcher had attributed the saved edit to source generation 1; exact ownership of the auto request is recorded, and revision-based coalescing needs investigation.

## What the measurement says to do next

1. Keep the unique-path memoization: it cut the measured scan from about 99 s to 1–5 s while preserving all 642 freshness decisions.
2. Reuse `scan["external_dependencies"]` during publish, then verify the manifest still reflects every depfile for the objects actually linked. This is the smallest next optimization and targets the measured 27.793-second repeat pass.
3. Fix worker total/publish timer boundaries so phase totals reconcile to the dev-loop attempt within an explicit small residual.
4. Coalesce manual and watcher requests by source revision/generation to prevent a six-minute no-op build after an already-completed build.
5. Profile manifest, snapshot, compiler, and linker stages with aligned process and system metrics. The measured snapshot times varied dramatically and a host sample showed resource saturation; repeat the same controlled one-source trial when there is CPU/memory headroom before drawing conclusions about the linker’s intrinsic floor.
6. After resource attribution and the publish/duplicate-request fixes, repeat a real source edit and compare the 36.3-second compile and 61.7-second link against a comparable low-load baseline. This trial does not establish sub-second compile or link.

## Runtime and repository state

The successful attempt published generation 433. A subsequent automatic no-op attempt completed with generation 433 still current. The source manifest and DLL revision matched after completion. No additional process launch was requested by this agent. The only C++ edit in this trial is the comment in `src/effects/zombie-tower-effects.cpp`; the report/changelog are linked from the shared build-latency investigation issue.
