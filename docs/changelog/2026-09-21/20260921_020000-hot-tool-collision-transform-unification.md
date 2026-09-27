# Hot tool collision transform unification

## Change

The collision path now applies the hot player pose before the body/weapon
collision pass. When a tool is hot-owned, `recomputeWeaponCapsule` also reads
the resolved `AttachmentState` transform used by hot presentation and uses it
for the weapon collision world/model transform. The previous cold arm/local
transform remains the fallback when no resolved hot attachment is available.

## Evidence

- Hot DLL build succeeded as `build/hotreload/mimita-live-g20260921.dll`.
- The patch does not change the GameAPI ABI or require an executable rebuild.
- `mimita.exe` is absent, so live collision alignment and human acceptance are
  not yet proven.

## Next

Run a live debug comparison of visible tool transform, hot attachment muzzle,
weapon capsule endpoints, and melee hitbox endpoints. Then tune shape data only
after those values agree.
