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
