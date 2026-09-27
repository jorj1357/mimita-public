# Force Punch one-tick actor contact

Date: 2026-09-26
Status: PASS_WITH_HUMAN_REVIEW

## Result

Force Punch is now configured as a one-tick sphere attack with `hitboxRadius`
from `config/weapons.json` and fixed `minDamage`/`maxDamage` of 250. The active
community weapon set now includes `force_punch`; before this, the client could
render/equip it locally while the server rejected the generic attack request as
weapon-set-disabled.

The server physical-contact owner now queries both players and ServerNpc actors.
NPC health and knockback are authoritative, and NPC results use the existing
reliable damage broadcast and kill-recording paths. The sphere contact normal is
the target-center direction from the sphere's contact point, which is the
knockback direction for the target.

## Spy Knife comparison

Spy Knife detects its swept blade on the client, queues a contact claim, sends a
batch packet, and has explicit server NPC/player validation and application.
Force Punch now uses the simpler server-owned equivalent: one generic attack
request starts the one-tick shape, then the server tests the shape against both
actor collections. The longer-term unification direction is recorded in
`docs/regressions/2026-09-26/force-punch-npc-contact-REG.md`.

## Validation

- `config/weapons.json` and `config/weaponsets.json` parsed successfully.
- Canonical build: `Status: SUCCESS`, previous build result after the actor
  implementation; changed server translation units compiled and linked.
- Focused `--snapshot-chunk-selftest` returned exit code 1 without output and is
  not counted as passing evidence.
- Live acceptance remains required: verify server request acceptance, NPC
  damage, knockback direction, repeated one-tick attacks, respawn, and killfeed.
