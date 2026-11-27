// 2026-11-08
/* purpose
* record the historical isolated-lane design and the current shared-checkout build authority
* this file DOES NOT define gameplay behavior or the module ABI
* this file DOES NOT permit agents to invoke build.py or module workers
*/

# Agent lanes and build authority

## Current repository policy (2026-11-20)

For normal agent development, the user has selected one shared checkout:
`C:\mimita-v9`, its current branch, the root dev-loop, and one source tree.
Mode 2 source lanes remain deferred. Do not create, enter, build, sync, promote,
or clean up a lane or worktree. Existing lane worktrees, branches, and edits are
parked and preserved; do not delete or merge them as part of this policy change.
Per-agent source isolation is a future option; the specification must change
again before worktrees or source overlays are used.

Agents may edit and inspect the shared checkout, but must not invoke a compiler,
`build.py`, a module worker, or a second dev-loop. Agents may work concurrently
only on disjoint files; coordinate ownership before editing. The root dev-loop
remains the only build authority.

Agents may request builds and isolated test runs through the root dev-loop, but
an agent-requested build needs an approved coordination build request first
(see "Build approval protocol"). Every request records the agent, task, permit,
build ID, and test run. One EXE build runs at a time. Candidate game-DLL builds
use the same approval and run concurrently only when an approved preview
explicitly permits a second build within configured CPU and memory limits. They
must not overwrite the normal published `build/game/current.json` or
`ready.json`.

Isolated test runs pin one EXE artifact and one game-DLL generation or explicit
DLL path. They own separate processes, ports, command queues, manifests, and
`events.jsonl` journals and do not replace the shared runtime. A pinned run may
start while another agent's build is pending. Mode 3 batch triggering is the
default: source saves accumulate into a pending batch and only an explicit root
dev-loop build request starts compilation. Set `MIMITA_BUILD_TRIGGER_MODE=auto`
before daemon startup to deliberately use the Mode 1 automatic-on-save fallback.

The detailed Mode 2 procedure below is retained as historical/reference
documentation and is superseded by this policy. Resume it only after the user
changes the project policy and the CLI guard is deliberately revised.

## Mode 3 — batched coordinated builds (active default)

**Status: active default since 2026-11-23, with approval-gated manual sealing.**
The root dev-loop records saved compiler inputs in a pending batch and does not
start compilation on save. The root coordinator confirms that active
compiler-input owners are ready and that a build request is approved, then seals
the batch with `python devscripts/dev-loop.py --request batch-build` or builds
through the approval-gated `--request agent-build --build-request <id>`. An
explicit `--request build` or `--request game-dll` also seals a pending batch and
is a build-owner/human action. The current dev-loop enforces `approved` status
for `agent-build`; it does not yet enforce per-agent ready flags or an approval
token on `batch-build`, so those remain policy obligations until the follow-up
guard lands. Mode 2 lanes remain retired.

### What we want

Mode 3 treats compilation as an expensive, deliberate, batched event instead of
an automatic side effect of saving a file.

- **Save does not build.** Saving a watched compiler input records a new source
  generation and marks the published artifact stale, but does not start a build
  on its own.
- **The root coordinator seals one build window.** Before issuing the explicit
  batch request, the coordinator confirms that compiler-input owners have
  finished editing and are ready. The request records the exact source
  generation, paths, target classes, and requester. The software does not yet
  collect readiness flags from each owner.
- **The build gets the whole machine, briefly.** Because it is short and
  bounded, the coordinated build may use all logical processors at high
  priority. The machine may feel slow for the duration; that is accepted while
  it is a short, rare burst and not a permanent state.
- **Build, then test, then repeat.** After the build publishes and activates a
  generation, each agent tests its own change in its own pinned runtime and
  captures `events.jsonl` proof. Failed or incomplete changes go back for edits
  and the next agreed build; proven changes are done.
- **Live DLL reload stays.** Mode 3 keeps same-process DLL reload and
  pinned test runtimes; it changes when a build is requested, not the DLL
  activation mechanism.

### Why

The hypothesis behind Mode 3 is a positive feedback loop:

```text
save -> automatic build -> overlapping builds + live game processes
      -> resource pressure -> slower compile/link -> longer builds
      -> more saves overlap -> more pressure ... (300-500 s builds)
```

