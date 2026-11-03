# VPS layout and Drive authorization readiness

**Time:** `2026-11-03T15:26:55Z` (2026-11-03 10:26:55 EST)  
**Result:** Read-only storage inventory completed; no game files or backups removed. rclone remains at v1.75.2.

## Disk evidence

At inspection, `/` was 23 GB total, 17 GB used, 5.0 GB free (78%). Top-level usage: `/var` 3.8 GB, `/usr` 6.1 GB, `/root` 2.9 GB. `/var/log` was the largest subdirectory at 1.9 GB, followed by `/var/lib` at 1.7 GB. The `/root/mimita-site` checkout was 1.1 GB: `.git` 546 MB, `website` 241 MB, `assets` 135 MB, `include` 36 MB, and `src` 12 MB. This confirms game-related files are present, but source/assets are only about 183 MB; removing them alone would not reclaim the bulk of the VPS disk use. No files were deleted.

The repo checkout had 3,030 tracked files. Its pre-existing untracked paths were `backups/`, three dated website environment backups, `website/db-check-temp.mjs`, and `website/src/pages/ProfilePage.jsx.pre-reward-20260901-150551`; all remain untouched.

## Drive status

- rclone reports v1.75.2. The root config is still mode 0600 and only has `gdrive:`; there is no `gdrive-crypt:`.
- A top-level `rclone lsf gdrive:` request exited successfully, but the sanitized `rclone config userinfo gdrive:` check failed. Do not treat the remote as the intended account until userinfo confirms `contact@ravgrowth.com`.
- No OAuth client was created, no credential was entered, and no upload, encryption, or scheduled backup was performed.

## Changes and preservation

The production website repository was not pulled. The proposed candidate differs from the VPS commit by 3,138 tracked files, including unrelated game code. All inspected storage commands were read-only. No system logs, `/var/lib`, PostgreSQL data, VPS snapshots, local backups, or Git history were changed.
