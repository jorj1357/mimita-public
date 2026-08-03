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

Entitlement rules:

- Paid VIP starts only after a verified Stripe webhook.
- The server verifies event signature, event idempotency, order ownership, Price ID, amount, currency, tier, and purchase type.
- Upgrades do not prorate; the highest currently active tier is displayed immediately.
- Lower-tier prepaid/subscription entitlement remains active until its own end date.
- One-month and twelve-month prepaid purchases use UTC calendar month arithmetic.
- Subscriptions use Stripe `current_period_start` and `current_period_end`.
- Staff role colors override VIP name colors; staff can still display the highest active VIP badge.
- Game clients may request short-lived, one-time-use, room-bound join tickets, but servers must treat missing or invalid tickets as free gray styling instead of trusting client-supplied VIP data. Coordinator join tokens are not sent to the website VIP API.