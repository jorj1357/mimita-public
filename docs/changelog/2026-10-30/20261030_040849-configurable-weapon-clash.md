# Configurable weapon-versus-weapon clash policy

- Status: PASS_WITH_HUMAN_REVIEW
- Time: 2026-10-30T04:08:49Z
- Branch: `20261011zt`
- HEAD: `a7e5285a`

## Result

Added a shared `weapon_vs_weapon` policy to `config/collision.json` and
`CollisionConfig`. Direct blade clashes and block contacts now use one
pair-scoped cooldown, one configurable distance tolerance, one deterministic
speed-based knockback curve, and replicated spark/ricochet enable flags.
Weapon-to-actor damage remains on its existing cadence and is not gated by the
clash cooldown.

## Exact change

- `config/collision.json`: added `weapon_vs_weapon` with
  `distance_tolerance_m: 0.12`, `cooldown_ticks: 6`,
  `effects.spark/ricochet: true`, and knockback
  `min_speed_mps: 8`, `max_speed_mps: 63`, `reference_speed_mps: 25`.
- `src/config/collision-config.h/.cpp`: added `WeaponVsWeaponSettings`,
  `impulseForRelativeSpeed`, a mutex-protected fixed-tick snapshot accessor,
  staged validation (invalid type/range keeps the last valid block), and
  `collision.config.reload` / `collision.config.reload_rejected` diagnostics.
- `src/network/server-physical-contact-actors.cpp`: replaced hardcoded
  `0.12`, `6`, `8`, `63`, and `25` clash values with the snapshot. Added a
  canonical lower-actor-key cooldown entry shared by both evaluation
  directions and block contacts. Block contacts now apply the shared curve,
  share the cooldown, and still prevent actor damage when the cooldown is
  active. A blocking actor no longer initiates a direct clash. Clash logs now
  include distance, threshold, tolerance, cooldown, relative speed, impulse,
  and effect mask.
- `src/network/packets.h`: added `MeleeHitEventPacket::clashFlags` and
  `MELEE_CLASH_FX_SPARK` / `MELEE_CLASH_FX_RICOCHET`; bumped protocol `45 -> 46`.
- `src/network/multiplayer-projectiles.cpp`: client presentation now honors the
  replicated spark/ricochet flags and writes `weapon.clash.fx` evidence.
- `src/network/server.cpp`: dedicated-server loop now validates/publishes
  `collision.json` at its fixed tick.
- `devscripts/config_selftest.py`: added offline `weapon_vs_weapon` schema and
  range validation.
- `docs/specs/weapons/melee/melee-weapons.md` and
  `docs/specs/weapons/melee/swordsword.md`: named
  `config/collision.json -> weapon_vs_weapon` as the clash owner and documented
  the shared curve, cooldown, effects, and staged reload boundary.

## Source review

- Direct clash owner: `tickServerAnimatedPhysicalContactWeapons` in
  `src/network/server-physical-contact-actors.cpp`.
- Block owner: `blocksMeleeContact` plus the block branch in the same function.
- Damage owner: `resolveServerContact` remains unchanged and is not reached
  through the clash cooldown path.
- Before, block recoil used `max(1.5, force * 12)` and ignored the clash
  cooldown. After, it uses the shared configured curve and cooldown while
  preserving the block's damage-prevention behavior.
- Pair cooldown entries are no longer cleared when an actor changes weapon, so
  a weapon switch inside the cooldown window cannot produce a duplicate
  response.

## Validation

- `git diff --check`: no whitespace errors.
- `python devscripts/config_selftest.py`: the new collision section passed
  (`[CONFIG SELFTEST] collision weapon_vs_weapon: checked`); the overall script
  still reports pre-existing avatar JSON failures unrelated to this change.
- Build through the dev loop: request `2026-10-30T04:07:07.082Z-d78c71f1`
  resulted in `SUCCESS`, published build `3853`, executable
  `.dev/builds/3853/mimita.exe`.
- Runtime journal, run `20261030_040108`
  (`logs/2026-10-30/20261030_040108/events.jsonl`):
  - `collision.config.reload` showed the defaults;
  - editing tolerance/cooldown/effects/curve to `0.20 / 12 / spark=false /
    10..50@30` produced a server apply at tick `3911`, and later
    `weapon.clash` events reported `cooldown_ticks=12`,
    `distance_tolerance_m=0.2`, `impulse<=50`, and `effect_mask=2`;
  - `weapon.clash.fx` reported `spark=false, ricochet=true` on the client;
  - setting `cooldown_ticks=9999` produced
    `collision.config.reload_rejected` with the previous values retained, and
    subsequent clashes still used `12 / 0.2 / 10..50@30`;
  - restoring the defaults produced subsequent clashes with `6 / 0.12 / 8..63@25`.
  - all server `weapon.clash` events for one actor pair were at least 6 ticks
    apart (53 events, 39 pairs, 0 violations).
- Runtime journal, run `20261030_040715`
  (`logs/2026-10-30/20261030_040715/events.jsonl`, build `3853`):
  - TDM observed swordsword/chainsaw/revolver/shotgun clashes through the same
    server owner;
  - client `weapon.clash.fx` events carried the replicated effect flags and a
    non-zero server tick;
  - all server `weapon.clash` events for one actor pair were at least 6 ticks
    apart (9 events, 8 pairs, 0 violations);
  - 24 `weapon.contact.result` melee damage contacts and 51
    `server.damage.applied` events occurred in the same run, independent of the
    clash cadence.
- The block-contact runtime path was not observed in these runs; no
  `weapon.block` event was produced. The block change is source-verified only
  and still needs the dedicated block scenario described in the acceptance
  plan.

## Human review still required

- Confirm the feel of `0.12 m` tolerance and the `8..63 m/s @ 25 m/s` curve.
- Confirm the changed block recoil strength and shared cooldown with an active
  sword block.
- Confirm spark/ricochet visibility and audio with the effect toggles.

Unrelated pre-existing worktree changes, including runtime-written config
files and the modified `devscripts/blender-ai` submodule, were preserved.
