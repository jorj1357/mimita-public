# Role and Actor-Preset Clarity Intake

- Task: Record the player's request for a simpler explanation of `roles.json` and actor presets in the open gameplay issues inbox.
- Result: `PASS`
- Time: `2026-10-22T01:48:55Z` (Eastern display: `2026-10-21 21:48:55 EDT`)
- Branch: `20261011zt`
- HEAD: `9ec73c3ae724037da33d032d54843cbf1186f97a` (no commit created)

## Pre-existing workspace changes

The worktree already contained unrelated edits to `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, `config/gamemodes/npc_waves.json`, `config/gamemodes/sandbox.json`, `config/healthbar.json`, `config/npc-difficulty.json`, `config/playervisuals.json`, `src/config/player-visuals-config.cpp`, `src/config/player-visuals-config.h`, `src/engine/engine-tick-ui-game-hud.cpp`, `src/entities/player-render.cpp`, `src/entities/player.h`, `src/network/multiplayer-reconcile.cpp`, `src/network/multiplayer-tick.cpp`, `src/network/server-damage.cpp`, `src/network/server-gamemode.cpp`, `src/network/server-players.cpp`, and `src/render/render-player.cpp`. The existing changelog folder also contained records from earlier tasks. They were left untouched.

During this task, additional unrelated edits appeared in `src/engine/engine-tick-setup.cpp`, `src/render/dynamic-light.cpp`, `src/render/dynamic-light.h`, and `config/tips.json`; they were also left untouched.

## Change

### `docs/specs/open-gameplay-issues.md`

- Before at line 97: `## Issue index`; there was no dated 2026-10-22 entry about role/preset configuration clarity.
- After at lines 97-106: added a dated intake recording that the player has trouble understanding `config/roles.json` and actor presets, wants a simpler setup explanation, and needs a plain-language guide with a concrete Sandbox NPC example. The entry leaves any structural/configuration redesign undecided.
- After at line 111: added the same issue to the summary index, linked to the Actor Presets, Gamemodes, and NPC Behavior owners.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/skills/documentation-checker-v1.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, and `docs/operations/task-completion/task-completion.md`.
- Confirmed all three owner links in the new issue entry resolve to existing documents. `git diff --check -- docs/specs/open-gameplay-issues.md` passed; Git printed only the existing LF-to-CRLF conversion warning.
- The router-referenced `docs/doc-review-09-03-2026.md` is absent from this checkout.
- Related existing TODO at `docs/specs/actor-presets/actor-presets.md:251`: `2026 10 19 1834 jroj todo explain this stuff is so cool like`. Suggested expansion: plainly explain the role → actor-preset relationship with one minimal Sandbox example, including which file to edit for role selection versus reusable actor setup. This TODO was not edited.
- No tests, build, runtime scenario, or human acceptance were needed or performed; this was an issue-inbox documentation update only.
