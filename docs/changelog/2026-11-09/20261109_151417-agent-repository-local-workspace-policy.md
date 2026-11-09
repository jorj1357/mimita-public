# Repository-local agent workspace policy

- Date: 2026-11-09T15:14:17Z
- Summary: Aligned agent instructions and build/run documentation on a repository-contained workspace target.
- Status: Documentation updated; source enforcement is not implemented by this change.

## Changes

- Defined lane worktrees under `.dev/agent-lanes/<lane-id>/`, temporary files under `.dev/tmp/`, and shared cache/scheduler diagnostics under the repository's ignored `.dev/` tree.
- Replaced the external scheduler-directory target with named Windows build-slot mutexes; attempt and wait evidence remains project-local.
- Prohibited agent access to sibling worktrees, user-profile/temp folders, and external crash artifacts. Installed toolchains may be invoked through the dev-loop without agents inspecting their installation paths.
- Aligned `AGENTS.md`, the agent-lane authority spec, dev-loop and build guides, live-development and multi-instance guidance, task completion, crash-artifact guidance, and the pending integration handoff.
- Marked current code gaps explicitly: the lane manager still chooses a sibling worktree root, the scheduler still uses `%LOCALAPPDATA%`/system-temp defaults, the room-code temp file still uses the system default, ccache is per-worktree rather than shared in coordinator `.dev`, and crash artifacts still have an external path.

## Validation and limits

- `git diff --check` reported no whitespace errors.
- Searched the active agent/build docs for old sibling paths and external scheduler instructions; the pending handoff now labels old lane locations as historical and blocks agent access pending an in-project import.
- No code, build, or runtime behavior changed or was tested. Repo-local lane enforcement and path-containment tests remain required before agents can use mode 2 lanes again.
