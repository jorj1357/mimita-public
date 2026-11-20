# Incremental build and header fan-out

## Current reality

- The game DLL builds about 628 game-owned translation units.
- Every C++ unit is compiled with `-include pch.h`.
- `.d` dependency files list included headers.
- The object cache is keyed by source path and source SHA-256.
- Header staleness is checked by file mtime, including the PCH.
- `cold-link-trial` can remove hundreds of objects; that is not incremental.

## Why a header edit rebuilds many files

A shared header is textually included by every dependent translation unit. If
that header's timestamp or content changes, each dependent `.o` must be rebuilt.
`src/pch.h` is a shared dependency for every unit. `camera.h`, input headers,
and other broad headers can have very large fan-out.

"One header = one file" is not a valid C++ expectation for a shared header.
The workable rule is:

- one header has one owner;
- its public surface is minimal;
- volatile headers are not part of the PCH;
- unrelated subsystems are forward-declared or separated;
- its dependent count is measured and tracked.

## Required tooling

- Report top headers by dependent count from `.d` files.
- Report the exact affected objects before a build.
- Report whether PCH changed and whether it invalidated all objects.
- Report explicit cold object deletion separately from compile time.
- Store per-attempt timing at `.dev/build-timings/<attempt-id>.json`.
- Keep an aggregate history outside git, e.g. `build/game/timing-history.jsonl`.

## Rules for new code

- Do not include a large aggregate header from a new subsystem.
- Do not put gameplay/volatile headers in `pch.h`.
- Prefer forward declarations and implementation files.
- Split a high-fanout header only when measurement shows the split reduces
  rebuild cost without creating a new ownership ambiguity.
- Do not split into per-file DLLs. If link time later dominates, split a small
  number of isolated module DLLs with stable capabilities.

## Acceptance

- A leaf `.cpp` edit rebuilds one object.
- A header edit reports its exact dependent count.
- A PCH change reports that it invalidates all units.
- A build that recompiles everything identifies whether the cause was PCH,
  source-state churn, explicit object deletion, or header fan-out.
- No cold-link trial deletes objects during normal live work.

## Measured generated-identity fan-out (2026-11-19)

The whole-game build writes `build/game/identity/module-identity.h` with
`MIMITA_MODULE_GENERATION` and the full manifest revision as
`MIMITA_MODULE_SOURCE_HASH`. Eleven module-wrapper translation units include
that header. The generation changes on every new generation and the source
hash changes on any source or header edit, so those eleven objects are rebuilt
on every attempt even when their own files are unchanged. Observed: generation
345 compiled 12 objects (eleven wrappers plus one edited source); generation
350 compiled 15 (eleven wrappers plus four changed sources).

The separate-module loader requires a candidate module to report the
compile-time generation and code hash from its `current.json`
(`src/hot-reload/module-host.cpp`). Folded modules inside the whole-game DLL
only validate name and struct size and initialize in place
(`src/game/module-host-bridge-module.cpp`); their reported generation feeds
`activeGeneration()` and `outGeneration` log/cache stamps.

Proposed direction, not implemented: give folded builds runtime identity from
host services (ABI version bump) so the volatile generation and revision are
not compile-time inputs, while standalone module DLLs keep the generated
literal identity and their existing validation. Acceptance for that change:
a leaf edit rebuilds one object, no wrapper fan-out on generation or revision
change, a standalone module still loads, and the live journal reports the
correct generation. The no-op fast path and bulk scan reporting do not depend
on this change.

## Header fan-out gate

`devscripts/dependency_graph.py` reads compiler `.d` files and reports direct
header fan-in. The gate form is:

```text
python devscripts/dependency_graph.py <object-dir> --gate
```

`config/header-fanout-allowlist.json` records the current wide headers and a
maximum dependent count. A header above the limit without an allowlist entry
fails the gate. Growth beyond an allowlisted maximum also fails. An allowlist
entry that has fallen under the limit is reported as stale, not failed.

Baseline measured on the 2026-11-19 game object set: 1,539 depfiles, 642
sources, 594 project headers. Top fan-in: `src/pch.h` 629,
`src/debug/structured-log.h` 338, `src/physics/physics-types.h` 299,
`src/map/map_common.h` 286, `src/physics/movement/movement-types.h` 277.

The gate is advisory evidence until the owning header split is implemented. New
headers must not add themselves above the limit without an explicit allowlist
entry and an ownership decision.

## Deferred many-file latency target

The measured monolith link floor (12-36 s with zero compiles) and per-TU compile
cost mean that 50 changed native C++ translation units cannot reach one second
on the reference laptop. The deferred architecture for that workload is a
stable native kernel plus a power-first JIT/script hot layer that contains the
actual behavior logic, with a small number of coarse native modules for
performance-critical mechanisms. It is specified in
[`../../specs/hotreload/hot-layer-latency-and-language.md`](../../specs/hotreload/hot-layer-latency-and-language.md).

Cutover-1 remains one replaceable game DLL. A per-file DLL design is rejected:
it leaves the 50 compiles unchanged and adds loader, ABI, and static-state
costs. Coarse module splitting becomes justified only after the measured link
time above and a separate human approval.
