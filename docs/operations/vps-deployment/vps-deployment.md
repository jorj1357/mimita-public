2026-09-07T17:13:59Z  jorj  here do this ssh mimita-vps  
thecmd is this 
ssh mimita-vps
for AI agents 
to get into the vps and stuff and check 
todo make this prettier and update docs to look nicer 

also todo 2026-09-07T17:17:34Z  jorj  the vps does not need  game code no like cpp and stuff just website thats all the vps needs go

# VPS Branch Verification and Deployment

Before pulling code onto the VPS:

1. Identify the most recently updated candidate branch from the authoritative Git remote.
2. Verify the branch name, commit SHA/date, intended task, and authenticated
   remote state. The standing website authorization below removes per-release
   confirmation for changes inside its scope; it does not waive validation.
3. Do not assume `develop/v2.0.1` or any version branch is current; those branches may be stale.
4. Pull only an authorized, confirmed branch with
   `git pull --ff-only origin <confirmed-branch>`. Website agents must use the
   website-only release process below; never pull the monorepo into the live
   website checkout because it contains game source.
5. Preserve and report any pre-existing untracked or local VPS files; never delete or overwrite them as part of a pull.
6. Verify the deployed commit and restart only the relevant existing service after a successful pull.

Never launch `mimita.exe` without `--server` or `--timeout <secs>`. Without these flags the game opens a full graphics window and stays open indefinitely (it won't automatically exit). If you need to test server behavior, always use `--server --timeout 30 --no-coordinator` or similar so the process self-terminates.

For build purposes, human or AI agents are authorized to terminate existing `mimita.exe` processes at any time, until this instruction is changed. This authorization applies only to releasing the executable lock so the updated build can be produced and tested.

# Development Rules

* Local repository is the source of truth.
* VPS is deployment target only.
* Development happens locally first.
* Do not create production-only fixes.
* Do not edit production files unless investigating.
* Fixes discovered on VPS must be implemented locally.

## VPS storage policy

Treat the VPS as a storage-constrained deployment target. The VPS should hold
only the website runtime, its production `dist/` output, the website's
installed Node dependencies, required configuration/secrets, database
backups, and the minimum service files needed to run them. Do not copy or
build the game source, C++ files, game assets, development artifacts, local
test output, or unrelated repositories on the VPS. Keep those in the local
repository or CI/build storage and deploy only the website files required by
the active service.

Before and after deployment, report disk usage and preserve unrelated
untracked files. Do not delete files merely to save space; identify their
owner and obtain explicit approval before cleanup. Prefer pruning known build
cache or package-cache data only after verifying it is not needed by a running
service.
* Test locally before deployment.

## Standing website-agent authorization

The user explicitly authorized authenticated AI agents to repair and deploy
routine `mimita.fun` website changes end to end on 2026-11-22. The durable
authorization is recorded in `AGENTS.md` and issue
`issue-260667351e04`. Agents with valid GitHub push and `ssh mimita-vps`
authentication do not need per-deployment confirmation for changes within
that scope. The access key remains in the operator's SSH agent/configuration;
never copy it into Codex prompts or this repository.

The release payload is website-only. Deploy exact files from an already pushed
commit using the allowlist in
`docs/operations/website-agent-deployment.md`; do not pull the monorepo into a
live website checkout, deploy game/C++ assets, or use a floating branch as the
release identity. Build and validate locally, preserve the current production
release and all untracked/user data, verify disk and database headroom, record
the immutable SHA, and health-check after restarting only `mimita-api` and (if
the frontend changed) reloading nginx. Automatic rollback is required when
post-deploy readiness fails.

Stop for a human decision if work requires a database/schema/data migration,
destructive cleanup, secrets or permission changes, DNS/billing changes,
another VPS service, or a deployment whose target/persistent-data mapping or
rollback cannot be verified. The standing authorization does not turn a failed
health check or unknown production drift into permission to guess.

## Website editing safety: preserve database recovery

Website changes must not make PostgreSQL availability failures harder to detect,
recover, or explain. Before changing auth, sessions, migrations, database access,
startup, or error handling:

1. Keep the API/database dependency explicit. Do not turn a database failure into
   a successful-looking login, signup, empty profile, or generic HTTP 200.
2. Preserve a structured temporary-unavailable response and a clear user-facing
   message that no account change was confirmed.
3. Do not make the API appear healthy merely because the Node process is alive;
   verify PostgreSQL readiness and a non-mutating auth probe separately.
4. Keep recovery owned by ordinary operations/systemd/monitoring, not by an AI
   loop or an undocumented manual step.
5. Any auth/database edit must include focused local validation plus deployment
   verification of database health, API status, and the invalid-login 401 probe.

The permanent lesson from the 2026-09-16 outage is that `mimita-api online`
does not prove PostgreSQL is available. Future website edits must preserve the
database watchdog, structured 503 contract, and recovery evidence.

## Validation before deployment

Use the task-specific focused skills under `docs/skills/` and record their
exact paths and results in the session changelog. Deployment verification is
separate from source validation: confirm the deployed commit, service health,
and recent logs after the approved pull and restart.

## VPS and Production Deployment Rules

The production VPS (107.191.48.226) must not be hand-edited. All website
source and service changes must arrive through the committed release procedure
below.

**Production source is never hand-edited on the VPS.** It must come from an
exact commit already pushed to GitHub. Ordinary services use the reviewed
`git pull --ff-only` path. The user has also granted standing authority for
authenticated AI agents to deploy routine mimita.fun changes without
per-deployment confirmation; website releases use the SHA-bound, allowlisted
website-only package procedure in
`docs/operations/website-agent-deployment.md`. Do not pull the monorepo into
the website runtime or send game source to the website VPS. Preserve any
pre-existing untracked files and user data. Do not use a remote editor or an
ad-hoc patch.

Other production application changes follow this path:
1. Make the change in the local repository.
2. Test the change locally.
3. Commit it to Git.
4. Push it to GitHub.
5. Deploy by pulling the committed revision on the VPS.
6. **Rebuild the frontend** on the VPS: `cd /root/mimita-site/website && npm run build`
7. Restart or reload only the affected service.
8. Verify health and logs after deployment.

**Every deployment that touches `website/src/` MUST rebuild the frontend from
the same commit before activation.** A stale `dist/` means the live site serves
old JavaScript that may lack new components, routes, or fixes. The website-only
release procedure packages that build output and activates it with the matching
API source.

### Production paths and historical deployment evidence

Paths below were inspected historically. Refresh their service entry points,
Git state, and environment presence with `docs/operations/vps-audit.md` before
deployment. Historical branch labels are not approved deployment targets.

| Path on VPS | Service | Port | Git Tracked |
|---|---|---|---|
| `/root/mimita-site` (Git clone; application under `website/`) | Website API (Node.js) | 3002 | Yes; verify current branch and commit |
| `/root/mimita-coordinator/server.js` (standalone) | ICE Coordinator (Node.js) | 3001 | **Moved to `coordinator-server/server.js` in repo** |
| `/etc/turnserver.conf` | coTURN STUN/TURN | 3478 | Runtime-secret config; the repo-managed storage helper edits only the exact `verbose` directive and preserves all other settings |
| nginx (system service) | Reverse proxy | 80 / 443 | Config not yet in repo |

### Deployment preparation

The 09-06-2026 read-only website audit observed branch `8292026stash` at
`5c4d846d95a8a0e56aa8ae2d69a8b4f457d3f426` and preserved the untracked file
`website/src/pages/ProfilePage.jsx.pre-reward-20260901-150551`. This is dated
evidence, not authorization to deploy that branch or remove the file.

Before deploying persistence, follow `docs/operations/persistence-recovery.md`
to establish a usable backup and isolated restore evidence. Inspect the proposed
migration against the live schema, test it locally, and review the exact commit.
Use only `git pull --ff-only origin <confirmed-branch>` in the verified clone.
Run the tracked migration entry point from `website/` and require success before
restarting only `mimita-api`. Build the frontend from that same commit. On failure,
stop the rollout; do not reset data or retry unrelated migration commands.
Verify the deployed commit, database health, migration versions, auth path,
profiles, and leaderboards after the restart. Multiplayer save confirmation and
restart preservation require their separate end-to-end acceptance tests.

Do not assume the coordinator is a Git clone. Verify its actual PM2 entry point
and repository state before applying the same reviewed-commit procedure there.

### Allowed Direct VPS Actions

- Read logs (`journalctl -u <service> -n 100`, `pm2 logs <name> --lines 100 --nostream`, `tail -n 100 <path>`)
- Inspect service status (`systemctl status <name>`, `pm2 status`, `pm2 describe <name>`)
- Inspect ports and process state (`ss -lntup`, `ps aux | grep <name>`)
- Check Git state (`git status --short`, `git log -5 --oneline`, `git rev-parse HEAD`)
- Verify only environment-variable presence through the audit procedure; never expand a fallback that prints its value
- Pull an already reviewed and committed revision
- Restart or reload a service as part of an intentional deployment
- Roll back to a known committed revision (`git checkout <sha>`)

## Article Content Tracking

Articles created through the admin editor at `/admin/articles` are stored as
`.md` files in `content/articles/` at the repo root. This directory MUST be
committed to git. If articles exist only on the VPS filesystem (untracked),
a `git pull` will overwrite them. Always run `git add content/articles/` and
commit after creating or editing articles through the admin panel.

