#!/usr/bin/env bash
set -u

# Human/system-owned recovery helper. It does not call an AI service.
CLUSTER="${PG_CLUSTER:-14-main}"
ALERT_FILE="${ALERT_FILE:-/var/log/mimita-db-watchdog.log}"

if pg_isready -q -d "${PGDATABASE:-mimita_db}"; then
    exit 0
fi

timestamp="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
printf '[%s] PostgreSQL %s is unavailable; attempting systemd recovery\n' "$timestamp" "$CLUSTER" >> "$ALERT_FILE"
systemctl start "postgresql@${CLUSTER}" >> "$ALERT_FILE" 2>&1 || true

if pg_isready -q -d "${PGDATABASE:-mimita_db}"; then
    printf '[%s] PostgreSQL %s recovered\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$CLUSTER" >> "$ALERT_FILE"
    systemctl restart mimita-api >> "$ALERT_FILE" 2>&1 || true
else
    printf '[%s] PostgreSQL %s still unavailable; manual action required\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)" "$CLUSTER" >> "$ALERT_FILE"
    if [ -n "${ALERT_COMMAND:-}" ]; then
        "$ALERT_COMMAND" "MiMITA account database unavailable"
    fi
    exit 1
fi
