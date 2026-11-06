Time created: 2026-11-02T14:20:03Z
Time last updated: 2026-11-02T14:20:03Z

Purpose:
Record implementation, operational recovery, validation, and remaining
acceptance for the recurring website database outage, disk pressure, encrypted
backup automation, external monitoring, and temporary direct-only TURN test.

This changelog does not claim a production source deployment, external monitor
setup, Drive upload, isolated restore, or two-PC networking acceptance.

## Initial production evidence

At 2026-11-02T14:14:16Z, the VPS root filesystem had 26,848 KiB free (100%);
PostgreSQL 14/main was down while nginx and Coturn were active. `/var/log/syslog.1`
was 1,443,228,656 bytes, the active syslog about 237 MB, and journald about
920 MB. Coturn's `verbose` setting was enabled and sampled logs contained
millions of routine session messages. The database stop preceded the largest
sampled log's growth, so storage remains a possible contributor, not a proven
cause.

## Production recovery and backup checkpoint

- Removed `/root/.vscode-server/cli/servers` (about 3.1 GB) after checking the
  directory was root-owned and no VS Code server process was active. Preserved
  `/root/.vscode-server/data` and `/root/.vscode-server/extensions`.
- Cleared the unused root npm download cache after checking no npm process was
  running.
- Reduced the journal from about 920 MB to 200 MB, freeing 720.2 MB.
- Compressed the closed rotated `/var/log/syslog.1` (1,443,228,656 bytes) to
  `/var/log/syslog.1.gz` (80,035,118 bytes), checked the gzip archive, and kept
  syslog:adm ownership and mode 0640. The active syslog was not truncated.
- Root free space rose to 5,702,672 KiB. PostgreSQL was started only after this
  exceeded the 5 GiB floor. `pg_isready` accepted connections and a read-only
  `SELECT 1` returned 1. The service remained active and reports
  `enabled-runtime`.
- Preserved the existing `/root/db_backups` directory and its files. It is
  postgres:postgres mode 0750 and contains small historical SQL/schema files,
  including zero-byte recent dumps; it is not a verified full backup.
- Created a new root:root mode-0700 directory and ran the existing production
  `website/server/backup-database.mjs` tool. It reported
  `backup_archive_verified`, 7,178,142 bytes, 443 archive entries, and
  `restoreValidated=false`. Final archive:
  `/root/mimita-db-verified-backups/mimita-2026-11-02T14-18-04-435Z-4627313a-be29-4da4-9294-a69f88aa84d8.dump`.
  `pg_restore --list` passed. No old backup was overwritten or deleted.
- A live invalid-sign-in probe returned HTTP 401 with the expected invalid
  credentials message; it recorded an invalid attempt but did not create or
  change an account. The public homepage returned HTTP 200.
- After the archive was created, root free space was 5,686,720 KiB (76%).
- The VPS has only a `gdrive:` remote; its identity could not be verified as
  `contact@ravgrowth.com`, and `gdrive-crypt:` is absent. No upload was attempted.

These operational actions did not edit production source, database schema, or
configuration. The PostgreSQL start and archive creation are recovery actions;
no account, progression, or schema data was edited.

## Local implementation

- Added nested PostgreSQL-availability error classification. Database
  availability failures now return HTTP 503 with
  `code: database_unavailable` and state that no account change was confirmed.
- Added detail-free `/api/health/ready`, `/api/health/storage-warning`, and
  `/api/health/storage-critical` endpoints. Readiness checks `SELECT 1`, a
  5 GiB free-space floor, and the 80% disk threshold; warning and critical
  endpoints use 80% and 90% thresholds.
- Added a repo-managed TURN policy and server/client behavior to discard local
  TURN relay configuration when the coordinator explicitly reports relay
  disabled. The JSON switch is currently false locally. The live coordinator
  was not changed; no activation time is recorded because there was no actual
  deployment.
- Added a one-attempt-per-outage PostgreSQL watchdog, a daily full-archive
  backup service, upload verification before retention cleanup, a 30-day
  encrypted Drive retention rule, one-local-copy retention, and a 200 MB
  journald configuration file. These systemd/config files are not installed on
  the VPS yet. The backup script requires a root-only `gdrive-crypt:` remote,
  confirms the configured Google account, and fails closed if the Drive identity
  or crypt remote is missing.
- Added website architecture and operations documentation and appended the
  2026-11-02 recurrence to
  `docs/regressions/gameplay/website-auth-database-REG-2026-09-16.md`.
  Disk exhaustion is documented as suspected only.
- Corrected website, persistence, and regression routes in `docs/ROUTER.md`.
- No unrelated pre-existing working-tree edits were changed.

## Validation

- `node --test website/server/database-errors.test.js website/server/health.test.js`:
  8 passed after fixing the test's disk-size fixture.
- `node --test website/server/backup-database.test.js website/server/database-errors.test.js website/server/health.test.js`:
  21 passed, 1 platform-specific test skipped, 0 failed.
- `node --test coordinator-server/turn-policy.test.js`: 4 passed.
- Node syntax checks passed for the website modules and coordinator modules.
- Git Bash `bash -n` passed for both deployment shell scripts.
- `npm run build` in `website/` passed. Vite reported the existing large
  JavaScript chunk advisory.
- First `python devscripts/quality/agent-gate.py` run built and passed its
  self-tests/scenarios but failed lint because two repeated windows were added
  (553 vs baseline 551). Consolidated the duplicate TURN credential setup in
  `src/network/server-ice.cpp` and reran the gate. Final result: GREEN on build
  0218; all configured self-tests, lint baseline, JSON sync checks, and both
  runtime scenarios passed.
- `git diff --check` passed for files changed by this task. The unrelated
  pre-existing `docs/specs/open-gameplay-issues.md` edit contains trailing
  whitespace and was left untouched.
- VPS evidence after recovery: database query and invalid-login probe passed;
  archive listing passed; homepage returned HTTP 200. There is no restore,
  human sign-in/signup, or independent external monitor proof.

## Remaining acceptance and rollout limits

- The currently active Coturn config still has verbose logging. The journald
  cap, rsyslog rotation policy, and Coturn config change are documented/tracked
  but not installed. Logs can grow again until the reviewed deployment.
- No external uptime monitor or down/up emails are configured. The UptimeRobot
  setup is documented, but must wait until the readiness endpoint is deployed
  and its monitor can be checked.
- No encrypted offsite backup exists yet. Configure and verify the root-only
  Drive OAuth identity and crypt key, upload and download-check an archive, and
  rehearse restoration on an isolated host. The local archive is verified but
  not restore-validated.
- The TURN direct-only test needs a real two-PC relay baseline first, then
  deployment of the false policy and direct-path evidence from both peers.
  Same-PC loopback does not meet this acceptance. Do not record
  `2026-11-02 08:34 EST` as activation; production policy was not changed.
- Current local branch is `20261011zt` at
  `20578d110014c9ab962dbfce3712afcd5d3b4a68`. It has pre-existing user edits
  and this task's uncommitted changes. Per VPS deployment procedure, confirm
  the exact reviewed candidate branch and commit with the user before pulling
  or deploying. No production source deployment was performed.
