# VIP server quote table and slider display repair

- Branch: `8292026stash`
- Code commit deployed: `a90fa4f5e54313658e0651dfb4807a6101ec1733`
- Deployment date: `2026-09-07`
- Service restarted: `mimita-api`

## Pre-existing work preserved

Unrelated replay source edits and replay documentation were already present in
the worktree. Only the VIP source, VIP test, regression, and this changelog
were staged for this task. No unrelated gameplay files were staged.

## Root cause

The server/Stripe amount for VIP 12 months was 1998 cents, but
`website/src/pages/Vip.jsx` calculated the browser display as:

`monthly base × months - monthly base × discount`

instead of:

`monthly base × months - (monthly base × months × discount)`

With a 333-cent monthly base, the browser showed 3829 cents (`$38.29`) and
only 167 cents (`$1.67`) saved, while Stripe correctly received 1998 cents
(`$19.98`).

## Exact changes

- `website/server/vip-config.js`: public VIP configuration now includes
  server-calculated `amounts_cents` and `savings_cents` arrays for all 12
  integer month values for every tier.
- `website/src/pages/Vip.jsx`: removed the duplicate browser arithmetic and
  reads the server quote arrays. The savings label uses the selected month
  count and calculated discount instead of hardcoded 12-month wording.
- `website/server/vip-config.test.js`: verifies the VIP quote table starts
  `[333, 636, 909]`, ends at 1998 cents, and reports 1998 cents saved at 12
  months.
- `docs/regressions/regressions-v1.md`: appended the confirmed slider display
  regression and resolution.

## Validation

- Focused VIP tests: 25 passed, 0 failed.
- Local `npm run build`: passed.
- VPS `npm run build`: passed.
- VPS commit verified: `a90fa4f5e54313658e0651dfb4807a6101ec1733`.
- VPS PM2 `mimita-api`: online after restart.
- VPS `/api/vip/config`: returned the server quote table. VIP values were
  `[333,636,909,1151,1363,1544,1696,1817,1908,1968,1998,1998]` cents.

## Pricing clarification

The deployed formula preserves the exact endpoints: 1 month has no discount
and 12 months has 50% discount. Under a straight line between those endpoints,
6 months is 22.7% off, not exactly 25%. The requested examples “2 months =
$6.50” and “3 months = 12.5% off” are not mathematically consistent with both
the 1-month `$3.33` endpoint and the 12-month 50%-off endpoint. The API and UI
currently share the endpoint-consistent server formula.

## Human acceptance

Refresh the tunneled local VIP page and verify 1, 2, 3, 6, and 12 months. The
displayed amount and savings should come from the API quote table, and Stripe
Checkout should show the same amount. Then test the webhook and entitlement
flow in Stripe test mode.
