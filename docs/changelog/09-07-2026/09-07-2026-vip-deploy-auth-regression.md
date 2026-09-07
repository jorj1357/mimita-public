09-07-2026, 20 25 EST

Branch: 8292026stash
Deployment revision: bbaf43d09ad0f2fb5bcb29d6115171f463c9490e
Purpose: Record the resolved VIP frontend/API mismatch and the new unresolved authentication 500 regression observed after deployment.

Documentation-only change. No source code, database, VPS, environment variable, or service changes were made in this session.

Regression records appended:
- Resolved: the slider was missing because local frontend source expected prepaid/lifetime data while the tunneled VPS API was old. The VPS pull, frontend build, migration, and mimita-api restart brought the API and frontend revisions together.
- Unresolved: after that deployment, browser requests to `/api/auth/me` and `/api/auth/signin` return HTTP 500, while `/api/vip/config` returns success. The exact server exception is not yet known.

Next evidence required: PM2 mimita-api logs at the request time, PostgreSQL readiness/migration state, a direct invalid-sign-in probe that should return 401/400, and environment-presence checks without printing secrets.
