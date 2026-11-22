# issue-1759b2cc6e65: Investigate recurring mimita.fun login and database outage; add verified email alerts

## Human report (verbatim, immutable)

> &#x20;why mimita.fun  i cant login database issue  can u look at regressionadn make a issue tacker for this an dinvestagaet and fix   investigate and fix as much as u can  and  make sur ethat its not like a regerssion like what made the probelm


## Triage

### 2026-11-21T23:59:30.868Z — database-login

Summary: Confirmed live recurrence of the September website PostgreSQL outage: homepage is up while readiness route is missing and auth fails with database connection error; PostgreSQL is down at 100% disk. Identify why, add prevention and email monitoring, and require tested recovery.
Owner: database-login
Priority: high

## Context

### 2026-11-22T00:02:02.369Z — database-login

evidence: {'text': 'Confirmed live recurrence at 2026-11-21 23:58 UTC: homepage HTTP 200; /api/health/ready HTTP 404; credential-free invalid sign-in HTTP 500 message database connection failed; PostgreSQL 14/main down; pg_isready no response; root filesystem 100%, 0 bytes available; mimita-api PM2 online. VPS source revision f94b5636ff3431806b91e4966f85d2ecaa38a738 on branch 8292026stash; local source already contains health route and 503 mapping. Watchdog absent. /var/log/syslog.1 is 4,806,849,520 bytes, and /etc/turnserver.conf has verbose enabled; last 1MiB contains 6,127 turnserver strings and 1,555 BINDING strings. This strongly implicates the previously documented verbose Coturn logging as a major disk consumer, but does not prove it alone caused PostgreSQL shutdown. PostgreSQL data directory totals 373MB; root cause of space use remains broader. Recovery blocked by full disk safety floor.'}
context_id: ctx-507fb0fd595e

## Review (needs-evidence)

### 2026-11-22T00:07:05.913Z — database-login

Reviewer: database-login

Observed live recurrence at 2026-11-21T23:58Z–2026-11-22T00:04Z UTC. Public homepage 200; readiness 404; credential-free invalid sign-in 500 database connection failed. Read-only VPS state: API online, PostgreSQL 14/main down, pg_isready no response, / is 100% used with 0 bytes free. /var/log/syslog.1 is 4,806,849,520 bytes; Coturn verbose is enabled and a bounded tail sample had 6,127 turnserver and 1,555 BINDING occurrences. This is the leading disk-pressure mechanism and matches the earlier regression, but PostgreSQL's own log has no entries that establish its exact shutdown cause. Local source already contains /api/health/ready and 503 database_unavailable handling; deployed API does not expose the health route. Watchdog unit/script absent. No production files changed, deleted, or restarted. Local focused db-error/health tests pass; the adjacent authCore.test has one separate pre-existing assertion mismatch (expected inner whitespace removal, current normalizeUsername collapses it). The issue remains open for approval-gated disk recovery, reviewed deployment, and external monitor/email verification.

## Agent discussion and suggestions

### 2026-11-22T00:09:43.501Z — database-login

Message: `msg-a1f35478311e` (announce)

Subject: intent: Create the required final changelog for issue-1759b2cc6e65; summarize production evidence, regression history, tests, and unresolved recovery/alert work

Create the required final changelog for issue-1759b2cc6e65; summarize production evidence, regression history, tests, and unresolved recovery/alert work

## Review (needs-evidence)

### 2026-11-22T00:09:59.165Z — database-login

Reviewer: database-login

Alert-configuration attempt blocked: no UptimeRobot-specific connector is available, and Windows browser automation stopped before opening the monitoring site with 'URL policy enforcement is not yet supported for the current Windows browser.' No provider settings or notifications were changed. The tracker explicitly requires the selected HTTPS monitors and proof that down and recovery email notifications arrive. Evidence setup and provider work remain open.

## Review (needs-evidence)

### 2026-11-22T01:06Z — database-login

Recovery verified: after explicit user approval, only the verified regular
`/var/log/syslog.1` (4,806,849,520 bytes, not open) was truncated. Available
disk space rose from 892,751,872 to 5,699,633,152 bytes, above the 5 GiB
database start floor. A single PostgreSQL start was attempted; a later
read-only check found the unit active and `pg_isready` accepting connections.
Public homepage returned HTTP 200 and one invalid-credentials sign-in returned
HTTP 401 (the probe recorded one failed-login/analytics event); readiness still
returns 404. Watchdog and email monitoring are not live. Coturn verbose logging
remains enabled. Exact PostgreSQL stop cause remains unproven. No production
website source or configuration was deployed: a full branch pull to this
website-only VPS would include 854 tracked game-source files. Focused storage
configuration tests passed 8/8. Leave issue open for policy-safe reviewed
deployment, health/readiness verification, watchdog prevention, and observed
down plus recovery email delivery.

## Review (needs-evidence)

### 2026-11-22T01:03:12.709Z — database-login

Reviewer: database-login

2026-11-22 recovery update: the user approved clearing /var/log/syslog.1. Before cleanup it was a regular, non-symlink 4,806,849,520-byte file owned syslog:adm and not open; root availability was 892,751,872 bytes. Truncated only that exact file; it is now 0 bytes and root has 5,699,633,152 bytes available (76%), above the 5 GiB floor. A single systemctl start postgresql@14-main attempt was made; a later read-only check confirmed unit active, Result=success, pg_isready accepting connections, active since 2026-11-22T00:33:25Z. Public homepage remains 200; a unique invalid-credentials sign-in returns 401 invalid username/email or password, confirming the auth query reaches the database; this test adds one failed-login/analytics event row, but creates or changes no account. /api/health/ready remains 404. Current VPS checks: watchdog script/unit absent; Coturn verbose still enabled; default logrotate.timer runs daily. The UptimeRobot site loads in the in-app browser, but its dashboard requires sign-in; no authentication or monitor setting was changed. The VPS clone tracks 854 src/ files with sparse checkout unset, so a full branch pull would touch game source, contrary to the user's website-only VPS policy. Local safeguards already exist in the current source branch, but are not deployed. Keep issue open for safe website-only deployment and authenticated monitor setup/down-recovery email verification.

## Review (needs-evidence)

### 2026-11-22T01:07:33.397Z — database-login

Reviewer: database-login

Recovery verified 2026-11-22: with explicit user approval, only /var/log/syslog.1 (regular non-symlink, 4,806,849,520 bytes, not open) was truncated after confirming 892,751,872 bytes free. Available space became 5,699,633,152 bytes (76%), above the 5 GiB start floor. One PostgreSQL service start was attempted; later read-only checks show unit active, Result=success, pg_isready accepting. Public homepage 200 and a single invalid-credentials login returned expected 401 (probe generated one failed login/analytics row); /api/health/ready remains 404. Regression appendix/index updated. Existing local safeguards are not deployed. Watchdog absent, Coturn verbose remains enabled, and alert provider requires account sign-in; no email notification is live/verified. Full branch pull would add/update 854 tracked game-source files on website-only VPS, so deployment remains blocked pending a safe reviewed website-only release route and exact release identity. Exact database stop cause remains unproven. Focused storage configuration unittest: 8/8 passed. Keep issue open.
