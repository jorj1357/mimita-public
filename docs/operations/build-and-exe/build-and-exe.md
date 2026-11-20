# EXE Safety

# Mimita Engine

This is a C++17 OpenGL game engine.

For merging reviewed agent lanes into one EXE, or for the pending
world-streaming/navmesh/reconnect compile and runtime checklist, start at
[`agent-lane-integration-and-pending-validation.md`](agent-lane-integration-and-pending-validation.md).

## Current build authority

The dev-loop is the only EXE builder. The running game's `ModuleHost` is the
only module builder. Agents never invoke `build.py`, `module-build-worker.py`,
a compiler, or a second dev-loop directly. They may request an EXE build,
candidate game-DLL build, or isolated test run through the root dev-loop. One
EXE build runs at a time; candidate DLL builds and pinned runs are bounded by
PC resources. Mode 2 source lanes remain deferred; the current policy is one
shared source tree. See
[`docs/specs/dev-loop/agent-lanes-build-authority.md`](../../specs/dev-loop/agent-lanes-build-authority.md),
[`change-to-evidence-latency.md`](../../specs/dev-loop/change-to-evidence-latency.md),
and [`agent-coordination.md`](../../specs/dev-loop/agent-coordination.md).

Direct compiler and `build.py` use remains forbidden. `MIMITA_BUILD_OVERRIDE=1`
is an emergency human-only variable; agents do not set it. A normal agent
request uses the root dev-loop build/test surface and records its identity:

```text
python devscripts/dev-loop.py --request build
python devscripts/dev-loop.py --request game-dll
python devscripts/dev-loop.py --start-test-run --profile <profile> --launch-mode <mode>
```

The persistent dev-loop owns request ordering, build status, artifact
publication, and `[1]` launch selection. It runs the compiler pipeline on one
worker while continuing to accept launch and status requests. Pressing `1`
launches the latest successfully published artifact immediately, even if it is
older than the source currently building; build completion publishes a new
artifact but does not launch it automatically.

The repository-local scheduler uses named Windows synchronization objects for
exactly one active EXE build and two module slots; it creates no coordination
files outside the project. Scheduler diagnostics and attempt records live
under the coordinator checkout's ignored `.dev/build-scheduler/` directory.
Additional lane builds wait for the EXE slot, so agents do not compile
simultaneously. The active EXE build uses one compiler job per logical
processor and runs with high process priority on Windows. It waits when free
physical memory is below the configured threshold. Compiler tools and the
helper DLL build run without opening terminal windows.

The lane manager creates worktrees only under `.dev/agent-lanes/`, and `build.py`
uses the shared coordinator `.dev/ccache/` cache. A configured scheduler or
cache path outside the repository fails clearly instead of falling back. See
`docs/architecture/live-development/self-contained-workspace.md` and
`docs/architecture/live-development/module-migration-plan.md`.

On Windows, `[1]` launches the selected build through the fixed
`.dev/current/mimita.exe` path. Each new published artifact uses its UTC
timestamp as the ID and stores its executable SHA-256 in
`build-identity.json`; the immutable folder replaces timestamp colons with
hyphens for Windows path compatibility. The fixed path is a hardlinked runtime
bundle that DevLoop updates after stopping the previous server. Firewall rules
should target this fixed executable path so all builds keep the same app
identity. Status and event metadata show the fixed launch path, canonical UTC
artifact ID, SHA-256 build ID, and immutable artifact path. Existing numeric
artifact folders remain readable and launchable.

`build_agent.py`, `buildv2.py`, and setup entry points are compatibility
wrappers that submit to this same dev-loop. `build.py build-only` and
`build_game_dll.py` are internal compiler steps. Agents must not use them.
Separate Git worktrees have independent locks and build outputs; the
repository-local scheduler and named OS build slots prevent excessive
concurrent compilation. Use repository-local lane worktrees under
`.dev/agent-lanes/`; do not share one mutable source checkout between agents.
Do not use direct compiler scripts as a separate live build path.

The build never terminates a running game to publish an artifact. Build
identity and process lifetime are separate: selecting a new artifact requires
an explicit `[1]` request, and a healthy old run remains alive after a build
failure.

## Repository Workflow

When solving coding problems:

1. Search the repository first.
2. Identify only relevant files.
3. Read only relevant sections.
4. Reason about the issue.
5. Implement the smallest correct fix.

