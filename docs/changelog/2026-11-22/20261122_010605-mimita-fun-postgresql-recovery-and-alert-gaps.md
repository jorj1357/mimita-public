# MiMITA.fun PostgreSQL recovery and alert gaps

- Recorded: 2026-11-22T01:06:05Z
- Issue: `issue-1759b2cc6e65`
- Branch: current shared checkout branch `checkpoint/one-checkout-condensation-20261111`; candidate origin revision previously inspected: `935c0e7ed4a702055b553aea19ea6c51b6c31a94`.

## What changed

- After explicit user authorization, truncated only the verified regular file
  `/var/log/syslog.1` on the VPS. It was 4,806,849,520 bytes, not open, with
  892,751,872 bytes free beforehand. It is now empty; root free space reached
  5,699,633,152 bytes (76%). No other files were removed.
- Started `postgresql@14-main.service` once after the disk crossed the documented
  5 GiB floor. A later read-only check confirmed the service active with
  `Result=success`; `pg_isready` accepted connections.
- Updated the existing website auth/database regression appendix and its index
  with recovery status and remaining safeguards.
- The issue tracker now records successful recovery and leaves deployment,
  watchdog, and down/recovery email requirements open.

## Evidence

- Public homepage: HTTP 200.
- Public invalid-credentials sign-in: HTTP 401, indicating the auth query
  reached the database. This single probe recorded a failed login/analytics
  event; it did not create or change an account.
- Public `/api/health/ready`: still HTTP 404.
- VPS watchdog script and systemd unit remain absent. Coturn `verbose` remains
  enabled; logrotate is daily. The large rotated syslog sample strongly
  implicates verbose Coturn logging as a major disk consumer, but no evidence
  proves the exact PostgreSQL stop cause.
- Existing local source contains readiness and sanitized database-unavailable
  handling, a PostgreSQL watchdog/timer, and the safe VPS storage configuration
  helper. They are not deployed to production.
- UptimeRobot sign-in is required to configure public monitors; no credentials,
  monitor settings, or notifications were changed. Down and recovery email
  delivery remains unverified.
- Production repository checkout has sparse checkout disabled and tracks 854
  game source files. A full branch pull would violate the website-only VPS
  constraint, so no website release was deployed. Production branch/commit
  review and a safe website-only release route remain outstanding.
- `python -m unittest deploy.test_vps_storage_config`: 8 tests passed. Prior
  focused website database-error/health tests passed 11/11; adjacent auth core
  suite has a pre-existing whitespace-normalization expectation mismatch.

## Scope and proof status

No application source or production configuration was changed in this
continuation. Build/deployment evidence: none. Runtime recovery evidence is
listed above; full prevention and email-monitor proof remain incomplete. The
working tree also contains unrelated concurrent edits; none were included in
this record or represented as this task's changes.
