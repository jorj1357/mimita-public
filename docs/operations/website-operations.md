// 2026-11-02
/* purpose
* Give operators a safe checklist for website health, VPS storage, backups, and recovery.
* Define how website and coordinator changes reach production from Git.
* DOES NOT authorize schema changes, data deletion, or deployment without a reviewed revision.
* DOES NOT replace isolated restore rehearsal or the VPS database audit.
*/

# Website and VPS operations

Audience: MiMITA website operators and developers.

## Production editing rules

The local repository is the source of truth. Make source and tracked configuration
changes locally, validate them, commit and push the reviewed revision, and deploy
only the branch and commit approved for the VPS. On the VPS, use
`git pull --ff-only origin <confirmed-branch>` from the verified service clone.
Do not edit application code or configuration with an SSH editor, `scp`, or an
ad hoc patch. Preserve VPS untracked files and report them before deployment.
Never print, source, or paste `/root/mimita-site/website/.env`; check only whether
required variables are present.

The website frontend is under `website/`; its production files are `website/dist/`.
Any deployment that changes `website/src/` must run `npm run build` from
`/root/mimita-site/website` before restarting only `mimita-api`. The coordinator
is a separate PM2 service. Inspect its actual entry point and Git state before
deploying it; do not assume its directory is a repository clone.

## Quick health and storage checks

Run from PowerShell on the development machine:

```powershell
# Public website response (does not prove the database is healthy).
curl.exe -sS -o NUL -w "homepage HTTP %{http_code}; total %{time_total}s`n" https://mimita.fun/

# These readiness routes become available after the reviewed website revision is deployed.
curl.exe -sS -i https://mimita.fun/api/health/ready
curl.exe -sS -i https://mimita.fun/api/health/storage-warning
curl.exe -sS -i https://mimita.fun/api/health/storage-critical
curl.exe -sS -i https://mimita.fun/api/health/backup

# VPS database, disk, and service status.
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'date -Is; df -h /; pg_lsclusters; pg_isready; systemctl is-active nginx; pm2 status'

# Largest top-level log and root-directory consumers (stay on the root filesystem).
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'sudo du -xhd1 /var/log /root 2>/dev/null | sort -h | tail -n 20'

