# MiMITA VIP Configuration

<!-- 08 03 2026, 17 20 -->

VIP checkout uses server-selected Stripe Price IDs. Do not put real Price IDs in client code or committed config.

Required server environment variables for checkout:

- `STRIPE_SECRET_KEY`
- `STRIPE_VIP_WEBHOOK_SECRET`
- `APP_ORIGIN`
- `MIMITA_STRIPE_PRICE_VIP_ONE_MONTH`
- `MIMITA_STRIPE_PRICE_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_VIP_TWELVE_MONTH`
- `MIMITA_STRIPE_PRICE_SUPER_VIP_ONE_MONTH`
- `MIMITA_STRIPE_PRICE_SUPER_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_SUPER_VIP_TWELVE_MONTH`
- `MIMITA_STRIPE_PRICE_ULTRA_VIP_ONE_MONTH`
- `MIMITA_STRIPE_PRICE_ULTRA_VIP_MONTHLY`
- `MIMITA_STRIPE_PRICE_ULTRA_VIP_TWELVE_MONTH`

Webhook events handled by `/api/vip/payment/webhook`:

- `checkout.session.completed`
- `invoice.payment_succeeded`
- `invoice.payment_failed`
- `customer.subscription.created`
- `customer.subscription.updated`
- `customer.subscription.deleted`
- `charge.refunded`
- `charge.dispute.created`

## Webhook endpoint setup

Stripe delivers events only to webhook endpoints created in the Stripe Dashboard
(Developers -> Webhooks). The VIP flow needs an endpoint:

- **URL:** `https://mimita.fun/api/vip/payment/webhook` (or `http://localhost:3002/api/vip/payment/webhook` locally)
- **Events:** the 8 events listed above.

After creating the endpoint, copy its signing secret (`whsec_...`) into the server
environment as `STRIPE_VIP_WEBHOOK_SECRET`, then restart the API. The webhook
endpoint appears in the Dashboard and can be edited there at any time.

## Paid checkout recovery (webhook fallback)

`/api/vip/me` reconciles `pending` orders older than 60 seconds by checking the
checkout session directly with the Stripe API. If the session is paid but the
webhook never arrived (for example a misconfigured or missing webhook endpoint),
the entitlement is granted the same way a webhook would grant it. This makes a
paid one-time purchase appear even if Stripe webhooks are not configured.

## Local testing

1. Start the site server on port 3002 (`npm run server`).
2. Forward Stripe test webhooks to it:
   ```
   stripe listen --forward-to localhost:3002/api/vip/payment/webhook
   ```
3. Use the `whsec_...` value printed by `stripe listen` as `STRIPE_VIP_WEBHOOK_SECRET` in `.env`.
4. Create a checkout from the VIP page, pay with a Stripe test card (`4242 4242 4242 4242`), and confirm the entitlement appears.

If you do not run `stripe listen`, the paid-checkout recovery above still grants
one-time purchases once the session is older than 60 seconds.

## Refunds

One-time VIP purchases (`one_month`, `twelve_month`) are refundable for 100% within
30 days of `paid_at`. The website exposes:

- `GET /api/vip/orders` (authenticated) — the user's orders with `refund_until` and `refundable` flags.
- `POST /api/vip/refund` (authenticated, body `{ order_id }`) — issues a Stripe refund and revokes the
  matching entitlement immediately (server-verified via the Stripe API, independent of webhooks).

Monthly subscriptions are not refunded through this endpoint; they are cancelled in the
Stripe billing portal (`POST /api/vip/manage-subscription`).

## Manage Subscription

The "manage subscription" action opens the Stripe billing portal. It is only shown when the
account has an active subscription (`active`, `trialing`, or `past_due`). Accounts that only
bought one-time packages have no subscription to manage and see a note instead of the portal.

## Admin VIP tools

Admins can manage any player's VIP from the admin dashboard (all routes under `/api/admin/vip`,
all `requireAdmin`):

- `GET /api/admin/vip/lookup?query=` — find a user by id, username, or email. Returns their
  VIP state, active entitlements, subscriptions, and flags:
  - `has_active_subscription` / `subscription_tier`
  - `desync` — true when they have an active subscription but are displaying a lower tier
    ("they actually have it but aren't getting it").
- `POST /api/admin/vip/grant` — grant a tier for N months (`admin` source).
- `POST /api/admin/vip/revoke` — expire all active entitlements.
- `POST /api/admin/vip/style` — set a player's name style (validated against their tier).
- `POST /api/admin/vip/resync` — re-create entitlements for active subscriptions that are
  missing them (fixes `desync`).

The dashboard shows a large pulsing red flag when `desync` is detected and warns whenever a
manual grant/revoke would override a real subscription.

Entitlement rules:

- Paid VIP starts only after a verified Stripe webhook.
- The server verifies event signature, event idempotency, order ownership, Price ID, amount, currency, tier, and purchase type.
- Upgrades do not prorate; the highest currently active tier is displayed immediately.
- Lower-tier prepaid/subscription entitlement remains active until its own end date.
- One-month and twelve-month prepaid purchases use UTC calendar month arithmetic.
- Subscriptions use Stripe `current_period_start` and `current_period_end`.
- Staff role colors override VIP name colors; staff can still display the highest active VIP badge.
- Game clients may request short-lived, one-time-use, room-bound join tickets, but servers must treat missing or invalid tickets as free gray styling instead of trusting client-supplied VIP data. Coordinator join tokens are not sent to the website VIP API.