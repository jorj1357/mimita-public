# Bounded authenticated website release path

- Recorded: 2026-11-22T01:38:00Z
- Issue: `issue-260667351e04`
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Local HEAD: `935c0e7ed4a702055b553aea19ea6c51b6c31a94`
- Production website checkout (read-only): branch `8292026stash`, commit
  `f94b5636ff3431806b91e4966f85d2ecaa38a738`.

## What changed

- Adopted and completed the standing website authorization already written in
  the shared checkout before this activation by `website-deploy-authority`:
  `AGENTS.md`, `docs/operations/vps-deployment/vps-deployment.md`, and
  `docs/operations/website-agent-deployment.md`. Those files were uncommitted
  when this activation began and are not claimed as new work.
- Added `deploy/website_release.py`, a website-only preflight packager. It
  validates an exact commit pair, refuses diffs outside the website allowlist,
  refuses `website/server/migrations/` and secret/dependency paths, requires
  the release commit to be reachable from the pushed branch, optionally runs
  `npm test` and `npm run build`, and writes a content-addressed `tar.gz` plus
  manifest under `.dev/website-releases/`. It never contacts the VPS.
- Added `deploy/test_website_release.py` covering allowlist/secrets/migration
  rejection, archive manifest round-trip and tamper detection, symlink and
  out-of-scope member rejection, and git change detection.
- Added a `Release packager` section to
  `docs/operations/website-agent-deployment.md` and routed that document from
  `docs/ROUTER.md`.

## Evidence

- `python -m unittest deploy.test_website_release`: 11 tests passed.
- Packager smoke test against HEAD: archive
  `.dev/website-releases/website-935c0e7ed4a7.tar.gz`, 275 entries,
  5,238,974 bytes, verified archive SHA-256
  `ea0bad81e3fe12b6408dd4751dd464cdbb9dbaac6e70e791b78153f490094c41`.
- `python -m deploy.website_release validate --from f94b5636... --to HEAD`:
  correctly refused the pending release because it contains
  `website/server/migrations/009_social_features.sql`; database migrations are
  outside the standing authorization.
- `npm run build` in `website/`: passed in 1.53 s.
- `npm test` in `website/`: 182 passed, 29 failed, 1 skipped, 212 total. The
  failures are in pre-existing VIP/payment, profile, and auth suites and were
  not introduced by this change. A failed local check must block a release.
- Read-only VPS inspection confirmed `mimita-api` online, PostgreSQL accepting
  connections, nginx active, nginx root
  `/root/mimita-site/website/dist`, and `/` at 76% with 5.3 GB free.

## Scope and proof status

- No production deployment, service restart, database change, or secret change
  was performed.
- The one-time runtime separation from the monorepo checkout, the first
  website-only activation, post-deploy readiness/login verification, and
  forced down/recovery email proof remain outstanding. UptimeRobot setup needs
  an authenticated account session and must not be claimed until both alert
  types are observed.
- A release of the pending website changes is blocked by the migration guard
  and by the 29 failing local tests. Both are correct stop conditions, not
  bypasses.
- Unrelated concurrent edits in the working tree were preserved and are not
  included in this record.
