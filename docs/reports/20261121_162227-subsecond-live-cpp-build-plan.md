# Subsecond live C++ edit plan

This report turns the G432 measurements into a staged plan for reducing edit-to-game latency. It separates observed costs from targets that still need a controlled experiment. It does not authorize replacing the current MinGW/DLL path.

## Purpose and limits

The target is to make a small `.cpp` behavior edit visible in the already-running game without closing the EXE or rebuilding the whole game. “Under one second” is a goal for a narrow warm edit path, not a demonstrated promise for arbitrary C++, clean builds, header changes, or the complete current DLL pipeline.

The measured attempt started at 20:14:17.235Z and completed at 20:17:09.579Z: 172.333 seconds. G432 was activated at 20:17:11.211Z, or 173.976 seconds after attempt start. The physical save time was not recorded, and first visible NPC behavior was not proven. Therefore this is attempt-to-activation evidence, not save-to-visible evidence.

## Stage evidence and meaning

| Stage | G432 evidence | What it actually does | First target |
|---|---:|---|---:|
| Pre-worker | 26.383 s | Codegen checks, 1,008-record manifest/hash/mirror refresh, hardlink source snapshot, validation and unassigned boundary work | warm leaf edit <1.0 s total |
| Object-cache scan | 98.861 s | Checks 642 source/object candidates, validates cached object freshness against dependencies, reuses 630 and marks 12 stale | warm `.cpp` leaf edit <0.1 s; fallback audit separately budgeted |
| Compile wrappers | 18.425 s wall | Compiles 11 unchanged module wrappers plus edited `player-ai.cpp`; 11 wrappers were invalidated by generated generation/revision identity | zero unrelated wrapper compiles; expected wall saving up to 18.425 s on this attempt |
| Link | 11.022 s phase; linker process 10.942 s | GNU linker combines 642 objects/665 response entries (47,212-byte response file; 480 MB object data) into a 243 MB DLL | warm incremental link <1.0 s is an experiment; full link <1 s is not supported by current evidence |
| Publish | 1.674 s | Moves/copies candidate DLL and writes metadata, dependency manifest and cache marker | under 0.5 s after removing repeated scans, subject to filesystem measurements |

### Pre-worker breakdown

Named stages accounted for resident-state codegen 0.392 s, packet-registry codegen 3.642 s, manifest snapshot 11.581 s, manifest validation 0.011 s, and source snapshot 6.901 s. Roughly 3.856 s remained between named stages, including orchestration and event bookkeeping. Stage event wall spans exceed their duration fields by about 120–177 ms for the two long snapshot stages. The outer attempt clock is authoritative; component timers currently leave boundary gaps.

The manifest refresh hashes/checks the source set and updates a mirror; the source snapshot then makes an immutable attempt input. These serve correctness and reproducibility, but a one-file edit should not require rebuilding a full manifest and snapshot by rescanning/re-hashing all unchanged content. The safe optimization is persistent metadata plus content-addressed reuse and copying/linking only changed files, with a periodic integrity audit and a full snapshot fallback on uncertain watcher state.

### Object-cache scan breakdown

Of 98.861 s, the dependency prepass was 85.309 s. About 84.749 s was iterating dependency path/fanout occurrences, with approximately 160,547 per-TU dependency token occurrences but only about 1,513 distinct dependency paths. Parsing depfiles was 18 ms, loading the persistent dependency index 135 ms, saving it 171 ms, and stat prewarming 235 ms. Dependency freshness checks then took 13.274 s. The object cache is persistent; the expensive repeated traversal of its cached dependency data is the gap. This is not evidence that all dependency checks can be removed: a changed header, compiler flags, PCH, generated input, or missing object must still invalidate its dependents.

The proposed warm path is a persistent reverse index (`dependency path -> dependent object/source`) plus watcher-maintained dirty paths. For a changed `.cpp`, test that one source/object is stale without iterating every dependency token. For a changed header, query only reverse dependents. Keep a bounded verification pass and periodic/full audit to detect missed watcher events and index corruption. Record per-source invalidation reason and candidate count. Initial target: <100 ms warm lookup; correctness gates: leaf `.cpp` yields one object, header changes yield the exact transitive affected set, and forced audit agrees with incremental selection.

### Eleven wrappers

The 11 wrappers are active module entry points, not obsolete NPC-only files. A generated `module-identity.h` embeds generation and source revision, and is included by these wrappers, making each wrapper stale every generation. Keep their standalone module identity checks. Proposed fix for folded whole-game modules is runtime identity supplied by the host (requires an ABI design/change and proof), so stable wrapper objects can be reused. Expected saving on G432 is up to 18.425 s of compile-batch wall time, but actual gain must be measured on a clean non-overlapping attempt. Do not delete wrappers to save compile time.

### Link

The current DLL link is a monolithic GNU link over hundreds of large object files. A link is not simply concatenating files: it resolves symbol references, lays out sections, performs relocations, handles imports/exports, and emits the final DLL. Eleven seconds for approximately 480 MB of object inputs is the observed link cost. Deleting compile work does not automatically delete link work because the whole-game link still consumes all cached objects.

First benchmark a stable-output incremental linker mode if the selected MinGW linker supports a correct persistent incremental state. Compare unchanged link, one-object change, and full link, preserving atomic candidate publication. Then compare MSVC `/INCREMENTAL` with persistent `.ilk` state and a Blink prototype on a compatible MSVC/PDB build. Keep those as isolated experiments; do not switch the active compiler. If the candidate DLL path changes per generation, incremental-link state may not be reusable, so benchmark a stable link slot and its safety/locking costs. A full clean link under one second is unlikely at the observed input scale; a one-object incremental link or function patch is the relevant target.

