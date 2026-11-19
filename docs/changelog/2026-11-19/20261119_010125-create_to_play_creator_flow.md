# Create-to-play creator flow

- Expanded `docs/specs/creators/monster-authoring.md` with the end-to-end
  in-game Create → New Map → Edit Map → author monster/mode → Play Test →
  return-to-edit workflow.
- Defined the desired same-application play/edit boundary, project revision
  behavior, safe live content reload, and the distinction between data changes
  and code changes that may require the development build workflow.
- Evidence inspected: existing `worldproject` create/open/export commands,
  Map Editor controls, world-project architecture, Avatar Animation Editor,
  and animation authoring/runtime specs. The world-project spec describes its
  CLI as an offline foundation and in-game integration as unimplemented.
- Validation: `git diff --check` and related-link checks. No implementation,
  build, runtime, or human acceptance was performed.
- Review instructions used: `docs/skills/spec-writing-v1.md` and
  `docs/skills/spec-behavior-review-v1.md`.
