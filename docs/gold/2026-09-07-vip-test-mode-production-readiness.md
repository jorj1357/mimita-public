// 2026-09-07T17:55:09Z
/* purpose
* preserve the successful MiMITA VIP Stripe test-mode workflow
* record the VPS environment lesson and human-visible Checkout evidence
* provide a production-cutover checklist without storing credentials
* this file does NOT claim that live payments have been tested
* this file does NOT contain Stripe keys, webhook secrets, or customer data
* this file does NOT authorize switching Stripe from test mode to live mode
*/

# Gold behavior: VIP Stripe test-mode flow is coherent

- Timestamp: `2026-09-07T17:55:09Z`
- Environment: Stripe test mode through the VPS-backed SSH-tunnel development site
- VPS role: website runtime only; Stripe configuration lives on the VPS because
  the tunnel sends local frontend API calls to the VPS
- Result: website amounts, server quote amounts, and Stripe Checkout amounts
  agreed for the observed VIP, Super VIP, and Ultra VIP flows

## Human-visible evidence

- VIP prepaid: 1 month `$3.33`, 7 months `$16.96`, 9 months `$19.08` matched
  Stripe Checkout.
- VIP monthly subscription: `$3.33` matched Stripe.
- VIP lifetime: `$111.11` matched Stripe.
- Ultra VIP test purchase reached Stripe Checkout loading for the admin test
  account.
- The success page appeared after Checkout and gave clear purchase feedback.

## Configuration lesson

The local `.env` is not enough for this development setup. The SSH tunnel
connects the browser to the VPS API, so the VPS must contain the matching
Stripe test secret and Price IDs. The secret and every Price ID must belong to
the same Stripe account and mode. A key being present is not proof that Stripe
can retrieve the configured Price.

The verified VPS startup state was:

`[VIP CONFIG] mode=test configured=true missing=none`

## Why this is gold behavior

The working flow came from conversational inspection with GPT-5.6 in the
Codex desktop app, including iterative feedback from the GPT-5.6 Luna/light
experience. The useful behavior was interactive: inspect the browser symptom,
read the VPS logs, compare local and VPS Stripe account context, update the
correct deployment configuration, rebuild, restart, and retest. This was more
effective than treating the frontend display and remote API as unrelated.

## Production cutover still required

Before production:

1. Create or verify the six live monthly/lifetime Price IDs in the intended
   live Stripe account.
2. Create the live webhook endpoint at
   `https://mimita.fun/api/vip/payment/webhook` and copy its live signing secret
   to the VPS only.
3. Replace the VPS test secret and test Price IDs with live values as one
   reviewed configuration change. Never mix test and live objects.
4. Restart `mimita-api`, verify the startup mode is `live` and no required
   values are missing, and verify the public VIP config without exposing
   secrets.
5. Make one controlled live purchase at the lowest-risk tier, confirm the
   Checkout amount, webhook delivery, database order, entitlement, email, and
   receipt, then issue and verify a refund according to the refund policy.
6. Test monthly cancellation through the Stripe billing portal and confirm
   the cancellation webhook updates the account.
7. Monitor the first production transactions and keep rollback credentials and
   the pre-cutover `.env` backup protected and available.

Live payment acceptance is still outstanding; this document records a strong
test-mode result and a successfully deployed production implementation, not
proof that a real-money refund or every live payment path has been manually
accepted.

## Gold-behavior extension: live VIP implementation and deployment

- Observation checkpoint: `2026-09-07T16:05:00-05:00` (4:05 PM EST;
  `2026-09-07T20:05:00Z`)
- Scope: VIP work performed across 2026-09-06 and 2026-09-07, including local
  code, the VPS website runtime, Stripe configuration, database migrations,
  checkout, entitlement, email, management, refund, and documentation work.
- Current result: the implementation is deployed, the VPS migration is
  registered and applied, `mimita-api` is online, and the public VIP config
  reports live Stripe mode with no missing configuration.

### What was built and verified

1. **Three VIP tiers and three purchase modes**
   - VIP, Super VIP, and Ultra VIP share one server-authoritative purchase
     system.
   - Prepaid purchases accept an exact integer duration from 1 through 12
     months and are one-time payments.
   - Monthly purchases are recurring Stripe subscriptions.
   - Lifetime purchases are one-time payments with explicit permanent
     entitlement state and lifetime badges.
2. **Server-authoritative pricing**
   - The browser no longer invents prepaid prices or savings.
   - `/api/vip/config` returns the server-calculated amount and savings table
     for every tier and month.
   - Checkout recalculates/validates the requested tier, mode, integer month
     count, Stripe account, currency, and amount on the server.
   - Stripe Checkout receives the exact server-selected amount, including the
     slider amount rather than a fixed 12-month amount.