Batching removes the overlap: one build at a time, no continuous compile/load
pressure, and no build competing with the tests it is supposed to enable.

### What success looks like

- No sustained multi-hour saturation. Whole-machine CPU and RAM are near idle
  between builds instead of pinned near 98-99%.
- Build pressure is a short, bounded burst that ends; a brief whole-PC slowdown
  during the burst is acceptable if it is quick and rare.
- Edit-to-visible latency meets the issue target (~10 s or less under measured
  Juggernaut pressure), and the build itself is not slowed by unrelated
  builds or by the game.

### Flow

1. **Edit.** Agents edit disjoint files. Saves update the source generation and
   pending batch status only; no native build is queued.
2. **Request and approve.** An agent files a durable build request, the rotating
   build owner (or two non-requester peers when no owner is set) approves it, and
   the requester posts the request ID plus expected duration. Silence has a
   bounded timeout fallback (see "Build approval protocol").
3. **Build.** The approved request builds through
   `python devscripts/dev-loop.py --request agent-build --build-request <id>`.
   The build owner/human may instead seal the batch once with
   `--request batch-build`; never start a second build while one request is
   approved, queued, or building.
4. **Drain.** The sealed batch completes and its owned compiler/linker
   processes exit before proof runs start.
5. **Postmortem.** Read `game-generation.ready.postmortem`, append the largest
   measured cause and next action to the issue, and act on it before the next
   request.
6. **Test.** Each agent launches or reloads its own runtime pinned to the new
   generation, exercises the behavior, and captures that run's `events.jsonl`.
7. **Repeat.** Unproven edits return to step 1; proven agents exit. The build
   owner approves the next ready request.

## Build approval protocol (2026-11-27)

Purpose: exactly one shared build window at a time, no unapproved compiler
start, and the smallest necessary number of builds. Approval is a coordination
gate, not a human sign-off for every routine change.

1. **Request.** Run
   `python devscripts/agent-coord.py build request --kind integration
   --paths <changed paths> --issue <id> [--priority high]`. The requester posts
   the request ID, changed paths, reason, expected build seconds, and current
   resource state on the issue/board.
2. **Approve.** The rotating build owner
   (`agent-coord.py build owner`) approves with
   `agent-coord.py build approve <request>`. When no owner is set, two active
   agents that are not the requester must confirm on the issue/board and at
   least one records the approval. A requester never self-approves; if the
   requester is the build owner, a different agent approves.
3. **Build.** Use
   `python devscripts/dev-loop.py --request agent-build --build-request <request>`.
   The dev-loop checks `status=approved` and rejects unknown or unapproved
   requests. This is the default agent build path.
4. **Seal instead of queueing more.** `--request batch-build` and
   `--request build|game-dll` are build-owner/human seal actions. They are
   permitted only after an approved request is recorded, and only once for that
   request. Do not seal a second time to "retry" without a new approved request.
5. **Single-flight.** While a request is approved, queued, or building, no
   second build request may be approved. New changes join the pending batch or
   wait; amend the existing request instead of opening a duplicate.
6. **Timeout fallback.** If no owner or peer responds within 5 minutes, post one
   final notice. After 10 minutes of silence, with no objection, halt/freeze, or
   active/approved build, the requester may self-approve and must record
   `approval=timeout` with the waited seconds. A timeout never preempts an
   active or already-approved build and does not override a halt or freeze.
7. **Postmortem.** After publish or failure, read
   `game-generation.ready.postmortem`; append the largest measured cause and
   the next action to the issue before requesting the next build.

Implementation status: `agent-build` already enforces `approved`. The unguarded
`batch-build`, `build`, and `game-dll` commands are restricted by this policy
now; making them require `--build-request` or a minted owner token is follow-up
work, not a current guarantee.

## Better build coordination — TODO proposals (2026-11-27)

These proposals target fewer, cheaper builds without blocking progress.

- **Merge-queue token.** Mint one build token per approved request; `batch-build`
  without a valid token is rejected. One token is valid for one attempt and
  expires back to the queue instead of allowing a retry loop.
- **Fixed build windows.** Agree two or three windows per hour (for example
  :00 and :30). Agents target a window; a window with no ready change is
  skipped. Predictable bursts are cheaper than ad-hoc builds.
- **Change-value gate.** Build only when a change affects behavior or unblocks
  proof. Docs/config-only changes and proven no-op reverts should not consume a
  build (the dev-loop already filters fully reverted paths).
