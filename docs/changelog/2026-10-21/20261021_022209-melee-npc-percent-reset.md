# Melee NPC percent spawn reset

- Time: 2026-10-20 22:22:09 EDT / 2026-10-21T02:22:09Z
- Branch: `20261011zt`
- HEAD before this task: `094e6e5a0f09521c63d760dc79c8c4f143ef04d1`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Existing work preserved

This checkout already had user and prior-session edits, including changes to `docs/specs/gamemodes/supersmashbrosmelee.md`, `src/network/server-gamemode.cpp`, and `src/network/server-melee-actions.cpp`. It also contained other modified configuration, NPC, animation, and sword files and multiple untracked changelogs. I preserved all of them. This task's spec edits are limited to the percent reset sentence in section 25 and the spawn/respawn sentence in section 52.

## Changes

### `src/network/server-melee-actions.cpp` — `serverMeleeResetNpcForSpawn` (lines 791–805)

Before, after `serverMeleeResetForSpawn(ServerPlayer&)`, the next function began immediately:

```cpp
}

void tickServerWeaponAnimationActions(std::unordered_map<uint32_t, ServerPlayer>& players,
```

After, new overloads clear the NPC's private Melee percent/action state and its replicated `ServerNpc` percent/action fields:

```cpp
void serverMeleeResetNpcForSpawn(uint32_t npcId)
{
    g_npcMelee.erase(npcId);
    g_meleeActionSnapshots.erase(npcId);
}

void serverMeleeResetNpcForSpawn(ServerNpc& npc)
{
    serverMeleeResetNpcForSpawn(npc.entityId);
    npc.meleePercent = 0.0f;
    npc.meleeActionActive = false;
    npc.meleeActionId.clear();
    npc.meleeActionStartTick = 0;
    npc.meleeActionForward = glm::vec3(1.0f, 0.0f, 0.0f);
    npc.meleeActionYaw = 0.0f;
    npc.meleeHitTargets.clear();
    npc.meleeNextAttackTick = 0;
}
```

### `src/network/server.h` — Melee spawn reset declarations (lines 1168–1170)

Before:

```cpp
void serverMeleeResetForSpawn(ServerPlayer& player);
bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player,
```

After:

```cpp
void serverMeleeResetForSpawn(ServerPlayer& player);
void serverMeleeResetNpcForSpawn(uint32_t npcId);
void serverMeleeResetNpcForSpawn(ServerNpc& npc);
bool serverSendToPlayer(SOCKET sock, const ServerPlayer& player,
```

### `src/network/server-npcs.cpp` — `finalizeServerNpcSpawn` (line 450)

Before:

```cpp
npc.body.respawnTimer = 0.0f;
// Melee: an NPC fighter receives 60 server ticks of spawn protection.
```

After:

```cpp
npc.body.respawnTimer = 0.0f;
serverMeleeResetNpcForSpawn(npc.id);
// Melee: an NPC fighter receives 60 server ticks of spawn protection.
```

This clears the side-map value on initial creation and on actual NPC respawn before the next Melee action tick can recreate it.

### `src/network/server-gamemode.cpp` — `finalizeServerNpcMirrorSpawn` (line 124)

Before:

```cpp
{
    ActorSpawnEvent event;
    event.entityId = npc.entityId;
```

After:

```cpp
{
    serverMeleeResetNpcForSpawn(npc);
    ActorSpawnEvent event;
    event.entityId = npc.entityId;
```

This clears the percent and action data on the server-side NPC mirror used by the snapshot path, including respawn-all flows.

### `docs/specs/gamemodes/supersmashbrosmelee.md` — sections 25 and 52

Before, section 25 ended with:

```text
Percent resets to 0 on respawn.
```

After:

```text
Percent resets to 0 for both human and NPC fighters on every respawn.
```

Section 52 now explicitly says:

```text
Both human and NPC percent meters reset to
0 on every spawn and respawn.
```

## Review and documents

Read `docs/ROUTER.md`, `docs/specs/gamemodes/supersmashbrosmelee.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/specs/networking/networking.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/regressions/README.md`, and `docs/skills/spec-behavior-review-v1.md`. The router-referenced `docs/architecture/player-npc-systems/player-npc-systems.md` and `docs/regressions/regressions-v1.md` do not exist in this checkout; the current NPC movement reference is a historical notes file and was not used as an authority.

Finding: the Melee spec already required percent to reset on respawn, but the NPC's separate server-side `NpcMeleeState::percent` and snapshot-side `ServerNpc::meleePercent` had no common explicit spawn reset. The new reset function clears both owners at their spawn boundaries. No change to player reset behavior was needed.

Focused behavior review: `PASS_WITH_HUMAN_REVIEW`. Source wiring and build succeeded; real NPC respawn and visible percent-meter confirmation remain unverified.

## Validation

- `git diff --check`: passed; only existing Windows line-ending warnings.
- Canonical dev-loop build succeeded: build 2722; source generation current.
- Executable: `C:\mimita-v9\.dev\builds\2722\mimita.exe`.
- Build SHA-256: `f600a8e70e03242510d5e14c36de663400086158a144c016d9dd877f8804fae6`.
- `--versioninfo` printed `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_222202/events.jsonl`, resolved to `C:\mimita-v9\logs\10-20-2026\20261020_222202\events.jsonl`. This confirms executable identity only, not gameplay.
- At the last dev-loop status check, the existing client was still running build 2718, `auto_restart=false`, and the server was not alive. I left that client running and did not start a competing match. No live NPC respawn scenario was run.
- No tests were added or run.

## Human review still needed

Run a Melee match, let the NPC gain percent, cause it to lose a stock and respawn, and confirm its displayed percent is 0. Then confirm the same after any other NPC spawn/respawn path used by Melee.
