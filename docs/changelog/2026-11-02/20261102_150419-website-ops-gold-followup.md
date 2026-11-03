// 2026-11-02
/* purpose
* Record verified current backup/status evidence and clarify the website recovery guides.
* Does not deploy services, configure Drive, change retention, or alter production data.
*/

# Website operations and recovery evidence follow-up

## Findings

- At 2026-11-02T15:03Z, the VPS root filesystem had 5.4 GB available (76% used),
  PostgreSQL accepted connections, and the public homepage returned HTTP 200.
- The VPS has an `rclone` remote named `gdrive:` and old database-related files
  in Drive, including files named from 2025-09 and 2025-10. This proves some
  historical Drive storage exists, but does not prove a current, complete,
  restorable database backup is being uploaded on schedule.
- The VPS has one locally verified 7,178,142-byte database archive dated
  2026-11-02. The readiness URL returned HTTP 404; the new health endpoint and
  backup/watchdog systemd units are not installed. No new Drive upload was
  attempted. The configured Drive identity and encrypted `gdrive-crypt:` remote
  remain unverified/unavailable.
- A 6.9 MB local archive is valuable for recovery but too small to materially
  change disk pressure. Moving database copies to Drive protects against VPS
  loss; it only frees local disk if local copies are removed, which is not the
  plan (retain one verified local copy).

## Documentation updates

- Added copyable public-site, readiness, VPS service/database/disk, and recent
  service-log checks to `docs/operations/website-operations.md`, including the
  fact that readiness endpoints currently return 404 until deployment.
- Added user-reported recovery experience to
  `docs/gold/2026-09-16-lightweight-auth-recovery.md`: regression tracking
  helped; the user estimates one or two prompts with GPT-6 Luna Medium. The
  model and prompt count are explicitly not independently verified. The
  approximate 09:54 New York time is recorded as 14:54Z, assuming EST.

## Remaining work

Configure and verify the VPS Drive identity and encryption remote, then install
and exercise the daily verified backup and bounded PostgreSQL watchdog. Deploy
the health routes before configuring external uptime alerts. Reduce ongoing
log growth by installing the reviewed retention settings and disabling
Coturn's verbose session logging. Rehearse a restore away from production and
complete the two-PC TURN baseline/direct-only test. Database backups alone are
not a meaningful disk-space reduction; larger reclaim candidates need separate
manager-aware safety checks.

No production source/configuration was edited or deployed in this follow-up.
No tests were run; changes are documentation-only.
