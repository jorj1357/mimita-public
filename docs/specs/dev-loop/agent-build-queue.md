// 2026-11-20
/* purpose
* specify the shared build queue, rotating build owner, per-agent/cohort DLL pointers,
* and preview overlays for concurrent MiMITA agents
* make integration the shared truth and preview an optional isolated build
* this file DOES NOT grant compiler authority to agents
* this file DOES NOT change the one-EXE/one-game-DLL Cutover-1 contract
*/

# Agent build queue and per-agent DLLs

## Authority and scope

The root dev-loop remains the only build authority. Agents submit requests,
review/approve them, and pin published artifacts. Agents never run a compiler,
`build.py`, a module worker, or a second loop.

This specification covers:

- build requests
- rotating build-owner role
- resource-aware queueing
- integration and preview builds
- immutable per-agent/cohort DLL pointers
- preview overlay source views
- host-ABI/EXE boundary

It does not cover gameplay behavior, map content, or the hot-layer architecture.

## Build request

A build request is a durable record in the coordination brain:

```text
build_request_id
agent
cohort (optional)
issue_id
kind: integration | preview
priority: high | normal | low
base_generation (preview only)
requested_paths
overlay_paths (preview only)
purpose
dependencies
resource_estimate
status
build_attempt_id
generation
dll_path
dll_sha256
snapshot_path
wait_reason
created_utc
updated_utc
```

Statuses:

- `proposed`
- `approved`
- `queued`
- `building`
- `published`
- `failed`
- `rejected`
- `superseded`
- `excluded` (host ABI/EXE change; shared build required)

### Integration request

An integration build snapshots the current saved project source and links one
complete game DLL. It is the shared truth.

- Default request kind.
- Multiple integration requests coalesce.
- All agents may test the published integration generation.
- Proof for issue closure requires an integration generation.

### Preview request

A preview build starts from a base integration generation and overlays only the
requesting agent/cohort files.

- Optional; lower priority than integration.
- Used for isolated debugging, A/B comparison, and keeping an agent productive
  while integration is broken.
- Preview proof cannot close an issue.
- Promotion to integration is a separate integration request.

## Build approval policy (2026-11-27)

Approval is policy-driven; no human approval is required. The requester runs
`agent-coord.py build request --auto`; the brain evaluates the request and
records the decision and evidence in `build_approvals`.

- Active peers are agents with `status='active'` and a heartbeat
  (`last_seen_utc` / `activity.updated_utc`) within **5 minutes**, excluding the
  requester.
- An idle peer (no unexpired edit lease, no live registered runtime, not in a
  building/editing/testing phase) does not block.
- A working peer or a lease on a requested path produces `ask`: the requester
  messages the peers. One non-requester approval or the build owner approves.
  After two asks with no response, record both asks and `no_response`, then
  approve as `auto:no_response`. Do not sleep or poll between asks.
- Another request in `approved`, `queued`, or `building` produces `wait`
  (`build_in_progress`). Amend the active request; do not open a duplicate.
  An active request whose `updated_utc` is older than the stale window
  (default 30 minutes) is proven abandoned by the coordination tools and is
  superseded by the next auto-approval with a `coord.build.superseded` event;
  the reaping request, age, and reason are recorded in the evidence.
- CPU above 90% or less than 1024 MB available RAM produces `wait`
  (`resource_pressure`), rechecked on the next request or `build.finished`.
- `halt` or `freeze` produces `reject`.
- No active peers produces `auto:no_peers`; active-but-idle peers produces
  `auto:peers_idle`.

Every approval and rejection is an append-only `build_approvals` row with the
actor (`auto:*`, `peer:<agent>`, `owner:<agent>`, `human`), reason, and evidence.
The requester never approves its own request.

## Rotating build owner

The owner remains a tie-breaker, auditor, and resource monitor; it is not a gate.

- Rotation occurs after publish/failure or a configured short interval.
- The role is visible on `agent-coord board`.
- The role does not grant compiler authority or permission to edit another
  agent's files.
- The owner records/objects to approvals, rejections, coalescing, waits, and
  resource decisions.
- A requester that is also the owner still needs a different agent to approve a
  peer-active request; the alone case is `auto:*` and does not need the owner.

### Owner/agent/user consultation

Escalate to a short agent thread or the user when:

- a shared header or host ABI is touched;
- two overlays disagree;
- the host is under heavy external load;
- the runtime cap is reached;
- a preview would delay integration;
- an irreversible or critical class change is proposed;
- a previous build attempt failed with an unresolved cause.

The escalation records: attempts, options, recommendation, missing input, and
why the owner cannot proceed safely.

## Resource-aware concurrency

Active game-DLL builds are bounded by measured resources:

```text
active_builds = clamp(
  (available_memory - reserve) / measured_build_rss,
  min=1,
  max=physical_cores / minimum_compile_jobs
)
```

Rules:

- Queue length may be unbounded; active concurrency is bounded.
- Reserve memory and CPU for the OS, active test runtimes, and the editor.
- Integration has priority over preview.
- Start with one active game-DLL build. Permit a second only after measurement
  shows tests and builds remain responsive.
