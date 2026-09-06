// 09 06 2026, 14 55 EST
/* purpose
* Record the current state of the data-saving implementation work.
* Separate completed local implementation and evidence from unfinished work.
* Preserve the VPS audit facts and the remaining deployment/acceptance work.
* Does NOT claim that production was migrated, deployed, restored, or accepted.
* Does NOT replace the authoritative data-saving specification.
*/

# Data-saving implementation progress

## Scope and authority

- Repository: `C:\mimita-priv-v8`.
- Branch: `8292026stash`.
- The authoritative behavior remains `docs/specs/data-saving/data-saving.md`.
- The confirmed reward policy is that every authenticated community host may award permanent XP, gold, and stats; host owners may invent results.
- This changelog records work through 2026-09-06 14:55 EST. No production migration, deployment, restart, backup creation, restore, or production data write was performed in this phase.

## Implemented so far

### Game/server persistence

- Added a server-owned persistence queue and session model under `src/persistence/`.
- Added authoritative event emission for player kills, NPC kills, and deaths.
- Added cumulative per-session counters for gold, XP, player kills, NPC kills, deaths, and 60 Hz playtime ticks.
- Added retry-oriented revisions, batch identifiers, duplicate-event handling, shutdown flushing, and retained failed work.
- Wired progression observation/ticking into dedicated and listen-server authoritative tick paths.
- Added progression event packets/notices for local feedback.
- Added client/server API fields for progression tickets and persisted totals.
- Added reward HUD configuration and reward popup support changes.

### Website/API/database

- Added versioned migration handling and a progression migration at `website/server/migrations/005_progression.sql`.
- Added `progression_sessions` and `progression_players` persistence structures.
- Added tick-based playtime, BIGINT-safe counters, nonnegative constraints, indexes, and backfill/adoption of existing users.
- Added authenticated host session and batch endpoints in `website/server/progression-routes.js`.
- Added validation for host authentication, join-ticket identity, room membership, expiry, cumulative counters, revisions, duplicate batches, conflicts, stale delivery, overflow, and transactional rollback.
- Retired the old unauthenticated/legacy progression write routes so they cannot silently award global progression.
- Added public profile and five progression leaderboard views using exact decimal-string formatting for large counters.
- Added profile statistics and periodic refresh UI.
- Added read-only database audit tooling at `website/server/audit-database.mjs`.
- Added credential-safe database backup publication and verification tooling at `website/server/backup-database.mjs`.

### Documentation and operations

- Added the VPS inspection procedure in `docs/operations/vps-audit.md`.
- Added persistence backup/recovery and isolated-restore guidance in `docs/operations/persistence-recovery.md`.
- Updated the VPS deployment guidance to require branch/commit confirmation, backup/recovery preparation, `git pull --ff-only`, and post-deployment verification.
- Routed data-saving and VPS-audit work through `docs/ROUTER.md`.
- Extended `docs/specs/data-saving/data-saving.md` with the approved authority policy, canonical field mapping, API contract, cumulative-save rules, retry/concurrency rules, playtime definition, website acceptance criteria, and migration/backup requirements.

## VPS/database evidence collected

The VPS inspection was read-only and used the deployed website environment without printing credentials or user records.

- VPS host was reachable.
- `mimita-api` and `mimita-coordinator` were online under PM2.
- PostgreSQL 14 was online at the time of inspection.
- Existing database counts were approximately: `users=83`, `game_stats=0`, `processed_events=0`, `kill_events=0`, and `player_kill_relationships=0`.
- All 83 existing users lacked a `game_stats` row at inspection time.
- Existing `game_stats` contained legacy/stat fields including `total_xp`, `gold`, lifetime player/NPC kills, deaths, and `playtime_seconds`, but no deployed progression session ledger or tick-playtime structures.
- `progression_sessions`, `progression_players`, and `schema_migrations` were not present in the inspected deployed database at that time.
- The VPS branch/commit and an untracked profile backup file were recorded before any proposed deployment.
- The available VPS backup directory contained suspiciously small recent dump artifacts; a valid production restore has not been claimed.

## Validation completed

- Focused progression/backend/frontend tests passed: 14 tests passed in the combined progression and persistent-stats run.
- Backup-tool tests passed: 13 passed and one POSIX-only permission test was skipped on Windows.
- Website production build passed with Vite.
- The full website test command ran but did not pass because the local machine has no PostgreSQL listener for the legacy database-backed tests (`ECONNREFUSED` on localhost:5432), plus unrelated pre-existing fixture/behavior failures. The progression-focused PGlite tests still passed.
- Website lint ran but reports a large pre-existing repository-wide lint backlog; it is not a clean gate for this work.
- The canonical C++ build was started, but this progress checkpoint does not claim a completed current build result. The existing build changelog still reports the previous successful build from 2026-09-01, not proof of this combined C++ diff.
- `overseer.py` and the completion hook have not yet been run for this checkpoint.

## Not done / must happen next

1. Complete and record a fresh canonical `mimita.exe` build for the combined C++ changes; inspect the build result rather than relying on the old build changelog.
2. Resolve or explicitly isolate any C++ compile errors from the current persistence/network integration.
3. Review the C++ queue for remaining correctness risks: failed old sessions must not starve current sessions, expired ticket registration must not block valid players, event watermarks must stay bounded, and all observed players must have a verified account/ticket identity.
4. Decide how existing match-history submission is preserved now that the old progression write endpoint is retired. Do not silently lose legitimate match-history behavior.
5. Confirm that progression notices are consumed by the intended HUD path and that kill/death/reward feedback is visible in an actual game session.
6. Run focused local server tests with a real local PostgreSQL instance or an approved equivalent, in addition to the isolated PGlite contract tests.
7. Run `python overseer.py`, address any findings owned by this work, and record its exact result.
8. Run the required task-completion hook after validation.
9. Before any VPS deployment, confirm the exact candidate branch, commit, backup validity, rollback path, and maintenance window with the user.
10. On the VPS, run the tracked migration only after backup/restore validation, then verify schema, signup-created stats rows, authenticated host session registration, one batch commit, retry/idempotency, profile totals, and all five leaderboards.
11. Perform human acceptance with two clients: join-ticket identity, kills/NPC kills/deaths, 60 Hz playtime, retry after network interruption, restart/shutdown flush, profile refresh, leaderboard ordering/ties, and the accepted host-owner cheating policy.
12. Create a separate final changelog for the later completion/deployment session; do not overwrite this progress record.

## Pre-existing worktree state

The worktree already contained unrelated documentation, specification/archive, configuration, rendering, networking, and regression edits before this data-saving implementation checkpoint. Those changes were preserved and are not claimed as part of this work. No unrelated files were reverted.
