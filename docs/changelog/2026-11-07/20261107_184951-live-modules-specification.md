# Live modules specification — 2026-11-07

## Change

Expanded `docs/specs/hotreload/hotreload.md` into the source-checked target
specification for automatic native C++ module builds, safe tick-boundary
activation, host-owned state, versioned module state, per-process generation
evidence, the NPC utility goal/action proof, all-systems migration inventory,
and edit-to-evidence timing measurement. It states that native C++ still needs
compilation and describes a later restricted script/data tier for policies
that should avoid native compilation.

Added current-source reconciliation notes to the hot-kernel architecture,
hot/cold audit, and migration history; linked the source-checked spec from the
live-development architecture. Historical `HOT` and `implemented` entries are
now explicitly identified as historical rather than proof of current source.

## Source and behavior review

- Read `docs/skills/documentation-checker-v1.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/logging-checker-v1.md`, plus the routed hot-reload/NPC/runtime
  evidence documents.
- Inspected the tracked hot-reload files and `build_game_dll.py`: the current
  DLL target includes only effect-part and quick-hit debug-visual code; the
  loader invokes the focused build synchronously and exposes a small `GameAPI`.
- Inspected `src/npc/npc.cpp`, `src/npc/npc-utility.cpp`,
  `src/network/server-npcs.cpp`, and `src/hot-reload/`. Current NPC decision
  code remains in the EXE source path; the proposed `npc-behavior` module is a
  target, not current implementation.
- Older architecture/audit material claimed broader NPC/gameplay module
  implementations whose sources are absent from the current tracked tree.
  Added explicit reconciliation banners rather than treating historical notes
  as current proof.
- Current NPC evidence includes bounded target, utility-decision, pursuit,
  movement-decision, and target-change events. The spec requires generation
  identity and a real fixed-tick server trial before calling the NPC module
  implemented.

## Validation and limits

- `git diff --check` passed for the edited documentation.
- The docs TODO scan found only instructions and historical mentions of TODO
  phrases, not an actionable “explain/clean/add explanation” TODO comment.
- Documentation-only change. No compiler build, game run, or `events.jsonl`
  runtime proof was performed. The module system and NPC module remain
  unimplemented; runtime and human acceptance are still required.
- Preserved the pre-existing modification to
  `docs/specs/npc-behavior/npc-behavior.md`.
