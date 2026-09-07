2026-09-07T16:21:39Z

Branch: 8292026stash
Local HEAD: bbaf43d09ad0f2fb5bcb29d6115171f463c9490e
Purpose: Investigate the authentication HTTP 500 regression after the VIP deployment by collecting VPS-side logs and database/service evidence.

Pre-existing changes: the worktree already contained unrelated local modifications and previously created VIP regression/changelog records. This session did not modify application source, the VPS, the database, environment values, or PM2 configuration.

Documents read: AGENTS.md, docs/ROUTER.md, docs/operations/vps-deployment/vps-deployment.md, docs/regressions/regressions-v1.md. The task was routed as VPS inspection, logging, and regression diagnosis.

Commands/evidence run through the confirmed `mimita-vps` SSH alias:
- Read `/root/mimita-site` Git revision and status.
- Read `pm2 status` and `pm2 describe mimita-api`.
- Read 250 recent `mimita-api` PM2 log lines.
- Checked PostgreSQL with `systemctl is-active postgresql` and `pg_isready`.
- Attempted non-destructive migration/schema inspection; the postgres role requested a password when connecting directly to `mimita_db`, so no database rows or credentials were exposed.

Confirmed result:
- VPS revision: `bbaf43d09ad0f2fb5bcb29d6115171f463c9490e`.
- `mimita-api` was online at `/root/mimita-site/website/server/server.js`.
- PostgreSQL was active and accepting connections.
- PM2 repeatedly logged PostgreSQL error code `42703`: `column "style_revision" does not exist`.
- The error affected `/api/auth/me`, `/api/auth/signin`, `/api/profile/131`, game login, and VIP entitlement subscription sync.
- The migration runner reported success, but the application still lacked the column. The repository migration runner treats the historical bootstrap as version 1, so changing already-applied bootstrap statements does not reliably update an existing database.

Regression documentation updated: `docs/regressions/regressions-v1.md` now includes the direct VPS evidence and records the issue as unresolved.

Remaining human review: implement and locally test a new forward migration for `vip_name_styles.style_revision`; deploy it only through the confirmed Git revision and migration runner; restart `mimita-api`; verify auth, profile, game login, and VIP subscription-sync paths. Do not run an ad hoc SQL patch on production.
