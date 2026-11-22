# MiMITA.fun database outage investigation

## Task

- Issue: `issue-1759b2cc6e65` — investigate recurring login/database outage and verified email alerts
- Owner: `database-login`
- Time: `2026-11-22T00:09:27Z` (2026-11-21 19:09 EST, America/New_York)
- Local branch/base: `checkpoint/one-checkout-condensation-20261111` at `935c0e7ed4a702055b553aea19ea6c51b6c31a94`
- VPS clone branch/commit observed: `8292026stash` at `f94b5636ff3431806b91e4966f85d2ecaa38a738`
- Result: Live recurrence confirmed and tracked. No production repair, cleanup, restart, deployment, or email monitor configuration was performed.

## Evidence gathered

At `2026-11-21T23:58:51Z` through `2026-11-22T00:04Z` UTC:

- Public `https://mimita.fun/`: HTTP 200.
- Public `GET /api/health/ready`: HTTP 404 (`Cannot GET`), even though the local source mounts this route.
- Credential-free invalid sign-in probe to `/api/auth/signin`: HTTP 500, `database connection failed. check server logs.` No real account credentials were submitted.
- Read-only VPS check: `postgresql@14-main` failed; `pg_lsclusters` showed `14/main` down; `pg_isready` reported no response. `mimita-api` remained online in PM2.
- Root filesystem: 23 GB total, 100% used, zero bytes available.
- `/var/log/syslog.1`: 4,806,849,520 bytes. `/etc/turnserver.conf` had the standalone `verbose` directive enabled. A bounded 1 MiB tail sample had 6,127 `turnserver` strings and 1,555 `BINDING` strings.
- PostgreSQL data directory: 373 MiB. The available PostgreSQL log evidence did not establish the exact shutdown error.
- The documented watchdog executable and systemd unit were absent on the VPS.

The confirmed failure chain is: the public page is served -> the readiness route is missing -> sign-in cannot reach PostgreSQL -> PostgreSQL is down while the root filesystem is full. The very large syslog, enabled Coturn verbosity, and TURN-heavy bounded sample strongly implicate verbose Coturn logging as a major disk consumer, consistent with the September/November regression record. They do not prove the exact reason PostgreSQL stopped or account for all disk usage.

## Source and regression comparison

Local `website/server/server.js` mounts `createHealthRouter` at `/api/health` and maps recognized database-unavailable errors to HTTP 503 with `code: database_unavailable`. `website/server/database-errors.js` recognizes PostgreSQL connection and shutdown codes. Live `/ready` 404 and auth 500 show these local protections are not present in the production behavior observed in this incident.

Appended this occurrence to [the website auth/database regression](../../regressions/gameplay/website-auth-database-REG-2026-09-16.md) and updated its [domain index](../../regressions/gameplay/README.md). The record now keeps the old occurrence and its uncertain disk causality intact, and labels the current Coturn/log finding as strongly implicated rather than proven.

## Issue tracker

- Canonical issue: `issue-1759b2cc6e65`, state `in_review`, owner `database-login`.
- Both user messages are preserved and linked; the follow-up requests down and recovery emails.
- Read-only VPS diagnosis and local safeguard/regression review tasks are complete with evidence-only exemptions.
- Still open: recover PostgreSQL only after the documented 5 GiB free-space floor is met; review/deploy an exact approved website revision; configure HTTPS monitors and verify both down and recovery emails to `contact@ravgrowth.com`.
- An accidental duplicate issue `issue-f8b4b3a4716d`, created when the first issue intake call failed after issue creation, was closed as a duplicate of the canonical issue.
- The generated `docs/issues/README.md` index was updated by the issue tracker.

## Validation

- `node --test website/server/database-errors.test.js website/server/health.test.js`: **11 passed, 0 failed**. This validates local code only.
- `node --test server/database-errors.test.js server/health.test.js server/authCore.test.js` from `website/`: **16 passed, 1 failed**. The separate failure is `authCore.test.js` expecting `normalizeUsername("Mi mita User")` to remove internal whitespace (`MimitaUser`), while current source collapses whitespace (`Mi mita User`). It was not changed because it is not the database outage cause.
- No game build was relevant. No application source was edited, so there is no new build or deployment proof.
- Browser access for UptimeRobot setup stopped before the site opened: the Windows browser-control layer reported that URL policy enforcement was unsupported for the current browser. No monitor or notification settings changed. No UptimeRobot connector was available in the connected tool list.

## Safety and remaining work

No VPS files were written, deleted, or edited. No services were restarted, no database queries or mutations were run, and no notifications were sent. PostgreSQL must not be started while the documented 5 GiB disk floor is unmet. The VPS deployment instructions require explicit approval before cleanup and confirmation of the exact branch/commit before deployment; the observed VPS branch is not authorization to deploy it. Current production login remains broken until safe disk recovery and verified PostgreSQL restart.