- **Coalesce by path set.** Deduplicate requests that touch the same files into
  one integration request with multiple owners instead of serial builds.
- **Resource admission.** Estimate build RSS/CPU from the postmortem and reserve
  memory for active pinned runtimes; deny approval when the host is above the
  reserve.
- **Quiet-window acks.** Let active compiler-input owners pre-register "no
  changes this window" so the coordinator can seal immediately without polling.
- **Preview only when needed.** Use preview builds for isolated A/B work while
  integration is broken; otherwise test the last integration generation.
- **Postmortem-driven tuning.** Act on the slowest TU, header fanout, and queue
  wait from `game-generation.ready.postmortem` before adding another build;
  split or reduce the measured hot spot first.

### Invariants

- The root dev-loop remains the only builder. One EXE build at a time; agents
  never invoke a compiler, `build.py`, a module worker, or a second loop.
- Caching, stable module identity across generations, and the MinGW DLL fallback
  are unchanged.
- A failed build leaves the last-good DLL/generation active.
- Testing uses pinned artifacts and generations; a journal from another artifact
  cannot prove a change.
- No agent is blocked indefinitely on another agent's longer test. Agents may
  test the previous generation, review, or take a disjoint task, and the build
  order/queue is visible and adjustable on the coordination board.

### Proof and measurement

Mode 3's performance objective is only accepted if it measurably beats the
Mode 1 baseline with matched workloads:

- Per-attempt stage timings from `.dev/build-timings/` (request, scan, snapshot,
  compile, link, publish, activate).
- Whole-machine CPU and RAM sampled before, during, and between build windows.
  Success means negligible between-build pressure plus short bounded bursts,
  not hours at saturation.
- Active build/process count per window (target: one build).
- Edit-to-visible latency under measured Juggernaut load.
- A recorded Mode 1 baseline vs Mode 3 comparison with exact run/build IDs and
  journal paths, with source, build, runtime, and human review reported
  separately.

### Acceptance

- A save alone does not start a build.
- A coordinated window produces exactly one build for the newest generation.
- Between-build resource pressure stays below an agreed documented threshold.
- Each agent can test its own change in its own runtime and produce journal
  proof.
- The measured comparison against the Mode 1 baseline shows the intended
  improvement (or the mode is rejected or revised).
- Last-good fallback and existing authority/proof contracts are preserved.

### Remaining coordination work

- Replace the current root-owner readiness confirmation with per-agent ready
  flags tied to active compiler-input participants, and display saved-but-not-
  ready owners in the dashboard.
- What quiescence/debounce window precedes a batch build?
- How is build ordering handled when one agent's test is much longer than the
  others?
- Does a max wait or human override force a build when someone is stuck?
  **Resolved as policy 2026-11-27:** the 5/10-minute timeout approval in the
  Build approval protocol; the hard dev-loop guard is still TODO.
- What numeric thresholds define "negligible" between-build pressure?
- Does batch mode also apply to a lone human editing, or only to concurrent
  multi-agent sessions?

### Relationship to other documents

- [`dev-loop.md`](dev-loop.md) §5 currently defines save-triggered builds; Mode 3
  would revise that trigger and the associated guard.
- [`agent-build-queue.md`](agent-build-queue.md) already defines build requests,
  coalescing, rotating ownership, and resource-aware concurrency; Mode 3 layers
  the batched build window and the save-does-not-build trigger on top of it.
- [`build-time-observability.md`](build-time-observability.md) supplies the stage
  timing and resource evidence used to prove the comparison.

## Live build and journal coordination

- **The root dev-loop is the only build/run authority.** It owns the build lock,
  source snapshot, codegen checks, queueing, publication, activation status, and
  run control. Do not start a second watcher or invoke
  `game_build_worker.py` directly.
- **One EXE build at a time; bounded candidate DLL builds.** The dev-loop
  coalesces saves and waits for an active EXE build before starting the next
  one. Per-agent candidate DLL requests are accepted; active compile workers are
  bounded by CPU and memory. Failed candidate builds do not stop another
  agent's pinned run.
- **Agents request; the dev-loop builds.** Use the agent build/test request
  surface, not a compiler. Record the request and wait reason in the
  coordination activity board. A wait above the measured budget or an avoidable
  wait creates a build-friction issue.