# Recent service messages when the public check or database check fails.
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'journalctl -u postgresql@14-main -u nginx --since "1 hour ago" --no-pager -n 100'
```

At the 2026-11-02 check, the homepage returned HTTP 200 and PostgreSQL accepted
connections, while `/api/health/ready` returned HTTP 404. The readiness routes
are in the local source but are not live until that reviewed revision is
deployed. Until then, use the VPS checks above; a homepage 200 only proves the
web server returned a page. Review logs before sharing them and remove any
account identifiers or other sensitive values.

Use `df -h /` for free bytes and percentage. Use bounded directory/file-size
summaries to identify the producer before cleanup. Do not run broad recursive
deletion commands. Preserve PostgreSQL data, valid backups, snapshots, active
service files, and unrelated untracked files. Clean only identified log/cache
files whose owner and active use have been checked.

The site monitor treats less than 5 GB free as unhealthy. Contact the operator
at 80% disk use; treat 90% as critical. Coturn's `verbose` setting previously
produced millions of routine session lines in syslog. Keep STUN available, turn
off verbose per-session logging, and bound systemd journal/syslog retention so a
relay diagnostic cannot consume the database's free space.

The tracked `deploy/journald-mimita-storage.conf` caps journald at 200 MB and
preserves 5 GB free where possible. Install it only through a reviewed pull,
then run the guarded repo-managed policy helper. It updates the existing rsyslog
stanza to `daily`, `rotate 7`, `maxsize 100M`, `compress`, and `delaycompress`
instead of adding a duplicate rule. It validates the complete logrotate config
and keeps a root-only copy of the previous stanza. The same helper installs the
tracked journald limit and restarts only journald. Remove only the standalone
`verbose` directive from the inspected Coturn config; the helper preserves its
secret values and every other setting and keeps a root-only pre-change copy. It
does not restart Coturn. Check `ss -uapn` for active relay allocations and wait
for them to drain before restarting; a restart can interrupt current relay
sessions. If Coturn does not start, restore the exact root-only backup with the
helper before restarting again.

The Ubuntu package indexes include duplicate definitions for
`archive.ubuntu.com` and `ubuntu.mirror.constant.com`. The guarded helper removes
mirror entries only when their suite, components, architecture options, and
repository path exactly match a canonical entry. It preserves the canonical
source and NodeSource. Do not run it while APT or dpkg holds a package lock.
Afterwards run `apt-get update` and `apt-get clean`, then verify APT completes
successfully and the Ubuntu indexes have refreshed. To prune Snap storage, first
capture `snap list --all`; remove only rows marked `disabled` with Snap's own
`snap remove <name> --revision=<revision>` command. Never manually delete APT or
Snap files.

## Install the tracked safeguards

After the local revision is committed, pushed, and confirmed as the VPS target,
pull it using the deployment procedure. Before enabling backups, confirm that
the root-owned rclone config has a `gdrive-crypt:` remote of type `crypt` backed
by `gdrive:`, verify the OAuth account is `contact@ravgrowth.com`, and confirm
the encryption key is available in the password manager. The uploader checks
that the config file is root-owned mode 0600 and verifies the account without
printing its identity result. Never print the rclone config or key. The
scheduled uploader verifies the encrypted copy before it applies the 30-day
Drive and one-local-copy retention rules.

The tracked service files install from the pulled repository as follows:

```bash
install -d -o root -g root -m 0700 /root/mimita-db-verified-backups
install -d -o root -g root -m 0755 /var/lib/mimita-database-backup
install -o root -g root -m 0750 deploy/mimita-postgres-watchdog.sh /usr/local/sbin/mimita-postgres-watchdog.sh
install -o root -g root -m 0750 deploy/mimita-database-backup.sh /usr/local/sbin/mimita-database-backup.sh
install -o root -g root -m 0644 deploy/mimita-postgres-watchdog.service /etc/systemd/system/
install -o root -g root -m 0644 deploy/mimita-postgres-watchdog.timer /etc/systemd/system/
install -o root -g root -m 0644 deploy/mimita-database-backup.service /etc/systemd/system/
install -o root -g root -m 0644 deploy/mimita-database-backup.timer /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now mimita-postgres-watchdog.timer
```

Apply the tracked logging policy after the pull:

```bash
python3 deploy/vps_storage_config.py --log-policy
```

The helper writes the root-only configuration backups and restarts journald.
For the duplicate Ubuntu source, wait until all APT work has finished, then run
the guarded helper followed by a metadata refresh and package-cache cleanup:

```bash
python3 deploy/vps_storage_config.py --apt-mirror
apt-get update
apt-get clean
```

The helper refuses to edit the source if the mirror definitions are not exact
duplicates or if APT/dpkg holds a package lock. To change Coturn logging, first
check `ss -uapn` for active relay allocations, then run:

```bash
python3 deploy/vps_storage_config.py --coturn
systemctl restart coturn
systemctl is-active coturn
ss -uapn | grep -E 'turnserver|:3478|:5349'
```

The helper prints the root-only backup filename without printing config
contents. If Coturn fails to start, restore it using
`python3 deploy/vps_storage_config.py --restore-coturn-backup <printed-name>`
and restart the service.

Enable `mimita-database-backup.timer` only after the remote identity and
encryption checks above pass, PostgreSQL is healthy, and one full archive has
been verified locally. Test the first encrypted upload and `rclone check`, then
enable the daily schedule:

```bash
systemctl start mimita-database-backup.service
systemctl status mimita-database-backup.service --no-pager
journalctl -t mimita-database-backup --since '2 days ago' --no-pager
rclone lsf gdrive-crypt:mimita-database-backups --files-only | sort | tail -n 5
systemctl enable --now mimita-database-backup.timer
systemctl list-timers --all mimita-database-backup.timer
```

After the timer has fired on schedule and that timer-triggered encrypted upload
has completed and been verified (a manual service run does not prove the
schedule works), retire only the duplicate root-crontab database dump and its
old `/root/db_backups` prune entry:

```bash
python3 deploy/vps_storage_config.py --retire-legacy-db-cron
```

The command refuses to proceed unless the new timer is enabled and active and
its verified success marker is newer than 26 hours. It saves the original root
crontab with mode 0600 in the protected backup directory. It leaves the
03:15 VPS snapshot sync, the schema-only PostgreSQL schedule, and unrelated
cron jobs untouched.

The backup job writes only `success <unix-time>` or `failure <unix-time>` to
`/var/lib/mimita-database-backup/status`; the public health response reveals
only healthy/unavailable. The endpoint is unavailable if no verified upload
exists, the last success is older than 26 hours, or the latest attempt failed.
Configure its external monitor after the first verified upload makes
`/api/health/backup` return HTTP 200, and verify down/recovery email alerts to
`contact@ravgrowth.com`. The watchdog will not start
PostgreSQL with less than 5 GB free and makes one automatic start attempt per
outage. If it fails, it waits for operator action; a healthy database clears
the attempt marker for a future outage.

## Database outage recovery

1. Stop schema migrations and data edits. Check `df -h /`, `pg_lsclusters`,
   `pg_isready`, `systemctl status postgresql@14-main`, nginx, and PM2.
2. If root has less than 5 GB free, do not repeatedly start PostgreSQL. Identify
   the confirmed log/cache producer and recover space without touching
   `/var/lib/postgresql`, database dumps, or snapshots.
3. With at least 5 GB free, run the bounded watchdog once:
   `sudo systemctl start mimita-postgres-watchdog.service`. It attempts one
   PostgreSQL start and verifies `pg_isready`; it does not loop when disk space
   is low or recovery fails.
4. Verify `/api/health/ready` returns HTTP 200 and use a non-mutating invalid
   sign-in probe that returns HTTP 401. A successful sign-in/signup still needs
   a human account-flow check.
5. If PostgreSQL will not start, preserve its data directory and sanitized
   status/error-code evidence. Do not restore over the production cluster.
   Follow `vps-audit.md` and rehearse any restore in a separate disposable host.

An online API process is not database proof. The API readiness endpoint must
query PostgreSQL and check disk headroom; a PostgreSQL restart must not be
attempted when the 5 GB floor is unmet.

## Database backup before a migration or data edit

Do not change database schema or data until a full database archive exists and
has passed the backup checks. First ensure PostgreSQL is healthy and there is
enough free disk for a dump. From the website directory on the VPS, run the
tracked backup tool:

```bash
cd /root/mimita-site/website
node server/backup-database.mjs --output-dir /root/mimita-db-verified-backups
```

Accept only a `backup_archive_verified` result with a nonzero custom-format
`PGDMP` archive and expected table data entries. Upload the archive to the
dedicated encrypted Google Drive backup remote and verify the remote copy before
retention cleanup. Keep one verified local archive and 30 daily Drive copies.
The first isolated restore rehearsal must use a disposable host with no route to
production. An archive listing alone is not restore proof.

The root-only rclone config, Drive OAuth token, and encryption key are runtime
secrets; never commit or print them. Store the decryption key separately in the
operator's password manager. A failed dump or upload must never delete the last
verified local or Drive backup.

The old `/root/db_backups` directory is owned by the PostgreSQL service account
and contains historical small/empty files. Preserve it; the verified archive
tool uses a separate root-owned mode-0700 directory.

The 2026-11-02 read-only Drive audit found `full_dump-2026-11-02.sql` and
`full_dump-2026-11-01.sql` at zero bytes, with corresponding 20-byte compressed
files. These are not backups. The VPS has two overlapping 03:00 database dump
cron entries; one script logs repeated localhost connection failures, and a
local-socket `pg_dumpall` attempt timed out while `pg_isready` and a simple
local query succeeded. This points to a broken legacy dump path, but does not
prove which concurrent command produced each empty artifact. Keep those old
files for investigation and do not count the plain `gdrive:` copies as current,
valid backups. Disable only the confirmed redundant database cron entries
after the new encrypted job has completed successfully.

At that audit the VPS had one fresh 7,178,142-byte local custom-format archive
whose `pg_restore --list` check passed, but `restoreValidated=false`. The VPS
had only `gdrive:` configured; there was no encrypted `gdrive-crypt:` remote,
the Drive account identity was not verified, and the new backup timer was
absent. Treat the local archive as verified for structure, not yet as
restore-proven.

## External alerts

Configure an external HTTPS monitor for:

- `https://mimita.fun/` — public page availability.
- `https://mimita.fun/api/health/ready` — database query and minimum disk headroom.
- `https://mimita.fun/api/health/storage-warning` — 80% storage warning.
- `https://mimita.fun/api/health/storage-critical` — 90% storage critical state.
- `https://mimita.fun/api/health/backup` — recent encrypted database backup verified.

