# VPS Branch Verification and Deployment

Before pulling code onto the VPS:

1. Identify the most recently updated candidate branch from the authoritative Git remote.
2. Confirm its branch name, latest commit, commit date, and intended task with the user before deployment.
3. Do not assume `develop/v2.0.1` or any version branch is current; those branches may be stale.
4. Pull only the confirmed branch with `git pull --ff-only origin <confirmed-branch>`.
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
* Test locally before deployment.

## Validation before deployment

Use the task-specific focused skills under `docs/skills/` and record their
exact paths and results in the session changelog. Deployment verification is
separate from source validation: confirm the deployed commit, service health,
and recent logs after the approved pull and restart.

## VPS and Production Deployment Rules

The production VPS (107.191.48.226) must not be edited directly.

**Code is never written to the VPS. The VPS only ever receives code by pulling changes that were committed locally and pushed to GitHub.** No file writes, no `scp`, no editors, no patches on the VPS. This guarantees the deployed files always match GitHub exactly and prevents version mismatches between GitHub and the VPS. If a file exists on the VPS that is not on GitHub (or differs from GitHub), that is a bug and must be reconciled by committing the correct version locally and deploying via `git pull`.

All application changes must follow this path:
1. Make the change in the local repository.
2. Test the change locally.
3. Commit it to Git.
4. Push it to GitHub.
5. Deploy by pulling the committed revision on the VPS.
6. **Rebuild the frontend** on the VPS: `cd /root/mimita-site/website && npm run build`
7. Restart or reload only the affected service.
8. Verify health and logs after deployment.

**Every deployment that touches `website/src/` MUST rebuild the frontend.**
A stale `dist/` means the live site serves old JavaScript that may lack new
components, routes, or fixes. Always run `npm run build` after pulling and
before restarting services.

### Production paths and historical deployment evidence

Paths below were inspected historically. Refresh their service entry points,
Git state, and environment presence with `docs/operations/vps-audit.md` before
deployment. Historical branch labels are not approved deployment targets.

| Path on VPS | Service | Port | Git Tracked |
|---|---|---|---|
| `/root/mimita-site` (Git clone; application under `website/`) | Website API (Node.js) | 3002 | Yes; verify current branch and commit |
| `/root/mimita-coordinator/server.js` (standalone) | ICE Coordinator (Node.js) | 3001 | **Moved to `coordinator-server/server.js` in repo** |
| `/etc/turnserver.conf` | coTURN STUN/TURN | 3478 | No — config provided in repo |
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

