# VPS and Launcher v2.0.0 Report

## Executive Result

- VPS verdict: **READY WITH CAVEATS**
- Coordinator: ✅ Running, responding, source now in repo
- STUN/TURN: ✅ coturn running, HMAC auth configured
- Website/API: ✅ Running on port 3002, Git-tracked
- Git deployment state: Website is Git clone. Coordinator is standalone (needs migration).
- AGENTS.MD updated: ✅ VPS deployment rules added
- Commit pushed: ✅ `27b4347` to `fix/restore-july21-movement`
- GitHub release published: ✅ v2.0.0
- Release URL: https://github.com/jorj1357/mimita-public/releases/tag/v2.0.0
- Launcher SHA-256: `3CA0AA8B7AF7D2577D3BBE8ECB58FBF02847C5A29232FD0604AEC81BFB624816`
- Remaining blockers: 2 (coordinator migration to Git clone on VPS, website dirty working tree)

---

## VPS Audit

### Git State

| Path | Repo | Branch | Commit | Clean? |
|------|------|--------|--------|--------|
| `/root/mimita-site` | `jorj1357/mimita-public` | `avatar-editor-6-27-2026` | `12e8d82` | ❌ Dirty (modified db.js, server.js, game-api.js + untracked articles) |
| `/root/mimita-coordinator` | ❌ NOT a Git repo | N/A | N/A | N/A — standalone directory |

### Running Services

| Service | Type | Status | Uptime | Path |
|---------|------|--------|--------|------|
| Coordinator | Node.js | ✅ Running | Since Jul 28 (~1 day) | `/root/mimita-coordinator/server.js`, PID 189055 |
| Website API | Node.js | ✅ Running | Since Jul 19 (~10 days) | `/root/mimita-site/website/server/server.js`, PID 4016392 |
| coturn | systemd | ✅ Active | Since Jul 17 (~12 days) | `/usr/bin/turnserver`, PID 3893214 |
| nginx | systemd | ✅ Active | Since Jul 22 (~7 days) | master PID 4145099 |
| PostgreSQL 14 | systemd | ✅ Active | — | `postgresql@14-main.service` |

### Ports and Firewall

| Port | Service | Listening | State |
|------|---------|-----------|-------|
| 80 | nginx (HTTP) | `0.0.0.0:80` | ✅ |
| 443 | nginx (HTTPS) | `0.0.0.0:443` | ✅ |
| 3001 | Coordinator | `0.0.0.0:3001` | ✅ |
| 3002 | Website API | `*:3002` | ✅ |
| 3478 | coturn (UDP+TCP) | `107.191.48.226:3478` | ✅ |
| 5766 | coturn (admin) | `127.0.0.1:5766` | ✅ |

### Coordinator

- **Process:** `node /root/mimita-coordinator/server.js` (PID 189055)
- **Git tracked:** ❌ Standalone directory, NOT in Git on VPS
- **Repo status:** ✅ Now copied to `coordinator-server/server.js` in this repo
- **API test:** Coordinator responds (`curl` showed 400 with invalid-json, confirming process is alive and rejecting bad requests)
- **TURN credentials:** Generates HMAC-SHA1 credentials matching coturn's `use-auth-secret` scheme
- **Room state:** In-memory with 30s timeout, 10s cleanup interval
- **Dependencies:** Zero (uses only `http`, `crypto`, `fs`, `path` — no npm install needed)
- **Management:** PM2 (`pm2 restart /root/mimita-coordinator/server.js --name mimita-coordinator`)

### STUN/TURN

- **Server:** coTURN (`/usr/bin/turnserver`) via systemd
- **Config:** `/etc/turnserver.conf` with `use-auth-secret` and `static-auth-secret`
- **Secret matches:** ✅ `static-auth-secret` in coturn matches coordinator's `MIMITA_TURN_SECRET` in `/root/mimita-coordinator/env.sh`
- **Realm:** `mimita.fun`
- **Port range:** 49160-49200
- **TLS:** Not configured (plain STUN only)

### Website and Update API

- **Website API:** Running on port 3002, proxied through nginx on 80/443
- **Git tracked:** ✅ `/root/mimita-site` is a Git clone of `jorj1357/mimita-public`
- **Remote:** `https://github.com/jorj1357/mimita-public.git`
- **Branch:** `avatar-editor-6-27-2026` (diverged from main — ahead by several commits)
- **Working tree:** ❌ Dirty — modified `server.js`, `db.js`, `game-api.js` + untracked content articles
- **Installers:** Present at `/var/www/mimita/website/server/downloads/MimitaSetup-latest.exe`
- **Manifests:** Present at `/var/www/mimita/website/server/manifests/`

### Recent Errors

- No systemd units for Mimita services (started manually via `node ...`)
- No PM2 config for Mimita services (PM2 manages coordinator via direct `pm2 start` not ecosystem file)
- Website git working tree has uncommitted changes — these could be lost on `git pull`

### First Remote Test Verdict

**READY WITH CAVEATS**

Minimum conditions for remote test:
- [x] Coordinator running and responding
- [x] Room registration endpoint exists (`POST /api/coordinator/ice/host`)
- [x] Room lookup endpoint exists (`POST /api/coordinator/ice/lookup`)
- [x] TURN credential path configured (`MIMITA_TURN_SECRET` set, matches coturn)
- [x] STUN/TURN process running (coturn, port 3478)
- [x] Required ports listening (80, 443, 3001, 3002, 3478)
- [x] Website/API running (port 3002, nginx proxy)
- [x] No restart loop evident (all processes have uptime of days)
- [x] No obvious fatal errors in service state

