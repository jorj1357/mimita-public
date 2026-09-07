# VPS VIP configuration deployment

- Branch: `8292026stash`
- Local implementation commit already present before this session: `c5a488e78a54e483dfb35342dfe93ee4ca37be06`
- Documentation commit created in this session: `adc8076ec71ddc2770ace496b9dae89ec9ed6d5d`
- Deployment time: `2026-09-07T17:13:59Z` through `2026-09-07T17:16:00Z`
- VPS deployment target: `/root/mimita-site`
- VPS service restarted: `mimita-api`

## Pre-existing work preserved

`docs/regressions/regressions-v1.md` was already modified before this session
and was not staged, rewritten, or included in the documentation commit. The
VPS already contained these untracked files before deployment and they were
preserved:

- `website/db-check-temp.mjs`
- `website/src/pages/ProfilePage.jsx.pre-reward-20260901-150551`

No secret values, database records, or user records are included in this file.

## Local change made

File: `docs/operations/vps-deployment/vps-deployment.md:34-49`

Old behavior: the deployment guide had only the general local-development and
VPS-target rules.

New behavior: the guide now has a “VPS storage policy” section stating that
the VPS is storage-constrained, should contain only the website runtime,
production `dist/`, Node dependencies, required configuration, backups, and
minimum service files, and must not receive game source, C++ files, game
assets, development artifacts, or unrelated repositories. It also requires
disk-usage reporting, preservation of unrelated untracked files, and explicit
approval before cleanup.

The file was committed and pushed as `adc8076ec71ddc2770ace496b9dae89ec9ed6d5d`.

## Deployed VIP implementation

The approved existing commit `c5a488e78a54e483dfb35342dfe93ee4ca37be06`
contains the VIP implementation used by this deployment:

- `website/server/vip-config.js:254-319` calculates prepaid amounts from the
  configured monthly tier amount and exposes a secret-free Stripe readiness
  report.
- `website/server/vip-payments.js:689-703` reads the configured monthly Stripe
  Price amount for prepaid calculations; fixed monthly and lifetime purchases
  use their configured Stripe Price IDs.
- `website/server/server.js` logs only Stripe mode, readiness, and missing-key
  names at startup.
- `website/.env.example:8-25` documents the six required monthly/lifetime
  Price ID variables without real secret values.
- `website/VIP_CONFIGURATION.md:5-31,68-82` documents VPS authority, the
  server-authoritative 1–12 month slider, and lifetime purchases.

## VPS configuration and deployment evidence

Before pulling, the VPS was on `6c2b08218571b54e8a3f834e8c7722d684893a01`,
had only `STRIPE_SECRET_KEY` and `STRIPE_VIP_WEBHOOK_SECRET` among the VIP
Stripe environment keys, and had 687 MB available (97% used).

The approved branch was pulled with `git pull --ff-only origin 8292026stash`.
The resulting VPS commit is:

`adc8076ec71ddc2770ace496b9dae89ec9ed6d5d`

The VPS `.env` was backed up as:

`/root/mimita-site/website/.env.backup-20260907_171101`

Only non-secret Stripe Price IDs and metadata were added to the live VPS
environment. The metadata is:

- `VIP_STRIPE_MODE=test`
- `VIP_STRIPE_CONFIG_VERSION=2026-09-07-v1`
- `VIP_STRIPE_CONFIG_UPDATED_AT_UTC=2026-09-07T17:11:01Z`

The six required Price ID names were verified present with dotenv parsing:

- `MIMITA_STRIPE_PRICE_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_VIP_LIFETIME`
- `MIMITA_STRIPE_PRICE_SUPER_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_SUPER_VIP_LIFETIME`
- `MIMITA_STRIPE_PRICE_ULTRA_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_ULTRA_VIP_LIFETIME`

The website was rebuilt on the VPS with `npm run build`, then `mimita-api` was
restarted with updated environment values. PM2 reported the service online.
Startup logs reported:

`[VIP CONFIG] mode=test configured=true missing=none`

The public `/api/vip/config` response reported prepaid, monthly subscription,
and lifetime configured for `vip`, `super_vip`, and `ultra_vip`, with lifetime
defaults of 11111, 22222, and 33333 cents respectively.

After deployment the VPS still reported 686 MB available (97% used). This is a
deployment warning, not a reason to delete files automatically.

## Local validation

- `node --test server/vip-config.test.js server/vip-payments.test.js --test-name-pattern='prepaid slider|lifetime checkout|subscription checkout|syncActiveSubscriptions creates'`: 24 passed, 0 failed.
- `npm run build` from `website/`: passed.
- VPS `npm run build`: passed, with the existing Vite chunk-size warning.
- PM2 startup/config log and public `/api/vip/config`: passed.

## Human review still required

Use Stripe test mode to perform one real test checkout for each tier and each
mode (prepaid slider, monthly subscription, lifetime), then verify the matching
account, amount, webhook delivery, confirmation email, order record, refund
behavior, and subscription-management behavior. Keep the VPS in test mode
until those acceptance checks pass. Live mode requires replacing the Stripe
secret, webhook signing secret, and all six Price IDs with live-mode values.
