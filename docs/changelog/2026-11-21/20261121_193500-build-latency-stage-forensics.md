# Build latency stage forensics

- **Issue:** `issue-136d4553ff92`
- **Report:** `docs/reports/20261121_193500-build-latency-stage-forensics.md`
- Compared successful DLL generations 427 (131.141 seconds) and 428 (377.329 seconds), including per-stage timings, translation-unit counts, and the latest saved dependency index.
- Generation 428 compiled 12 of 642 units and skipped 630. Its 144.375-second object scan contained a 116.466-second dependency prepass over approximately 160,547 dependency-token occurrences. The prepass calls path classification inside the per-token loop; that classification performs `Path.resolve()`.
- Identified a 49.344-second packet registry code-generation stage that scans project C++ sources, a 60.150-second `src/actor/player-ai.cpp` compile, and a 26.860-second gap between publish phase wall time (27.766 seconds) and the narrower `publish_seconds` metric (0.906 seconds).
- Captured host specs and live CPU/RAM/process snapshots during the already-running build; CPU reached 98.3–99.1%, RAM use was 88–91%, and multiple `mimita.exe` processes were present. These observations suggest resource contention but do not establish causality.
- Documented exact code locations, the progress percentage behavior, missing stale reasons, missing substage timing, and a measurement-first optimization sequence.
- No source/build configuration/runtime changes, builds, tests, launches, or process terminations were performed by this investigation. Generation 428 was built by an existing root-dev-loop request from another agent; publication is not runtime activation proof.
