// 2026-11-08
/* purpose
* define the repository-local workspace rule for agents and live development
* keep every writable build/runtime artifact inside the MiMITA checkout
* prevent agents from needing permission to write outside the repository
* this file DOES NOT define gameplay behavior or the module ABI
* this file DOES NOT forbid reading installed toolchain/device dependencies
*/

# Self-contained workspace

## Rule

The repository root is the workspace root. Agents must be able to do all normal
editing, building, running, testing, and evidence collection without requesting
write access outside it.

Writable state lives under:

```text
.dev/agent-lanes/          isolated agent worktrees and lane metadata
.dev/build-scheduler/      repo-local EXE/module build slots
.dev/builds/               immutable dev-loop artifacts
.dev/current/              stable launch hardlinks
.dev/stable-builds/        promoted stable artifacts
.dev/host/                 canonical shared host EXE and stable pointer
.dev/modules/              shared content-addressed module generations
.dev/runtime/              shared runtime-root config/assets view
.dev/ccache/               shared compiler cache for lanes
.dev/tmp/                  all temporary files
.dev/runtime/              room files and transient runtime files
.dev/world-projects/       local authored world projects
build/                     object files, module generations, link output
logs/                      per-run journals
```

`.dev/`, `build/`, and `logs/` are gitignored. No generated lane, build,
module, cache, room, or journal file is tracked.

External paths are read-only dependencies only: the installed compiler
toolchain, GLFW, Blender, GPU/audio drivers, and OS services. They are not
agent write targets.

## Shared host and sparse lanes (2026-11-11)

Feature lanes do not need a full duplicate checkout. The approved reuse model:

- one stable host EXE in `.dev/host/`, used by module-only lanes;
- one shared content-addressed module store in `.dev/modules/`;
- a runtime root (`MIMITA_RUNTIME_ROOT`) for config/assets, with a lane overlay
  (`MIMITA_RUNTIME_OVERLAY`) checked first;
- sparse feature-lane checkouts may omit `assets/`, host build output, lane EXE
  artifacts, and unchanged large dependency trees; those resolve from the
  runtime root;
- the shared compiler cache remains `.dev/ccache/`.

Do not junction or hardlink tracked editable directories. Hardlinks and
junctions share writes, mutate both paths, and break lane isolation. Use sparse
checkout plus the runtime-root resolver instead. Published immutable host
artifacts and promoted module generations are the only shared executable
content; working source, object files, PCH, journals, and validation module
generations remain lane-local.

Only the lane land/publish path may write the shared host and module store.
Game processes and lane module workers must never write the shared store.

## Agent commands

Agents use only the lane dev-loop:

```text
python devscripts/agent-workflow.py --mode 2 create <lane>
python devscripts/agent-workflow.py --mode 2 ensure <lane>
python devscripts/agent-workflow.py --mode 2 status <lane>
python devscripts/agent-workflow.py --mode 2 run <lane> --launch-mode <id>
python devscripts/agent-workflow.py --mode 2 check <lane>
```

The lane dev-loop is the only EXE builder. The running lane game's `ModuleHost`
is the only module builder. Agents never run `build.py`, the module worker, a
build request, a compiler, or `MIMITA_BUILD_OVERRIDE`.

## Why repo-local scheduling

The build scheduler defaults to `.dev/build-scheduler`. This serializes all
lanes in one checkout without writing outside the repository. Separate copies
of the repository are intentionally not coordinated; if multiple checkouts are
built simultaneously they can compete for CPU. The scheduler is repo-local
only.

## Migration from outside paths

The old layout wrote lanes to a sibling directory and scheduler/cache/temp files
to the user profile. New lanes must be created under `.dev/agent-lanes`.
Existing outside lanes are transitional evidence only; do not create new ones.

## Acceptance

- `agent-lane create` prints a worktree under `.dev/agent-lanes`.
- `ensure`, `status`, `run`, and `check` write only inside the repository.
- Scheduler status reports `.dev/build-scheduler`.
- New room files appear under `.dev/runtime/rooms`, not `%TEMP%`.
- New world projects appear under `.dev/world-projects`.
- `git status` does not list generated workspace state.
- Normal agent work needs no external-directory permission.
