# DLL latency evidence and agent gap-closure rule

## User goal

Investigate how to reduce manifest, source snapshot, preflight compile, link, and DLL activation latency while MiMITA is running; keep evidence gaps actionable across agent work.

## Evidence recorded

Updated `issue-136d4553ff92` with measured attempt `game-dll-20261121T222152398558Z-ad2cd0` / generation 436:

- Whole attempt: 903.413 seconds. Nested worker metric: 886.724 seconds, leaving a 16.689-second accounting mismatch.
- Manifest snapshot: 409.415 seconds across 1,008 source files and 1,217 headers; hash cache held 2,225 entries, but hit/miss/bytes and internal stage costs are not recorded.
- Source snapshot: 111.612 seconds for 2,899 hardlinks and zero copies.
- An unaccounted 32.855-second interval separates snapshot completion from worker input validation.
- Changed-source preflight: 148.014 seconds for three translation units, with per-file compile times 96-148 seconds and parallel work overlapping.
- Object scan: 7.403 seconds. Remaining compile wall: 77.711 seconds; 11 extra wrappers were rebuilt because the generated module identity header changes each generation.
- GNU link: 30.683 seconds over 642 object files / about 480 MB object data; resulting DLL about 242.6 MB.
- Publish: 8.633 seconds, including 6.246 seconds generating the external dependency manifest.
- Generation 436 had no `game.generation.activated` event in the inspected runtime journal. Successful build/publish is not proof of live activation or intended behavior.

## Recommended optimization order

1. Instrument manifest enumeration, header walk, hash-cache hits/misses and bytes, ownership resolution, mirror diff/copy/verification/retries. The existing hash cache reuses digests but still appears to walk and stat the tree; do not call it a persistent incremental index.
2. Replace per-save exhaustive discovery with a maintained file-change journal/watcher and persistent reverse dependency index. Process changed paths and their actual dependents; perform a full audit on watcher overflow and periodically. Preserve correctness for deletes/renames, generated inputs, headers, compiler/flags/PCH changes, external inputs, and concurrent writes.
3. Measure snapshot traversal, per-file hardlink/copy, bytes, and verification. Reuse immutable content-addressed snapshots or safely update only changed paths; retain per-attempt source immutability. Investigate filesystem/antivirus overhead before assuming hardlinks are cheap.
4. Record individual changed-TU compile wall/CPU/RSS and compiler timing data. Reduce heavy includes/template work or use a compatible PCH only after profiling. Splitting a file can reduce the edit's compile surface only if it avoids recompiling the same heavy dependencies; it can also add work.
5. Stop rebuilding the eleven folded wrappers solely to embed generation identity. Prototype runtime identity from the host for folded modules, with an ABI review and live journal proof; never ignore a real dependency invalidation.
6. Benchmark a compatible faster linker and incremental strategy. A 642-object, 242.6 MB full DLL link under one second is not established and is unlikely without a smaller separately linkable hot layer or function-level patching. Blink may avoid whole-image linking for supported function patches but its compiler/debug-symbol workflow needs an isolated MSVC/PDB compatibility prototype. `cr.h` reloads a rebuilt library and does not remove compile/link cost.
7. Record load request, DLL load/validation, same-process activation, and behavior confirmation separately. Do not report generation 436 as active without the corresponding runtime event and intended NPC behavior evidence.
8. Compare matched warm no-op, one `.cpp` body edit, and header edit while exactly one existing game pair runs. Capture host CPU, memory, disk, compiler child processes, and per-stage wall time. Do not infer that `jobs=10` caps preflight, manifest, snapshot, linking, or total system CPU.

A sub-second warm no-op and change-index lookup is a reasonable measurable target to pursue. Sub-second compile of an arbitrary changed C++ translation unit and a full 642-object DLL link are different targets and are not demonstrated by current evidence. Getting a code edit live in roughly one second likely requires a small hot-function/module patch path or a script/data behavior layer, while preserving the current MinGW DLL path as fallback.

## Agent coordination

Added the user's new context and a cross-agent gap-closure suggestion to `issue-91e3384b219b`, plus a coordinator note and high-priority investigation directive: actionable evidence/instrumentation/capability gaps must become owned issues/tasks with a next action and proof condition; agents should continue safe work until fixed or explicitly blocked by a documented external dependency or user decision. Shared build/runtime authority remains protected. Reliable Python/API/CLI evidence is preferred over UI automation when it answers the same question.

## Scope and verification

No C++ source, compiler selection, active build path, game architecture, running game pair, or dev-loop runtime was changed. No build was launched. Verification was read-only inspection of the existing report, issue records, current code stage boundaries, and `python devscripts/dev-loop.py --status`; the status showed generation 436 published and the existing run still active. This session records evidence and follow-up work; it does not claim a latency fix or live behavior proof.