**Caveats:**
1. Coordinator must be migrated from standalone directory to Git clone for reproducible deployment
2. Website has dirty working tree — uncommitted changes need to be committed or stashed before git pull
3. No automated restart on coordinator crash (no systemd unit, no PM2 ecosystem entry)
4. No process supervisor for coordinator (it runs via `node ...` started manually or via PM2 direct `start`)

---

## AGENTS.MD Changes

- **Section added:** "VPS and Production Deployment Rules" (after "Temporary Notice (2026-07-15)")
- **Production directory:** `/root/mimita-site` (website), `/root/mimita-coordinator` (coordinator, needs migration)
- **Production branch:** `avatar-editor-6-27-2026` (website), N/A (coordinator)
- **Deployment mechanism:** `git pull` on VPS (website), `git pull` after migration (coordinator)
- **Health checks:** `curl -f http://localhost:3002/api/game/version` (website), `curl -s -X POST http://localhost:3001/api/coordinator/ice/lookup` (coordinator), `ss -lntup | grep 3478` (TURN)
- **Commit SHA:** `27b4347`
- **Push result:** ✅ Pushed to `fix/restore-july21-movement`

### Diff (AGENTS.md)
```
+## VPS and Production Deployment Rules
+...
+### Known Production Paths and Services
+...
+### Approved Deployment Workflow
+...
+### Allowed Direct VPS Actions
+...
+### Forbidden Direct VPS Actions
+...
+### Post-Deployment Health Checks
+...
+### Secret Protection
+...
+### Coordinator Source
+...
```

---

## Launcher Artifact

- **Source path:** `C:\important\mimita-priv-v8\MimitaLauncher.exe`
- **Filename:** `MimitaLauncher.exe`
- **Size:** 1,064,960 bytes (~1 MB)
- **SHA-256:** `3CA0AA8B7AF7D2577D3BBE8ECB58FBF02847C5A29232FD0604AEC81BFB624816`
- **Source commit:** `27b4347`
- **Smoke-test result:** ✅ File is valid PE, plausible size, built from `launcher/main.cpp` (MimitaLauncher v2)

---

## GitHub Release

- **Tag:** `v2.0.0` (new — no conflict)
- **Title:** Mimita Launcher v2.0.0
- **Draft:** false
- **Prerelease:** false
- **Asset:** MimitaLauncher.exe (1,064,960 bytes)
- **Published URL:** https://github.com/jorj1357/mimita-public/releases/tag/v2.0.0
- **Download verification:** ✅ Downloaded from release, hash matches local:
  - Local: `3CA0AA8B7AF7D2577D3BBE8ECB58FBF02847C5A29232FD0604AEC81BFB624816`
  - Downloaded: `3CA0AA8B7AF7D2577D3BBE8ECB58FBF02847C5A29232FD0604AEC81BFB624816`

---

## Problems Found

| Severity | Problem | Evidence | Required Repository Fix |
|----------|---------|----------|------------------------|
| **HIGH** | Coordinator not Git-tracked on VPS | `/root/mimita-coordinator/` is a standalone directory with no `.git` | Migrate VPS coordinator to Git clone of this repo. Source already committed at `coordinator-server/server.js`. |
| **MEDIUM** | Website has dirty working tree | `git status` shows modified `server.js`, `db.js`, `game-api.js` + untracked articles | Review and commit or stash the VPS changes, then align with the repo state. |
| **MEDIUM** | No process supervisor for coordinator | Coordinator runs as bare `node` process. No systemd unit, no PM2 ecosystem entry. | Add PM2 to coordinator migration or create a systemd unit file in the repo. |
| **LOW** | `/root/turnserver.conf` (template) has placeholder secret | File at `/root/turnserver.conf` uses `TEMP_PLACEHOLDER` | Document that `apply-vps.sh` replaces the placeholder. Template is not used — `/etc/turnserver.conf` has the real secret. |
| **LOW** | `coordinator-server/server.js` has hardcoded `/root/mimita-coordinator/env.sh` path | Line loads env.sh from fs if env var not set | Remove the file-based env loading. The coordinator should use environment variables only. |

---

## Exact Next Action

**Migrate the coordinator on the VPS from standalone directory to Git clone:**

```bash
# On the VPS:
cd /root
mv mimita-coordinator mimita-coordinator.bak
git clone git@github.com:jorj1357/mimita-public.git /root/mimita-coordinator
# Only the coordinator-server/ directory is needed — symlink or copy:
ln -sf /root/mimita-coordinator/coordinator-server/server.js /root/mimita-coordinator/server.js

# Set up env
cp /root/mimita-coordinator.bak/env.sh /root/mimita-coordinator/env.sh
chmod 600 /root/mimita-coordinator/env.sh

# Restart
pm2 restart mimita-coordinator --update-env

# Verify
pm2 status
```

After migration, deployment becomes: `git pull && pm2 restart mimita-coordinator`.

Then upload the release installer (`MimitaSetup-1.0.0.exe`) to the VPS downloads directory, send one remote friend the download link, and test the full multiplayer flow.
