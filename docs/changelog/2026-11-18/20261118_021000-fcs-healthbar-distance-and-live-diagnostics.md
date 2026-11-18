# FCS healthbar visibility and live diagnostics

## Change

- `config/healthbar.json`: raised the shared actor-healthbar maximum distance
  from `0.01 m` to `999 m` and extended the end-fade distance to `999 m`.
- `config/gamemodes/funcounterstrike.json` already enables friendly and enemy
  healthbars and disables player/friendly outlines. `show_own_healthbar` stays
  false in the shared healthbar config. Existing FCS smoke occlusion remains in
  the client healthbar path.

## Evidence

- Source/config: the FCS mode booleans were already true; the shared distance
  cap at `0.01 m` was inconsistent with visible actor bars at normal gameplay
  distances. This patch is configuration-only and affects other modes that
  render healthbars as well.
- Build/runtime: `python devscripts/dev-loop.py --status` reported published
  game DLL generation 329 and run `20261118_020227`, server PID 19308, client
  PID 22896. That proves publication and launch identity only. No visual
  acceptance or healthbar-specific runtime event was captured in this task.
- Performance: the earlier run's sampled server windows were about 5–7 Hz,
  with NPC phase/tick costs in the hundreds of milliseconds. In the current
  `20261118_020227` journal, windows report roughly 44–61 Hz while 19 NPCs are
  active; sampled NPC averages are about 7–16 ms in the latest windows. This
  shows the low rate is not constant and the current run recovered near the
  60 Hz target. NPC navigation plans exceeded their nominal 6 ms budget in the
  earlier sample. Neither sample proves DLL reload caused the earlier low Hz.
- Collision: the live journal has actor/world collision and bounce events, but
  the observed records do not consistently expose an impact point/normal tied
  to an attacker, target, and damage event. The existing 2026-11-03 snag
  recording is a separate historical repro and does not prove the current
  corner snag.
- NPC objective: the observed FCS bomb carrier had `can_plant:false` with
  `eligibility_reason:"outside_bomb_site"`; this confirms the carrier was not
  plant-eligible in that sample, not why the CTs clustered or whether the round
  strategy is generally wrong.

## Remaining acceptance

- Check the active run in-game for teammate and enemy bars at useful distance,
  own bar hidden, outlines absent, and smoke hiding bars through occlusion.
- Capture a current snag/hit reproduction with time, map location, actor IDs,
  weapon/action, and the corresponding live journal events before changing
  collision behavior.
- Trace FCS objective goals and CT movement from the live journal/source before
  changing NPC aggression. Preserve the 60 Hz fixed-tick requirement and
  existing collision broadphase.
