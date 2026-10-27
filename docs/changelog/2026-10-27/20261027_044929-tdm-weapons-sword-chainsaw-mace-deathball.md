# TDM shared weapons, SwordSword, Chainsaw, Mace, Deathball

Date: 2026-10-27T04:49:29Z

## Skill review

- Skill: `docs/skills/spec-behavior-review-v1.md`
- Result: requested behavior settles the previously open SwordSword impulse and
  defensive-pose conflicts. The specification now records the user's decision.
- Findings:
  - TDM had three competing loadout owners: mode set 29, role-level
    `standard`/`elite_arsenal` overrides, and the dev profile's explicit
    `--weapon-set`.
  - SwordSWord previously specified no attack impulse and an animation-only
    defensive pose; both are now superseded by the requested behavior.
  - Chainsaw and mace had no complete behavior contract; placeholder contracts
    are now published.
- Unresolved warnings: smooth continuous chainsaw pitch/volume decay and
  authored mace animation are placeholder-level, not final presentation.
- Runtime and human acceptance still required for the items listed below.

## Changes

### TDM and NPC loadout

- `config/roles.json`: removed the `standard` and `elite_arsenal` role-level
  weapon-set overrides. TDM players and NPCs now inherit the mode set.
- `config/weaponsets.json`: set 29 is now revolver, shotgun, rocket launcher,
  swordsword, chainsaw, mace, deathball. Large machine gun removed.
- `config/behavior-profiles.json`: the elite cycle now includes swordsword,
  chainsaw, mace, and deathball.
- `devscripts/dev-launch-modes.json`: removed the TDM profile's explicit
  `--weapon-set 1`.
- Added `devscripts/dev-profiles/tdm-validation.json` with weapon set 29 for
  reproducible TDM runtime validation.

### Melee and animation

- `src/combat/weapon-melee-config.*`: added action-level contact damage,
  knockback, force cap, damage multiplier, front-arc block, NPC selectability,
  and procedural pose recipes.
- `src/entities/player.*` and `src/entities/player-animation.cpp`: added a
  procedural right-arm/torso melee pose fallback.
- `src/combat/melee-client.cpp` and
  `src/network/server-melee-actions.cpp`: animation-only melee actions now apply
  authored impulses; procedural poses are driven by the same action phase data.
- `src/network/server-physical-contact-actors.cpp`: uses active-action contact
  values, chainsaw rev decay, front-arc blocking with physical sword contact,
  and melee-vs-melee clash impulses.
- Clash presentation reuses `MeleeHitEventPacket` attack type 3; clients play a
  random metal ricochet and spawn short-lived spark particles.

### SwordSword

- Model changed to `assets/objects/weapons/mimita-swordsword-v2.glb`.
- Reuses HAFS swing/lunge/knockback/world-hit/equip sounds.
- Single left slash adds a 3 m/s aim-direction impulse.
- Double left is a 2x-damage heavy slash with no impulse.
- Right click is a procedural wide sweep.
- Double right holds a front-arc defensive pose.

### Chainsaw

- Added placeholder weapon `assets/objects/weapons/mimita-chainsaw-v1.glb`.
- Always-active contact, 5–15 base damage, 2x rev damage (10–30).
- Rev decays over 60 fixed ticks; each click resets it.
- Rev reach comes from the procedural arm extension.
- Chainsaw equip/idle/hit sounds wired.
- Weapon tint interpolates from dark red at 1x to bright red at 2x.

### Mace

- Added placeholder weapon `assets/objects/weapons/mimita-mace-v1.glb`.
- Idle contact 1–5 damage.
- 60-tick windup, 30-tick active, 90-tick recovery; active contact 25–125.
- Reuses HAFS sounds at 0.6x pitch and 1.5x volume.
- Uses placeholder procedural windup/active/recovery poses.

### Deathball

- Added placeholder weapon `assets/objects/weapons/mimita-deathball-v.glb`.
- One-round magazine, 1.0 second reload, speed 9 m/s, 10 second lifetime.
- Passes through world geometry and loses lifetime 3x faster while touching it.
- No explosion.
- Per-tick player intersection damage: edge 1, center 10, with 0.1 knockback per
  damage from the victim origin.
- Cyan-to-dark fade configured for local and network projectiles.
- Raygun equip/reload/shoot sounds wired.
- Shared projectile physics now carries `WorldPassThrough` and
  `PlayerPassThrough` results.

### Build-only fix

- `src/vehicles/vehicle-system.h`: aligned the `updateDrivetrain` declaration
  with the existing definition and call site so the pre-existing tree could
  build. This is not part of the weapon feature.

## Evidence

- Build: dev-loop request `2026-10-27T04:49:00.235Z-a81d02a2`, build 3424,
  status success.
- Post-build launch: profile `tdm-validation`, mode 31, map `atdm`, server and
  client ready.
- Runtime journal:
  `C:\mimita-v9\logs\2026-10-27\20261027_043837\events.jsonl`
- TDM validation launch: profile `tdm-validation`, mode 31, map `atdm`,
  `--weapon-set 29`.
- Observed in the live journal:
  - NPC loadout: revolver, shotgun, rocket_launcher, swordsword, chainsaw,
    mace, deathball, nothing. No large machine gun.
  - NPC weapon cycle selected swordsword, chainsaw, and mace.
  - `weapon.action.start` for chainsaw `rev` and mace `mace_swing`.
  - `weapon.contact.result` chainsaw damage at 23, 7, and 14.
  - `weapon.clash` opposing impulses for sword, chainsaw, and mace weapons.
  - `deathball.wall-pass` with `wall_decay_multiplier=3`, advancing age and
    retaining the 10-second lifetime.
- Not yet runtime-proven:
  - Deathball player intersection damage and edge/center interpolation.
  - Deathball cyan-to-dark visual fade by human inspection.
  - SwordSword single-left impulse, double-left 2x damage, sweep, and held
    front-arc block on a real player/NPC interaction.
  - Smooth continuous chainsaw sound decay. The action-start sound uses the
    revved pitch/volume, but continuous pitch/volume modulation is still
    placeholder work.
  - Mace authored animation quality. Placeholder procedural poses are in use.

## Human review needed

- Visual: new models, hand attachment offsets, chainsaw tint, deathball fade.
- Feel: SwordSword sweep/block, mace weight, chainsaw rev reach.
- Multiplayer: two real clients for sword block and melee clash.
