# EXE Safety

# Mimita Engine

This is a C++17 OpenGL game engine.

For merging reviewed agent lanes into one EXE, or for the pending
world-streaming/navmesh/reconnect compile and runtime checklist, start at
[`agent-lane-integration-and-pending-validation.md`](agent-lane-integration-and-pending-validation.md).

## Current build authority

The dev-loop is the only EXE builder. The running game's `ModuleHost` is the
only module builder. Agents never invoke `build.py`, `module-build-worker.py`,
a build request, or a compiler directly. The mode 2 lane is repository-local;
agents ensure its dev-loop is running and read status/journals while it
auto-detects saved edits. Lane creation rejects paths, metadata, symlinks,
junctions, schedulers, and caches that escape this checkout. See
[`docs/specs/dev-loop/agent-lanes-build-authority.md`](../../specs/dev-loop/agent-lanes-build-authority.md).

A human may submit an explicit build or use the emergency override:

```text
python devscripts/dev-loop.py --request build --wait-for-build
MIMITA_BUILD_OVERRIDE=1 python build.py build-only
```

The persistent dev-loop owns request ordering, build status, artifact
publication, and `[1]` launch selection. It runs the compiler pipeline on one
worker while continuing to accept launch and status requests. Pressing `1`
launches the latest successfully published artifact immediately, even if it is
older than the source currently building; build completion publishes a new
artifact but does not launch it automatically.

The repository-local scheduler uses named Windows synchronization objects for
exactly one EXE slot and two module slots; it creates no coordination files
outside the project. Scheduler diagnostics and attempt records live under the
coordinator checkout's ignored `.dev/build-scheduler/` directory. It queues
rather than failing, coalesces newer saves, runs below normal priority, caps
compiler jobs at four on this machine, and waits when free physical memory is
below the configured threshold. The per-checkout OS lock remains a second
guard.

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

Each agent build is intended to come from a separate worktree under
`.dev/agent-lanes/<lane-id>/`, so lanes do not share source, object files, PCH,
executable output, or dev-loop state. Shared OS build slots limit simultaneous
compilation across lanes. Set `--jobs` per lane to keep compiler load within
machine capacity. Each build also records its lane's
source revision, submodule revisions, dirty-tree fingerprint, and whether the
source changed while it was building.

Build entry:

```
python devscripts/dev-loop.py --request build --wait-for-build
```

Use `--build-mode release` when a release configuration is specifically
needed. The build output remains timestamped unless `MIMITA_EXE_NAME` is set
by an internal build workflow.

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

All contributors:

```
python devscripts/dev-loop.py --request build --wait-for-build
```

Use the dev-loop launch request to run the selected published artifact. For an
agent lane, use `python devscripts/agent-lane.py run <lane-id> --launch-mode
<id>` so the lane's own dev-loop records the executable and exact runtime
journals.

Do not set `MIMITA_EXE_NAME` to overwrite a running executable and do not create
alternate test executables. Never kill a running `mimita*.exe`; a new timestamped
build is how you get your own executable to test.

## Agent workflow mode 1 and mode 2

Mode 2 is the required agent workflow after repository-local lane support is
implemented. Each agent gets one isolated lane with its own worktree, `.dev`,
artifact folder, module builds, run IDs, ports, and journals. Mode 1 remains a
coordinator/human fallback and is not an agent workflow. Until implementation
supports the `.dev/agent-lanes/` root, agents must not create or launch lanes.

After repo-local lane support is implemented, agents never build. They ensure
the lane dev-loop is running and read its status. The dev-loop auto-detects
saved edits.

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
`docs/architecture/live-development/live-development.md`. It is not currently
a selectable command or a guarantee that arbitrary C++ changes can be loaded
into a running process. Until code has a safe replaceable boundary, use the
executable build and run path. For agents, the repository-local mode 2 lane
path becomes available only after its implementation is migrated; the human
coordinator may use the canonical single-checkout dev-loop meanwhile. The
final reviewed integration still needs a stable build.

## Independent agent lanes and stable promotion

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