Never answer from assumptions before searching.

Use repository search before reasoning.

Rules:

* Read repository files directly.
* Answer coding questions concretely.
* Prefer minimal patches.
* Avoid unnecessary rewrites.
* Do not ask for clarification unless required to proceed.
* Fix problems directly.
* Keep solutions practical and shippable.

If there is a TODO comment in the file you are working on, and it is easy enough to do, just do it and continue rather than skipping it.

## Immutable EXE artifacts

The compiler links `mimita.exe` in its own checkout. After a successful build,
the dev-loop verifies and copies that executable and required runtime DLLs into
`.dev/builds/<UTC artifact ID>/`. It does not replace another checkout's
executable or a running published artifact. The artifact ID and executable
SHA-256 are stored in `build-identity.json`.

Current policy is one shared checkout and one source tree. Per-agent source
isolation is deferred; do not create worktrees or source overlays. Each build
records the source revision, submodule revisions, dirty-tree fingerprint, source
snapshot, whether the source changed while it was building, and the requesting
agent/task. Shared OS build slots and CPU/memory guards limit simultaneous
compilation. One EXE build runs at a time; candidate game-DLL builds may run
concurrently within those limits and write isolated candidate output, not the
normal global `current.json`/`ready.json`.

Automatic build entry:

The root dev-loop watches saved compiler inputs and queues the incremental
build itself. A saved edit remains the normal automatic trigger. An active
agent may also request a build, candidate game-DLL build, or isolated test run
through the same root dev-loop. The request is recorded with the agent, task,
permit, build ID, wait reason, and test run.

A human may still use the emergency override for an exceptional manual build:

```
MIMITA_BUILD_OVERRIDE=1 python devscripts/dev-loop.py --request build --wait-for-build
```

Agents do not set that variable. Use `--build-mode release` when a release
configuration is specifically needed. The build output remains timestamped
unless `MIMITA_EXE_NAME` is set by an internal build workflow.

The previous "single canonical `mimita.exe`" rule is replaced by this
timestamped rule. Do not create feature-specific *test* executables (for example
`mimita-chat-test.exe`); use a normal timestamped build instead. The only fixed
name that still exists is the historical `mimita.exe` from older builds, which
may be running and must be left alone.

## Build result

## Cold-build record

Whenever an agent intentionally runs a cold build, it must append one
`Cold-build occurrence N` section to the dedicated record described in
`docs/regressions/README.md`. The entry is required even when the build
succeeds. Explain why the live build could not produce the requested result,
identify the exact cold source/owner or runtime boundary, state what the cold
build was needed to prove, and name the smallest change that would make this
work live-reloadable next time. Link the final session changelog and keep the
existing running-process safety rules unchanged.

After a build, read `build/build-result.json` (machine readable) or
`build/changelog.txt`. Both include the produced executable name and path:

```
=== BUILD CHANGELOG ===
Status: SUCCESS
Executable: mimita-20260916T173758.exe
```

`Status: SUCCESS` means a new executable was linked. `Status: NOTHING_CHANGED`
means the object files were already up to date; the referenced executable (if it
exists) is the current one. If a build is expected to change code but reports
`NOTHING_CHANGED`, that is still the current tree for the requested name.

## Running a build

Agents:

1. Confirm the single root dev-loop is online with
   `python devscripts/dev-loop.py --status`, or start/verify it with
   `python devscripts/dev-loop.py --ensure-running`.
2. Save source files. The dev-loop detects the change and queues the build.
3. Keep working. Do not run `--request build`, `--request game-dll`,
   `--wait-for-build`, or a compiler, and do not poll in a loop.
4. Read status and the journal once when build identity or runtime evidence is
   needed.

Humans only may submit an explicit manual request when the automatic path
cannot produce the required artifact:

```
MIMITA_BUILD_OVERRIDE=1 python devscripts/dev-loop.py --request build --wait-for-build
```

Use the dev-loop launch request to run the selected published artifact. For an
agent lane, use `python devscripts/agent-lane.py run <lane-id> --launch-mode
<id>` so the lane's own dev-loop records the executable and exact runtime
journals.

Do not set `MIMITA_EXE_NAME` to overwrite a running executable and do not create
alternate test executables. Never kill a running `mimita*.exe`; a new timestamped
build is how you get your own executable to test.

## Agent workflow: Mode 1 shared checkout

