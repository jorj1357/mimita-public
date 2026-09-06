// 09 06 2026, 16 30
/* purpose
* Give operators a preservation, backup, isolated restore, and rollout procedure.
* Cover the account-to-host-to-database-to-profile persistence contract.
* Separate observed evidence from archive inspection and actual restore proof.
* DOES NOT authorize production writes, backups, migration, or deployment.
* DOES NOT reconstruct missing progression from guesses or private logs.
* DOES NOT replace the data-saving specification or deployment approval.
*/

# Persistence operations and recovery

Audience: MiMITA operators and agents preparing a reviewed deployment or recovery.
Read [the router](../ROUTER.md), [data-saving specification](../specs/data-saving/data-saving.md),
[read-only audit](vps-audit.md), [deployment procedure](vps-deployment/vps-deployment.md),
and [completion procedure](task-completion/task-completion.md).

## Evidence and current limit

The parent task's September 6, 2026 VPS audit reports 83 accounts, zero game_stats
rows, and September 4–6 SQL files of zero bytes with 20-byte gzip files. These are
historical observations, not hardcoded expected counts. Refresh through the
read-only audit before a deployment or recovery decision. An empty table does
not establish whether progression was never saved, saved elsewhere, or lost.
Do not invent historical rewards or replace real totals with zeros.

A separate September 6 read-only probe inspected two backup producer files and
bounded tails (up to 2 MB each) of four backup logs. It returned only fixed
category counts: pg_dumpall=2, gzip=2, localhost-TCP=2, stdout-redirection=2,
pipefail=1, dotenv=0, PGPASSWORD=0, shell-source-env=0. Logs contained six
connection-refused matches and zero matches for the allowlisted missing/rejected
password, peer authentication, missing role/database, permission, version,
disk-space, and missing-command categories. Counts are occurrences, not job counts.
The scan did not attribute these errors to individual September 4–6 runs.

Shell output redirection can create an empty destination before a failing dump
starts; compression can then package that empty file. That explains a possible
empty-file mechanism, but does not prove the exact failure for each dated backup.
The missing environment-loading features do not prove an authentication failure:
the sampled logs identify connection refusal. Inspect the actual job's target,
PostgreSQL availability at its execution time, and scheduler result before naming
a final cause. Perform filtering on the VPS and return allowlisted categories,
counts, and statuses only. Never stream raw cron lines, process arguments,
environment files, logs, SQL dumps, or individual accounts into chat.

No production backup, production change, or isolated PostgreSQL restore was run
for this backup-tool implementation. Restore validation remains outstanding.

## Persistence contract to preserve

The specification says: "The database is the permanent source of truth for player
totals." It also requires "Only after the MiMITA backend/database confirms
successful persistence should the game display" the confirmed-save notification.
Archive success is a separate operational status; it never acknowledges gameplay.

| Boundary | Required behavior and recovery implication |
|---|---|
| Account and join | Authenticated membership ties a player to the host session. Preserve accounts and join-ticket records; do not accept client-supplied permanent totals as authority. |
| Gameplay authority | Host-confirmed PvP kill: +1 PvP kill, +50 gold, +100 XP; victim +1 death. Self/environment: death only. NPC: +10 XP, +0 gold, no PvP kill; display +0 gold intentionally. |
| Dirty state and save | Batch dirty players every 3,600 simulation ticks; reuse the pipeline for match end, disconnect, and clean shutdown. Skip no-change batches. Keep failed or newer revisions dirty. |
| Commit and retry | A lost response can cause a retry after commit. Preserve stable event/batch identity, acknowledged revision, cumulative counters, and recorded result with totals. |
| Playtime | Store ticks at 60 ticks/second. Convert only for presentation; legacy seconds convert once in migration. |
| Website | Own/other profiles and XP, gold, playtime, PvP-kill, and death rankings read committed database values, including when no host is online. |
| Recovery | Restore a consistent database snapshot and its retry ledgers. Totals without ledgers can double-award a retry; ledgers without totals can suppress missing gains. |

