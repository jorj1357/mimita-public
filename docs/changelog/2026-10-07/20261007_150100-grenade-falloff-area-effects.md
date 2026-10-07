# Grenade falloff and area effects

- Tuned frag to 250 center damage with a steep 20 m cutoff/falloff, approximately 10 damage at 10 m and effectively zero at 20 m.
- Added configurable 2-second fuse timing to all named throwable definitions and used it in local prediction, client physics, and authoritative server projectile lifetime.
- Changed fire to a 7 m radius, 3 m high cylinder that applies 5 damage every 5 fixed ticks through the existing server area-effect damage owner.
- Wired named projectile detonation to spawn its configured server area effect.
- Added darkbang distance/aim/line-of-sight response and a configurable client darkbang overlay.
- Added client smoke-volume presentation with a 7.5 m radius, gray sphere, edge fade, and configured lifetime.
- Added grenade policy fields to `config/grenades.json` and preserved fixed-tick/server authority for damage.

Validation:

- `config/weapons.json` and `config/grenades.json` parsed successfully.
- Forced canonical build succeeded: 100 compiled units, 449 skipped, link succeeded.
- `C:\mimita-v9\mimita.exe --versioninfo` emitted `logs/10-07-2026/20261007_150015/events.jsonl`.
- Live gameplay acceptance for exact visual obstruction, darkbang perception, and damage distances remains to be performed.
