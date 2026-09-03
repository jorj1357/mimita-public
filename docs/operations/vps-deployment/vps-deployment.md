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
6. Restart or reload only the affected service.
7. Verify health and logs after deployment.

### Known Production Paths and Services

Verified 2026-07-29 via read-only SSH audit:

| Path on VPS | Service | Port | Git Tracked |
|---|---|---|---|
| `/root/mimita-site` (Git clone) | Website API (Node.js) | 3002 | Yes — `avatar-editor-6-27-2026` branch |
| `/root/mimita-coordinator/server.js` (standalone) | ICE Coordinator (Node.js) | 3001 | **Moved to `coordinator-server/server.js` in repo** |
| `/etc/turnserver.conf` | coTURN STUN/TURN | 3478 | No — config provided in repo |
| nginx (system service) | Reverse proxy | 80 / 443 | Config not yet in repo |

### Approved Deployment Workflow

For the **coordinator** (once VPS migrates to Git clone):
```bash
ssh root@107.191.48.226 "cd /root/mimita-coordinator && git pull && pm2 restart mimita-coordinator"
```

For the **website API** (already Git-tracked):
```bash
ssh root@107.191.48.226 "cd /root/mimita-site && git pull && pm2 restart mimita-api"
```

Current state: the website at `/root/mimita-site` is on branch `avatar-editor-6-27-2026` (commit `12e8d82`). The coordinator at `/root/mimita-coordinator` is a standalone directory — it must be migrated to a Git clone for reproducible deployment.

### Allowed Direct VPS Actions

- Read logs (`journalctl -u <service> -n 100`, `pm2 logs <name> --lines 100 --nostream`, `tail -n 100 <path>`)
- Inspect service status (`systemctl status <name>`, `pm2 status`, `pm2 describe <name>`)
- Inspect ports and process state (`ss -lntup`, `ps aux | grep <name>`)
- Check Git state (`git status --short`, `git log -5 --oneline`, `git rev-parse HEAD`)
- Verify environment-variable presence WITHOUT printing values (`echo NAME:${VAR:+SET}:${VAR:-UNSET}`)
- Pull an already reviewed and committed revision
- Restart or reload a service as part of an intentional deployment
- Roll back to a known committed revision (`git checkout <sha>`)

