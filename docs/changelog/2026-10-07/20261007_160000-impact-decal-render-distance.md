# Expose long-range impact decal distances

Date: 2026-10-07
Branch: current working tree
Result: PASS_WITH_HUMAN_REVIEW

## Scope

Exposed render distance and fade controls for bullet holes and world cracks in
`config/impact_decals.json`. Preserved the existing blood render-distance,
client-feedback, and spray-debris settings while updating the file.

## Exact change

`config/impact_decals.json` now contains these values for both `bulletHoles`
and `worldCracks`:

- `renderFadeStartDistance`: `1000.0`
- `renderFadeEndDistance`: `1250.0`
- `renderDistance`: `1250.0`

The existing generic impact-decal parser and effect renderer already consume
these fields for each group, so no C++ wiring was required.

## Validation

- JSON parsing succeeded.
- Confirmed blood, bullet-hole, and world-crack distance triples are all
  `1250/1000/1250`.
- Confirmed blood spray debris and client feedback remain enabled.
- `git diff --check` reported no whitespace errors.
- No build was run because this is hot-reloadable JSON configuration only.

## Human review still needed

Reload the impact-decal configuration or restart the client, then verify
bullet holes and world cracks remain visible at the intended long range and
fade between 1000 and 1250 world units.
