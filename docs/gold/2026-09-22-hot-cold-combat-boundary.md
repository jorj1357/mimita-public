# Hot/cold boundary for server combat (2026-09-22)

Status: `REFERENCE` — what is hot-editable today and what must stay cold.

## Goal

A gameplay *decision* must be hot-editable (rebuild `mimita-game.dll` only). A
*kernel mechanism* (socket, packet queue, ECS container, ABI struct) stays cold
but must contain no gameplay policy.

Rule: if a new gameplay behavior needs a new `mimita.exe` field, it is probably
policy that should have been a hot module instead.

## Hot (DLL-only edit)

- Weapon behavior: `modules/tools/hitscan-tool.cpp`, `melee-tool.cpp`,
  `physical-contact-tool.cpp`, `rocket-tool.cpp`, `grenade-tool.cpp`,
  `thrown-grenade-tool.cpp`, `banana-launcher.cpp`.
- Projectile lifecycle + broadcast: `modules/tools/hot-projectiles.cpp`.
- Attack routing/validation policy: `modules/tools/attack-policy.cpp`
  (`GAME_EVENT_ATTACK_POLICY` / `attack.policy`): spawn-state, slot, geometry
  tolerance, per-tick shot limit, community set, reported ammo/cooldown.
- Damage policy + safety cap: `modules/rocket-behavior.cpp` handles
  `GAME_EVENT_DAMAGE_POLICY` (spawn protection, creation mode, `outDamageLimit`).
- Consequence orchestration + packet contents:
  `hot-reload/hot-consequences.h`, `hot-projectile-event.h`,
  `hot-damage-event.h`, `hot-event-broadcast.h`.
- Tuning: `hot-tool-tuning.h` reads `weapon.tuning` (registry, honors
  `behaviorSource` json/cpp).
- Shared hot headers (editing them rebuilds only the DLL): `pellet-pattern.h`,
  `hitscan-model.h`, `hot-hitscan-target.h`, `hot-tool-tuning.h`,
  `hot-damage-resolve.h`, `hot-projectile-event.h`, `hot-consequences.h`,
  `network/packets.h` (packet layout).

## Cold (mechanism only; no policy)

- `network/server-attack.cpp`: packet parse, `Ecs::ensure`, `sendAttackResult`,
  `shotsThisTick`, and the cold family trace used when hot declines. The
  validation policy now lives in `attack-policy.cpp`.
- `network/server-event-broadcast.cpp`: reliable queue / unreliable send bytes.
- `network/reliable-gameplay-events.cpp`: retransmit, TTL, ACK.
- `network/server-ice.cpp` `serverSendToPlayer`: the socket write.
- `network/server-weapon-tuning.cpp`: registry → POD projection.
- `network/server-damage-policy.cpp`: dispatches the hot policy; the cold
  constant is only the fallback when nothing handles.
- `live-code/live-behavior.cpp`: builds `GameplayContextV1`, owns capability
  registration and host lifetime. Table-only; add events via registration.

## Proof of live edit

Edit `attack-policy.cpp` (e.g. flip a decision) → `python build_game_dll.py` →
run the unchanged EXE → the selftest observes the new decision. Verified
2026-09-22 with a one-line acceptance flip (`hot attack policy accepts a valid
request` flipped to FAIL, then restored).

## Deferred

Anti-cheat / server-side validation hardening, and a hot-safe sandbox for the
now-hot validation code. Full hot routing is intentional until then.