The backend implementation uses existing users, game_stats, processed_events,
and vip_join_tickets, plus progression_sessions and progression_players.
processed_events retains batch fingerprints and replayable responses using
progression session/batch identities; progression_players retains revision and
cumulative session counters. Preserve these tables together with constraints,
indexes, sequences, and schema_migrations. Confirm exact names against the
reviewed revision; inspect db.js as well as migrations, since legacy bootstrap
is not solely represented in the SQL directory.

The approved trust policy permits every authenticated community host to award
permanent progression and accepts the possibility of host-owner cheating. Backend
authentication verifies host/session ownership and player join membership; it
does not independently simulate combat or prove an honest host. Ordinary clients
must not retain a direct arbitrary stats-write bypass. /api/progression/session
loads saved totals and establishes the host session; /api/progression/batch carries
stable batch identities, per-player revisions, and cumulative session deltas.
The backend applies only the difference from the stored session counters in one
transaction, preserving concurrent gains from other hosts. Audit this ownership
and membership boundary during the isolated integration test, including rejected
foreign sessions, fabricated members, stale/conflicting batches, and disabled
legacy direct-write routes. Preserve BIGINT values exactly across API/website
serialization and check deterministic leaderboard ties by account ID.

The local migration plan adopts the existing bootstrap as version 1. Version 2
backfills playtime_ticks from playtime_seconds * 60, widens counters to BIGINT,
adds nonnegative constraints, inserts missing game_stats rows without replacing
existing rows, and adds ranking indexes and progression session ledgers.
These changes are not evidence that historical missing progression was recovered.
Run the reviewed migration only in isolation first; a failing nonnegative check
requires investigation, not silent clamping or resetting totals.

## Local backup tool

[backup-database.mjs](../../website/server/backup-database.mjs) is deliberately
separate from application startup and the read-only audit: it writes an archive,
but never imports db.js, runs migrations, restores, or schedules a job.
It uses Node built-ins and the project's dotenv dependency, with installed
pg_dump and pg_restore executables from a trusted PATH. Choose clients compatible
with the source server; record both tool and server versions in the operator's
private recovery record. Do not install packages or upgrade PostgreSQL as an
incidental deployment step.

Read-only help, runnable from the repository root:

```text
node website/server/backup-database.mjs --help
```

The following invocation creates a backup and must wait for reviewed deployment
and explicit operational authorization. The operator first provisions an absolute
backup directory outside the repository/web root, owned by the invoking account,
with mode 0700 and sufficient free disk space. Example production destination:
/root/db_backups. Do not change permissions on a populated directory blindly.

```text
node website/server/backup-database.mjs --output-dir /root/db_backups
```

The tool resolves website/.env relative to its own file regardless of working
directory, parses it with dotenv, and permits existing process variables to take
precedence. Do not source .env as shell code. DB_HOST, DB_PORT (default 5432),
DB_USER, DB_NAME, and DB_PASSWORD are supported. DATABASE_URL takes precedence and
is parsed inside Node; only a simple PostgreSQL URL with optional sslmode is
supported. Unsupported URL options fail closed. DB_SSL=true maps to require
unless the URL supplies sslmode. Review TLS settings for the intended target.
Credentials are passed through child environment fields, never a URL/password
argument. Unrelated application secrets and inherited libpq service settings are
not forwarded. OS administrators can still access process environments: this is
not isolation from a privileged local user.

The CLI requires POSIX permissions and refuses Windows backup execution because
Node mode bits do not establish a restrictive Windows ACL. Offline tests run on
Windows; rehearse real backups on an isolated Linux host with a private directory.

One invocation performs these gates:

1. Check the existing destination is a private directory owned by the operator.
2. Allocate a unique .incomplete-* staging directory and exclusively create a
   mode-0600 archive.partial file. Stream pg_dump custom-format stdout directly
   into that file. The database connection defaults to read-only transactions.
3. Require successful process completion and no stderr; the dump has a 15-minute
   timeout, a 10-second connection timeout, and a 10-second lock wait limit.
