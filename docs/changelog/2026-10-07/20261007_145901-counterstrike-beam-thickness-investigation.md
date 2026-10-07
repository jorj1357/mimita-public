# Counter-Strike hitscan beam thickness investigation

- Status: `INVESTIGATION_COMPLETE_IMPLEMENTATION_PENDING`
- Timestamp: `2026-10-07 14:59:01 -04:00` (America/New_York)
- Scope: read-only investigation of `config/actor-presets/counter_strike.json`,
  hitscan collision, and network hit-claim acceptance. No gameplay or config
  behavior was changed.

## Confirmed findings

1. The requested path `config/actor-presets/counter/_strike.json` does not
   exist. The active file is `config/actor-presets/counter_strike.json`.

2. The active Counter-Strike preset sets `beam_thickness: 0.0` and
   `world_thickness: 0.0` for revolver, shotgun, and hitscan rifle. The preset
   overlay applies those values to the active weapon registry.

3. Client hitscan uses `rayAabb` when beam thickness is zero; positive
   thickness alone selects the swept-sphere path. The authoritative server
   uses the same zero-radius body-part AABB trace through
   `WeaponExecution::rayPlayerTarget`.

4. The networked generic attack path can still accept a client hit claim after
   the authoritative rewind trace misses. The fallback expands each body-part
   AABB by `rewind_hit_tolerance + claim_lag_allowance`. Current config values
   are `2.5 + 3.0 = 5.5` units. This is not the beam radius, but it is an
   effective generous hit acceptance volume and is the strongest explanation
   found for hits feeling too easy.

5. The server also rewinds target poses before tracing, so networking can make
   moving targets easier to hit even though the beam itself remains thin.

## Evidence boundary

- Source/config inspection: completed.
- Existing runtime journals: inspected, but they do not contain enough
  per-shot geometry to prove the current symptom is claim-fallback driven.
- Build: not run.
- New runtime scenario: not run.
- Human gameplay acceptance: not performed.

## Suggested next step

Run one real Counter-Strike scenario with bounded attack diagnostics that
records beam thickness, client trace result, server trace result, claim
accept/reject, and effective tolerance. If the complaint reproduces through
`ATTACK CLAIM ACCEPT`, tune or gate that network fallback separately from
`beam_thickness`; do not change the beam value to solve a networking tolerance
problem.