## Why C++ translation units take time

Source line count is a weak proxy. A translation unit pays for compiler startup, preprocessing/includes, template instantiation, parsing/type checking, optimization, code generation, debug information, and writing the object and depfile. Expensive cases include broad/high-fanout headers, template-heavy libraries, huge inline functions, complex overload/concepts/constexpr evaluation, optimization-heavy code, large debug info, and storage contention. A precompiled header can reduce repeated parsing, but this project’s `src/pch.h.gch` is about 121 MB and PCH validity/configuration must be verified; PCH does not remove semantic work in the edited code or dependency invalidation.

In G432, `player-ai.cpp` (44 KB/1,055 lines, 322 dependencies) took 14.177 s to compile. Ten of the extra wrappers took roughly 12.4–12.8 s each despite some being only 88 lines/2.7 KB; the last took 5.909 s but waited 12.475 s in the queue. This weak relationship between file size and elapsed time points toward shared PCH/header/front-end cost or CPU/resource contention, but there is no per-process resource trace to distinguish them. Wall times overlap; do not sum compiler elapsed times as attempt wall time.

Splitting is worthwhile only when the hot edited portion becomes a small translation unit with fewer expensive includes/templates and the other pieces stay unchanged. Splitting a file can instead add translation units, repeated PCH/front-end work, link inputs, and header fan-out. Measure before/after compile wall time, CPU time, dependency count, object size, and number of affected objects. Consider moving frequently tuned behavior rules into data/script/JIT only if this native compile floor remains too high; that is an architecture decision beyond this import/investigation task.

## Ordered plan

1. **Close measurement gaps first.** Use one attempt ID and monotonic outer clock from file-save/watcher detection through debounce, queue wait, codegen, manifest, mirror, snapshot, worker spawn, every scan/compile/link/publish substage, parent finalization, DLL activation, and first expected in-game event. Sample CPU, memory, disk activity, and the full compiler/linker process tree every second. Capture compiler command, source/object/dependency counts, cache hit/miss and exact stale reason per object. Preserve parent wall clock as authoritative and report unassigned residual; target residual below 50 ms.
2. **Remove known wrapper churn.** Prototype runtime identity for folded whole-game module wrappers; retain standalone module validation. Acceptance: zero wrapper recompiles on generation-only changes, accurate generation in the live journal, standalone loading unchanged.
3. **Replace repeated full dependency traversal on warm edits.** Maintain reverse dependencies and watcher dirty sets. Use content hashes/signatures and index generations; update records only for changed/recompiled TUs. Keep uncertainty fallback and scheduled audit. Benchmark leaf source and high-fanout header separately.
4. **Make pre-worker preparation incremental.** Cache codegen input signatures so unchanged generated outputs can be accepted without rerunning expensive generation; maintain manifest hashes from filesystem events/metadata and only verify changed files; reuse immutable snapshot files and materialize only changed files. Never trust timestamps alone for content identity. Measure each layer and retain a hardlink/copy fallback.
5. **Measure compile cost in a controlled dev-loop diagnostic build.** Record GCC version/flags, PCH hit validity, compiler CPU time and wall time, source bytes/lines, include/dependency count, template-heavy headers, object/depfile bytes and resource pressure. GCC `-ftime-report` can break compiler time into front-end/optimization/codegen categories; diagnostic overhead must be measured and excluded from latency baseline. No direct compiler launch outside dev-loop authority.
6. **Test link alternatives without changing the active path.** Baseline repeatability first, then incremental-link state and stable candidate output, then an isolated MSVC/PDB + Blink feasibility prototype if justified. Report compile, linker, patch, publication, rollback and activation separately. Compare identical edit and runtime behavior.
7. **Set achievable acceptance tiers.** Warm leaf edit should first reach <1 s from save to active patch for a patch/JIT path, or <1 s only for scan/setup while native compile remains longer. A native MinGW path with current 14 s compile and 11 s full link cannot reach end-to-end <1 s by caching scan alone. Use results to choose between improved MinGW DLL increments, MSVC incremental link/Blink, or a hot behavior layer; do not promise arbitrary edits subsecond until runtime proof exists.

## Library implications

- **Blink:** may avoid relinking the complete DLL by patching changed functions into a compatible running MSVC process, but still needs compatible compilation/debug data and does not solve expensive C++ compilation or arbitrary object-layout migration. Its MSVC/PDB constraints make it an alternate toolchain prototype, not a drop-in for the active MinGW game.
- **cr.h:** can simplify reloadable DLL loading/state handoff, but cannot eliminate the 98.861 s scan, wrapper recompiles, C++ compilation, or full link by itself.
- **Flecs:** can help represent mutable game state/components and support data-oriented schema evolution patterns; it does not patch machine code or automatically migrate arbitrary C++ object layouts.
- **Ninja:** may improve build graph scheduling/incrementality only if integrated with correct dependency data; it will not make a full linker or expensive TU intrinsically fast.

No imported library has been shown to make every `.cpp` edit instantly live. The likely path combines eliminating repeated cache/setup work, stopping volatile identity from invalidating stable wrappers, and a patch/JIT or true incremental-link mechanism for the remaining compile/link floor.

## Scope and verification

This is documentation and read-only inspection. No compiler, build worker, full game build, or additional game instance was launched. No active compiler, main build path, architecture, hot-reload infrastructure, or game configuration was changed. Full source-to-runtime latency and CPU/disk attribution remain unmeasured.
