// 09 06 2026, 15 45
/* purpose
* define the universal date, time, timezone, folder, and filename format
* give logs, tests, changelogs, feature records, and generated data one standard
* preserve machine-sortable evidence while showing local time to humans
* this file DOES NOT define feature-specific log content or retention
* this file DOES NOT rename historical files automatically
* this file DOES NOT replace debug-logging or task-completion procedures
*/

# Universal time and file-format standard

This is the authoritative cross-repository standard for new generated dates,
times, folders, filenames, logs, test artifacts, changelogs, crash reports,
build reports, and feature evidence.

## Canonical machine time

All stored machine timestamps use UTC and ISO 8601:

```text
2026-09-06T19:32:00.123Z
```

Use:

- `Z` for UTC.
- 24-hour time.
- zero-padded month, day, hour, minute, second, and fractional seconds when available.
- the timezone offset or `Z` must never be omitted from a full timestamp.

The Windows development machine may display Eastern local time, but local time
is display context, not the canonical stored value.

## Folder format

New date-based folders use:

```text
yyyy-mm-dd
```

Examples:

```text
logs/2026-09-06/
docs/changelog/2026-09-06/
logs/features/camera/2026-09-06/
```

## Generated filename format

New generated filenames use a compact UTC timestamp for sorting:

```text
yyyymmdd_hhmmss
```

Examples:

```text
combat_20260906_193200.log
20260906_193200-camera-fov.md
networking_20260906_193200.log
```

Use a descriptive lowercase or established category prefix before the
timestamp. Do not use spaces, locale-dependent month names, or ambiguous
day/month ordering.

## Human-readable display

When a tool or log is shown to a user, it may include local time, but it must
also preserve the canonical UTC value:

```text
time_utc=2026-09-06T19:32:00.123Z
display_timezone=America/New_York
display_time=2026-09-06 15:32:00 EDT
```

## Historical files and migration

Existing files using `mm-dd-yyyy`, `mmddyyyy_hhmmss`, or other formats remain
valid historical evidence. Do not rename or delete them only because this
standard is adopted.

New code and new documentation artifacts must use this standard. A migration
task must update the owning code, tests, scripts, and readers together.

## Required references

- Runtime logging: `docs/specs/debug-logging/debug-logging.md`
- AI session records: `docs/operations/task-completion/task-completion.md`
- Feature records: `docs/features/README.md`
- Test and evidence procedures: `docs/operations/testing/testing.md` when present
