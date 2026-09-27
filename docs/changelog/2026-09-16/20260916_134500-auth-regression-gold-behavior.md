# Auth outage regression and lightweight-recovery guidance

## Result

PASS. Added a detailed regression document, linked it from the append-only
regression index, added website-editing safety rules, and recorded the desired
lightweight-AI/two-prompt recovery behavior as a gold architecture target.

## Files

- `docs/regressions/2026-09-16-website-auth-database-outage.md`: complete
  incident record, cause, proof, prevention, and remaining acceptance.
- `docs/regressions/regressions-v1.md`: link from the confirmed incident entry.
- `docs/operations/vps-deployment/vps-deployment.md`: rules preventing website
  edits from weakening database error detection, recovery, or messaging.
- `docs/gold/2026-09-16-lightweight-auth-recovery.md`: gold behavior for clear
  ownership, non-AI recovery, and roughly two-prompt diagnosis by a lightweight
  model.

## Evidence

Documentation-only change. The underlying outage and recovery evidence remains
in the prior auth recovery changelog and regression record. No production files
were edited by this documentation update.
