# VIP migration registration fix

- Branch: `8292026stash`
- Timestamp: `2026-09-07T19:45:00Z`
- Pre-existing worktree changes: unrelated game/config edits were present and were not staged or modified.
- Commit: to be recorded after validation.

## Change

Updated `website/server/db.js` so the versioned migration runner executes migration 8 and maps it to `website/server/migrations/008_vip_refunds.sql`.

Before this change, the runner iterated only `[1, 5, 6, 7]`. The refund migration existed but was never applied, so the live `/api/vip/orders` query referenced missing `refund_status` and returned HTTP 500.

## Validation

- Focused source validation and website build will run before deployment.
- VPS migration will be run explicitly.
- VPS API logs and `/api/vip/orders` will be checked after restart.

## Remaining human review

After the endpoint returns 200, the user should refresh `/vip`, open the purchase-management button, and perform a controlled refund test.
