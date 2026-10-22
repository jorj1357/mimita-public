# Sandbox `nervousbad` NPC Profile

- Task: Make Sandbox resolve its `nervousbad` behavior profile and tune it to be less accurate, slower to react, less aggressive, and less likely to pursue hidden targets.
- Result: `PASS_WITH_HUMAN_REVIEW`
- Time: `2026-10-22T01:44:50Z` (Eastern display: `2026-10-21 21:44:50 EDT`)
- Branch: `20261011zt`
- HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a` (no commit created)

## Pre-existing workspace changes

At task start, unrelated modified files were present in `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/gamemodes/npc_waves.json`, `config/healthbar.json`, `config/npc-difficulty.json`, `config/playervisuals.json`, `src/config/player-visuals-config.cpp`, `src/config/player-visuals-config.h`, `src/engine/engine-tick-ui-game-hud.cpp`, `src/entities/player-render.cpp`, `src/entities/player.h`, `src/network/multiplayer-reconcile.cpp`, `src/network/multiplayer-tick.cpp`, `src/network/server-damage.cpp`, `src/network/server-gamemode.cpp`, `src/network/server-players.cpp`, and `src/render/render-player.cpp`. The existing `docs/changelog/2026-10-22/` directory also contained the earlier Sandbox friendly-fire record. Those changes were preserved.

At task start, `config/gamemodes/sandbox.json` already selected `npc_behavior_profile: nervousbad`, but its team `role` was also set to `nervousbad`. Friendly fire was already enabled. This task kept both settings and corrected only the invalid role ID plus its explanatory comment.

Additional unrelated modified files appeared later in the worktree during this task: `src/engine/engine-tick-setup.cpp`, `src/render/dynamic-light.cpp`, and `src/render/dynamic-light.h`. They were left untouched.

## Changes

### `config/gamemodes/sandbox.json`

- Before at the team role: `// "role": "juggernaut_fighter",` followed by `"role": "nervousbad",`.
- After at lines 20-21: `// Role IDs come from config/roles.json; nervousbad is a behavior profile.` followed by `"role": "sandbox_juggernaut_npc",`.
- Reason: `nervousbad` exists in `config/behavior-profiles.json`, but not as a role in `config/roles.json`. The existing `sandbox_juggernaut_npc` role is valid. The gamemode resolver gives the NPC-only `npc_behavior_profile` precedence over a role's optional behavior profile, so the mode setting remains the right place to choose `nervousbad`.

### `config/behavior-profiles.json`

- The `nervousbad` entry at lines 502-539 previously used `repath_interval_seconds: 99.9`, `goal_move_threshold_meters: 299.5`, commitment values `direction_commit_seconds: 0.01`, `progress_check_seconds: 99.0`, and `minimum_progress_meters: 110.5`, plus `recent_path_avoid_radius: 0.01` and `reverse_penalty: 0.01`. Those movement values exceeded parser bounds and were silently clamped.
- New lines 504-519 use valid, deliberate movement values: 8-second repathing, a 10-meter goal-change threshold, 12-second direction commitment, 5-second progress checks, a 2-meter progress target, 5-meter recent-path avoidance, no combat-movement override for visible enemies, and a normal reverse penalty.
- Before/after combat tuning: `reaction_delay` `0.25` → `1.5`; `fire_cadence_multiplier` `0.01` → `0.25`; `aggression` `0.01` → `0.05`; `preferred_range` `18.0` → `30.0`; `target_stickiness` `0.05` → `0.8`; `target_switch_threshold` `0.03` → `1.0`. The existing `aim_error_deg: 82.0` and forget-on-hidden pursuit settings remain in place.
- Reason: express weak, hesitant combat and less reactive movement using in-range settings rather than relying on parser clamping or values that can prevent sensible replanning.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/terminal-commands/terminal-commands.md`, and `docs/operations/task-completion/task-completion.md`.
- Reviewed prior records `docs/changelog/2026-10-03/20261003_193100-sandbox-npc-behavior-profile.md` and `docs/changelog/2026-10-20/20261020_200448-sandbox-role-movement-runtime-fix.md`. The router-referenced `docs/regressions/regressions-v1.md` is absent in this checkout; no matching `nervousbad` regression record was found.
- Source review confirmed mode profile precedence over role behavior for NPCs and that invalid/unknown roles cannot supply the Sandbox actor's role-owned data.

## Validation and remaining evidence

- Repository JSONC parsing confirmed that Sandbox selects `nervousbad`, `sandbox_juggernaut_npc` resolves to an existing role, and the edited profile values load within the movement parser's clamp ranges.
- `git diff --check -- config/gamemodes/sandbox.json config/behavior-profiles.json` passed.
- No code changed, so no build was run. No live Sandbox scenario was run. The dev-loop status showed a live client in the unrelated `duel_presentation` launch mode, with its server process not alive; that session was left undisturbed. The active Sandbox profile and its visible passivity still need confirmation in a fresh Sandbox runtime.