4. Require nonempty PGDMP archive magic and successful pg_restore --list, bounded
   to one minute and 16 MB. Listing does not receive database credentials.
5. Require real TABLE and TABLE DATA entries for users, game_stats,
   processed_events, and vip_join_tickets. If either progression ledger table is
   present, also require both ledgers and schema_migrations with their data entries.
   Empty table data entries are valid; their presence does not prove nonzero rows.
6. Flush the archive, atomically hard-link it to a UTC-and-UUID .dump filename
   without replacing an existing file, and flush the destination directory.
   Remove only this invocation's staging link and empty staging directory.
7. Emit backup_archive_verified with filename, size, TOC entry count, timestamp,
   and restoreValidated=false. The exit status is zero only after all gates pass.

Failures emit one fixed backup_<stage>_failed category and nonzero status. They
never echo raw stderr, exception messages, settings, table contents, or TOC text.
The scheduler must capture this sanitized JSON in its protected backup log and
alert on nonzero status, missing scheduled results, or stale last success.
stderr, spawn errors, timeouts, bad structure, and publication errors all fail
closed. Failed staging files remain for private inspection; they are not successful
backups. A failure after publication may leave a verified .dump plus staging link;
check them before retrying. No retention deletion or existing-backup modification
is implemented. The destination filesystem must support same-filesystem hard links
and directory fsync; unsupported storage fails the operation.

## Isolated restore rehearsal — never production

Use a separate disposable Linux VM or container with its own PostgreSQL cluster,
no route to production, no production credentials, and no application services.
A different database name on the production cluster is insufficient isolation.
Use a trusted archive copied read-only into the sandbox with controlled access;
dumps contain private data and restored SQL can execute code. Record the original
archive's SHA-256 without modifying it. Do not run the application or migrations
before validating the restored original snapshot.

1. Record source snapshot time, archive hash/size, database/tool versions, source
   revision and schema versions, and available aggregate baseline. Preserve all
   old SQL/gzip files and provider snapshots. An independently captured audit may
   differ from a live dump due to concurrent writes; exact baseline comparison
   requires a coordinated snapshot or quiesced rehearsal fixture.
2. In the isolated environment, clear inherited PG*, DB_*, and DATABASE_URL
   settings; provision fresh test-only access with PGHOST pointing to its private
   socket or loopback and a test-only PGPASSWORD if needed. Confirm the cluster
   identity, port, and role. These credentials must be incapable of reaching or
   authenticating to production. Never load production website/.env here.
3. Create a new unique database from template0 using the isolated operator role.
   Supply no --create or --clean option to restore; they can reconnect to the
   archived database name or destroy existing objects. Example commands below run
   only inside that isolated environment, after target verification:

```text
createdb --no-password --template=template0 mimita_restore_review_20260906_unique
pg_restore --no-password --exit-on-error --single-transaction --no-owner --no-privileges --dbname=mimita_restore_review_20260906_unique /private/selected-reviewed.dump
```

4. Require both exit statuses to be zero. Keep full restore output private in the
   sandbox; share only fixed categories/status counts. --no-owner and
   --no-privileges deliberately avoid replaying production ownership/grants;
   production role/grant reconstruction needs separate review.
5. Run the reviewed audit against this database using a separate test website
   environment. Confirm schema versions, tables, constraints, indexes, sequence
   state, counts, progression sums, and no orphan/duplicate stats rows. Before
   migration, compare against the archived version, not today's schema. A
   historical snapshot with missing stats must reproduce that observation.
6. Against a second fresh restore, apply the reviewed backend migrations and
   compare before/after aggregates. Existing gold/XP/kills/deaths remain identical;
   legacy seconds convert exactly to ticks; missing stats rows become explicit
   zero rows without claiming lost rewards were recovered. Reapplying migrations
   must not change data again.