Mode 1 is the active workflow for agents and the human coordinator. Everyone
uses this checkout, its current branch, root dev-loop, build files, and EXE.
Agents may inspect and edit the shared checkout, but only one agent edits a
shared source file at a time. Agents do not start builds; the root dev-loop is
the only build/run authority. Its source watcher may build after a saved
compiler input changes. C++ DLL migration, not agent lanes, is the route to
reducing cold EXE builds.

Mode 2 lane instructions below are historical reference only. Do not create,
start, build, merge, or delete a lane as normal work. Preserve existing lane
worktrees and edits until they have been reviewed and deliberately integrated
or parked.

### Historical Mode 2 reference — do not run these commands

The former workflow gave each agent an isolated worktree, `.dev` state,
artifact folder, run IDs, ports, and journals. Its example commands are kept
below only to explain old lane state; they are not current instructions.

```text
python devscripts/agent-lane.py create <lane-id>
python devscripts/agent-lane.py ensure <lane-id>
python devscripts/agent-lane.py run <lane-id> --launch-mode <id>
python devscripts/agent-lane.py check <lane-id>
python devscripts/agent-lane.py status <lane-id>
```

Do not use `--mode 2 build` or `--mode 1 build`; those actions are refused.
Mode 2 shares only the content-addressed compiler cache under the coordinator
checkout's `.dev/ccache`; each lane still has its own object files, executable,
dev-loop, module DLLs, and `events.jsonl`. Named OS build slots allow one EXE
build and two module builds across lanes without an external scheduler folder.
One compiler job per lane is the default. Independent builds can still slow a
computer when many agents compile unique changes at once.

The dev-loop also watches compiler inputs and starts an incremental build after
saved changes, with a short debounce. That is compile-and-publish automation;
it does not guarantee that the already-running game receives the change. The
watcher coalesces changes during a build and runs a follow-up for the newest
source generation. A `.cpp` edit may affect one translation unit; a header
edit may affect many. Linking and artifact publication still take place.

Mode 3 is the future save-to-live-code direction described in
`docs/specs/dev-loop/dev-loop.md` and
`docs/specs/hotreload/hotreload.md`. It is not currently
a selectable command or a guarantee that arbitrary C++ changes can be loaded
into a running process. Until code has a safe replaceable boundary, use the
executable build and run path. For agents, the repository-local mode 2 lane
path becomes available only after its implementation is migrated; the human
coordinator may use the canonical single-checkout dev-loop meanwhile. The
final reviewed integration still needs a stable build.

### Historical: independent agent lanes and stable promotion

After repo-local lane support is implemented, create one isolated worktree per
agent task under `<coordinator-repo>/.dev/agent-lanes/<lane-id>/`:

```text
python devscripts/agent-lane.py create <lane-id> [--base <ref>]
python devscripts/agent-lane.py ensure <lane-id>
python devscripts/agent-lane.py run <lane-id> --launch-mode <id>
python devscripts/agent-lane.py check <lane-id> [--base <ref>]
python devscripts/agent-lane.py status <lane-id>
```

Do not run `agent-lane.py build`. The lane dev-loop builds automatically after
saved source changes; `check` reports lane build status without invoking a
build.

Assign the agent the repository-contained worktree path printed by `create`.
Each lane has an independent dev-loop daemon and per-process journals. `check`
runs focused checks selected from changed files; build results remain separate
from runtime and human acceptance evidence. `cleanup <lane-id>` stops only that
lane's dev-loop processes and preserves its worktree and evidence. All temporary
files must be placed under the lane's `.dev/tmp/` directory.

After reviewing completed commits, assemble them in order in an integration
worktree and inspect the combined diff:

```text
python devscripts/agent-lane.py promote integration --base <stable-branch> --commit <sha> [--commit <sha> ...]
```

Use `--build` on `promote` to build and publish in one operation. Stable
promotion requires a clean integration tree that remains unchanged during the
build. It copies the verified immutable artifact to `.dev/stable-builds/` and
atomically points `.dev/stable-build.json` at it. That manifest contains the
source revision, build ID, and executable path. Promotion never overwrites a
running executable; previous stable artifacts remain available.

After any build, check the status printed in the output and the lane status.
The build link ledger detects objects newer than the executable, missing or
zero objects, stale pending links, and flag changes, so a stale relink repairs
itself on the next lane dev-loop build. Do not delete objects manually.
