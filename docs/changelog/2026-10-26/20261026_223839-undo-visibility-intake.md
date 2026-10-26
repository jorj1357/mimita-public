# Ctrl+Z entity-visibility issue intake

- **Time:** 2026-10-26T22:38:39Z (18:38:39 EDT, America/New_York).
- **Branch:** `20261011zt`.
- **HEAD at session:** `c393337d` — `reporetdly working image decal shared shader state`.
- **Result:** PASS_WITH_HUMAN_REVIEW. Recorded the user-reported editor issue and a bounded reproduction direction; no code or runtime behavior was changed or verified.

## Change

Updated `docs/specs/open-gameplay-issues.md`, under “New intake — Payload map editor usability and cart placement — 2026-10-25”, immediately after the responsiveness issue (new heading at line 598).

The insertion point previously ended with:

> Profile each action separately before choosing a fix.

Added:

> **Undo visibility issue:** The player reports that pressing Ctrl+Z disables
> entity visibility while editing. Undo should reverse an authored-data edit
> without unexpectedly hiding entity markers. Reproduce with
> `entity_visibility on`, make an undoable map-editor change, and press Ctrl+Z;
> record whether marker visibility changes, which undo/input handler runs, and
> whether the visibility setting is restored by redo, editor exit, or reload.
> The cause and whether this is a reproducible behavior regression are unknown.

This keeps the report explicitly user-reported and separates the expected
behavior from the unconfirmed cause. The requested issue is documented; no
claim is made that the symptom was reproduced.

## Review and validation

- Read `AGENTS.md` supplied by the user, `docs/ROUTER.md`,
  `docs/skills/documentation-checker-v1.md`,
  `docs/specs/open-gameplay-issues.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and
  `docs/operations/task-completion/task-completion.md`.
- The router references `docs/doc-review-09-03-2026.md`, but that path does not
  exist in this checkout. The maintained documentation checker was available
  and used for this focused intake.
- Checked the existing worktree first. The issue index was already modified,
  along with unrelated config/source files and untracked assets/evidence and
  changelogs. Those pre-existing changes were preserved; this session only
  inserted the new paragraph in the issue index and added this changelog.
- Focused documentation review: PASS. The entry identifies the report,
  expected result, reproduction action, diagnostic evidence to capture, and
  unresolved cause. No automated test or build was run for this documentation
  only change.
- Runtime reproduction and human confirmation remain open.
