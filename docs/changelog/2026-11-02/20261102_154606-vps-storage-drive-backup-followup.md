# VPS storage and verified Drive backup follow-up

## Scope

Continue the 2026-11-02 plan to curb VPS disk growth and establish verifiable,
encrypted Google Drive database backups. Preserve database files, snapshots,
Git histories, active service data, and unrelated local edits.

## Repository changes

- Added `deploy/vps_storage_config.py` with guarded, reversible changes for
  Coturn's standalone `verbose` directive, journald/logrotate limits, exact
  duplicate Ubuntu APT source removal, and narrowly matched legacy backup
  cron retirement. It saves root-only configuration copies and refuses APT
  edits while package locks are held.
- Added `deploy/test_vps_storage_config.py` to cover those transformations and
  preservation behavior.
- Updated `deploy/mimita-database-backup.sh` to publish a detail-free status
  marker only after archive validation, encrypted upload, remote download
  verification, and retention. Failures and interruptions mark the backup
  unhealthy before any retention can delete prior copies.
- Added `/api/health/backup`, which returns only healthy/unavailable and goes
  unhealthy when the most recent recorded attempt failed, is missing, is in
  the future, or is older than 26 hours.
- Updated website operations, architecture, deployment, and regression
  documentation. The regression entry records unusable legacy Drive files and
  the overlapping dump jobs; PostgreSQL's original stop cause remains
  unproven.

## Local validation

- VPS policy unit tests: 8 passed.
- Website backup, health, and database error tests: 24 passed, 1 existing
  skip, 0 failed.
- Backup shell syntax and `git diff --check`: passed.
- `python devscripts/quality/agent-gate.py`: **RED**. The build step could not
  read the active checkout build lock (permission denied). Its collision
  selftest passed, but the `npc_wall_containment_v1` live scenario then timed
  out delivering its first command while two existing `mimita.exe` processes
  were running. Those unrelated processes were left untouched. Full gate
  validation must be rerun when the existing runtime/build activity is no
  longer using the checkout.

## VPS actions and evidence

- Removed four confirmed disabled Snap revisions using Snap's own commands.
  Disk free space first rose from 5.4 GB to 5.8 GB. Later, while the existing
  unattended APT task ran, its package archive cache fell from 378 MB to 32 KB
  and available root space rose to 6.0 GB (73% used). No database, snapshot,
  package index, or Git history was manually deleted.
- The long-running APT/unattended-upgrade task is now inactive and no APT/dpkg
  lock owners were observed. The Ubuntu mirror configuration was not edited;
  that repo-managed operation awaits review/deployment. At the final check,
  `/var/lib/apt/lists` still used 753 MB.
- Coturn's verbose configuration, journald limits, and rsyslog rotation policy
  were not changed. Coturn had active relay sessions at the last session
  inspection, so its restart remains deferred until sessions drain.
- Existing `gdrive:` files dated November 1 and 2 were 0-byte SQL files with
  20-byte compressed counterparts. VPS identity verification for
  `contact@ravgrowth.com` failed; no `gdrive-crypt:` remote, new timer, website
  backup-health route, or external email monitor was established. The fresh
  7,178,142-byte local archive passed `pg_restore --list` but has not passed an
  isolated restore rehearsal. Legacy database cron entries remain enabled.
- No repository commit or production deployment was made. Deployment still
  requires a green mandatory gate, a reviewed committed branch/commit, VPS
  Drive authorization and encryption setup, and an observed scheduled backup
  plus restore verification.

## Remaining work

1. Rerun the required agent gate after existing build/runtime activity clears;
   address any real failures without changing unrelated user work.
2. Review and commit only the scoped storage/backup changes, then deploy that
   exact reviewed revision after confirming it with the user.
3. Apply logging limits; wait for Coturn relay sessions to drain before
   removing verbose logging and restarting the service.
4. Set up and verify the intended Drive account and root-only encrypted
   remote, run and verify a scheduled archive, rehearse restore in isolation,
   then retire only the redundant database-dump cron entries.
5. Configure and test external down/recovery email alerts. Only after APT is
   quiescent and the helper is deployed, remove exact duplicate Ubuntu mirror
   definitions, refresh indexes, and verify package updates.

## Review notes

The VPS deployment guide still contains two stale TODO comments about document
formatting and separating game files from website files. This change did not
modify those unrelated notes; the desired storage separation is already
described in its operational sections.
