#!/usr/bin/env bash
set -euo pipefail

readonly minimum_free_kib=$((5 * 1024 * 1024))
readonly state_file=/var/lib/mimita-postgres-watchdog/last-attempt-epoch
readonly database_unit=postgresql@14-main.service

if pg_isready --quiet; then
    rm -f "$state_file"
    exit 0
fi

available_kib=$(df -Pk / | awk 'NR == 2 { print $4 }')
if [[ ! "$available_kib" =~ ^[0-9]+$ ]] || (( available_kib < minimum_free_kib )); then
    logger -t mimita-postgres-watchdog "category=database_start_skipped_disk_floor"
    exit 1
fi

now=$(date +%s)
if [[ -r "$state_file" ]]; then
    logger -t mimita-postgres-watchdog "category=database_start_already_attempted"
    exit 1
fi

printf '%s\n' "$now" > "$state_file"
logger -t mimita-postgres-watchdog "category=database_start_attempt"
if ! systemctl start "$database_unit"; then
    logger -t mimita-postgres-watchdog "category=database_start_command_failed"
    exit 1
fi

for _ in {1..20}; do
    if pg_isready --quiet; then
        logger -t mimita-postgres-watchdog "category=database_ready_after_start"
        exit 0
    fi
    sleep 1
done

logger -t mimita-postgres-watchdog "category=database_not_ready_after_start"
exit 1
