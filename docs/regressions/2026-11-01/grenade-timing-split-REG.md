# Grenade timing defined in multiple competing places

Time created: 2026-11-01T20:50:32Z
Time last updated: 2026-11-01T20:50:32Z

Status: UNRESOLVED

Related specification:
`docs/specs/weapons/weapons.md`,
`docs/specs/gamemodes/funcounterstrike.md`,
`docs/specs/npc-behavior/npc-behavior.md`

Related changelog:
`docs/changelog/2026-11-01/20261101_205032-grenade-tick-single-source.md`

---

## Regression Occurrence 1

### Observed

Time: 2026-11-01T20:50:32Z (source audit during the grenade tick-unification task)

Grenade fuse and lifetime timing is split across at least five owners:

- `grenade_effect.duration_seconds` / `grenade_effect.fuse_seconds` in each
  weapon config;
- `custom_params.fuseTime` in the same weapon config;
- `AreaEffect::ageSeconds` / `durationSeconds` in `src/combat/area-effect.*`;
- `WeaponGrenadeEffect::durationSeconds` / `fuseSeconds` in
  `src/combat/weapon-types.h`;
- `ServerProjectile` fuse handling in `src/network/server-projectiles.cpp`; and
- server NPC grenade spawn using `projectileLifetime` (6 s) instead of the
  2 s configured fuse in `src/network/server-npcs.cpp`.

The result is that the same grenade can report different effective lifetimes
depending on who throws it and which path consumes the definition.

### Expected Behavior

One owner defines each grenade's authoritative timing. Per-weapon
`grenade_effect` is that owner. All other consumers derive from it:

- server projectile fuse;
- client prediction fuse;
- local/offline persistent-physics fuse;
- server NPC throw fuse;
- lingering area-effect lifetime;
- damage cadence;
- presentation countdown and expiry.

Authoritative timing is expressed in integer fixed ticks. Seconds are derived
for human display only.

### Actual Behavior

- Player/network grenades use `custom_params.fuseTime` through
  `projectileConfigFromDefinition`.
- NPC grenades use `projectileLifetime` through `server-npcs.cpp`.
- Area effects age in float seconds rather than ticks.
- Configs contain `duration_seconds`, `fuse_seconds`, and `custom_params.fuseTime`
  as parallel sources.

### Why This Is Bad

A single gameplay value has multiple owners. The same smoke or frag can detonate
at different times depending on the actor and network path. The divergence is
not visible in one file and survives config edits, which is exactly the duplicate
ownership rule the repository forbids.

### Specification

`docs/specs/gamemodes/funcounterstrike.md` says the canonical grenade policy lives
in each weapon definition under `config/weapons/<id>/config.json` in a shared
`grenade_effect` block and that there is one source of truth.

`docs/specs/weapons/weapons.md` owns the shared grenade schema.

### Wrong Code

Files and current competing definitions:

`src/combat/weapon-types.h`

```cpp
float durationSeconds = 0.0f;   // grenade_effect.duration_seconds
float fuseSeconds = 2.0f;       // grenade_effect.fuse_seconds
```

`src/combat/area-effect.h`

```cpp
float durationSeconds = 0.0f;
float ageSeconds = 0.0f;
```

`src/combat/area-effect.cpp`

```cpp
effect.ageSeconds += dt;
```

`src/network/server-projectiles.cpp`

```cpp
config.fuseSeconds = cp(def, "fuseTime", def->projectileLifetime);
```

`src/network/server-npcs.cpp`

```cpp
.lifetime = wdef->projectileLifetime, // used as the NPC grenade fuse
```

Weapon configs also keep both sources:

```json
"grenade_effect": { "duration_seconds": 10.0, "fuse_seconds": 2.0 },
"custom_params": { "fuseTime": 2.0 }
```

### Confirmed Cause

The tick-unification audit confirms duplicated timing ownership. The
NPC-vs-player fuse difference is directly visible in source: the NPC path reads
`projectileLifetime`, while the player path reads `fuseTime`/`grenade_effect`.

### Attempted Fix 1

Time: 2026-11-01T20:50:32Z

Change (in progress):

- Make per-weapon `grenade_effect` the only grenade timing owner using
  `duration_ticks` and `fuse_ticks`.
- Derive all seconds fields from ticks at load/display.
- Delete `custom_params.fuseTime` from grenade configs.
- Route player, NPC, client-prediction, and local persistent-physics grenades
  through the same tick fuse.
- Convert `AreaEffect` aging to integer ticks.

Result: pending implementation and runtime evidence.

### Corrected Code

Pending implementation.

### Fix

Pending implementation.

### Proof

Automated proof pending:

- `--grenade-selftest`;
- `--area-effect-selftest`;
- dev-loop scenario asserting server and NPC grenade fuse ticks agree and
  smoke/fire expiry occurs at the configured tick delta.

### Human Review Still Required

- Confirm all four grenade types detonate at the same visual time whether thrown
  by a player, an NPC, or spawned by a debug command.
