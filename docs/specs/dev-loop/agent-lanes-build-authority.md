// 2026-11-08
/* purpose
* define mandatory isolated agent lanes and single-owner build authority
* keep every agent's edits, executable, modules, runs, and journals separate
* prevent multiple agents from compiling the same machine at once
* this file DOES NOT define gameplay behavior or the module ABI
* this file DOES NOT permit agents to invoke build.py or module workers
*/

# Agent lanes and build authority

**Implementation status (2026-11-09):** the repository-local paths are enforced.
`devscripts/agent-lane.py` places worktrees under `.dev/agent-lanes/`,
`devscripts/build_scheduler.py` defaults to `.dev/build-scheduler/`, the dev-loop
room file uses `.dev/tmp/<lane-id>/`, and `build.py` uses the shared
`<coordinator>/.dev/ccache/`. `devscripts/path_containment.py` rejects paths,
metadata, symlinks, junctions, schedulers, and caches that resolve outside the
coordinator repository. The focused containment checks live in
`devscripts/quality/test_path_containment.py`.

## 1. The rule

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
packet-schema, document-storage, loader, and public-ABI changes. Expanding the
gap between cold builds is the primary performance goal; a module compile must
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

## 2. Why mode 2 is the target for parallel agent work

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
cache and per-attempt scheduler diagnostics. Lanes do not share source, objects,
executables, module DLLs, run state, or evidence. All these paths remain within
the coordinator repository.

Mode 1 remains available for the human coordinator as a fallback. It is not an
agent workflow.

## 3. Build scheduling without external files

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

Do not block on `--wait-for-build`. Poll status occasionally and keep working.
An agent that waits is wasting the time the dev-loop is already using.

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
