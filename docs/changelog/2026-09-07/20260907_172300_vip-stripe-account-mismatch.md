# VIP checkout Stripe-account mismatch investigation and repair

- Branch: `8292026stash`
- Local HEAD before this documentation record: `cb86e41`
- Investigation time: `2026-09-07T17:17:20Z` through `2026-09-07T17:22:00Z`
- Deployment target: `/root/mimita-site`
- Service restarted: `mimita-api`

## Pre-existing work preserved

The local worktree already contained modifications to
`config/analytics.json` and `docs/regressions/regressions-v1.md`, plus the
untracked replay changelog
`docs/changelog/2026-09-07/20260907_171023_replay-effects-camera-investigation.md`.
The existing analytics change and replay changelog were not modified. The VIP
regression file was intentionally appended to because this checkout failure
was confirmed during this session.

## Reported behavior

The browser sent `POST /api/vip/payment/checkout` through
`http://localhost:5173`, and the API returned HTTP 500 for prepaid, monthly,
and lifetime buttons. The SSH-tunnel launcher forwards local port 3002 to the
VPS, so this was a VPS API failure observed through the local frontend.

## Evidence

VPS PM2 logs recorded:

`StripeInvalidRequestError: No such price: 'price_1UCp8YGvytRPXxx5PrzYedv9'`

The active VPS test Stripe account could not retrieve any of the six configured
monthly/lifetime Price IDs. The local test environment's Stripe account could
retrieve all six with the expected amounts. Account identifiers were used only
for comparison and secret values are intentionally omitted.

## Change made

The VPS `.env` was backed up as:

`/root/mimita-site/website/.env.backup-20260907_172107`

Only `STRIPE_SECRET_KEY` was replaced with the matching test-account key. No
application source code, database records, user records, webhook secret, or
Price IDs were changed. `mimita-api` was restarted with `--update-env`.

## Verification

- All six Price IDs were retrieved successfully from Stripe using the active
  VPS test key.
- VPS startup reported `[VIP CONFIG] mode=test configured=true missing=none`.
- `https://mimita.fun/api/vip/config` reported every prepaid, monthly, and
  lifetime option configured for VIP, Super VIP, and Ultra VIP.
- Disk usage remained 97% used with approximately 686 MB available.
- The confirmed regression was appended to
  `docs/regressions/regressions-v1.md` under the 2026-09-07 checkout failure.

## Remaining human acceptance

Retest one prepaid slider checkout, one monthly subscription, and one lifetime
checkout in Stripe test mode. Confirm the exact Stripe amount, webhook
delivery, account-linked entitlement, confirmation email, subscription
management, and refund behavior before switching the VPS to live credentials.
