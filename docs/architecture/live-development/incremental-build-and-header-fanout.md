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
