# Prepare VPS rclone for Drive authorization

**Time:** `2026-11-03T14:24:12Z` (2026-11-03 09:24:12 EST)  
**Repository branch and HEAD:** `20261011zt` at `684f7101dc8046e9b745095d243ceed52244c1f2`  
**Result:** VPS rclone updated to stable v1.75.2; Google OAuth and encrypted remote still await operator setup.

## Production evidence

- Before update, `/root/mimita-site` was branch `8292026stash` at `f94b5636ff3431806b91e4966f85d2ecaa38a738`, clean except six preserved untracked paths (`backups/`, three dated website env backups, `website/db-check-temp.mjs`, and `website/src/pages/ProfilePage.jsx.pre-reward-20260901-150551`).
- Disk was 23 GB total, 17 GB used, 5.2 GB free (77%); PostgreSQL accepted connections and `mimita-api` was online.
- rclone was upgraded using the official stable installer from v1.53.3 to v1.75.2. The existing `/root/.config/rclone/rclone.conf` remains `root:root` mode 0600 with only `gdrive:` configured. No token or config content was read or printed. Disk afterward: 5.1 GB free (77%); PostgreSQL and API remained healthy.
- Sanitized `rclone config userinfo gdrive:` check did not verify `contact@ravgrowth.com`; the old remote is not yet accepted as the backup identity.
- No database, Drive contents, backup archives, website source, or service units were changed. No backup was uploaded and no timer is enabled.

## Deployment review

The candidate `684f7101dc8046e9b745095d243ceed52244c1f2` is the remote head, but comparing it with the VPS commit shows 3,138 tracked files changed (400,117 insertions, 142,262 deletions), including non-website source. The user requested website backup work, so this broad commit was not pulled. A focused website deployment revision is still needed before installing the tracked health and backup service files.

## Next operator step

The Google-owned OAuth client ID and secret must be created in Google Cloud Console and entered directly into the root rclone prompt. The VPS remote is headless; complete Google sign-in in the operator's browser using rclone's headless authorization flow. Do not send client secrets, tokens, or crypt passphrases through chat. After OAuth, verify the intended Drive account, create `gdrive-crypt:` with encrypted names/directories and a password-manager-held passphrase, then upload/check/restore before enabling the timer.

The repository worktree had unrelated user changes in dev-loop files, docs, a new `devscripts/dev_loop/` directory, and the `devscripts/blender-ai` submodule. They were not modified.