7. With synthetic test accounts in the sandbox, validate one PvP reward and victim
   death, NPC reward separation, self/environment death, no-change save, lost
   confirmation retry, and an old acknowledgement arriving after a newer revision.
   Restart the isolated backend/host and verify totals/retry results persist.
   Verify profiles and all five rankings. Use existing backend/game focused tests
   as well as the real isolated integration flow; mocks are not restore proof.
8. Record the restore command, target isolation evidence, archive hash, exit
   status, schema/count comparisons, migration/test results, and remaining human
   gameplay review. Only then label that exact archive restore-validated. Retain
   or decommission only the explicitly identified disposable environment under
   operator control; never delete old user backups as test cleanup.

pg_restore --list reads archive structure, not every compressed data block. Neither
that check nor the fake-tool tests establish payload integrity or recoverability.
Historical plain .sql files need a separately reviewed psql restore with
ON_ERROR_STOP on the isolated target; pg_restore is for custom archives. Never
feed an unreviewed cluster-wide pg_dumpall script into a shared PostgreSQL cluster.

## Reviewed rollout and incident recovery

Before deployment, the main agent owns audit-database.mjs, vps-audit.md, and the
single final changelog. Other agents hand off their exact files and validation;
they do not create parallel changelogs or commits for this work.

After local tests and review, confirm the candidate remote branch, commit, date,
and task with the user according to the deployment procedure. Preserve all dirty
or untracked VPS files; use only the confirmed fast-forward pull. Separately
review when startup migrations execute. Capture the pre-migration audit and a
new verified backup only when authorized; rehearse restoration before changing
production. Confirm deployed commit, migration results, service health, read-only
aggregate invariants, and real save/retry/profile behavior after rollout. Stop
rollout if counts unexpectedly shrink or save acknowledgements disagree with
committed values. Build/test the canonical game executable through the main
workflow when game source changes; the standalone backup tool needs no EXE build.

For a data incident, preserve the present state and old backups first. Inventory
provider snapshots or independently retained dumps before declaring loss. Restore
candidate snapshots into isolation and compare schema/time coverage and aggregates.
Do not merge selected totals, replay untrusted logs, clear processed_events, or
reset session revisions to force acceptance. Quiesce writers only as a separately
authorized recovery action. Review the exact recovery point, unavoidable lost
interval, preserved newer data, and host-session reconciliation before production
cutover. Do not replay a surviving host's cumulative state against an older
database automatically; its acknowledged deltas may no longer exist in the restored
snapshot. Reconcile that gap using reviewed durable evidence or state the loss.
Code rollback is not database rollback; prefer a compatible code revision or
reviewed forward repair, never destructive down-migration by default.

Schedule and retention are operator decisions still to be reviewed: choose a
concrete recovery-point objective, recovery-time objective, cadence, owner,
alerts, and restore-rehearsal frequency. Daily dumps alone can lose roughly a
day's committed progress, plus any never-saved host state. Tighter recovery may
require separately designed base backups/WAL archiving; this tool does not provide
point-in-time recovery. Keep encrypted independent copies off the VPS, verify
transfer hashes, and rehearse access and restoration. Back up required role/grant
definitions, protected secrets, application uploads, and configuration separately:
this is one database logical archive, not a whole-cluster or whole-site backup.
Do not enable automatic retention deletion before reviewing recovery coverage.

## Local verification and references

```text
node --test website/server/backup-database.test.js
node --check website/server/backup-database.mjs
```

Tests use synthetic files and fake PostgreSQL tools, including a real spawned
fake dump process that exits unsuccessfully. They check failed dump/list, empty or
plain SQL output, missing structure, partial ledgers, secret-free reports,
environment parsing, unique publication, and preservation of existing files.
The POSIX permission rejection test is skipped on Windows; deployment rehearsal
must verify permissions and hard-link/fsync behavior on the actual target platform.

PostgreSQL documents [custom-format pg_dump](https://www.postgresql.org/docs/current/app-pgdump.html)
and [pg_restore options and restore security](https://www.postgresql.org/docs/current/app-pgrestore.html).
Use the documentation for the installed PostgreSQL major version during rehearsal.
