# Add impact-config write and reload diagnostics

Date: 2026-10-07
Branch: afad20a-rebuild
Result: PASS_WITH_HUMAN_REVIEW

## Scope

Added bounded structured diagnostics to identify who reads, backs up, restores,
or otherwise observes the impact configuration during gamemode startup and
reset.

## Diagnostics

The canonical `events.jsonl` stream now records:

- settings-backup scope, including that `config/impact_decals.json` is
  explicitly excluded;
- every managed backup copy with source, destination, before/after
  fingerprints, and copy result;
- every impact-decals load and hot-reload detection with path, fingerprint,
  and runtime blood-override state.

The fingerprints are byte-count plus FNV-1a hash; no config contents are
duplicated into the log.

## Validation

- Canonical build succeeded: `Compiled: 1`, return code `0`.
- Existing JSON and runtime-override fixes were preserved.

## Human review still needed

Run the newly built client, start Sandbox, and inspect the active run's
`events.jsonl`. Search for `settings.backup` and `impact_decals`; the records
will show whether the game performs a write or only observes a changed file.
