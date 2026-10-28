# Collision, melee FX, animation controls, and dev-loop process hygiene

- Time: 2026-10-27T21:35:00Z (2026-10-27 17:35:00 EDT)
- Branch: `20261011zt`; no commit made.
- Result: `PASS_WITH_HUMAN_REVIEW` for the implemented slice. Triangle-vs-triangle
  multi-actor collision, full large-avatar world collision, and complete
  per-weapon animation-profile migration remain open.

## Changes

- Dev loop: added `max_mimita_processes` (default `1`) to the launch profile.
  Launch now audits and kills preceding `mimita.exe` processes for this
  checkout, creates a fresh journal on every launch, and records
  `dev-loop.process.audit`. This removes the full-journal readiness failure and
  the multi-process FPS collapse source.
- Camera config: removed `config/camconfig.json` from the mode backup/restore
  set. Mode FOV is now captured and restored in memory; stale
  `camconfig.json.bak` files are deleted instead of restored.
- Weapon collision: rebuilt the weapon collision transform after the fixed-tick
  pose update, stored the animated attachment on `weaponModelTransform`, and
  honored `collides_with_world`. This removes the one-frame-stale weapon box.
- Actor/weapon collision: extended the animated physical-contact owner from only
  animated melee weapons to every equipped weapon. Non-physical-contact weapons
  now shove targets without applying weapon contact damage; all weapons can
  clash blade-to-blade.
- Mace: added `cooldown_ticks` to melee actions, parsed it, enforced it on the
  server with fixed ticks, and set mace to 60 ticks. Mace still uses its
  authored attack clip.
- Animation clips: added `speedMultiplier`, `lockTicks`, `overrideMode`,
  `partMask`, and `rootTranslationScale`. Clip speed is applied to sampling and
  lock duration; part masks keep weapon arms intact for dash/state combinations.
- Air state: falling now has a jump/locomotion fallback when a profile lacks a
  fall clip; idle is only eligible while grounded and remains the lowest state.
- Root translation: falling and landing imported clips now use
  `rootTranslationMode: 2` so they no longer bounce the actor upward.
- Melee FX: added `hitfx.json` `meleeContact`. Contact tick 1 spawns the white
  force-scaled sphere; sparks use `sparkv1` ticks 2-6, `sparkv2` 7-13, and
  `sparkv3` 14-20 with configurable alpha and random rotation. Client FX fire for
  confirmed melee contact and weapon-vs-weapon clashes.
- Wireframes: outlines now require a loaded avatar and reject non-finite or
  exploded part transforms beyond a bounded radius, emitting
  `wireframe.bounds_rejected` instead of drawing giant floating outlines.
- `animation_json_check.py`: validates the new clip fields, ordered sets, set
  includes, and settings; tolerates full-line `//` comments in weapon configs.

## Evidence

- `python devscripts/animation_json_check.py`: `clips=116
  avatar_profiles_checked=34 errors=0`.
- `python devscripts/weapon_json_sync.py --check`: `masterlist=1 definitions=34
  errors=0`.
- Dev-loop build 3521 succeeded. Repeated launch replaced the running process
  pair, used a fresh journal, and left exactly two `mimita.exe` processes
  (one server, one client).
- Live TDM run `20261027_212947` on build 3521:
  - `actor.loadout.resolved` kept player set 29 and all eight TDM weapons.
  - TDM NPCs selected permitted human avatars with `candidate_count: 7`.
  - `weapon.contact.result` showed NPC melee damage.
  - `weapon.clash` showed revolver vs chainsaw and sword vs sword contacts.
  - `melee.contact.fx` appeared on the client with `sparks: 3`.\n  - `PERFORMANCE_FRAME` now includes `npcs_active`, `effects_alive`,\n    `blood_particles`, and `corpses_active` growth counters.
  - `anim.settings.loaded` reported the expected priorities and settings.
  - No `config.weapon.invalid` or `config.gamemode.invalid` appeared in the
    inspected interval.

## Remaining work

- The production collision mode is still capsule/box approximation. Triangle
  mode for player-vs-player/weapon-vs-world is not implemented; the code and
  architecture note still need the long-term 100-actor/240-fps triangle target.
- Server weapon-vs-world collision is not yet fully authoritative; the fixed
  transform and client weapon-world path were corrected, but the server movement
  resolver still tests the player root capsule only.
- Large-avatar capsule scaling and limb-vs-world ground collision still need the
  full Phase 3 pass.
- Per-weapon animation profiles and per-action weapon attachment rotation have
  not been migrated into `config/weapons/<id>/config.json` yet.
- NPC/remote animation parity diagnostics were not added in this slice.\n- Combat/simulation frame spikes remain; growth counters were added, but the\n  logging/corpse/contact cost pass is not complete.
- Visual review is required for chainsaw wall push-out, TDM outlines, white
  sphere/spark timing, falling/landing root motion, and mace cooldown feedback.