3. **Stripe configuration and account matching**
   - The VPS was found to contain Price IDs from a different Stripe account
     than its secret key, causing checkout HTTP 500 errors.
   - VPS environment backups were created before the corrected configuration
     was installed.
   - Matching test-mode configuration was validated first, then the VPS was
     cut over to the intended live Stripe secret, webhook secret, and six
     live Price IDs without storing secrets in repository documentation.
4. **Checkout, webhook, entitlement, receipt, and email behavior**
   - Checkout creates a pending order tied to the signed-in account.
   - Stripe webhook processing is the authority that marks payment paid and
     grants the entitlement.
   - The order stores user ID, tier, purchase type, amount, currency, Stripe
     identifiers, timestamps, result status, and confirmation-email status.
   - The success page shows the account email, order, purchase type, amount,
     expiration/permanent state, and confirmation-email status.
5. **Management behavior**
   - Recurring subscriptions expose `manage subscription` and open Stripe
     Billing Portal for billing details and cancellation.
   - Active prepaid and lifetime purchases expose `manage VIP purchase /
     refund` and open the matching purchase-management page.
6. **Automatic refund behavior**
   - Prepaid and lifetime refunds use one final customer confirmation click.
   - The server verifies account ownership, paid status, one-time purchase
     type, stored Payment Intent, and the 30-day refund window.
   - The server sends Stripe the stored Payment Intent and stored amount; the
     browser cannot choose the refund amount or another user’s order.
   - Stripe webhook confirmation remains authoritative before the order and
     entitlement become refunded.
   - The entitlement is removed after the refund event, refund status and
     Stripe refund ID are recorded, and a refund confirmation email is
     attempted.
   - Duplicate refund requests and monthly-subscription refund attempts are
     rejected safely.

### Regressions encountered and fixes

- **Slider not visible:** local `Vip.jsx` was newer than the VPS API/frontend
  used through the SSH tunnel. The VPS was pulled from the confirmed branch,
  rebuilt, and the API restarted.
- **Stripe checkout HTTP 500:** the VPS secret and configured Price IDs came
  from different Stripe accounts. The VPS was backed up and updated with
  matching account/mode values.
- **Slider amount/display disagreement:** the browser calculated savings from
  the base-month price instead of the full selected-month total. The server
  quote table became the single source of truth.
- **Lifetime buttons disabled:** the VPS lacked the lifetime configuration
  values even though the local `.env` contained them. Required lifetime
  values were added to the VPS environment.
- **Prepaid/lifetime management missing:** only recurring subscriptions had a
  management button. The `/vip` page now loads orders and exposes the proper
  one-time purchase/refund route.
- **Refund management page displayed “Purchase details are still loading”:**
  the new refund columns were queried before their migration was registered.
  Migration 008 existed but `db.js` only ran versions 1, 5, 6, and 7. Version
  8 was registered and applied on the VPS, then the API was rebuilt/restarted.
- **Unrelated browser warnings:** Cloudflare Insights integrity/CORS,
  Metricool loading, font visibility, and layout-flash warnings were not the
  cause of the VIP failure. The decisive failure was the VPS database error:
  `column "refund_status" does not exist`.

### Refund status at the observation checkpoint

At the 2026-09-07 16:05 EST checkpoint, the automatic refund implementation
was deployed and the missing-migration failure had been fixed. The focused
refund tests passed, including the owned-order Stripe refund request and the
refund webhook entitlement-removal path. A completed real-money live refund
was not claimed as human-observed evidence in this record; that still requires
one deliberately selected live transaction to be refunded and checked in
Stripe Dashboard, the webhook logs, the database, the entitlement state, and
the customer email.

### Why this is gold behavior

This implementation is a strong example of the intended lower-input,
higher-output workflow:

- inspect the actual local source, VPS code, environment, logs, database, and
  browser requests before guessing;
- keep pricing, payment authority, account ownership, and entitlement changes
  on the server;
- use the VPS as a minimal website runtime and deployment target rather than
  treating it as a second development repository;
- make one focused change, run the relevant tests/build, deploy the exact
  commit, migrate, restart only the relevant service, and verify the live
  endpoint;
- document regressions and exact fixes so the same failure needs less future
  investigation.

The live-editing experience was especially effective because the website
frontend could be rebuilt and served quickly while the VPS API could be
inspected and restarted directly. That made the visible browser behavior,
server response, Stripe result, and deployment state converge in short
feedback loops instead of repeatedly editing blindly and rebuilding without
checking the remote owner.

The user reports using GPT-5.6 Luna/light and medium-style Codex interaction,
and possibly Mimo v2.5 through OpenCode for some VIP work. The exact model
ownership of each change was not independently recorded, so this is preserved
as user-reported context rather than deployment evidence. The verifiable gold
behavior is the source, test, migration, deployment, log, and browser/API
evidence recorded in the linked changelogs and regressions.