- **Pinned journals and artifacts.** Each test run records the exact EXE
  artifact, SHA-256, game generation/path, run ID, and journal paths. A journal
  from a different artifact or unpinned executable cannot prove the change.
- **Source is shared today.** Builds take a frozen snapshot of the shared source
  at request time and may include another agent's concurrent edits. Leases,
  activity, tasks, and dependencies are the coordination mechanism. Source
  isolation is deferred, not silently emulated.
- **Fallback is explicit.** Resident fallback is a migration aid, not a hot-path
  success. Migrated behavior must reject rather than silently use resident code.

**Implementation status (2026-11-09):** the repository-local paths are enforced.
`devscripts/agent-lane.py` places worktrees under `.dev/agent-lanes/`,
`devscripts/build_scheduler.py` defaults to `.dev/build-scheduler/`, the dev-loop
room file uses `.dev/tmp/<lane-id>/`, and `build.py` uses the shared
`<coordinator>/.dev/ccache/`. `devscripts/path_containment.py` rejects paths,
metadata, symlinks, junctions, schedulers, and caches that resolve outside the
coordinator repository. The focused containment checks live in
`devscripts/quality/test_path_containment.py`.

## Historical Mode 2 design — not current instructions

The remaining sections document the former lane workflow only. The current
Mode 3 policy above controls: agents share the root checkout and only its
dev-loop may build or run. Do not use the lane commands below unless the user
explicitly resumes lane work.

## 1. Historical lane build rule

An agent edits and observes. It never builds.

- The **lane dev-loop** is the only EXE builder for that lane.
- The lane game's **ModuleHost** is the only module builder for that lane.
- A **machine-wide scheduler** allows one EXE build and two module builds at a
  time across every checkout and lane.
- The **human** may run an explicit emergency build with
  `MIMITA_BUILD_OVERRIDE=1`. Agents must never set that variable.

An agent's supported commands are:

```text
python devscripts/agent-lane.py ensure <lane>
python devscripts/agent-lane.py status <lane>
python devscripts/agent-lane.py run <lane> --launch-mode <id>
python devscripts/agent-lane.py check <lane>
```

Long-term direction: every gameplay and tool behavior source belongs to a
reloadable module. Cold EXE builds then shrink to process, device, transport,
packet-schema, document-storage, loader, and public-ABI changes. Networking
state machines, NPC perception/sensing, targeting, navigation choice, and
combat policy are hot subsystem behavior, not cold host code. Expanding the gap
between cold builds is the primary performance goal; a module compile must
never require an EXE relink.

An agent must never run:

```text
build.py
build_agent.py
buildv2.py
devscripts/module-build-worker.py
devscripts/dev-loop.py --request build
devscripts/dev-loop.py --request build --wait-for-build
devscripts/agent-workflow.py --mode 1 build
devscripts/agent-lane.py build
```

Direct compiler or `ccache` invocation is also forbidden. The supported build
entry points are the dev-loop's internal worker and `ModuleHost`'s module
worker.

## 1.1 Historical shared-host and auto-land proposal

The approved direction is one canonical host EXE plus a shared module store,
not one EXE per lane. A lane still owns its editable source/config and its
validation artifacts, but module-only lanes must not rebuild the EXE.

Lane roles:

- **module-lane**: edits module sources, module-private headers, and config.
  It consumes the stable shared host EXE, builds only modules, and validates
  with a lane overlay.
- **host-lane**: edits host code, public ABI, packet/wire schemas, loaders,
  transport, or fixed-tick mechanisms. It needs one host boundary build.
- **integration-lane**: cherry-picks reviewed commits onto current main and
  validates the combined source.

Shared, published artifacts:

- one stable host EXE artifact;
- a content-addressed module store keyed by
  `(module, abi, state_schema, code_hash)`;
- one runtime root for config/assets/modules, with a lane overlay for
  unvalidated work;
- the shared compiler cache.

Working source, object files, PCH, lane journals, and validation module
generations remain lane-local. Published host artifacts and promoted module
generations may be shared. This replaces the older rule that lanes may never
share executables or module DLLs; it does not permit sharing mutable working
source.

Module generations must be canonical. Identical code must map to the same
generation, or the module-set hash and peer agreement will not match. Only the
land command may publish into the shared store; game processes and lane module
workers must never write it.

