# Projectile rifle client trail fire phases

- Added a client-only projectile rifle trail using the existing pooled `EffectPartSystem`.
- Each trail particle is emitted behind the projectile at a configurable rate and lives for 15 60 Hz ticks by default.
- Trail color transitions from white to orange to red while its size and alpha fade.
- Added `weapons.json` controls for enabling the trail, emission rate, rear offset, lifetime, size, alpha, and phase colors.
- Preserved the existing rocket and grenade trail behavior.
- No server packets, collision, damage, or projectile hit logic changed.

Validation:

- `config/weapons.json` parsed successfully.
- Full build was attempted but stopped on an unrelated existing `player-nameplates.cpp` / `HealthbarCullReason::Disabled` compile mismatch.
- Runtime visual acceptance remains pending because the build did not complete.
