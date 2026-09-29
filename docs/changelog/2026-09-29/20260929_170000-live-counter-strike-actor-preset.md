# Live Counter-Strike actor preset

Date: 2026-09-29

## Implemented

- Extended `config/actor-presets/counter_strike.json` with camera, movement, avatar, weapon-set, presentation, and revolver override rules.
- Added in-memory actor-preset weapon overlays; base weapon JSON and camera/effect files are not rewritten.
- Added explicit reserve-ammo override support, head-only body-part damage policy, per-weapon tracer controls, and preset-controlled hit-effect suppression.
- Applied actor preset rules to local testing and authoritative server mode state, including replication of the active preset ID and first-person rule.
- Added runtime-only ragdoll/blood overrides so gamemode presentation changes do not write configuration files.
- Added live preset refresh at the existing configuration polling boundary while preserving the pre-activation local settings for reset.

## Validation

- `config/actor-presets/counter_strike.json`, `config/weaponsets.json`, and `config/gamemodes/counterstrike.json` parsed successfully.
- Incremental `python build.py build-only` completed with `BUILD SUCCESS` and exit code 0 after the final server-gamemode fix.
- Static check confirmed the actor-preset path contains no writes to the base camera, weapon, hit-effect, ragdoll, or impact-decals JSON files.

## Runtime acceptance still required

- Run the built client/server and execute `actor_preset counter_strike`.
- Visually confirm FOV 70, forced first-person, movement, avatar, loadout, revolver cooldown/ammo, head-only damage, and suppressed effects.
- Confirm `actor_preset_reset` restores the prior local settings.
