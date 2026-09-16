# Website auth database recovery and guardrails

## Result

PASS_WITH_HUMAN_REVIEW. The repeated outage was confirmed and live service was recovered. Local source now distinguishes database unavailability from generic server errors, the sign-in/sign-up UI explains that no account change occurred, and a non-AI systemd watchdog is staged for deployment.

## Evidence

- Pre-existing working-tree change: `config/analytics.json` only; unrelated to website auth.
- Live at 2026-09-16T17:14:31Z: PM2 `mimita-api` online, PostgreSQL `14/main` down, port 5432 not listening.
- Recovery: started `postgresql@14-main`, restarted only `mimita-api` so its pool reconnected.
- Runtime proof: correctly encoded invalid sign-in returned HTTP 401 `invalid username/email or password`; no real account was created or changed.
- Local validation: `npm run build` passed. Full `npm test` is not a pass because local PostgreSQL-dependent tests fail without the required local database/schema; non-database tests continue to pass.

## Files

- `website/server/server.js`: `ECONNREFUSED` responses are HTTP 503 with `database_unavailable`.
- `website/src/pages/Auth.jsx`: temporary database/network failures get explicit user-facing fallback text.
- `deploy/mimita-db-watchdog.sh`, `.service`, `.timer`: system-owned recovery and alert hook; no AI dependency.
- `docs/regressions/regressions-v1.md`: append-only confirmed incident.

## Still required

Install the watchdog through the approved VPS deployment path, configure an approved `ALERT_COMMAND` if desired, and perform a controlled database outage recovery test. Do not claim sign-up creation acceptance until a human performs a real test account flow.
