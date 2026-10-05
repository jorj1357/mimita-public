2026-10-05T01:13:00Z

# Mode Packs and Disaster Runtime

## Purpose

Let a validated JSON mode pack define a game mode and its disasters so shared
server code does not branch on mode names. The first user-visible result is a
selectable `survive_disasters` mode running `random_weapon_last_alive`.

## Desired behavior

- A mode pack under `config/mode-packs/` is discovered, validated, and loaded
  atomically; a bad pack keeps the last valid catalog.
- `survive_disasters` appears in the existing mode dropdown and runs the shared
  lifecycle: 15s intermission, 3-2-1 countdown, GO, active, results.
- Each participant receives a deterministic random weapon from the replicated
  round seed. No respawns. Last actor alive wins; a bounded duration resolves a
  deterministic timeout winner.
- Clients display the disaster name/description from replicated state.

## Current behavior

See the changelog for the milestone-1 evidence. The mode is selectable and the
server/branch-free runtime is implemented; live multiplayer and visual
acceptance are still pending human review.

## Current status

implemented (milestone 1) as of 2026-10-05T01:13:00Z.

## Decisions

- Only the new mode must be branch-free in M1; legacy modes keep their branches.
- Disaster audio uses existing `playEventSound` primitives; the hot-audio recipe
  layer is deferred.
- Disaster state uses a dedicated `DisasterStatePacket`, not `DuelStatePacket`.
- Packs live in `config/mode-packs/`; `onlinemodes.json` + `config/gamemodes/`
  remain compatibility inputs.
- Win policy is a generic `win_condition` string (`last_actor_alive`), not a
  mode name.

## Ownership

- Primary code owner: `src/gamemode/mode-pack-registry.*`,
  `src/gamemode/action-graph.*`, `src/gamemode/capability-registry.*`,
  `src/gamemode/disaster-runtime.*`
- Runtime/event owner: `src/network/server-gamemode.cpp` (`serverCommunityStartMatch`,
  the shared state machine, `broadcastDuelState`)
- Configuration owner: `config/mode-packs/`, `config/gamemodes/`,
  `config/onlinemodes.json`, `config/gui/gamemode-meta-gui.json`
- Network owner: `src/network/packets.h`, `src/network/community-match-client.*`,
  `src/network/multiplayer-tick.cpp`
- Animation/physics owner: none (reuses shared systems)

## Related authoritative documents

- Specification: `docs/specs/gamemodes/mode-packs.md`,
  `docs/specs/gamemodes/gamemodes.md`
- Architecture: `docs/architecture/json-configuration/json-configuration.md`
- Workflow: `docs/RUTER` chain via `AGENTS.md`
- Focused review skill: `docs/skills/spec-behavior-review-v1.md`
- Regression record: none yet

## Relevant files

- `src/gamemode/mode-pack.h`, `mode-pack-registry.*`, `capability-registry.*`,
  `action-graph.*`, `disaster-runtime.*`, `deterministic-rng.h`
- `config/mode-packs/survive_disasters.json`,
  `config/gamemodes/survive_disasters.json`
- `tests/mode-pack-registry-test.cpp`, `tests/disaster-determinism-test.cpp`

## Tests and evidence

- Automated tests: `tests/mode-pack-registry-test.cpp` (21 checks),
  `tests/disaster-determinism-test.cpp` (16 checks).
- Runtime commands: `mimita.exe --mode-pack-selftest`,
  `mimita.exe --survive-disasters-selftest`,
  `mimita.exe --server --timeout 6 --no-coordinator`.
- Logs: `[MODEPACK] loaded=1`, `[DISASTER] configured/begin`, `[ELIMINATION]`.
- Human playtest: still needed (select mode, observe intermission/banner/GO,
  no-respawn last-alive, results loop, second disaster without restart).

## Acceptance criteria

- Registry: one/many packs load; malformed JSON, duplicate ids, unknown
  capabilities, and unsupported schema versions are rejected atomically;
  previous catalog retained.
- Determinism: same manifest+seed → same disaster; same seed → same per-actor
  weapon; survivor/actor/entry order does not change results.
- Runtime: mode selectable, server-authoritative, HUD shows disaster name,
  no-respawn last-alive resolves, results return to intermission.
- No mode-name branch exists in the `survive_disasters` execution path.

## Changelog and regression links

- `docs/changelog/2026-10-05/20261005_011300-mode-packs-disaster-runtime.md`
