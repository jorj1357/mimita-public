# Generic JSON-Defined Game Modes and Disaster Packs - Milestone 1

Date: 2026-10-05
UTC timestamp: 2026-10-05T01:13:00Z
Branch: afad20a-rebuild

## Result

PASS_WITH_HUMAN_REVIEW

Milestone 1 is implemented and builds. Registry, determinism, and in-binary
self-tests pass. Live multiplayer replication and visual/human acceptance still
require a human to run two clients.

## Scope

Add a validated community mode-pack registry, a capability registry and
fixed-tick action graph, a deterministic disaster runtime, and the
`survive_disasters` mode with the `random_weapon_last_alive` disaster. Only the
new mode is branch-free; legacy modes are unchanged.

## Current-state finding

Before this change there was no capability/action/pack runtime. Mode behavior
was selected by mode-name strings throughout `src/network/server-gamemode.cpp`
and client HUD code. Randomness for gameplay used `std::random_device`/`rand`
and was not seed-replicated. The hot-audio recipe system referenced by the
request does not exist on this branch (only in git history on
`origin/20260924cleanup`).

## Files changed

- `src/gamemode/deterministic-rng.h` (new): order-independent, platform-stable
  hash/mix used by all disaster decisions.
- `src/gamemode/mode-pack.h`, `mode-pack-registry.{h,cpp}` (new): validated,
  atomic mode-pack catalog with timestamp reload.
- `src/gamemode/capability-registry.{h,cpp}` (new): stable capability allowlist.
- `src/gamemode/action-graph.{h,cpp}` (new): manifest -> scheduled actions.
- `src/gamemode/disaster-runtime.{h,cpp}` (new): disaster selection, per-actor
  weapon assignment, duration, timeout winner, pure self-test.
- `src/network/packets.h`: `PACKET_DISASTER_STATE` + `DisasterStatePacket`.
- `src/network/server-gamemode.h/.cpp`: disaster state in `ServerGamemodeState`;
  data-driven configure at match start, `disasterBegin` at ACTIVE, timeout
  resolution, disaster broadcast; generic `last_actor_alive` win policy.
- `src/network/server-players.cpp`, `server-npcs.cpp`: per-actor disaster
  loadout hook in the shared spawn paths.
- `src/network/server.cpp`: load + hot-reload `config/mode-packs`.
- `src/network/community-match-client.{h,cpp}`, `multiplayer-tick.cpp`: client
  disaster state + dispatch.
- `src/engine/engine-tick-ui-overlays.cpp`: `{disaster_name}` /
  `{disaster_description}` tokens + generic `disasterText` element.
- `src/game/game-cli.cpp`: `--mode-pack-selftest`, `--survive-disasters-selftest`.
- `config/mode-packs/survive_disasters.json`,
  `config/gamemodes/survive_disasters.json`, `config/onlinemodes.json`,
  `config/gui/gamemode-meta-gui.json`.
- `tests/mode-pack-registry-test.cpp`, `tests/disaster-determinism-test.cpp`.
- `docs/specs/gamemodes/mode-packs.md`, `docs/features/gamemodes/mode-packs.md`.

## Reasoning

The registry/action graph are additive and data-driven: the server calls
`ModePackRegistry::get(resolvedGamemodeId)`, and presence of a declared disaster
decides whether the disaster runtime runs. No code compares `matchMode` to a
mode name for the new mode. The existing `last_team_standing` path assigns
teams, so a new generic `last_actor_alive` win policy was added for true
teamless last-alive; it is a win-policy id, not a mode name.

Determinism uses score-max selection (highest `mixSeed(seed, id)`) rather than
index modulo, so reordering any list cannot change the result. The pure tests
caught two order-dependence bugs during development, which were fixed before
build.

## Spec / regression alignment

- Specification: `docs/specs/gamemodes/mode-packs.md`.
- JSON contract: `docs/architecture/json-configuration/json-configuration.md`
  (owner, validation, atomic replacement, timestamp reload).
- No confirmed regression; no regression record added.

## Validation

### Build

- `python build_agent.py` -> Status: SUCCESS (incremental; new objects present,
  `mimita.exe` relinked).

### Pure tests

- `g++ ... tests/mode-pack-registry-test.cpp src/gamemode/mode-pack-registry.cpp
  src/gamemode/capability-registry.cpp -o build/mode-pack-registry-test.exe`
  -> `21 checks, 0 failures; PASS`.
- `g++ ... tests/disaster-determinism-test.cpp src/gamemode/disaster-runtime.cpp
  src/gamemode/action-graph.cpp src/gamemode/capability-registry.cpp -o
  build/disaster-determinism-test.exe` -> `16 checks, 0 failures; PASS`.

### In-binary selftests

- `mimita.exe --mode-pack-selftest` -> loaded=1, disaster selftest PASS,
  overall PASS.
- `mimita.exe --survive-disasters-selftest` -> pack schema=1, capabilities=5,
  disasters=1, duration=90s, PASS.
- `mimita.exe --gamemode-selftest` -> PASS (existing Counter-Strike assertions
  unaffected).

### Runtime / server

- `mimita.exe --server --timeout 6 --no-coordinator` -> `[MODEPACK] loaded=1`,
  world/map/NPC startup healthy, no crash.

## Human review still needed

- Two-client multiplayer: confirm both clients agree on disaster id/seed/start
  tick and show the disaster banner during intermission/countdown.
- Visual acceptance: select `survive_disasters` from the dropdown, observe 15s
  intermission + disaster name, 3-2-1/GO, random weapons, no-respawn last-alive,
  results returning to intermission, and a second disaster without restart.
- Confirm NPCs receive the per-actor disaster weapon as expected.
- Verify legacy modes (FFA/TDM/Bomb Tag/NPC Waves/Counterstrike) show no
  behavior change.

## Known limitations

- Disaster audio uses existing `playEventSound` primitives; the hot-audio recipe
  layer is deferred (absent on this branch).
- The HUD does not yet show the disaster countdown duration; only the name.
- `GamemodeRegistry::loadDirectory` still clears before loading (pre-existing);
  the new `ModePackRegistry` uses the atomic pattern.

## Spec/doc TODOs noted (not edited)

- `docs/specs/gamemodes/gamemodes.md:3` - "todo this not good but move to this
  version more general things for gamemodes". Suggest: replace with a pointer to
  this spec's capabilities/action-graph model.
- `docs/specs/gamemodes/gamemodes.md:162` - "Todo explain this 9 2 2026 0848"
  (weapon set / hotbar). Suggest a dedicated weapons spec section.
- `docs/specs/gamemodes/gamemodes.md:178`, `:182` - chat/Discord online-server
  TODOs. Suggest moving to the networking/coordinator spec.
- `docs/specs/gamemodes/meta.md:8` - "todo expand this cuz this not full".
  Suggest folding meta presets into the mode-pack `gamemode_id` lifecycle block.
- `docs/specs/gamemodes/infinite-dungeon-slayer.md` - TODO stub.
