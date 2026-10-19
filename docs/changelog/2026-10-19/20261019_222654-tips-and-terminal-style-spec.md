# Gameplay tips and terminal style specification

- **Timestamp:** 2026-10-19T22:26:54Z (2026-10-19 18:26:54 EDT, America/New_York)
- **Branch/commit:** Not inspected for this documentation-only task.
- **Status:** `PASS_WITH_HUMAN_REVIEW`

## Final state

Expanded `docs/specs/tips/tips.md` from informal author notes into a behavior
specification covering source-backed current behavior, proposed host-command
terminal styling, rich tip presentation, a scalable content layout for a
100,000-tip library, migration constraints, validation, and unresolved choices.
Original notes remain in the document as context. The shard layout and style
format are proposals, not implementation claims or approved final values.

## Files and exact content

### `docs/specs/tips/tips.md`

- Lines 1-213: replaced the informal note with the new specification.
- Exact prior opening:

  ```text
  2026 10 19 1824 jorj todo
  like a event queue likem inecraft so u can see whats going on
  ```

- Exact new opening:

  ```text
  // 2026-10-19
  /* purpose
  * define the current and desired behavior of player-facing gameplay tips
  * specify an editable content format that can grow to a large library
  * define optional text and card presentation such as size, color, and animation
  ```

- New sections: `Current behavior (repository evidence, 2026-10-19)` (lines
  20-47), `Desired authoring and display behavior` (49-75), `Large-library
  structure proposal` (76-134), `Host command output color` (135-148), content
  conventions (150-163), future acceptance criteria (165-184), open decisions
  (186-197), and retained author notes (199-216).

### `docs/changelog/2026-10-19/20261019_222654-tips-and-terminal-style-spec.md`

- New file; no prior text existed. It records this session, findings, exact
  prior/new excerpts, validation, and review still needed.

## Evidence and review

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`,
  `docs/architecture/terminal-commands/terminal-commands.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`, and the
  available current GUI spec `docs/specs/gui/gui-general.md`.
- Router drift: `docs/ROUTER.md` names `docs/doc-review-09-03-2026.md` and
  `docs/specs/gui/gui.md`; neither exists in this checkout. Used the available
  `gui-general.md` and did not change router files.
- Source inspection: `src/utils/tips.cpp`, `config/tips.json`,
  `src/notifications/notifications.cpp`, `src/notifications/notifications.h`,
  `config/notifications.json`, `config/tipsconfig.json`,
  `config/gui/notifications.json`, `src/devtools/terminal.cpp`, and
  `src/devtools/terminal-render.cpp`.
- Documentation check: purpose, scope, audience, and DOES NOT boundaries are
  present. `git diff --check -- docs/specs/tips/tips.md` passed. No tests or
  builds were run; this task changed documentation only.
- Repository-wide TODO search found unrelated existing notes, left untouched.
  Relevant routed example: `docs/specs/gui/gui-general.md:1` contains
  `2026 10 19 1813 jorj todo explainnnnnnn`; suggested expansion: state that
  this document defines general GUI ownership/scope and link to focused GUI
  specs. The previous TODO in `docs/specs/tips/tips.md` is retained as author
  context and answered by the new sections.

## Human review and remaining decisions

No runtime or visual acceptance was performed. Human decisions remain on exact
font-size bounds, palette values, rainbow animation parameters, shard and
startup/memory budgets, initial eligibility filters, and whether host command
input echoes should also be light purple. Pre-existing unrelated working-tree
changes were preserved.
