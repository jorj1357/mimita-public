# Runtime test process and CPU plan

## Changes

- Captured the user's request in the coordination brain as context `ctx-d1d7cad3c484` and triaged improvement issue `issue-dc2f873290a7` with three owned plan tasks.
- Staged the work: (1) get matched process/thread and system-pressure baselines from a valid CPU profile, (2) reduce process churn with queued warm pinned runtimes and per-scenario journals, (3) preserve shared-server multi-client testing, and (4) evaluate multiple isolated client contexts in one OS process only if measurement justifies the architectural cost.

## Evidence and limits

- Existing workflow `docs/workflows/multi-client-dev-testing.md` already supports one server with multiple client processes; isolated test runs currently start a server/client process pair, and a run can add a second client.
- `docs/specs/dev-loop/agent-runtime-registry.md` already describes capped concurrency, queued starts, and warm pinned runtimes.
- Existing CPU evidence includes high host load and multiple MiMITA processes, but does not identify the hot code path or prove a specific recent change caused the slowdown. The current profiling attempt could not run below its 2 GiB free-memory safety threshold; the preserved earlier ETL lost 30,931 events.
- No runtime processes were started or stopped and no game source was changed for this plan.
