# Grenade timing defined in multiple competing places

Time created: 2026-11-01T20:50:32Z
Time last updated: 2026-11-01T23:11:41Z

Status: ATTEMPTED FIX (1)

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

Time: 2026-11-01T21:00:00Z

Change:

- Made per-weapon `grenade_effect` the only grenade timing owner using
  `duration_ticks` and `fuse_ticks`.
- Removed `custom_params.fuseTime` from frag, smoke, fire, and darkbang.
- Added `weaponGrenadeTimingConfigured()` and
  `weaponGrenadeFuseSeconds()` as the single derived-fuse boundary.
- Routed player, NPC, client-prediction, and local persistent-physics grenades
  through that same fuse.
- Converted `AreaEffect` aging and lifetime to integer ticks.
- Added `grenade.config.resolved` and `grenade.area.tick` runtime evidence.

Result: build, selftests, and both scenarios passed; player/NPC grenade fuse
values now resolve from the same `fuse_ticks: 120`.

### Corrected Code

File:

`src/combat/weapon-types.h`

```cpp
struct WeaponGrenadeEffect {
    int durationTicks = 0;
    int fuseTicks = 120;
    float durationSeconds() const { return durationTicks / 60.0f; }
    float fuseSeconds() const { return fuseTicks / 60.0f; }
};

inline float weaponGrenadeFuseSeconds(const WeaponDefinition& def)
{
    if (weaponGrenadeTimingConfigured(def))
        return def.grenadeEffect.fuseTicks / 60.0f;
    return def.projectileLifetime > 0.0f ? def.projectileLifetime : 5.0f;
}
```

File:

`src/network/server-npcs.cpp`

```cpp
projectile.lifetime = weaponGrenadeTimingConfigured(*wdef)
    ? weaponGrenadeFuseSeconds(*wdef)
    : (wdef->projectileLifetime > 0.0f ? wdef->projectileLifetime : 5.0f);
```

File:

`src/combat/area-effect.h`

```cpp
int durationTicks = 0;
int ageTicks = 0;
int remainingTicks() const {
    return durationTicks > ageTicks ? durationTicks - ageTicks : 0;
}
```

File:

`config/weapons/frag/config.json`

```json
"grenade_effect": {
  "duration_ticks": 0,
  "fuse_ticks": 120,
  "visual_ticks": 30
}
```

`custom_params.fuseTime` was removed from all four grenade configs.

### Fix

All grenade timing now comes from one per-weapon block. Server, NPC, client
prediction, and local physics read the same `fuse_ticks`; the area-effect owner
ages in integer ticks. Seconds exist only as derived display/log values. The
previous NPC-vs-player 6 s/2 s split is gone.

### Proof

Build and selftests:

- Dev-loop build `192` published at
  `.dev/builds/0192/mimita.exe`.
- `--grenade-selftest`: PASS (4 definitions).
- `--area-effect-selftest`: PASS (`fire_cadence`, `expiry`, `smoke_segment`).
- `--team-brain-selftest`: PASS.
- `--objective-selftest`: PASS.
- `--gamemode-config-validate`: PASS (`masterlist=1 loaded=19 errors=0`).
- `weapon_json_sync.py --check`: `definitions=34 errors=0`.
- `gamemode_json_sync.py --check`: PASS.

Runtime scenarios:

- `devscripts/scenarios/grenade_tick_contract_v1.json`: PASS, 13 assertions.
  Journal `logs/2026-11-01/20261101_230947/events.jsonl`.
- `devscripts/scenarios/fun_cs_npc_objective_v1.json`: PASS, 5 assertions.
  Journal `logs/2026-11-01/20261101_230439/events.jsonl`.

Journal evidence from `20261101_230439`:

- `grenade.config.resolved` for frag/smoke/fire/darkbang all reports
  `fuse_ticks: 120`.
- `grenade.config.resolved` reports `duration_ticks` 0/600/420/150 for
  frag/smoke/fire/darkbang.
- `grenade.area.tick` emitted 821 bounded countdown samples with both
  `remaining_ticks` and derived `remaining_s`.
- 66 `grenade.frag.blast` records with radius 20 m and `damage_ticks: 1`.
- 572 `grenade.effect.damage` records; fire effect 16 owner 100018 also damaged
  actor 100018, proving the thrower is a valid target.
- 104 `npc.hazard-avoid` records, including live frag projectiles as
  `hazard_kind: 4`, `radius_m: 20`.

### Human Review Still Required

- Confirm all four grenade types detonate at the same visual time whether thrown
  by a player, an NPC, or spawned by a debug command.
- Confirm the live two-decimal smoke/fire timers and the Darkbang world/HUD
  darkening and audio duck are readable in normal play.
