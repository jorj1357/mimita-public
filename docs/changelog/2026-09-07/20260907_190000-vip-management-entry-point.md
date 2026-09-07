# VIP management entry point

- Branch: `8292026stash`
- Timestamp: `2026-09-07T19:00:00Z`
- Pre-existing worktree changes: unrelated game/replay/config edits were present and were not staged or modified by this task.
- Commit: to be recorded after validation.

## Change

File: `website/src/pages/Vip.jsx`

Old behavior at the Current Status section: active recurring subscriptions rendered the existing `manage subscription` button, while any other active VIP entitlement rendered only a notice saying it was prepaid and had no active subscription.

New behavior:

- Authenticated `/vip` loads `/api/vip/orders`.
- A paid one-time order matching the active tier is selected for management.
- Active recurring subscriptions keep the Stripe Billing Portal button.
- Active prepaid and lifetime purchases show `manage VIP purchase / refund` and navigate to `/vip/success?order_id=<id>`, where the user can see the purchase/account details and reach the existing refund request route.
- No Stripe secrets or prices were added to the client.

File: `docs/regressions/regressions-v1.md`

Added the append-only regression record `2026-09-07T19:00:00Z` describing the missing prepaid/lifetime management entry point, its cause, fix, and remaining human acceptance.

## Documents and review

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/vip/vip.md`, `docs/regressions/regressions-v1.md`, and `docs/skills/spec-behavior-review-v1.md`.
- Existing `/api/vip/orders`, `/vip/success`, `/support?refund_order=...`, and `/api/vip/manage-subscription` owners were reused.

## Validation

- `npm run build` in `website`: passed.
- Focused VIP tests: 25 passed; one pre-existing VIP join-ticket route test failed with expected `free` vs `super_vip` and does not exercise this page change.
- Full website test suite still has unrelated local PostgreSQL connection failures and the same pre-existing join-ticket failure.
- Human review still required: sign in with an active prepaid/lifetime account, click the new button, verify the matching order/email/refund details; separately verify a recurring account opens Stripe Billing Portal.

## Deployment

This changelog is created before completion. The final deployed commit and service verification will be added to the completion report after the exact commit is deployed.
