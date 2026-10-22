# Gameplay presentation and parallel work issue intake

- Time: 2026-10-22T17:15:28Z (2026-10-22 13:15:28 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at start: `853366427f4285e43a21005e82838f000260668d`
- Result: `PASS_WITH_HUMAN_REVIEW` — both requested ideas are in the open gameplay issues inbox; design and workflow decisions remain open.

## Scope and source state

Added two user-reported items to `docs/specs/open-gameplay-issues.md` and its
issue index. The first records the need to locate enemies (especially the
Juggernaut) and recognize when damage registers. Blood, realistic-looking hit
effects, in-world damage numbers, maps, and movement are captured as options
to evaluate, not final choices. The existing blood/world-impact mark issue
continues to own effect consistency and image-shape concerns.

The second records the wish to write specifications, create Blender assets,
make sounds, and do other content work while AI codes. The existing live
collaboration feature record notes that non-code resource transfer and
simultaneous-edit acceptance remain integration work. This intake does not
claim those capabilities are available or decide their implementation.

At task start, the unrelated modified files were `config/accounts/default.json`,
`config/analytics.json`, and `config/audio/music-settings.json`; they were
preserved. No other files were changed by this work before the issue document
and this changelog.

## Exact change

File: `docs/specs/open-gameplay-issues.md`

Intake table, lines 107–108. Before, the Role row was followed by the existing
JSON configuration issue:

```markdown
| JSON configuration range and error feedback | The player wants JSON settings to accept very large values (example: `-9999999999999999999999999999` through `999999999999999999999999999999999`) and wants invalid or unsupported values to be obvious, including which file and setting caused the problem. | Open design issue. Decide safe per-setting numeric ranges and whether out-of-range values are accepted, clamped, or rejected while preserving the last valid configuration. Record a categorized error in `events.jsonl` with the config path, field, and reason, then surface a clear, deduplicated in-game tip through the existing tips presentation path. Exact wording, severity, and repeat/throttle behavior remain open. | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
```

After, two new rows were inserted before that existing row (currently lines
107–108):

```markdown
| Enemy readability and hit confirmation | Make enemies easy to locate, especially the Juggernaut, and make it clear whether attacks actually damage them. The player is considering more realistic hit effects, blood, or in-world damage numbers, and wonders whether map or movement changes could help. | Open presentation/design issue. First make enemy location and confirmed damage understandable; decide which effects communicate a registered hit without obscuring play. Blood, damage numbers, other effects, and map or movement changes are options to evaluate, not selected solutions. The existing blood/world-impact issue separately tracks effect consistency and image shape. | [`graphics/graphics-presentation.md`](graphics/graphics-presentation.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`weapons/weapons.md`](weapons/weapons.md), [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |
| Parallel human creation alongside AI coding | While AI works on code, the player wants productive work such as writing specifications, creating Blender assets, making sounds, and other content. | Workflow/productivity request. Define how the player and AI can identify independent work, keep ownership and feedback clear, and bring the results together. The workflow, tools, and conflict handling remain open; this request does not assume simultaneous edits to the same files are safe. | [`../features/live-code-development/live-collaboration-feature.md`](../features/live-code-development/live-collaboration-feature.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md), [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md) |
```

Issue index, currently lines 117–118, before (the following existing row):

```markdown
| Player jitters far from map geometry and NPCs remain embedded in geometry | User reports both symptoms; cause and whether they share an owner are unconfirmed. NPCs should be moved back out to clear space. | [`../architecture/collision/collision.md`](../architecture/collision/collision.md), [`movement/movement.md`](movement/movement.md), [`networking/networking.md`](networking/networking.md), [`../architecture/player-npc-systems/npc-movement.md`](../architecture/player-npc-systems/npc-movement.md) |
```

After, the index now includes these two rows before that existing row:

```markdown
| Enemies are hard to locate and confirmed damage is unclear, especially against the Juggernaut | User wants clearer enemy presentation and visible proof when damage registers; blood, in-world numbers, other effects, and map or movement changes are options for discussion | Open design issue; the feedback approach and whether map or movement changes are needed remain undecided. Existing blood/world-impact consistency and shape tracking stays a separate issue. | [`graphics/graphics-presentation.md`](graphics/graphics-presentation.md), [`gamemodes/juggernaut.md`](gamemodes/juggernaut.md), [`weapons/weapons.md`](weapons/weapons.md), [`visuals/blood-bulletholes-viscera.md`](visuals/blood-bulletholes-viscera.md) |
| Player wants to create specs, Blender assets, sounds, and other content while AI codes | Workflow request for useful parallel human/AI work with clear task ownership and a way to integrate results | Workflow, tools, and conflict handling remain open; no implementation or collaboration mechanism is claimed here. | [`../features/live-code-development/live-collaboration-feature.md`](../features/live-code-development/live-collaboration-feature.md), [`../operations/asset-management/asset-management.md`](../operations/asset-management/asset-management.md), [`animations/blender-animation-authoring.md`](animations/blender-animation-authoring.md), [`../architecture/live-development/hot-audio-contract.md`](../architecture/live-development/hot-audio-contract.md) |
```

No prior text existed for either new issue row. The existing JSON settings and
NPC geometry entries were retained.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`,
  `docs/specs/graphics/graphics-presentation.md`,
  `docs/operations/asset-management/asset-management.md`,
  `docs/features/live-code-development/live-collaboration-feature.md`,
  `docs/specs/animations/blender-animation-authoring.md`,
  `docs/architecture/live-development/hot-audio-contract.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`, and
  `docs/skills/documentation-checker-v1.md`.
- The router requests `docs/doc-review-09-03-2026.md`; it is absent in this
  checkout. The collaboration feature record says transferring non-code
  resources and simultaneous-edit acceptance remain integration work.
- Documentation Checker: `PASS_WITH_HUMAN_REVIEW`. All owner links on the two
  new issue rows resolve. The whole-docs TODO scan found existing notes,
  including `docs/specs/visuals/blood-bulletholes-viscera.md:2`,
  `jorj todo explain`, and `docs/specs/weapons/weapons.md:74`,
  `2026 10 20 1655 jorj todo explain`. Suggested expansions: explain the
  intended blood/world-mark visual style and readability goals; explain the
  weapon specification's organization and relationship among shared and
  weapon-specific behavior. These notes were not edited.

## Validation and review

- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed. Git printed
  only its existing LF-to-CRLF conversion warning.
- Confirmed all eight owner links introduced by the intake rows resolve to
  existing files.
- Documentation only. No tests, build, runtime, or gameplay visual review
  were performed or claimed.
- Human design review remains open for the best enemy visibility and hit
  confirmation feedback, and for the task-coordination/integration workflow
  that supports parallel human asset/spec work and AI coding.
