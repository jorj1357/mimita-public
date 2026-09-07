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
test-mode result, not proof that production payments are ready without the
controlled live test.
