// 09 06 2026, 08 34 EST
# Website auth recovery

## Final state

PostgreSQL cluster `14/main` on the VPS was started. It is online and accepting
connections. The public API's non-mutating invalid-signin probe returns HTTP 401
with `invalid username/email or password`, rather than the previous database
connection HTTP 500. Signup was not submitted because that would create a user
record; the shared database dependency used by signup and signin is recovered.

## Session details

- Branch: `8292026stash`
- Local HEAD at inspection: `7778db22308c73ec3543c207938dfc83961ee15d`.
- EST time: 09-06-2026 08:33-08:34.
- Pre-existing changes: broad documentation edits, deletions, and untracked
  files were present before this session; none were overwritten or reverted.
- Deployed VPS state observed before recovery: `/root/mimita-site`, branch
  `8292026stash`, commit `5c4d846d`; `mimita-api` online; PostgreSQL `14/main`
  down.

## Exact changes

- VPS service action: started existing `postgresql@14-main`; no application
  files, database records, credentials, or configuration files were edited.
- `docs/regressions/regressions-v1.md`: appended the 09-06-2026 resolution,
  cause, fix, and proof after the pre-existing 09-03-2026 report.
- This file is the single required session changelog.

## Documents and skills

- Read `docs/ROUTER.md`, `docs/regressions/regressions-v1.md`,
  `docs/operations/vps-deployment/vps-deployment.md`, and
  `docs/operations/task-completion/task-completion.md`.
- Read focused skills `docs/skills/documentation-checker-v1.md` and
  `docs/skills/logging-checker-v1.md`.

## Validation

- `pg_lsclusters`: PostgreSQL `14 main 5432 online`.
- `pg_isready`: accepting connections.
- `GET https://mimita.fun/api/auth/me`: HTTP 401, expected without a session.
- Non-mutating invalid signin: HTTP 401 `invalid username/email or password`.
- VPS API log: `POST /api/auth/signin` status 401 and
  `AUTH signin=invalid_credentials`; no database refusal on that probe.
- No build was run because no source code changed.

## Human review remaining

The owner should manually sign in and, if desired, create an account through the
website. PostgreSQL may need monitoring or a restart-policy investigation if the
cluster stops again; this session only restored the service and did not alter
production configuration.
