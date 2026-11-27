// 2026-11-27
/* purpose
* define the cheap checks that MUST pass before a whole-game build may run
* make compile errors, fan-out, and resource pressure visible before a build slot
* tie a passing check to the exact frozen source revision that will be built
* this file DOES NOT grant compiler authority to agents
* this file DOES NOT replace the root dev-loop as the only build authority
* this file DOES NOT claim a static check can prove a link
*/

# Build preflight and admission

**Status:** Desired behavior contract.

## Why

A build is expensive and single-flight. Today the first time a syntax error or
a 300-file header fan-out is discovered is inside the build. That wastes the
slot and blocks every other agent.

The build request must become the **seal** of work that is already known to be
healthy, not the first compile.

## Authority

The root dev-loop owns every check and every build. Agents request a check or a
build; they never invoke a compiler, a worker, or a second loop directly.

A check is not a build:

- it does not link, publish, activate, or change `current.json`;
- it does not consume a build slot;
- it may use the compiler at low priority with bounded jobs;
- it produces a durable `check` record and events.

## Definitions

```text
revision       SHA-256 of the frozen source snapshot
snapshot       the exact bytes a build would compile
check record   durable proof that one revision passed the ladder
build slot     the single whole-game build permit
admission      the resource gate before a check or build may start
```

A build MUST compile the same frozen snapshot that the check validated. If the
snapshot hash differs, the check is invalid.

## Check ladder

Run cheapest first. Stop at the first failure and report the owner.

### C0 — snapshot and revision

- Freeze the source tree exactly as a build would.
- Compute the revision SHA-256.
- Record changed paths and the previous revision.
- Cost: milliseconds.
- Failure: unreadable or changing source tree.

### C1 — dependency and fan-out

- Read compiler `.d` files for the snapshot.
- Report the exact affected translation units for every changed header.
- Compare each changed header against its budget.
- Reject when a header exceeds its budget and the diff adds a dependent.
- Cost: 0.1–0.3 s for the current graph.
- Output: affected count, top headers, estimated compile seconds.
- Failure: over-budget fan-out, unresolved include, dependency cycle.

### C2 — include lint

- A new include of an over-budget header from a leaf `.cpp` is rejected unless
  the file is already an allowlisted owner.
- A hot header including a cold or aggregate header is rejected.
- A new public header without a single owner is rejected.
- Cost: milliseconds.
- Failure: ownership or dependency-direction violation.

### C3 — syntax check

- Compile **changed translation units only** with `-fsyntax-only`.
- Use the same compiler, include paths, defines, PCH, and C++ standard as the
  real build.
- Use memory-aware jobs and below-normal priority.
- Cache by source hash plus flags identity.
- Cost: roughly 30–60% of a normal compile for the same TU.
- Failure: syntax, type, include, or template error, routed to the owning file.
- Known limit: a TU that takes 100 s to compile still costs tens of seconds to
  syntax-check. Such a TU is a build-performance defect and MUST be split.

### C4 — symbol and link preflight

- Maintain a persistent symbol index per object cache: defined and undefined
  symbols.
- Update the index only for objects rebuilt by this revision.
- Compare undefined references against defined symbols across the link input.
- Detect duplicate strong definitions and missing definitions.
- Cost: 0.2–1 s for the current object set.
- Failure: missing definition, duplicate definition, obviously incompatible
  object set.
- Honest limit: inline functions, template instantiations, weak symbols, and
  archive ordering mean this is a strong filter, not a proof.

### C5 — estimate and admission

- Estimate compile and link seconds from per-path history.
- Evaluate admission.
- Produce the top three actions that would reduce the estimate, with seconds.
- Cost: milliseconds.

## Admission

A check or build MUST start only when all are true:

```text
free physical memory        >= 4 GB
host CPU over the last 5 s  < 40%
active whole-game builds    == 0
live game runtimes          <= configured cap and all leases valid
free disk                   >= 8 GB
```

The thresholds are steady-state expectations, not emergency exceptions. The
system meets them by keeping clients capped, runs leased, and jobs
memory-aware. If admission fails, the check or build emits `admission.blocked`
with the top blocker and the exact event that clears it. It does not sleep and
retry.

## Check record

```json
{
  "check_id": "check-...",
  "revision": "<sha256>",
  "snapshot_path": ".dev/build-source/<revision>",
  "created_utc": "...",
  "changed_paths": ["..."],
  "affected_tu_count": 42,
  "syntax": { "checked": 7, "passed": 7, "failed": 0 },
  "symbols": { "missing": 0, "duplicate": 0 },
  "estimate": { "p50_seconds": 30, "p90_seconds": 120, "link_seconds": 12 },
  "admission": { "status": "pass", "free_mb": 6200, "host_cpu_percent": 22 },
  "result": "passed"
}
```

A build request MUST name a `check_id` whose `revision` equals the build
snapshot revision and whose `result` is `passed`. The dev-loop rejects any
other build request.

## Commands

```text
dev-loop.py --request check [--kind agent-check] [--why <text>]
dev-loop.py --request check --changed-only
dev-loop.py --check-status <check_id>
dev-loop.py --request agent-build --build-request <id> --check <check_id>
```

## Events

```text
check.requested     check id, revision, changed paths, requester
check.started       check id, revision, ladder stage
check.stage         check id, stage, status, seconds, detail
check.failed        check id, revision, stage, owner path, exact error
check.passed        check id, revision, estimate, admission evidence
check.invalidated   check id, old revision, new revision, reason
admission.blocked   check/build id, blocker, clearing event
```

Every event is appended to the coordination event stream and is durable.

## Failure routing

- A compile error names the source file and line and the owning agent from the
  most recent edit lease.
- A fan-out violation names the header and the dependent count.
- An admission failure names the process or build consuming the resource.
- A stale check names the revision and the files that changed after it.

The failure output is short enough to paste into an issue.

## Invalidation

The check is invalidated when any of these change after it passed:

- a source or header byte in the snapshot;
- the compiler, linker, or flags identity;
- the PCH;
- the code-generated identity inputs.

Invalidation emits `check.invalidated`. A build refuses an invalidated check.

## Honest limit

No static check can prove that a link will succeed. The design therefore has
two layers:

1. The ladder catches essentially every compile error and the common link
   errors cheaply.
2. The build itself becomes fast enough that the residual risk is cheap to
   resolve.

The long-term target is not "predict the build perfectly"; it is "the build is
so cheap that prediction matters less than doing it".

## Acceptance

- A syntax error is reported by `check` without consuming a build slot.
- A header edit reports the exact affected TU count before compiling.
- A build request without a matching passed check is rejected.
- Changing a source file after a check invalidates that check.
- Admission failure names a process and an event, never a sleep.
- A passed check for a one-file leaf change estimates within 25% of the
  measured build time on the same machine.
