# VIP migration registration fix

- Branch: `8292026stash`
- Timestamp: `2026-09-07T19:45:00Z`
- Pre-existing worktree changes: unrelated game/config edits were present and were not staged or modified.
- Application commit: `9392a492e4efc9be50451ea5a3010661fd0d2fef`

## Change

Updated `website/server/db.js` so the versioned migration runner executes migration 8 and maps it to `website/server/migrations/008_vip_refunds.sql`.

Before this change, the runner iterated only `[1, 5, 6, 7]`. The refund migration existed but was never applied, so the live `/api/vip/orders` query referenced missing `refund_status` and returned HTTP 500.

## Validation

- Focused source validation and website build will run before deployment.
- VPS migration will be run explicitly.
- VPS API logs and `/api/vip/orders` will be checked after restart.

## Remaining human review

After the endpoint returns 200, the user should refresh `/vip`, open the purchase-management button, and perform a controlled refund test.

## Deployment evidence

- VPS pulled the exact application commit with `git pull --ff-only`.
- `npm run migrate` completed after migration 008 was registered.
- VPS website build passed.
- `mimita-api` restarted and reported online.
- Unauthenticated `GET /api/vip/orders` now returns the expected HTTP 401 instead of the previous database HTTP 500.
- The previous missing-column error is no longer produced after restart.
- Deployed VPS commit: `9392a492e4efc9be50451ea5a3010661fd0d2fef`.
