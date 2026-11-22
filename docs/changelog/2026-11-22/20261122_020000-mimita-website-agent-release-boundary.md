# MiMITA website agent release authorization and setup boundary

Date: 2026-11-22
Issue: `issue-260667351e04`

## User authorization

The user explicitly authorized authenticated AI agents to fix and deploy routine `mimita.fun` website changes without asking for each release. The standing scope and exclusions are recorded in `AGENTS.md`, the VPS deployment policy, and the issue. It excludes database/schema/data migrations, destructive data changes, credentials/permissions, DNS/billing, and unrelated services.

## Changes in this session

- Documented the standing authorization and the release invariants in `AGENTS.md` and website/VPS operations docs.
- Added a website-only release runbook and retained the package-only helper `deploy/website_release.py` with allowlist and migration rejection.
- Corrected the proposed isolated runtime root to `/root/mimita-site-live` to match the nginx template.
- Removed a duplicate experimental remote deployment helper after reviewing that it could activate a wide website change without checking the production-base migration diff.
- Marked the runbook accurately: live website-only activation is not yet established or validated.

## Investigation and validation

- The production source base inspected in this task is `f94b5636ff3431806b91e4966f85d2ecaa38a738`; the checkout candidate is `935c0e7ed4a702055b553aea19ea6c51b6c31a94`.
- That range includes `website/server/migrations/009_social_features.sql` and broad new website behavior. The release allowlist rejects migrations. No schema migration was run and no website release was pushed or deployed in this session.
- `python -m unittest deploy.test_website_release deploy.test_vps_storage_config`: 19 passed.
- `python -m py_compile deploy/website_release.py`: passed.
- `git diff --check` on the touched policy, runbook, helper and nginx template: passed (line-ending warnings only).
- The overlapping package helper author reported `npm run build` passed and `npm test` had 182 passing / 29 pre-existing failing tests; those results are recorded as their report, not independently rerun here.
- UptimeRobot could not be configured because its browser sign-in surface failed to attach and no provider credential was available. No down/recovery emails have been verified.

## Production status and remaining work

The website remains on the previously running production revision; this session made no production service changes. The user-granted standing permission is durable, but authenticated agents are not yet equipped with a validated live activation path. The next safe step is to identify or prepare a migration-free website release against the actual production base, complete one-time runtime separation and rollback proof, then verify health and the invalid-login probe. Monitor creation and down/up email proof remain pending authenticated UptimeRobot access.