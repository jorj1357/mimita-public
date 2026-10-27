# Weapon config field completeness

- Time: 2026-10-27T20:03:53Z (2026-10-27 16:03:53 EDT)
- Branch: `20261011zt`; no commit made.
- Result: `PASS` for completeness of the current masterlist's 52 top-level fields across all 34 weapon configs. Nested object schemas remain outside the current masterlist contract.

## Change

The weapon masterlist already contained 52 top-level keys, and the 34 files already had each key. The missing safeguard was a repeatable way to add future masterlist fields to every weapon while respecting JSON-with-comments files such as `config/weapons/mace/config.json`.

Updated `devscripts/weapon_json_sync.py`:

- `--sync` inserts newly declared optional fields as `null` into each config without rewriting existing values or comments. A required field needs a masterlist default (except `id`, which comes from the folder name).
- `--check` now parses JSONC, requires the exact top-level key set, validates each field's type/nullability and matching folder ID, and rejects unknown keys.
- `--check` ignores key order; it checks completeness rather than presentation order.

Added a completeness gate to `devscripts/config_selftest.py` and documented the contract and the current nested-object limitation in `docs/architecture/json-configuration/json-configuration.md`. `config/weapons/masterlist/weapon-masterlist.json` now states that this version covers top-level fields only.

## Validation

- `py devscripts/weapon_json_sync.py --check` — `masterlist=1 definitions=34 errors=0`.
- `py devscripts/weapon_json_sync.py --sync` — `Synced missing masterlist fields in 0 weapon files`; follow-up check had zero errors.
- `git diff --check` on the changed script — passed (only line-ending conversion notice).
- No C++ source changed, so no build or live runtime check was required.

## Scope note

Fields inside optional objects such as `components.melee.actions`, `sound`, `viewmodel`, and dynamic `custom_params` are not yet enumerated or filled by this masterlist. To guarantee every nested field too, extend the masterlist with nested object schemas and dynamic-map rules, then teach `--sync` and runtime validation to apply those schemas. The current schema description says this explicitly so top-level completeness is not mistaken for a complete recursive schema.
