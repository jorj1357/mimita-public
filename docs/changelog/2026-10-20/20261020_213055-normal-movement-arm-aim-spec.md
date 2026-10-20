# Task

- Task ID: normal-movement-arm-aim-spec
- Summary: Added the clarified normal-movement physical arm-aim smoothing contract.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-20T21:30:55Z (17:30:55 EDT, America/New_York)
- Branch / base / final commit: `20261011zt` / `7556a218ec6b6bb8443485ff47b51c9ebf844e0d` / unchanged

# Pre-existing changes

- The target spec and `config/aimbody.json` already had working-tree edits before this turn; those edits were preserved. No config or code file was changed.

# Specification change

- File: `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md`
- Added “Normal-movement arm-aim smoothing”: ordinary movement only; shared camera-forward yaw/pitch target for both physical arms; immediate camera; arm/weapon-derived smoothed crosshair; dedicated editable aimbody multiplier (`1.0` current response, `10.0` about 10x response time, desired `1.5` with approximate 50 ms settling); quick-turn acceptance checks; ragdoll-mode behavior unchanged.
- Clarified “stable crosshair” as one coherent reticle that follows physical arm/weapon aim without jitter or doubling.

# Review and validation

- `docs/skills/documentation-checker-v1.md`: checked the changed section and its configuration reference. `docs/doc-review-09-03-2026.md`, listed by the router, is absent in this checkout.
- `git diff --check` flags only trailing whitespace in the pre-existing informal notes at the beginning of the spec; the added section has none.
- No tests, build, or runtime review; this is a specification edit. Human gameplay review is still needed to tune the approximate 50 ms target.
- Related open TODO: `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md:97` still asks for a long-term RMB arm settling solution; it is distinct from the normal-movement multiplier defined here.