### Auto-land policy

A lane becomes landable only after its own dev-loop proves the behavior in that
lane's `events.jsonl`. Auto-land means:

1. the agent freezes the lane and commits the reviewed change;
2. `promote` cherry-picks the commits onto current main in an integration lane;
3. `validate` runs the shared host plus integration overlay and asserts the
   journal;
4. module-only green lanes may land automatically;
5. host/ABI/packet/gameplay-sensitive lanes require explicit human approval;
6. `land` updates main, publishes the host artifact if needed, and updates
   module pointers atomically.

Conflicts, dirty main, failed validation, or source-identity changes stop the
land. Never auto-resolve a conflict. Keep the previous main commit, host
artifact, and module pointers for rollback.

Post-land shared-host smoke and automatic rollback are required policy but are
not implemented yet. The intended behavior is: after landing, run a bounded
smoke through the shared host; if it fails, restore the previous stable host and
module pointers and report the failure.

## 2. Historical rationale for mode 2

Mode 1 uses one shared checkout. Four agents editing that checkout interfere
with each other's files, build generations, artifacts, module generations,
ports, run IDs, and journals. A host edit from one agent can also rebuild code
another agent is testing. Keep this isolation, but do not create a lane until
the lane manager uses the repository-local root described here.

Mode 2 gives each agent:

- its own Git worktree and branch under
  `<coordinator-repo>/.dev/agent-lanes/<lane-id>/`;
- its own `.dev`, object files, PCH, executable artifacts, and build identity;
- its own module build directory and module-set agreement;
- its own run ID, server/client instances, ports, and `events.jsonl` files;
- its own lane ID in status and journals.

The coordinator checkout's `.dev/` owns the shared content-addressed `ccache`
cache, the stable host EXE, the shared module store, and per-attempt scheduler
diagnostics. Lanes do not share mutable working source, object files, PCH, run
state, or evidence. Published host artifacts and promoted module generations
are shared by design. All these paths remain within the coordinator repository.
Module-only lanes use a sparse worktree and a runtime-root overlay rather than
duplicating `assets/`, host EXE artifacts, or build trees. A lane build must normalize its cache inputs to the
lane worktree root (`CCACHE_BASEDIR`, `CCACHE_NOHASHDIR=1`, relative project
include/source paths) so identical source and flags hit the shared cache across
worktrees; otherwise every new lane becomes an unnecessary full cold build.

Mode 1 remains available for the human coordinator as a fallback. It is not an
agent workflow.

## 3. Historical mode 2 build scheduling design

Use these named Windows mutexes for build-slot ownership; do not create a
scheduler directory or coordination file outside the project:

- `Global\MiMITA-EXE-Build` for the one EXE slot;
- `Global\MiMITA-Module-Build-0` and
  `Global\MiMITA-Module-Build-1` for the two module slots.

These objects enforce the limits across Windows sessions and checkouts.
Persist attempt details, wait intervals, and diagnostics in the requesting
checkout's
`.dev/build-timings/` and `.dev/build-scheduler/` folders. Keep the shared
content-addressed cache at `<coordinator-repo>/.dev/ccache/`. These are ignored
project-local runtime data. Do not accept an override such as
`MIMITA_BUILD_SCHEDULER_DIR` when it resolves outside the project. Slots are:

| Slot | Limit | Owner |
|---|---:|---|
| `exe-0` | 1 | dev-loop `build.py` |
| `module-0`, `module-1` | 2 | running game `ModuleHost` worker |

Rules:

- A build waits for a free slot; it does not fail just because another lane is
  building.
- Repeated saves in one lane coalesce to the newest source generation.
- Only one EXE build may actively compile at a time across all checkouts and
  agent lanes; additional lane requests wait for that slot.
- The active EXE builder uses all available logical processors: one compiler
  job per logical processor, never a fixed four-job cap.
- On Windows, the EXE build worker and its compiler children run with
  `HIGH_PRIORITY_CLASS`. This is intentional: do not lower the build to below
  normal priority.
- If free physical memory is below `MIMITA_BUILD_MIN_FREE_MB` (default 16), the
  build waits and reports `waiting for memory`. The low default favors build
  speed; a human may raise it when the machine must stay responsive.
- An abandoned OS synchronization object is reclaimed by the operating system;
  do not use an external lock file or stale-file cleanup path.

