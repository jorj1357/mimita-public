#!/usr/bin/env bash
set -euo pipefail

readonly backup_dir=/root/mimita-db-verified-backups
readonly remote_dir=gdrive-crypt:mimita-database-backups
readonly minimum_free_kib=$((5 * 1024 * 1024))

fail() {
    logger -t mimita-database-backup "category=$1"
    exit 1
}

[[ -d "$backup_dir" && ! -L "$backup_dir" ]] || fail backup_directory_unavailable
[[ "$(stat -c '%a:%u' "$backup_dir")" == "700:0" ]] || fail backup_directory_permissions
command -v node >/dev/null && command -v pg_restore >/dev/null && command -v rclone >/dev/null && command -v jq >/dev/null || fail backup_tools_missing
available_kib=$(df -Pk / | awk 'NR == 2 { print $4 }')
[[ "$available_kib" =~ ^[0-9]+$ ]] && (( available_kib >= minimum_free_kib )) || fail backup_skipped_disk_floor
config_output=$(rclone config file) || fail rclone_config_unavailable
config_file=$(printf '%s\n' "$config_output" | tail -n 1)
[[ "$config_file" == /* && -f "$config_file" && ! -L "$config_file" ]] || fail rclone_config_unavailable
[[ "$(stat -c '%a:%u' "$config_file")" == "600:0" ]] || fail rclone_config_permissions
rclone listremotes | grep -Fxq 'gdrive-crypt:' || fail encrypted_remote_missing
rclone config dump | jq -e '.["gdrive-crypt"].type == "crypt" and (.["gdrive-crypt"].remote | startswith("gdrive:"))' >/dev/null || fail encrypted_remote_invalid
rclone config userinfo gdrive: | grep -Fq 'contact@ravgrowth.com' || fail drive_account_unverified

node /root/mimita-site/website/server/backup-database.mjs --output-dir "$backup_dir" || fail database_archive_failed

latest=$(find "$backup_dir" -maxdepth 1 -type f -name 'mimita-*.dump' -printf '%T@ %p\n' | sort -nr | head -n 1 | cut -d ' ' -f 2-)
[[ -n "$latest" && -f "$latest" && ! -L "$latest" ]] || fail verified_archive_missing
[[ "$(head -c 5 "$latest")" == PGDMP ]] || fail archive_magic_invalid
pg_restore --list "$latest" >/dev/null 2>&1 || fail archive_listing_failed

filename=${latest##*/}
remote_archive="$remote_dir/$filename"
rclone copyto "$latest" "$remote_archive" || fail encrypted_upload_failed
rclone check --download "$latest" "$remote_archive" >/dev/null 2>&1 || fail encrypted_upload_verification_failed

# Retention runs only after the new encrypted copy was verified.
rclone delete --min-age 30d --include 'mimita-*.dump' "$remote_dir" || fail remote_retention_failed
find "$backup_dir" -maxdepth 1 -type f -name 'mimita-*.dump' ! -name "$filename" -delete
logger -t mimita-database-backup "category=backup_archive_and_remote_verified"
