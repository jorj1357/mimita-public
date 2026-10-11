# Avatar editor transform contract and general live-fix workflow

Time (EST): 2026-10-11 17:15:00 -04:00
Branch: current working branch (not committed)
Status: PASS_WITH_HUMAN_REVIEW

## Request and result

The avatar-editor note was too informal to implement safely. It did not define
the GLB import contract, Blender/root naming, transform ownership, failure
behavior, or runtime acceptance. `docs/specs/avatar/avatar-editor.md` is now a
specific specification for the monster-GLB workflow. It defines the problem as
two transform authorities being applied to one custom model and makes the
desired behavior explicit: custom GLBs use Blender-authored transforms by
default; the old correction layer is available only as explicit
`body_transform_source=legacy_overrides`.

`docs/specs/debug-logging/AI-fix-live.md` was generalized from a weapon-equip
procedure into a reusable evidence-first procedure for editor, gameplay,
physics, damage-volume, network, audio, configuration, and presentation
issues. It now requires the real scenario, fresh executable identity, active
canonical `events.jsonl`, first-divergence tracing, and separated build/runtime
/human-acceptance claims.

## Implementation

- `src/avatar/avatar.h:128-136`: added `AvatarDefinition::bodyTransformSource`.
- `src/avatar/avatar.cpp:384-388,476-483,657-664`: serializes, parses, and
  validates `body_transform_source`.
- `src/avatar/avatar.cpp:1039-1069`: accepts `plrOrigin` plus a numeric Blender
  suffix and validates it together with the six required body nodes.
- `src/avatar/avatar.cpp:1089-1127`: selects model-authored versus legacy
  transforms, carries the policy into model loading, and emits the bounded
  `avatar.model.transforms` structured event.
- `src/avatar/avatar.cpp:825-850`: selecting a custom GLB sets `model`; removing
  it returns to the default body's legacy correction policy.
- `src/entities/player-loader.cpp:24-30,187-200`: added the model-transform
  bypass so custom authored rest transforms are not overwritten by
  `config/bodyparts.json` or avatar overrides.
- `src/entities/player-loader.cpp:565-658,816-819,917-925`: carried the policy
  through cached, synchronous, headless, and asynchronous model-load paths.
- `src/entities/player.h:168-172`: stored the policy in `PendingPlayerModel`.
- `src/ragdoll/ragdoll-mode.cpp:102-111`: accepts suffixed authored root names
  in hybrid bind diagnostics.
- `assets/avatars/monstertest1/avatar.json:42`: explicitly identifies the
  current monster test avatar as `body_transform_source=model`.

## Exact behavior change

Before: a custom GLB could receive the default-body transform configuration and
avatar bodypart overrides after its Blender-authored node transforms loaded.

After: a custom GLB selected with `model` keeps its authored rest transforms and
reports `override_count=0`. The default body still uses the existing legacy
configuration. Old correction behavior remains available only through the
explicit `legacy_overrides` source.

## Routed documents and focused review

- `AGENTS.md`
- `docs/ROUTER.md`
- `docs/specs/avatar/avatar-editor.md`
- `src/avatar/DESIGN.md`
- `docs/specs/debug-logging/debug-logging.md`
- `docs/specs/debug-logging/canonical-jsonl.md`
- `docs/specs/debug-logging/AI-fix-live.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/skills/documentation-checker-v1.md`
- `docs/skills/logging-checker-v1.md`
- `docs/workflows/runtime-scenario-validation.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/gold/2026-10-06-avatar-bind-anchors-and-jsonl-evidence.md`

Specification/behavior review: PASS for the defined transform ownership;
runtime and visual acceptance remain open.

Documentation review: PASS for purpose, audience, does-not sections, explicit
contracts, and acceptance criteria. The related `docs/specs/entity-editor/`
specification referenced by the avatar document is not present in this
checkout, so the relationship is marked future/conditional rather than treated
as an authority.

Logging review: PASS_WITH_HUMAN_REVIEW. The new event uses the existing Avatar
StructuredLogger category and canonical stream; it is emitted at model
application rather than per render frame.

## Pre-existing edits

The worktree already contained modified configuration files and untracked
monster/avatar assets, including `config/accounts/default.json`,
`config/analytics.json`, `config/audio/music-settings.json`,
`config/gui/menu-avatar-preview.json`, monster GLBs, and avatar folders. They
were preserved. The requested `monstertest1/avatar.json` change is included in
this session's scope; unrelated asset contents were not rewritten.

## Validation

- `git diff --check`: passed; only normal LF/CRLF conversion warnings were
  reported for changed text files.
- `assets/avatars/monstertest1/avatar.json`: parsed successfully.
- `python build_agent.py`: `BUILD SUCCESS`, `Compiled: 2`, `Skipped: 547`,
  `Status: SUCCESS`, duration `10.74s` in `build/changelog.txt`.
- The rebuilt units included `src/avatar/avatar.cpp` and
  `src/entities/player-loader.cpp`; the link completed successfully.
- The existing dev loop reported `source_changed`, `running_build=1`, and
  continued serving old build `2060` from run
  `20261011_203523`. A second loop was refused by the existing lock, so no
  competing loop or process was launched.

## Runtime and human review still required

Runtime proof is not claimed. The current dev-loop daemon is stuck with
`latest_successful_source_generation=0` while the repository is at generation
6, and its journal remains the old run:

`C:\mimita-v9\logs\2026-10-11\20261011_203523\events.jsonl`

The next acceptance run must use a fresh dev-loop executable and verify
`avatar.model.transforms` for `monstertest1`, including
`transform_source=model`, `custom_model=true`, and `override_count=0`, then
visually compare the editor and gameplay monster placement with Blender. Hybrid
and ragdoll visual acceptance remains separate.

## Existing documentation TODOs observed

The repository-wide TODO scan found pre-existing informal notes, including
`docs/specs/debug-logging/debug-logging.md:1` (“todo - need to make size limit
bc we keephaving std alloc memory error”), `docs/specs/weapons/weapons.md:1`
and `:10`, `docs/specs/replay/replay.md:1`, `docs/specs/visuals/visuals.md:1`,
and `docs/architecture/player-npc-systems/player-npc-systems.md:1`. They were
not edited because this task was scoped to the avatar editor and live-fix
workflow; each should be expanded in its owning task.
