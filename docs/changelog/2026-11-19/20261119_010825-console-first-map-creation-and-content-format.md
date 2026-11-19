# Console-first map creation and content format

- Expanded `docs/specs/creators/monster-authoring.md` with the proposed
  `mapeditor_create <project-id> [title]` command contract, starter floor/spawn,
  project opening/editor entry behavior, and shared command/action path for a
  later Create menu.
- Selected the existing world-project JSON/directory format for editable local
  content; postponed automatic download/discovery and polished packaging until
  the create-edit-play loop and dependency inventory are reliable.
- Specified reusable tick-indexed animation and hitbox tracks for monsters,
  weapons, and supported objects, distinguishing body collision from attack
  hitboxes.
- Evidence inspected: terminal command rules/checker, `worldproject` command
  registration, `WorldProjectStore`, `MapConfigRegistry`, and world-project
  specification. `mapeditor_create` is not currently implemented; existing
  project creation captures the current map and does not itself enter the
  editor on a new starter world.
- Validation: `git diff --check` and document/link review. No code change,
  build, runtime, or human acceptance was performed.
- Review instructions used: `docs/skills/terminal-command-checker-v1.md`,
  `docs/skills/spec-writing-v1.md`, and
  `docs/skills/spec-behavior-review-v1.md`.
