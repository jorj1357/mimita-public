# Own healthbar visibility and alpha

- Timestamp: 2026-10-01T23:06:12Z (America/New_York display context)
- Branch: `afad20a-rebuild`
- Result: PASS_WITH_HUMAN_REVIEW

## Change

Added two hot-reloadable healthbar settings while preserving the existing behavior by default:

- `config/healthbar.json:13` — `"show_own_healthbar": true`
- `config/healthbar.json:14` — `"own_healthbar_alpha": 1.0`

The config owner now loads and saves both fields in `src/gui/hud/healthbar-config.cpp:68-69` and `:104-105`, with `own_healthbar_alpha` clamped to the inclusive `0.0` to `1.0` range.

`src/gui/hud/player-nameplates.cpp:169-176` skips only the local player's healthbar when the toggle is false. At `:233-237`, the alpha scales the local player's healthbar presentation linearly; remote/NPC healthbars are unchanged.

## Authority and review

Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gui/guiv2.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/architecture/live-development/live-development.md`, and `docs/operations/task-completion/task-completion.md`.

The GUI specification requires presentation to be JSON-defined and hot reloadable. The existing `HealthbarConfig` loader/reload owner was reused; no second config path or renderer was added.

## Validation

- `git diff --check`: touched files clean. Pre-existing trailing whitespace remains in `docs/specs/20261001plan.md` and was not changed.
- Config keys, loader fields, save fields, local-player toggle path, and alpha path were inspected with exact line references above.
- Alpha input is clamped by the existing `read01` helper, so `0.0` is invisible and `1.0` is fully visible.
- Human visual acceptance was not performed.
- The documented `python devscripts/live-build.py` path was attempted but this checkout has no `devscripts/live-build.py`. Two existing MiMITA processes remained running and were not stopped, restarted, relinked, or replaced. No cold build was run.

## Pre-existing work

The worktree contained unrelated modified config, physics, impact, regression, planning, and changelog files before this session. They were preserved and not claimed as part of this change.
