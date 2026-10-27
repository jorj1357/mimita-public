# Sphere collision snag intake

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-27T02:07:08Z (2026-10-26 22:07:08 EDT)
- Branch: `20261011zt`
- Base commit: `1cc507aa449b7aa6aae66bdf2749d62e3c84bc7f`; no commit created.
- Pre-existing edits: the working tree already contained configuration edits, `src/network/server-gamemode.cpp/.h` edits, an untracked map asset, a prior Payload map-route changelog/regression, a Clown gamemode spec, and earlier issue/gold/changelog artifacts. They were preserved.

## Changed

- `docs/specs/open-gameplay-issues.md`, existing “Blocks snag actors and spawn ramps catch at their seam” intake entry (now renamed “World and entity collisions snag actors, including on block and sphere contacts”), near line 205.

## Exact content changes

- Added the user's latest report that they got caught on a sphere and want ordinary movement past solid objects to avoid catching.
- Recorded that the sphere's map/object, collision shape, location, and run evidence were not supplied; the report is not assumed to share a cause with block-edge or ramp-seam snags.
- Kept the earlier `tdmtest3` scenario evidence and reproduction guidance for block edges and the steep-ramp seam.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/architecture/collision/collision.md`, `docs/specs/movement/movement.md`, and `docs/skills/documentation-checker-v1.md`.
- Documentation Checker v1: PASS. The existing collision entry is extended rather than duplicated; sphere behavior and causal relationship remain labeled unverified.
- `git diff --check`: PASS for whitespace. Git printed a line-ending notice for pre-existing `config/collision.json`; it was not changed.
- No code, build, or runtime scenario changed or ran. Reproduce the sphere snag in the game and record the object/shape, map, build, movement state, contact, and first velocity/position divergence before identifying a cause or fix.
