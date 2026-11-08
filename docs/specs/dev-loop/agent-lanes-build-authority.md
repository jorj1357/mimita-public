// 2026-11-08
/* purpose
* define mandatory isolated agent lanes and single-owner build authority
* keep every agent's edits, executable, modules, runs, and journals separate
* prevent multiple agents from compiling the same machine at once
* this file DOES NOT define gameplay behavior or the module ABI
* this file DOES NOT permit agents to invoke build.py or module workers
*/

# Agent lanes and build authority

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

## 2. Why mode 2 is mandatory

Mode 1 uses one shared checkout. Four agents editing that checkout interfere
with each other's files, build generations, artifacts, module generations,
ports, run IDs, and journals. A host edit from one agent can also rebuild code
another agent is testing.

Mode 2 gives each agent:

- its own Git worktree and branch;
- its own `.dev`, object files, PCH, executable artifacts, and build identity;
- its own module build directory and module-set agreement;
- its own run ID, server/client instances, ports, and `events.jsonl` files;
- its own lane ID in status and journals.

Lanes share only the content-addressed `ccache` cache. They do not share source,
objects, executables, module DLLs, run state, or evidence.

Mode 1 remains available for the human coordinator as a fallback. It is not an
agent workflow.

## 3. Machine-wide build scheduling

The scheduler uses a shared directory outside the checkout, normally
`%LOCALAPPDATA%\MiMITA\build-scheduler`. Slots are:

| Slot | Limit | Owner |
|---|---:|---|
| `exe-0` | 1 | dev-loop `build.py` |
| `module-0`, `module-1` | 2 | running game `ModuleHost` worker |

Rules:

- A build waits for a free slot; it does not fail just because another lane is
  building.
- Repeated saves in one lane coalesce to the newest source generation.
- The EXE builder defaults to `min(4, logical_processors / 2)` compiler jobs.
- On this machine (8 cores, 12 logical processors, 16 GB RAM), the EXE builder
  uses at most 4 jobs.
- Builds run below normal priority.
- If free physical memory is below `MIMITA_BUILD_MIN_FREE_MB` (default 1024),
  the build waits and reports `waiting for memory`.
- A stale slot whose owner PID has exited is reclaimed after a bounded age.

Status output includes the scheduler wait reason and the active slot when the
build output is captured by the dev-loop.

## 4. Agent workflow

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
must never use it.
