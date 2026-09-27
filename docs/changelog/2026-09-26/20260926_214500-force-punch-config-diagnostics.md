# Force Punch configurable color and damage-path follow-up

Date: 2026-09-26
Status: PASS_WITH_HUMAN_REVIEW

## Changes

`force_punch` now owns `debugHitboxColorR`, `debugHitboxColorG`,
`debugHitboxColorB`, and `debugHitboxAlpha` in `config/weapons.json`. The local,
remote, and hot presentation paths consume those values; the hot policy clamps
RGB and alpha instead of overwriting RGB with white. The current alpha is `1.0`.

## Damage investigation

The current JSON has `hitboxRadius: 3.5` and is syntactically valid. Recent
client logs show registration and equip of `force_punch`, but no
`[ATTACK REQUEST SEND]`, `[QUICK HIT SERVER]`, or
`[PHYSICAL CONTACT DAMAGE]` entries. The running `C:\mimita-v9\mimita.exe`
therefore cannot prove the new network mapping is active; the previous cold
build failed to replace its locked runtime DLL. The source now maps
`force_punch` to `NETWORK_WEAPON_FORCE_PUNCH`, allowing the generic attack route
to send the request. The server's physical-contact loop currently evaluates
player targets only; NPC damage remains a separate unresolved path.

## Validation

- `config/weapons.json` parsed successfully after the color/alpha additions.
- `python build_game_dll.py` returned `DLL up to date, skipping`; no new hot DLL
  was produced by that command.
- No process was stopped or restarted. Live damage and visual acceptance remain
  required after a successful cold build installs the network-route change.

## Relevant owners

- `src/terminal/weapon-commands.cpp`: generic attack request route.
- `src/network/network-weapons.cpp`: weapon network mapping.
- `src/network/server-attack.cpp`: authoritative QuickHit activation.
- `src/network/server-physical-contact.cpp`: fixed-60-Hz player contact/damage.
- `src/combat/weapon-quick-hit.cpp`: local sphere and JSON presentation inputs.
- `src/hot-reload/quick-hit-debug-visual.cpp`: hot RGB/alpha policy.
