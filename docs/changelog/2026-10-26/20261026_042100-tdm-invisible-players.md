# TDM invisible enemy players at 99 per team

- Timestamp: 2026-10-26T04:21:00Z (2026-10-26 00:21:00 EDT, America/New_York).
- Branch: `20261011zt`.
- HEAD observed: `3f55d889`.
- Result: PASS. Added the reported issue to `docs/specs/open-gameplay-issues.md` as an unverified player report. No cause or fix is claimed.

## Intake

Recorded the player report that some enemy-team players are invisible in TDM when the scenario has 99 players on each team. Expected behavior is that all enemy players are visible. The issue notes that the current checked-in `config/gamemodes/tdm.json` sets team capacity to five, so it remains unclear whether 99 refers to a changed runtime configuration, another branch/config, or actual active population. The report did not include a build, map, run journal, actor IDs, or exact roster counts.

The next investigation should establish actual team counts and compare server roster/spawn state, replication, client entity binding, avatar/model readiness, and render submission. The intake explicitly avoids assigning a cause before those paths are traced.

## Review and validation

- Read `docs/ROUTER.md`, `docs/specs/open-gameplay-issues.md`, `docs/specs/gamemodes/teamdeathmatch.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/networking/networking.md`, `docs/doc-review-09-03-2026.md` (not present in the checkout), and `docs/skills/documentation-checker-v1.md`.
- Added one row to the existing open gameplay issue table adjacent to the general invisible-avatar report.
- Reviewed the row's routed links; all referenced current docs exist.
- `git diff --check`: PASS.
- No code or configuration changed. No build, test, or runtime investigation was requested or performed.

## Pre-existing edits

The unrelated `config/accounts/default.json` modification remained untouched. This task changed only the gameplay issue document and added this changelog.

