# Force Punch contact route and sphere presentation

Date: 2026-09-26
Status: PASS_WITH_HUMAN_REVIEW

## Result

Force Punch / `equipslot13` now has a network weapon mapping, so the existing
generic `AttackRequestPacket` route can reach the authoritative physical-contact
server path. Its configured active window remains JSON-owned (`activeHitboxTicks` in
`config/weapons.json`). The sphere presentation is configured as a solid white
sphere with radius `0.5` (1 m diameter), attached to the right-arm hand edge and
extended `0.15` m along the arm direction.

## Owners and data path

Local input runs `shoot` through `src/terminal/weapon-commands.cpp`, which sends
`MimitaNet::mpSendAttackRequest`. The server handles it in
`src/network/server-attack.cpp`, starts `QuickHitState` from the weapon JSON, and
`src/network/server-physical-contact.cpp` evaluates the sphere at fixed 60 Hz and
applies authoritative damage/knockback. The client shape and local visual are
owned by `src/combat/weapon-quick-hit.cpp`; hot presentation policy is
`src/hot-reload/quick-hit-debug-visual.cpp`.

## Changes

- Added `NETWORK_WEAPON_FORCE_PUNCH` and mapped `force_punch` in
  `src/network/network-weapons.cpp`; this fixes the prior `NONE` route that
  returned before sending the attack request.
- Changed the active sphere radius to `0.5`, alpha to `1.0`, and hand offset to
  `0.15` in `config/weapons.json` and the built-in fallback.
- Replaced the guessed camera-forward sphere placement with the transformed
  right-arm collider hand edge and arm-axis direction.
- Changed local, remote, and hot-module presentation from wireframe/turquoise
  to filled white.

## Validation

- `config/weapons.json` parsed successfully.
- Changed translation units compiled during `python build_agent.py` and the
  build reached `[LINK] mimita.exe`.
- The canonical build ended `FAILED` with `WinError 32` because the running
  `C:\mimita-v9\mimita.exe` locked a runtime DLL. No running process was stopped.
- No live two-client damage or visual acceptance was performed. Human review
  still needs to confirm the sphere is positioned correctly and a target loses
  health while the server log shows the physical-contact application.

## Documents and focused review

- `docs/ROUTER.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/networking/networking.md`
- `docs/architecture/json-configuration/json-configuration.md`
- `docs/features/live-code-development/live-code-development.md`
- `docs/architecture/live-development/live-development.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`
- `docs/skills/spec-behavior-review-v1.md`
