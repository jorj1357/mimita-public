# Mace damage animation modes and physical slam

- Result: `PASS_WITH_HUMAN_REVIEW`.
- Timestamp: 2026-10-30T23:20:12Z.
- Branch/HEAD: `20261011zt` / `a7e5285a`.
- Commits: none.

## Source and configuration

- Added top-level `damage_animation_mode` to the weapon masterlist and canonical weapon files. The runtime default is Mode 2 for null or absent fields; values other than integer 1 or 2 reject the entire candidate weapon snapshot while retaining the previous valid definitions. Mace selects Mode 1; other canonical weapons select the default.
- Mode 1 preserves an active attack clip through damage and consumes the hurt reaction so it cannot play after the attack. Mode 2 releases an active attack clip for the hurt animation. Death still owns its existing path. Damage, health, knockback, hit effects, and contact calculations were not changed by the mode.
- Kept mace primary `mace_swing` as a fixed action. Added secondary `mace_physics_slam`, using the existing right-arm ragdoll motor and world collision. Movement intent steers the arm, with a downward motor bias. The existing melee contact resolver samples moving weapon collision boxes and computes relative impact speed and force; no new damage formula or power multiplier was added.
- Scoped the physical arm to the selected action, including fixed-tick simulation and movement-aware action timing. Updated the two mace probe scenarios and the animation, melee, and gameplay-issues specifications. The issue spec leaves per-weapon versus game-wide policy as an open decision.
- Preserved unrelated pre-existing edits in the dirty shared worktree. The previous documentation-only changelog remains a separate earlier session record.

## Build, validation, and live evidence

- `python devscripts/weapon_json_sync.py --check`: `masterlist=1 definitions=34 errors=0`.
- `git diff --check` on the touched source, scenario, and focused spec files: passed; Git emitted only line-ending normalization warnings.
- Canonical dev-loop build 3866 succeeded. Exact artifact: `.dev/builds/3866/mimita.exe`; its `--versioninfo` reported `EVENTS_JSONL_PATH=logs/10-30-2026/20261030_192054/events.jsonl`. Build success is compilation evidence only.
- Live dev-loop client/server run `20261030_231916` (`logs/2026-10-30/20261030_231916/events.jsonl`) used build 3866 and Mode 1. Both mace actions started and traversed their phases; incoming damage did not cancel the fixed swing or select a hurt clip afterward. The physical action activated the arm motor, recorded nonzero movement intent and arm speed rising from 4.81 to 15.43 m/s. The scenario's four event-presence assertions passed.
- A temporary Mode 2 mace value was tested and restored byte for byte. Run `20261030_231947` (`logs/2026-10-30/20261030_231947/events.jsonl`) showed the fixed attack clip changing directly to `mimita_sizing_6_hurt_normal_001`, followed by action cancellation with reason `hurt`. Its four scenario assertions passed.
- A temporary omitted field loaded as Mode 2 in run `20261030_231725`. A temporary invalid value 3 emitted `config.weapon.invalid` on both client and server with `retained_previous: true` in run `20261030_231602`; the mace file was restored exactly after each probe.

## Remaining acceptance

- The live probes did not strike a target, so they do not prove server-confirmed slam damage, hit effects, knockback, or the contact-force value from a real victim. The physical speed-to-contact path was checked in source, while actual target impact still needs live play.
- The journals do not establish visual weapon alignment, world-contact feel, or whether movement steering feels rewarding. Human playtesting should compare primary fixed swing and secondary physical slam, including a real hit and incoming damage in both modes.
- The action-scoped physical arm was exercised for the local player. NPC action-specific arm simulation was not exercised in this session.
