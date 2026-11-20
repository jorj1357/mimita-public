// 2026-09-06 12:00 EST
/* purpose
* provide one consistent raw-evidence template for AI session changelogs
* preserve branch, commit, timestamp, path, line, and validation facts
* connect work to specifications, skills, tests, logs, features, and regressions
* this file DOES NOT replace the feature record
* this file DOES NOT rewrite historical changelogs
* this file DOES NOT claim human acceptance that did not happen
*/

# Arcade collision defaults on for all gamemodes

# Task

- Task ID: session-arcade-collision-default
- Session ID: session-arcade-collision-default
- Owner: arcade-collision-default
- Focus: arcade collision mode default
- Summary: Changed the gamemode and collision-runtime defaults so arcade capsule collision is on unless a mode explicitly sets `arcade_collision: false`. Added the explicit `true` value to every mode JSON so the existing complete-file contract stays valid. Regenerated the stale packet hotness ledger that was blocking the root build, then proved the result with pinned TDM run `20261120_064201`.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-11-20T06:44:50+00:00
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Base commit: `ce0009d5b2983f462d1d17dd640c4e66769c2a23`
- Final commit: `ce0009d5b2983f462d1d17dd640c4e66769c2a23` (no commit created)

# Pre-existing changes

- Exact status output: concurrent work remains present in `AGENTS.md`, dashboard/dev-loop scripts, coordination docs, contact specs, AI/controller code, render code, and other agents' changelogs/issues. None of those were reverted.
- Files not created or modified by this session: all concurrent paths above; only the paths listed under “Files” were changed by this session.

# Requested behavior

> yes, make arcade collision the default for all modes for now and then the other mode is opt in

# Specification alignment

- Current specification paths: `config/gamemodes/masterlist/gamemode-masterlist.json`, `docs/architecture/collision/collision.md`
- Exact requirements: Mode files are complete canonical documents. The arcade collision feature is parsed from `features.arcade_collision`. Arcade collision uses capsule actor/world and actor-pair collision and skips limb/weapon collision passes.
- Why the change follows the specification: The mode feature default now matches the requested policy, and each complete mode file declares the same explicit policy. Non-arcade collision remains reachable by setting `false`.
- Conflicts or decisions: This changes the default for every current mode, not only TDM. It does not remove the non-arcade code path.

# Exact implementation changes

## File: `src/gamemode/gamemode.h`

- `GamemodeFeatures::arcadeCollision` now defaults to `true`.
- The comment documents `features.arcade_collision=false` as the explicit non-arcade opt-in.

## File: `src/config/collision-config.h`

- `CollisionConfig::mArcadeCollision` now defaults to `true`.
- The comment documents that the active mode still owns the runtime value.

## File: `config/gamemodes/masterlist/gamemode-masterlist.json`

- `features.arcade_collision.default` changed from `false` to `true`.
- The description now states the default and the explicit opt-out.

## Files: `config/gamemodes/*.json`

- Added `"arcade_collision": true` to every mode file that did not already declare it:
  `bloodtest`, `bombtag`, `connected_world`, `counterstrike`, `duel`,
  `elimination`, `ffa`, `funcounterstrike`, `jugger_horde`,
  `juggernaut_duel`, `npc_waves`, `payload`, `retrograd`, `rocket_duel`,
  `sandbox`, `ssbm`, `survive_disasters`, `tdm`, `vehicle_arena`,
  `vs_jason`, `zombie_tower`.
- `juggernaut.json` already had `true`; it remains unchanged.

## File: `config/hot-reload/packet-ledger.json`

- Regenerated with `python devscripts/packet_registry_codegen.py` because the
  root build preflight rejected the prior generated ledger as stale. This is
  generated housekeeping, not an arcade behavior change.

# Validation and evidence

- Source/default checks:
  - `python devscripts/gamemode_json_sync.py --check` -> `[GAMEMODE SYNC] PASS`.
  - `python devscripts/packet_registry_codegen.py --check` -> `ok packets=78`.
  - `python devscripts/config_selftest.py` still exits `FAIL` because of
    pre-existing avatar validation errors; its gamemode section reports
    `gamemodes: 22`, with no `features.arcade_collision` errors.
- Build:
  - Root dev-loop published game DLL generation `396`
    (`build/game/generation-396/game-g396.dll`,
    SHA-256 `cf54c0d4721efb72f93ffebceac8ce8f55c08ee35aa168034f83f20a1450160c`).
  - The first attempted build failed on the stale packet ledger; after
    regeneration, a later retry reported `disk_low` before the eventual
    generation 396 publication.
- Runtime:
  - Pinned TDM run `20261120_064201` used published EXE artifact
    `2026-11-20T05:38:11.772319Z` and game generation `396`.
  - Server journal:
    `logs/2026-11-20/20261120_064201/server-01/events.jsonl`.
  - No `config.gamemode.invalid` events.
  - One `gamemode.match.configured` event for `mode=tdm`.
  - 79 `collision.bounce.applied` events; every captured event had an empty
    `actor_part`, no `"weapon_contact":true`, and no actor part label. This is
    the capsule/root collision path, not the limb/weapon path.
  - The isolated run was stopped with verified PIDs only.
- Issue proof:
  - `issue-56f7a692b646` closed with integration evidence
    `evidence-9a4ba45b64b9` against generation 396 and run `20261120_064201`.
  - Task proof `task-proof-f706cdcf7aef` verified against the same journal.
- Visual acceptance: not performed; no claim of visual or feel acceptance.

# Behavior and acceptance

- Arcade collision is now the default for current modes by three layers:
  source default, runtime collision default, and explicit mode data.
- A mode that needs non-arcade limb/weapon collision can opt in with
  `"arcade_collision": false`.
- Build and runtime evidence prove TDM loaded the policy and produced
  capsule/root contacts. Human gameplay acceptance remains pending.

# Files

- `src/gamemode/gamemode.h`
- `src/config/collision-config.h`
- `config/gamemodes/masterlist/gamemode-masterlist.json`
- `config/gamemodes/bloodtest.json`
- `config/gamemodes/bombtag.json`
- `config/gamemodes/connected_world.json`
- `config/gamemodes/counterstrike.json`
- `config/gamemodes/duel.json`
- `config/gamemodes/elimination.json`
- `config/gamemodes/ffa.json`
- `config/gamemodes/funcounterstrike.json`
- `config/gamemodes/jugger_horde.json`
- `config/gamemodes/juggernaut_duel.json`
- `config/gamemodes/npc_waves.json`
- `config/gamemodes/payload.json`
- `config/gamemodes/retrograd.json`
- `config/gamemodes/rocket_duel.json`
- `config/gamemodes/sandbox.json`
- `config/gamemodes/ssbm.json`
- `config/gamemodes/survive_disasters.json`
- `config/gamemodes/tdm.json`
- `config/gamemodes/vehicle_arena.json`
- `config/gamemodes/vs_jason.json`
- `config/gamemodes/zombie_tower.json`
- `config/hot-reload/packet-ledger.json`

# Status

- Source: implemented.
- Build: generation 396 published.
- Runtime: TDM pinned run proved capsule/root contact path and no invalid
  arcade config.
- Human review: pending; do not describe this as visual or gameplay-accepted.
