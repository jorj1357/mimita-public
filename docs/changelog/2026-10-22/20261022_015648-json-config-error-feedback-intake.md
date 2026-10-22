# JSON config range and error feedback issue intake

- Time: 2026-10-22T01:56:48Z (2026-10-21 21:56:48 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD at start: `9ec73c3ae724037da33d032d54843cbf1186f97a`
- Result: `PASS_WITH_HUMAN_REVIEW` — recorded the idea as an open issue; policy decisions remain open.

## Scope and source state

Added a note to `docs/specs/open-gameplay-issues.md` only. No parser, tip, log,
or runtime behavior was changed. The request for large numeric values is
recorded as an example (`-9999999` through `9999`), not a blanket promise that
every setting can safely accept those values. The issue asks for field-specific
range decisions, clear reject/clamp behavior, a categorized `events.jsonl`
record naming the config path and field, and a deduplicated player-facing tip
through the existing tips presentation path.

The issue file already had a user-authored role/actor-preset clarity intake
before this change. It was preserved. Other pre-existing worktree edits were
preserved in `config/analytics.json`, `config/behavior-profiles.json`,
`config/gamemodes/npc_waves.json`, `config/gamemodes/sandbox.json`,
`config/healthbar.json`, `config/npc-difficulty.json`,
`config/playervisuals.json`, `config/tips.json`,
`src/config/player-visuals-config.cpp`, `src/config/player-visuals-config.h`,
`src/engine/engine-tick-setup.cpp`, `src/engine/engine-tick-ui-game-hud.cpp`,
`src/entities/player-render.cpp`, `src/entities/player.h`,
`src/network/multiplayer-reconcile.cpp`, `src/network/multiplayer-tick.cpp`,
`src/network/server-damage.cpp`, `src/network/server-gamemode.cpp`,
`src/network/server-players.cpp`, `src/render/dynamic-light.cpp`,
`src/render/dynamic-light.h`, and `src/render/render-player.cpp`.

## Exact change

File: `docs/specs/open-gameplay-issues.md`

Intake table, immediately after the existing Role and actor-preset setup row:

Before (existing following section marker):

```markdown
## Issue index
```

After (new line inserted before that marker):

```markdown
| JSON configuration range and error feedback | The player wants JSON settings to accept very large values (example: `-9999999` through `9999`) and wants invalid or unsupported values to be obvious, including which file and setting caused the problem. | Open design issue. Decide safe per-setting numeric ranges and whether out-of-range values are accepted, clamped, or rejected while preserving the last valid configuration. Record a categorized error in `events.jsonl` with the config path, field, and reason, then surface a clear, deduplicated in-game tip through the existing tips presentation path. Exact wording, severity, and repeat/throttle behavior remain open. | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
```

Issue index, immediately after its existing Role and actor-preset setup row:

Before (existing following row):

```markdown
| Griever behavior still does not fully match intent: server position errors, hovering, and a report that it rolls but does not bounce enough | Prior reports and an attempt note exist; current root cause and latest runtime result are not established in this document | [`griever-monster/griever-monster.md`](griever-monster/griever-monster.md), [`movement/movement.md`](movement/movement.md), [`../architecture/collision/collision.md`](../architecture/collision/collision.md) |
```

After (new line inserted before it):

```markdown
| JSON settings reject or obscure very large or invalid values | User wants broad numeric support, a precise logged error, and a visible in-game tip naming the file and setting; safe per-setting limits and reject/clamp behavior are undecided | [`../architecture/json-configuration/json-configuration.md`](../architecture/json-configuration/json-configuration.md), [`debug-logging/debug-logging.md`](debug-logging/debug-logging.md), [`gui/gui-general.md`](gui/gui-general.md) |
```

No prior text existed for the new rows. The document's pre-existing role and
actor-preset row was retained verbatim.

## Documents and focused review

- Read `docs/ROUTER.md`, `docs/skills/documentation-checker-v1.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`, and the relevant
  open-issues and debug-logging documents.
- The router requests `docs/doc-review-09-03-2026.md`, but that file is absent
  in this checkout.
- Documentation Checker: `PASS_WITH_HUMAN_REVIEW`. The three links on the new
  row resolve to current files. Existing TODO notes remain outside this edit;
  in the touched issue file, lines 6 and 23 contain historical TODO text. The
  related visual direction TODO is `docs/specs/visuals/visuals.md:1`:
  `2026-09-07T18:40:16Z jorj - todo explain this  more like`. Suggested
  expansion: describe the intended visual direction, readability goals, and
  open presentation choices. No TODO text was edited.

## Validation and review

- `git diff --check -- docs/specs/open-gameplay-issues.md`: passed.
- Confirmed the three new owner links resolve to files.
- No tests or build were run; this was a documentation-only change.
- Human review is still needed to decide safe per-setting numeric ranges,
  reject versus clamp behavior, tip wording/severity, and duplicate suppression.
- No runtime behavior or human gameplay acceptance is claimed.

Changelog file itself: no prior file existed. Added title:

```markdown
# JSON config range and error feedback issue intake
```
