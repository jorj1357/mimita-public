# Regression: website auth database outage on 2026-09-16

## Summary

Sign-in and sign-up failed while the website/API still appeared online. The
Node process was healthy from PM2's perspective, but PostgreSQL `14/main` was
down and port 5432 had no listener.

## Expected behavior

- Sign-in and sign-up use the same reachable PostgreSQL dependency.
- If the database is unavailable, the API reports a temporary account-service
  outage with a structured status; it must not claim that an account changed.
- A normal operations monitor detects and recovers the database independently
  of AI assistance.

## Actual behavior

- `mimita-api`: online.
- PostgreSQL `14/main`: down.
- Port 5432: no listener.
- Auth request: HTTP 500 with generic `server error`.
- Sign-up and sign-in were both affected because both query PostgreSQL.

## Root cause

PostgreSQL was not supervised/recovered together with the API, and the
application mapped `ECONNREFUSED` to generic HTTP 500. There was no structured
client-visible outage state. The working-tree change in `config/analytics.json`
was unrelated to the website.

## Recovery proof

1. Started the existing `postgresql@14-main` service.
2. Restarted only `mimita-api` so its connection pool recreated connections.
3. Verified `pg_isready` accepted connections.
4. Sent a correctly encoded invalid sign-in probe; it returned HTTP 401
   `invalid username/email or password`, proving the query reached PostgreSQL.
5. No real account was created or modified.

## Permanent prevention

- Database connection failures now return HTTP 503 with
  `code: database_unavailable`.
- The auth UI explains that the account was not changed.
- `deploy/mimita-db-watchdog.sh` plus its systemd service/timer provides
  ordinary non-AI recovery and an optional alert command.
- Website editing rules in `docs/operations/vps-deployment/vps-deployment.md`
  require preserving this contract and proving both database and API health.

## Remaining acceptance

Install the watchdog through the reviewed deployment path and run a controlled
database stop/start recovery test. Then perform a human sign-in and sign-up
test; the invalid-login probe alone does not prove account creation.

---

## Dated correction and recurrence — 2026-11-02

Time:
`2026-11-02T14:04:34Z`

Correction to the earlier prevention note: the 2026-11-02 local repository audit
did not contain `deploy/mimita-db-watchdog.sh` or its referenced systemd units.
The earlier text described that watchdog as implemented, but this checkout does
not support that claim. The production install state was not verified in this
audit. This session adds a different tracked watchdog under `deploy/`; it is not
production-active until deployed and verified.

### Observed recurrence

- The user reports this is the third recurrence; independent timestamped
  evidence for each earlier recurrence is not available in this audit.
- The API process was online while PostgreSQL 14/main was stopped and rejecting
  connections. The public site still served HTTP 200, while account API database
  requests failed.
- Root filesystem: 23 GB total, 22 GB used, approximately 29 MB available.
- `/var/log/syslog.1` was approximately 1.44 GB, current `/var/log/syslog`
  approximately 235 MB, and the systemd journal approximately 920 MB.
- Coturn `verbose` logging was enabled; sampled syslog files contained millions
  of routine session messages.
- PostgreSQL's recorded failure was earlier than the largest sampled syslog
  file's growth. No contemporaneous disk measurement or PostgreSQL error proves
  that disk exhaustion stopped PostgreSQL.

### Expected behavior

Database-dependent account requests return HTTP 503 with
`code: database_unavailable` and state that no account change was confirmed.
Monitoring marks readiness unhealthy when PostgreSQL cannot answer a query or
the disk safety floor is breached. A bounded recovery attempt runs only when at
least 5 GB is free.

### Actual behavior

The database was unavailable while the API process and public page remained
online, making the outage easy to mistake for a healthy site. Available
evidence establishes the database outage and severe storage pressure; it does
not establish storage pressure as the PostgreSQL stop cause.

### Wrong code observed in the prior API contract

File: `website/server/server.js` (prior generic error handler)

```js
res.status(500).json({
    success: false,
    message: "server error"
})
```

### Attempted Fix 1

Related changelog: `docs/changelog/2026-11-02/20261102_142003-website-database-storage-turn-plan.md`

Local changes classify PostgreSQL availability errors as HTTP 503, add a
detail-free database-and-disk readiness endpoint, add bounded PostgreSQL
recovery and verified encrypted-backup scheduling files, cap journald use,
reduce Coturn verbose logging in the deployment runbook, and disable TURN relay
in the coordinator policy for the planned direct-only test. These changes have
not been deployed. The backup schedule cannot be enabled until the encrypted
Drive remote is verified for the intended account and a restorable archive is
created.

### Cause and confidence

Confirmed: PostgreSQL was down and the API's database-dependent requests could
not reach it. Confirmed: the VPS root filesystem was nearly full and repeated
Coturn log messages were a major sampled log producer. Suspected only: storage
pressure contributed to PostgreSQL stopping. The order and timestamps of the
sampled evidence do not prove that causal link.

### Corrected code

File: `website/server/server.js`

```js
if (isDatabaseUnavailable(error)) {
    return res.status(503).json({
        success: false,
        code: "database_unavailable",
        message: "Account service temporarily unavailable. No account change was confirmed. Please try again later."
    })
}
```

### Proof and remaining acceptance

Automated local validation and build evidence will be recorded in the linked
changelog. There is no production deployment, database recovery, verified
backup, isolated restore, external email-alert proof, or two-PC TURN/direct-path
proof in this occurrence yet. Keep the occurrence unresolved until a human
verifies the production recovery and monitoring behavior. Do not label storage
as the root cause without new evidence.

### Recovery progress — 2026-11-02T14:19:09Z

- Removed only the unused `/root/.vscode-server/cli/servers` binary cache after
  checking that no VS Code server process was running. The separate data and
  extensions directories were preserved.
- Reduced the systemd journal from about 920 MB to 200 MB, per the proposed
  retention cap. Compressed `/var/log/syslog.1` from about 1.44 GB to about
  80 MB and verified the gzip archive; this preserves the rotated log.
- PostgreSQL was started after root free space exceeded 5 GiB. `pg_isready`
  accepted connections and a read-only `SELECT 1` succeeded.
- Created `/root/mimita-db-verified-backups/mimita-2026-11-02T14-18-04-435Z-4627313a-be29-4da4-9294-a69f88aa84d8.dump`.
  The existing tool reported `backup_archive_verified`, 7,178,142 bytes, 443
  archive entries, and `restoreValidated=false`. No prior backup files were
  deleted or changed.
- The public invalid-sign-in probe returned HTTP 401 with the expected invalid
  credentials response, showing that the live auth query reached PostgreSQL.
  It recorded an invalid attempt; it did not create or change an account.
- Root filesystem free space after the backup was 5,695,148 KiB (about 5.4 GiB).
- The VPS has only a `gdrive:` remote; its identity could not be verified as
  `contact@ravgrowth.com`, and there is no `gdrive-crypt:` remote. The archive
  remains local and is not yet offsite-encrypted.

This recovery is operational only. No website/coordinator source or service
configuration was deployed. Coturn verbose logging and the public monitor setup
remain pending the reviewed deployment. PostgreSQL's original stop cause is
still unproven; disk pressure remains a suspected contributor.