Use down and recovery email alerts to `contact@ravgrowth.com`. The health paths
return only a generic status, never database or host details. The external
monitor can still alert when the VPS/API itself is unavailable.

The selected monitor is UptimeRobot. Create separate HTTPS monitors for the
five URLs above, use HTTP status checks, and configure down/up email
notifications to `contact@ravgrowth.com`. Do not treat this as configured until
both a forced down alert and a recovery alert arrive. The provider account
configuration remains an external setup step.

## Troubleshooting notes

- `database connection failed` / `database_unavailable`: check PostgreSQL and
  disk first, then use the API's sanitized database error category.
- HTTP 401 from an invalid sign-in means the auth query reached PostgreSQL; it
  does not prove signup or a real account login.
- HTTP 200 from `/` and PM2 `online` do not prove database readiness.
- Failed backups after PostgreSQL stops are expected; zero-byte or schema-only
  files are not complete database backups.
- TURN `BINDING` logs and Coturn session-cleanup events do not prove a gameplay
  session selected a relay. Inspect both peers' `ICE_PATH` runtime events.
- Record confirmed failures append-only in the owning regression file. Mark
  storage as a suspected cause until evidence shows it was full when PostgreSQL
  stopped.
- The direct-only TURN test disables relay credentials in
  `coordinator-server/turn-policy.json`; it leaves coordinator room discovery,
  signaling, and STUN enabled. Record activation time only when the deployed
  policy actually changes, then validate two peers on separate networks and
  require direct `ICE_PATH` evidence from both.
