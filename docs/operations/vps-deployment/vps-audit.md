// 09 06 2026, 14 40
/* purpose
* Give humans and agents a repeatable VPS and database inspection entry point.
* Compare live schema and aggregate values with the specification and repository.
* Define credential handling and the final session changelog evidence.
* DOES NOT authorize production edits, migrations, or restarts.
* DOES NOT export credentials or individual player records.
* DOES NOT treat backup existence as restore proof.
*/

# VPS read-only audit

Read AGENTS.md, docs/ROUTER.md, the relevant specification, and the VPS
deployment document first. Specifications own desired behavior; the repository
implements it and the VPS runs a deployed revision.

## Database entry point

From C:\mimita-priv-v8 in PowerShell:

```powershell
Get-Content -Raw website/server/audit-database.mjs | ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'cd /root/mimita-site/website && node --input-type=module'
```

Read the audit source before running it. SSH carries the reviewed script through
standard input; it runs in memory without changing a file on the VPS. Once
deployed through Git, it can also run as `node server/audit-database.mjs` from
the website directory.

The deployed environment file is /root/mimita-site/website/.env, not the Git
root. The script parses it with dotenv and passes DB_USER, DB_HOST, DB_NAME,
DB_PASSWORD, DB_PORT or DATABASE_URL to the driver inside the VPS. Passwords
never enter chat, command arguments, or output. Do not source .env as shell
code. Do not import server.js or invoke runMigrations() for inspection.

One repeatable-read, read-only transaction has connection, statement, and lock
timeouts. It prints schema metadata, constraints/indexes, allowlisted counts,
progression totals, negative-value counts, and missing/orphan/duplicate stats
counts. Missing fields are null, distinct from zero. Big integers remain decimal
strings. No arbitrary query or individual user selection is accepted.

Existing SSH/database access may have broader permissions: this transaction is
an execution guard, not a restricted account. Root SSH is not temporary access.

## Services and deployment

```powershell
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'hostname; date -Is; pg_lsclusters; pg_isready; systemctl is-active nginx; pm2 status; ss -lntp'
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'cd /root/mimita-site && git status --short && git branch --show-current && git rev-parse HEAD'
git status --short
git rev-parse HEAD
```

Preserve changed/untracked files. A listener on all interfaces does not prove
external reachability; firewall/authentication inspection is separate. PM2
online does not prove database health.

Compare schema against both website/server/db.js (legacy bootstrap) and
website/server/migrations/ (versioned changes). Absence in the SQL directory
alone does not mean the table is untracked. Compare API expectations, types,
constraints, and applied versions. Missing reward_events is not a defect if
processed_events supplies duplicate protection: inspect behavior, not names.

## Backups and logs

```powershell
ssh -T -o BatchMode=yes -o ConnectTimeout=10 root@107.191.48.226 'find /root/db_backups -maxdepth 1 -type f -printf "%f %s bytes %TY-%Tm-%Td %TH:%TM\n"'
```

Check freshness, nonzero size, dump exit status, archive integrity, and isolated
restore validation. Empty SQL or 20-byte gzip files do not establish a usable
backup. Follow docs/operations/persistence-recovery.md for recovery planning.

Do not stream raw logs into chat. Generic regex redaction cannot guarantee
removal of credentials or private rows. For a specific failure, inspect the
producer and extract only allowlisted error-code counts/status on the VPS.
If reliable filtering is unavailable, record the check as unverified.

## Temporary permissions

The entry point needs no new role. If a separate restricted audit login is
required, an operator should grant only metadata and dedicated aggregate-view
access. Broad table SELECT permits reading private account fields.
Set a concrete UTC password expiration through the administration interface.
Password expiration blocks new password logins; it does not end existing
sessions or revoke SSH. At completion revoke login, end the audit role's
sessions, revoke grants, and drop it. Do not grant future tables through ALTER
DEFAULT PRIVILEGES for a one-time audit. Provisioning is a separately authorized
administrative change. Never paste credentials into chat or commit them.

## Changelog evidence

Write exactly one final session changelog under docs/changelog/mm-dd-yyyy/.
Include UTC, America/New_York with offset (September is EDT), and EST when
required; local/deployed commits; pre-existing edits; exact commands; sanitized
facts; spec differences; failures; skipped checks. Do not store dumps or raw
logs in Git. Distinguish observed facts, suspected causes, build proof, and
human gameplay acceptance. Historical counts must be refreshed each audit.