Status output includes the requested slot, wait reason, and wait duration. It
reports another checkout's owner only when that identity is available through
the OS process interface; otherwise it says `owner=unknown`, never consulting
an outside coordination file.

## 4. Agent workflow

### 4.0 Filesystem boundary

The repository root is the boundary for every agent-created or agent-read
project file. Use `<repo>/.dev/agent-lanes/<lane-id>/` for worktrees,
`<repo>/.dev/tmp/<lane-id>/` for temporary files, and the lane's own `.dev/`,
`build/`, and `logs/` for state, artifacts, caches, and journals. Resolve paths
before use and reject symlinks, junctions, or configured roots that escape the
repository. Python temporary-file APIs must be given an explicit in-repository
`dir`; never rely on their system-temp default.

The dev-loop may invoke an installed compiler or operating-system service
through its supported interface. Agents must not browse or write to tool
installations, user-profile directories, or other checkouts. If a needed input
exists only outside the project, stop and ask a human to bring an authorized
copy into the project.

### 4.1 Create or reuse a lane

```text
python devscripts/agent-lane.py create <agent-id>
python devscripts/agent-lane.py list
```

One agent owns one lane. Do not share a lane between agents. The lane is
created from a recorded snapshot of the coordinator's current edits. `list`
shows every known lane, its dev-loop PID, and whether that dev-loop responds;
use `cleanup <lane>` to stop an idle lane's processes while preserving its
worktree, artifacts, and journals.

### 4.2 Ensure the auto-builder is running

```text
python devscripts/agent-lane.py ensure <agent-id>
```

This starts or verifies the lane's watchdog and dev-loop. It does not request a
build. From this point, saved source edits are detected automatically.

### 4.3 Edit and keep working

- Host source edit: the lane dev-loop queues an EXE build.
- Module source edit: the lane game's `ModuleHost` builds a module generation
  when the game is running.
- Shared ABI/header edit: treat it as a cold boundary; every lane will rebuild.
  Coordinate it with the human.
- JSON/config edit: the owning reload path applies it.

Do not run `--wait-for-build` and do not poll in a loop. Read status once when
needed, then keep working. An agent that waits is wasting the time the dev-loop
is already using.

### 4.4 Get the lane executable and test

For a host edit:

```text
python devscripts/agent-lane.py status <agent-id>
python devscripts/agent-lane.py run <agent-id> --launch-mode <id>
```

The lane launches only its own artifact and writes only its own journal under
`logs/<date>/<run_id>/`.

For a module edit:

1. Launch the lane once.
2. Save the module source.
3. Wait for `module.activated` with a new generation in the same PID.
4. Exercise the behavior and read the decision event's `generation` field.

### 4.5 Evidence

A completed edit reports separately:

- lane ID, worktree, and branch;
- EXE build ID and PID, or module name/generation and PID;
- the exact `events.jsonl` path for that lane's run;
- the owner event, fields, and before/after result;
- sample count, missed frames/ticks, and unexercised paths;
- build evidence, runtime-journal evidence, and human review as separate claims.

A build or activation alone is not behavior proof.

## 5. Failure behavior

- Missing build authority: the builder refuses to compile and prints the exact
  supported commands.
- Busy machine slot: the build waits, prints the wait reason, and leaves the
  previous artifact/generation active.
- Compile/link/ABI/state-migration failure: the previous EXE artifact or module
  generation remains active.
- Stale slot: reclaimed only after the owner PID is gone and the slot is old
  enough to avoid racing a legitimate build.
- Dev-loop absent: `ensure` starts it; an agent never starts a manual build.

## 6. Human emergency build

A human may run a direct build only when the dev-loop cannot produce the
required artifact:

```text
MIMITA_BUILD_OVERRIDE=1 python build.py build-only
```

This prints a warning and must be recorded as an intentional cold build. Agents
must never use it. An outside-project file path is a separate emergency
exception: only a human may explicitly authorize it, and the exact path,
reason, and cleanup must be recorded. There is no automatic external-path
fallback.

## 7. `check` observes, it does not build

`agent-lane.py check` reports the lane's current dev-loop build state for C++
changes and the exact journal/build identity. It never invokes `build_lane`, a
compiler, or a build request. Use `status` and the lane journal for compile
evidence; a `check` result is not a build.
