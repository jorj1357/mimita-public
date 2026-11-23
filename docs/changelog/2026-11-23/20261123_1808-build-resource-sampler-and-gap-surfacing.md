# Automatic build resource sampling and evidence-gap surfacing

**Date:** 2026-11-23 18:45 UTC  
**Agent:** `hot-reload-latency-root`  
**Issues:** `issue-136d4553ff92`, `issue-ed3ac775283d`, `issue-91e3384b219b`

## Evidence that prompted the change

Root build `game-dll-20261123T182740034015Z-98b8bc` published generation 569 in 644.432 seconds: 185 translation units compiled and 458 skipped. Its resource sidecar existed but reported `status=unavailable`, zero samples, and `ModuleNotFoundError: No module named 'psutil'`. Therefore this attempt cannot establish how host load affected compilation.

The shared brain now surfaces that attempt as stable gap `build-resource-sampling:game-dll-20261123T182740034015Z-98b8bc`. A separate retained sample during an earlier compile showed host CPU at 99.88% and available RAM at a mean 0.435 GiB, while four known MiMITA processes accounted for 51.70% of host capacity. The compiler child process and competing-process attribution were missing in that sample; this is correlation, not proof that load caused the long build.

## Changes

- Added a no-dependency Windows `kernel32` sampler for host CPU, physical memory, pagefile use, and logical CPU count. It primes the CPU counter at start so short builds can still capture a final interval.
- Kept optional `psutil` collection for per-process attribution and top competing processes. When unavailable, host metrics still record and per-process metrics are explicitly unavailable.
- Wired a per-attempt sampler around both game-DLL and cold EXE builds. Samples include build phase/progress, actual sample interval, runtime metadata as reported by dev-loop, and sampler setup/thread overhead. Telemetry remains best effort and cannot fail a build.
- Added bounded, stable `brain_check.build_resource_gaps` entries for missing/partial build resource sidecars. Updated agent wake guidance to compare issue acceptance with evidence, append each stable gap once, coordinate with the relevant owner, and implement safe evidence collection when it matters to the goal.

## Validation and remaining proof

`python -m unittest devscripts.quality.test_build_resource_sampler devscripts.quality.test_dev_loop_exe_noop_identity` passed (4 tests). `py_compile` passed for the sampler, both build orchestrators, coordination, and wake scripts. Targeted `git diff --check` passed; Git emitted only its line-ending normalization notices.

The current root daemon has not loaded these Python changes. No daemon restart, build request, compiler invocation, or game-process restart was performed. The next safe proof is a root-dev-loop build after its owner-controlled reload, confirming a nonempty `.resource.jsonl` with regular host CPU/RAM samples, captured stage/progress fields, and an honest `provider`/unavailable-field record. Per-process attribution requires a supported `psutil` provider and remains open otherwise. The sampler does not yet produce a matched prebuild baseline or journal-aligned FPS/tick data.
