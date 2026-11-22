# Website-only deployment for authenticated agents

This document records the standing authorization and current release workflow.

**Current setup status:** the local allowlist/package preflight exists, but the live website-only activation path is not yet established or validated. Do not use an ad hoc remote script or claim agents can deploy live until that setup is implemented and reviewed. The inspected source range also contains a SQL migration, which this authorization excludes.

**Audience:** authenticated AI agents and the website operator.

**Does not:** authorize database/schema/data changes, secret or permission
changes, deletion of production data/backups, DNS/billing changes, or changes to
services outside the website and its explicitly listed safeguards.

## Standing authority

On 2026-11-22 the user authorized AI agents with working GitHub and
`ssh mimita-vps` access to fix, validate, commit, push, and deploy routine
website work without asking for approval on each release. This permission is
recorded in `AGENTS.md` and issue `issue-260667351e04`. It applies to the
currently checked-out shared branch; agents must not make another branch or
checkout for the work.

The authorization covers the website UI/API and the following website-owned
availability safeguards:

- `website/` except local dependencies and runtime secrets;
- `content/articles/` when the commit intentionally changes tracked site
  content;
- `deploy/mimita-postgres-watchdog.sh` and its service/timer;
- `deploy/vps_storage_config.py` and `deploy/journald-mimita-storage.conf`;
- `deploy/nginx-mimita-site.conf` when its website root or website routes need
  an intentional change.

It does not cover game source/assets, the room coordinator, Coturn service
restarts, PostgreSQL schema or data, account permissions, database backup
retention, secrets, or unrelated services. A dependency-lock change requires
staged install and validation before activation. Stop and request a separate
human decision whenever the task crosses those boundaries, remote authentication
fails, production state is ambiguous, or the database/filesystem recovery floor
would be threatened.

## Release invariants

- Work from the active shared checkout and current branch. Do not use a
  floating branch name as release identity.
- Commit and push the reviewed website change first. Record the full commit SHA
  and confirm that the same SHA is available from the authoritative GitHub
  remote.
- **Never pull or check out the monorepo on the live website runtime.** The
  historical `/root/mimita-site` clone contains game source and is not a
  website release. Package only the allowlisted site files from the exact
  pushed commit; never send the rest of the repository to the VPS.
- Exclude `.env`, SSH keys, database URLs/passwords, `node_modules`, local
  backups, logs, and unrelated untracked files from every package. Use the
  VPS's existing environment and persistent user-data paths; never replace
  them with local copies.
- Verify the release manifest and archive entries against the allowlist before
  transfer. Preserve the previous working release and keep an immutable
  rollback copy of every file that activation may replace.
- Measure `/` before and after. PostgreSQL must retain at least the documented
  5 GiB free-space floor. If the release's staging/backup size could cross that
  floor, stop before transfer.
- Do not remove or overwrite untracked files, uploaded avatars, site-authored
  articles, `.env`, backups, database files, or snapshots. Resolve a collision
  by excluding/preserving the existing data or stop; never reset the VPS clone.
- Activate the new release atomically, restart only `mimita-api`, and reload
  nginx only if the frontend document root/config changed. On failed readiness,
  restore the prior release and verify recovery before ending the task.
- Record source tests, deployed commit, package manifest/hash, service action,
  post-deploy checks, rollback evidence, and any human account-flow review as
  separate claims in the issue and changelog.

## Local validation and release identity

For server/API changes, run the focused website tests from `website/`:

```powershell
npm test
```

For frontend changes, also run:

```powershell
npm run build
```

For website-storage helper changes, run:

```powershell
python -m unittest deploy.test_vps_storage_config
```

Inspect the final staged diff and ensure all changed production paths are in
the allowlist. Commit only those paths, push the active branch, and confirm
with `git ls-remote` that the pushed SHA matches. The release archive must be
generated from that exact SHA; frontend `dist/` must be built from the same
commit. Do not deploy files from an uncommitted working tree.

## Release packager

`deploy/website_release.py` performs the mechanical preflight without touching
production. From the repository root:

```powershell
python -m deploy.website_release validate --from <deployed-sha> --to <release-sha>
python -m deploy.website_release package --from <deployed-sha> --to <release-sha> --branch <active-branch>
python -m deploy.website_release verify --archive .dev/website-releases/website-<sha12>.tar.gz --manifest .dev/website-releases/website-<sha12>.manifest.json
```

`package` refuses any diff outside the website allowlist, refuses
`website/server/migrations/` because database changes require separate
approval, requires the release SHA to be reachable from the pushed branch,
runs `npm test` and `npm run build` unless `--skip-checks` is explicitly
recorded, and writes a content-addressed archive and manifest under
`.dev/website-releases/`. It does not connect to the VPS. A package is valid
only when the archive digest, commit SHA, and manifest entries all match the
pushed commit.

## One-time runtime separation

The currently active PM2 process runs from
`/root/mimita-site/website`, and nginx serves
`/root/mimita-site/website/dist`. That checkout is the monorepo, contains 854
tracked game source files, and also has untracked production backups/content.
Do not update it with `git pull`, and do not clean or relocate it as part of a
routine website release.

Before the first website-only release can go live, establish an isolated
runtime release root under `/root/mimita-site` containing only the allowlisted
website package. Keep a persistent shared directory for the current `.env`,
avatar uploads, and admin-authored articles; inspect exact live paths read-only
and link the release to those same paths without copying their contents. Keep
the existing monorepo clone intact as a rollback/reference until the new
runtime, nginx root, and PM2 process are proven healthy. Do not delete that
legacy checkout under this authorization.

Use a stable `current` symlink for the active release. Configure nginx to serve
`/root/mimita-site-live/current/website/dist` and PM2 to start
`/root/mimita-site-live/current/website/server/server.js` with that website directory
as its working directory. Preserve the existing TLS, proxy routes, environment,
avatar data, article data, and PM2 startup behavior. Validate the new release
before switching `current`; save the prior symlink target and nginx/PM2 state
for rollback. This one-time switch is a website deployment covered by the
standing authorization, provided every persistent path is identified and the
rollback is executable.

Future releases go in a new SHA-named directory and replace only the `current`
symlink after validation. They never alter the old monorepo checkout or send
game source to the VPS.

## Post-deploy proof

After activation verify all of the following from the public site and VPS:

- the deployed manifest and SHA match the pushed release;
- `mimita-api` is online and nginx is active;
- `/` returns HTTP 200;
- `/api/health/ready` returns HTTP 200 and PostgreSQL accepts queries;
- one invalid-credentials sign-in returns HTTP 401 (this creates one failed
  login/analytics event and proves route/database reachability; it does not
  prove a real account login);
- recent API/nginx logs show no startup or request errors;
- available disk remains above the PostgreSQL start floor.

For the database-outage report, install the tracked watchdog only after
verifying the exact service files and active timer. Apply the tracked journald
retention rule through its guarded helper. Coturn configuration may have its
verbose directive removed through the tracked helper, but do not restart Coturn
while relay use is present or unverified; that service is outside a normal
website deploy. Keep the exact database stop cause labeled unknown unless
PostgreSQL evidence establishes it.

## Email monitor access

Create the five HTTPS monitors and down/up emails in UptimeRobot as specified
in `docs/operations/website-operations.md`. Use the authenticated browser
session or an API credential from the operator's password manager; never ask
for or write credentials into chat, files, shell history, or logs. If the
account is not signed in, finish local/deployment work that does not require
UptimeRobot, then hand the browser to the user for sign-in. Do not claim alerts
are configured until a forced down check and recovery both generate the
expected emails.
