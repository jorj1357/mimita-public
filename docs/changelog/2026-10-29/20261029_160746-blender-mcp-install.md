# Blender MCP installation

- Result: `PASS_WITH_HUMAN_REVIEW` for source installation and marketplace registration; Blender/Codex connection remains for user activation.
- Time: `2026-10-29T16:07:46Z` (2026-10-29 12:07:46 EDT, America/New_York).
- Repository branch and commit: `20261011zt`, `37639fc6afc0cc2362dfbcc2c0fd56d4dd7aebc5`.
- Upstream: `https://github.com/ahujasid/mcp-for-blender`, checked out at `992233bb258975a05eec8b5691465c50c2831e34` (`2026-10-29`, `docs: add badge to README`). The existing root tree gitlink for `devscripts/blender-ai` already pointed to this commit. `git submodule status` reports no mapping in `.gitmodules`; this pre-existing metadata was left untouched.

## Scope and changes

- Cloned upstream into `devscripts/blender-ai`, the user-requested location. No game code or Blender project file was changed.
- Registered `devscripts/blender-ai/integrations/codex` as the local Codex plugin marketplace using `codex plugin marketplace add`.
- Updated the Blender 5.0 user add-on at `%APPDATA%\Blender Foundation\Blender\5.0\scripts\addons\blender_mcp.py` with `uvx --from mcp-for-blender mcp-for-blender install-addon`. The installer retained the prior file as `blender_mcp.py.bak`.
- In `devscripts/blender-ai/integrations/codex/plugins/mcp-for-blender/.mcp.json`, added `BLENDER_MCP_SAFE_MODE=1` and `DISABLE_TELEMETRY=true` to the server environment (the `env` object at lines 13–18). This enables script checks and disables the project's anonymous usage telemetry. Existing app/form flags remain unchanged.
- Prior to work, the root checkout already had modified `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/devtimertips.json`, `config/weaponcollisions.json`, `config/weapons/mace/config.json`, `config/weapons/swordsword/config.json`, `docs/specs/gamemodes/zombie-tower/zombie-tower.md`, `docs/specs/lore/lore.md`, `docs/specs/open-gameplay-issues.md`, and `docs/specs/weapons/melee/melee-weapons.md`, plus untracked `docs/specs/gamemodes/madness-combat.md`. These unrelated edits were preserved.

## Reasoning and validation

- Upstream documents a Blender add-on that hosts a localhost socket server and an MCP server that connects Codex to it. The integration can run Blender Python, so safe mode is enabled and the socket should remain on localhost.
- Parsed the edited `.mcp.json` successfully and verified the configured safe-mode and telemetry values.
- The Codex CLI confirmed marketplace registration. `uvx` found the package, and the add-on installer confirmed its Blender 5.0 destination. The old add-on backup exists.
- `codex mcp list` could not load `%USERPROFILE%\.codex\config.toml`: line 5 sets `service_tier = "default"`, but this CLI accepts only `fast` or `flex`. This existing user setting was not changed. A direct standalone MCP server entry was therefore not added or verified.

## Human activation still required

Blender was open during installation. It has not been restarted, and the new add-on has not been enabled or started. Restart Blender, enable **Interface: MCP for Blender** in Preferences, then start the server from the 3D View sidebar's **MCP for Blender** tab. In Codex, install **MCP for Blender** from the newly registered local marketplace and restart Codex. The add-on has not connected in a live session, and no `.blend` content or viewport result was reviewed.

## Documents and skills consulted

- `docs/ROUTER.md`
- `docs/operations/asset-management/asset-management.md`
- `docs/skills/asset-checker-v1.md`
- `docs/architecture/time-and-formatting/time-and-formatting.md`
- `docs/operations/task-completion/task-completion.md`
- Computer-use skill: `C:/Users/guita/.codex/plugins/cache/openai-bundled/computer-use/26.1021.31345/skills/computer-use/SKILL.md`, plus `docs/guidance.md` and `docs/confirmations.md`.

No gameplay tests, builds, or Blender scene edits were applicable or performed.
