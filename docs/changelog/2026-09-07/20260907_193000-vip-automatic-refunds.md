# Automatic VIP refunds

- Branch: `8292026stash`
- Timestamp: `2026-09-07T19:30:00Z`
- Pre-existing worktree changes: unrelated game/replay/config edits were present and were not staged or modified by this task.
- Application commit: `8cb50c563215d006d2c69076ddeedbc6789ed10a`

## Implemented

- Added `website/server/migrations/008_vip_refunds.sql` with refund request/completion/error and refund-email tracking fields.
- Added authenticated `POST /api/vip/orders/:id/refund` in `website/server/vip-routes.js`.
- The endpoint verifies account ownership, paid status, one-time purchase type, Payment Intent presence, and the existing 30-day refund window.
- The endpoint atomically claims the paid order as refund-requested, calls Stripe Refunds with the server-stored amount and Payment Intent, records the Stripe refund ID, and safely records Stripe failures.
- Extended `website/server/vip-payments.js` so Stripe refund webhook processing stores completed refund state, removes the entitlement, recomputes VIP state, and attempts refund confirmation email delivery.
- Added `sendVipRefundEmail` in `website/server/mail.js`.
- Updated `website/src/pages/VipSuccess.jsx` with a confirmation screen, automatic refund request, pending message, error handling, and final refunded state.
- Added route coverage for an owned refundable VIP order.
- Appended the confirmed behavior to `docs/regressions/regressions-v1.md`.

## Validation

- `node --test server/vip-payments.test.js server/vip-config.test.js`: 25 passed, 0 failed.
- New owned-refund route test passed.
- `npm run build`: passed.
- `node --test server/vip-routes.test.js`: new refund test passed; existing unrelated join-ticket test remains failing (`free` vs `super_vip`).
- Human acceptance still required in Stripe test mode: create prepaid/lifetime purchase, confirm refund, verify Stripe refund event, entitlement removal, email, and final page state.

## Deployment

## Deployment evidence

- The exact application commit was pushed to `origin/8292026stash`.
- VPS pulled with `git pull --ff-only origin 8292026stash`.
- VPS migration completed with `database migrations complete`.
- VPS website build passed.
- `mimita-api` restarted and reported online.
- Deployed VPS commit: `8cb50c563215d006d2c69076ddeedbc6789ed10a`.
- Live `/api/vip/config` returned `stripe.mode=live`, `configured=true`, and `missing=[]`.
- Existing VPS untracked backups and temporary files were preserved.
- Remaining human acceptance: perform a controlled Stripe test-mode refund, then a deliberately selected live refund only after verifying the test-mode flow.
