// 2026-10-22T17:20:26Z

# Task

- Task ID: Live Blender-to-game map editing issue intake
- Summary: Record the user's proposed Blender-to-game live map workflow and long-term in-game editor direction in the open gameplay issues inbox.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-22T17:20:26Z UTC; 2026-10-22 13:20:26 EDT, America/New_York
- Branch: `20261011zt`
- Base commit: `853366427f4285e43a21005e82838f000260668d`
- Final commit: `853366427f4285e43a21005e82838f000260668d` (no commit created)

# Pre-existing changes

- Preserved all existing unrelated working-tree changes, including modified config files, `docs/jorj-docs/20261022plan.md`, shader/render source files, the previously modified open-issues inbox, and the existing untracked changelogs `20261022_171528-gameplay-issues-intake.md` and `20261022_171650-zombie-tower-editor-blocker.md`.
- The inbox already contained uncommitted issue rows from prior work. This task added one intake row and one matching index row; it did not alter the earlier rows.

# Documents reviewed

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/open-gameplay-issues.md`, `docs/specs/entity-editor/entity-editor.md`, `docs/features/live-code-development/live-code-development.md`, `docs/architecture/live-development/live-development.md`, and `docs/operations/asset-management/asset-management.md`.
- Router-listed `docs/doc-review-09-03-2026.md` is absent from this checkout.
- Existing Entity Editor v0 is command-driven, writes authored entity data to the active map JSON, and has no documented Blender watcher or viewport manipulation/copy workflow. The live-code-development feature describes hot C++ module reload and does not itself define map/asset reload. No source or runtime inspection was performed to prove a watcher or safe GLB hot-reload path exists.

# Change

## `docs/specs/open-gameplay-issues.md`

- Added intake row at line 81 and issue-index row at line 119.
- Before: no issue row described a live Blender-to-game map edit workflow.
- After intake row: `| Live Blender-to-game map editing | The player wants to edit a map in Blender and see the game update without repeatedly exporting/loading by hand. A possible first step is a Python watcher that observes the selected map's Blender source or exported `.glb` and asks the game to reload the changed map; the longer-term goal is doing more of the map editing inside the game. | Early idea; no file-watching, Blender export, reload protocol, safety checks, or editor ownership has been selected or verified. Write a focused workflow/architecture spec before implementation: define whether the watched source is `.blend`, exported `.glb`, or both; how Blender produces a valid new asset; how the game detects a complete save, validates and safely reloads it, and reports errors; and how the in-game editor grows toward the same workflow. Keep this connected to the Zombie Tower spawn-volume authoring blocker, but treat it as a reusable map-authoring workflow rather than a Zombie Tower-only feature. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`../features/live-code-development/live-code-development.md`](../features/live-code-development/live-code-development.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md) |`
- After index row: `| Live Blender-to-game map editing workflow | User wants a selected map opened in Blender and changes reflected in-game, initially possibly through a Python watcher for the Blender source or exported `.glb`, with a longer-term goal of a complete in-game map editor | No watcher/export/reload architecture chosen. Define safe save detection, asset validation, reload/error behavior, and the relationship between Blender and in-game editing in a dedicated spec before implementation. | [`entity-editor/entity-editor.md`](entity-editor/entity-editor.md), [`../features/live-code-development/live-code-development.md`](../features/live-code-development/live-code-development.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md) |`
- Reason: preserve the proposed near-term watcher and long-term in-game editor without claiming that either exists or deciding the watched file/reload architecture.

# Validation and follow-up

- `git diff --check -- docs/specs/open-gameplay-issues.md` returned no whitespace errors; Git emitted only the existing LF-to-CRLF working-copy warning.
- No code, Blender file, GLB, or runtime behavior changed. No build, test, watcher experiment, map reload, or human acceptance was performed.
- Human/design follow-up: a focused Entity Editor workflow/architecture spec should define Blender source versus exported GLB ownership, export/detection, safe load and rollback on invalid assets, how the game is told what to reload, and how map JSON entities remain connected to the geometry.
- Related existing Entity Editor raw notes are preserved and not treated as a complete architecture decision.
