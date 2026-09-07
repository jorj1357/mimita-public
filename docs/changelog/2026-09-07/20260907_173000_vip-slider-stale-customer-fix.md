# VIP slider display and checkout recovery

- Branch: `8292026stash`
- Commit: `77318644a7ec4b9f2b1eb6deb0cf285f59823665`
- Investigation and deployment date: `2026-09-07`
- VPS deployed commit: `77318644a7ec4b9f2b1eb6deb0cf285f59823665`
- Service restarted: `mimita-api`

## Pre-existing work preserved

The worktree contained unrelated replay source edits. Only the three VIP
files named below were staged. Existing VPS untracked files and environment
backups were preserved.

## Confirmed checkout failure

The SSH-tunnel development launcher sends local frontend API traffic to the
VPS API. PM2 logs showed repeated HTTP 500 responses for
`POST /api/vip/payment/checkout` caused by:

`StripeInvalidRequestError: No such customer: cus_V7frcBpyPd2O9M`

The database contained a Stripe customer ID from the prior Stripe test
account. The checkout path trusted that ID without checking whether it existed
in the currently configured Stripe account.

## Exact code changes

1. `website/server/vip-payments.js:197-226`
   - Added `clearStripeCustomerId()`.
   - Changed `ensureStripeCustomer()` to retrieve an existing customer before
     reuse. A Stripe `resource_missing` result clears only the matching stale
     database value, logs a safe `stale_stripe_customer_cleared` event, and
     creates a customer in the active account.
2. `website/src/pages/Vip.jsx:200`
   - Replaced the hardcoded `buy 12 months for the price of 6` text with a
     selected-month display: `Save $X · N months at Y% off`.
   - The one shared slider remains 1–12 months with integer steps.
3. `website/server/vip-payments.test.js`
   - Added a stale-customer recovery test and fake Stripe retrieve behavior.

## Spec comparison

`docs/specs/vip/vip.md:340-366` defines one-month, monthly-subscription, and
twelve-month one-time options and lists the tier prices. It does not define
the newer arbitrary 1–12 month slider formula in detail. The implemented
slider extension calculates server-side from the configured monthly amount,
with a linear discount from 0% at one month to 50% at twelve months. The
browser sends only the integer month count.

`docs/specs/vip/vip.md:578-605` requires Stripe authority, immutable user IDs,
and trusted server-side fulfillment. The stale-customer recovery preserves
that authority; it does not grant VIP and only repairs the Stripe customer
reference before Checkout creation.

## Validation

- Focused VIP tests: 25 passed, 0 failed.
- Local website build: passed.
- VPS website build: passed.
- VPS commit verified: `77318644a7ec4b9f2b1eb6deb0cf285f59823665`.
- PM2 reports `mimita-api` online.
- Startup reports `[VIP CONFIG] mode=test configured=true missing=none`.

## Human retest

Refresh the local VIP page and retry 1 month, several middle slider values,
monthly subscription, and lifetime. The first checkout after this fix should
replace the stale customer and proceed to Stripe test Checkout. Verify the
displayed amount, Checkout amount, webhook, entitlement, email, and account
linkage.