- A second build must use reduced jobs or an isolated resource budget.
- No build starts while a halt/freeze forbids agent work unless the request is
  explicitly human-authorized.
- Record `wait_reason`, queue age, and resource snapshot.

## Immutable DLL pointers

Each successful build publishes an immutable generation and atomic pointers:

```text
build/game/generation-N/game-gN.dll
build/game/agents/<agent>/current.json
build/game/cohorts/<cohort>/current.json
```

Pointer fields:

```text
generation
dll_path
dll_sha256
snapshot_revision
snapshot_path
base_generation
build_request_id
issue_id
created_utc
```

Rules:

- Pointer writes are atomic.
- Pointers reference one immutable DLL.
- Subagents inherit the cohort pointer unless they explicitly request a preview.
- A runtime or proof pin keeps its generation/pointer alive.
- Older unreferenced pointers/generations are pruned only after active runtime,
  proof, and retention checks.

Runtime selection:

```text
agent-runtime start --runtime <id> --agent <agent> --game-generation N
agent-runtime start --runtime <id> --agent <agent> --dll <path>
```

The runtime verifies the DLL hash and records the generation in `run.json`.

## Preview overlay source view

A preview build source view is:

```text
base integration snapshot
  + overlay files from the requesting agent/cohort
```

Implementation requirements:

- Base source is immutable; prefer content-addressed blobs or hardlinks.
- Only overlay files are copied/refreshed.
- Build manifest is computed from the preview tree.
- Compile changed TUs and affected dependents; link the complete DLL.
- The preview must not silently include another agent's current unrequested
  changes.
- A shared-header overlay requires dependent recompilation and a coordination
  decision.
- If the base generation is missing or invalid, the preview fails before
  compilation with a concrete reason.

## Host ABI and EXE boundary

A per-agent game DLL cannot carry:

- host ABI changes;
- EXE-owned source changes;
- packet/wire schema changes that alter the host contract;
- production/deployment changes.

These requests must be `excluded` from the per-agent DLL path and resolved by a
shared EXE/cold build with a coordinated pause.

## Waiting policy

Waiting is the enemy. The target is to reduce avoidable waits toward zero, not
to hide them.

Track at minimum:

- request-to-approval latency
- queue age
- approval-to-build-start latency
- compile, link, and publish duration
- publish-to-first-runtime-event latency
- runtime startup and readiness
- proof verification latency
- time an agent is idle solely because another build/test is running

Every wait record must name the blocker and the removal option:

```text
slot_busy -> coalesce, prioritize, or add measured capacity
memory -> reduce jobs, prune, or wait for the user's heavy app
shared_header_conflict -> split, serialize, or decision
runtime_cap -> stop idle runtimes or queue a bounded test
link -> split/incremental link or accept the measured floor
compile -> header split, smaller TU, or cache improvement
```

Any avoidable wait or wait above the measured budget creates a
`build-friction` issue. Repeated waits must propose a concrete engineering
change and a metric target.

Waiting is a tracked defect, not an invisible delay.

Every request records:

- queue entry time
- preferred start time
- wait reason
- resource snapshot
- build slot time
- compile/link duration
- post-publish latency

If a wait exceeds its budget or is avoidable, the system creates a
`build-friction` issue with evidence.

Agents should use wait time to test the previous generation, review a decision,
or work on a disjoint task. They should not idle solely because a build is
running.

## Evidence and observability

Every request and generation records:

- agent, cohort, issue, purpose
- snapshot revision/path
- changed paths and build cause
- cache hits/misses
- compile/link/publish timings
- wait reasons
- DLL hash/generation
- pointer publication
- runtime/proof links

The dev-loop journal remains the terminal build record. Coordination records the
request, owner decision, queue, and pointer state.

## Acceptance

- Two agents can request disjoint integration work; requests coalesce safely.
- A preview contains only its base plus requested overlay files.
- Preview proof cannot close an issue.
- Integration proof can close an issue.
- The build owner records every approval, rejection, wait, and resource reason.
- Per-agent/cohort pointers are atomic and immutable.
- A pinned runtime loads exactly the recorded DLL/generation.
- Host ABI/EXE requests are excluded from per-agent DLL-only builds.
- Resource-aware concurrency does not starve live tests.
- Avoidable/over-budget waits create build-friction issues.
- No agent invokes a compiler, `build.py`, or a second dev-loop.

## Open review questions

- Should a build owner be allowed to auto-approve its own low-risk request, or
  must a second agent approve it?
- What is the default wait budget before auto-creating a build-friction issue?
- Should preview requests be allowed while an integration build is queued, or
  only when the integration queue is empty?
- Should cohort pointers be created automatically at cohort creation, or only
  on first successful cohort build?
- What exact memory reserve should be kept for active tests and the editor?
## Revision gate and retry latch

Queued integration requests are coalesced by source revision. A new revision
supersedes older pending requests; the active build is never duplicated. After
completion, at most one follow-up build runs for the newest revision.

A failed revision is latched until the source changes or an explicit retry is
requested. Compile failures never auto-retry. Transient disk, permission, or
publication failures may retry once after the resource condition clears. The
queue records superseded request IDs and the failed signature so no agent has
to infer whether a retry is pending.

