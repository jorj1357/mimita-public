// 08 02 2026, 15 10
/* purpose
* Handle paid banner checkout sessions and Stripe webhooks for the MiMITA website.
* Create internal payment orders, create Stripe Checkout Sessions, verify webhooks,
* and mark orders paid exactly once.
* DOES NOT render banner content or manage the banner display system.
* DOES NOT expose Stripe secret keys to clients.
* DOES NOT activate payment based on a success-page redirect.
*/

import { Router } from "express"
import Stripe from "stripe"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { createRateLimit } from "./rateLimit.js"
import {
    BANNER_CONFIG,
    PRICE_CENTS_PER_DAY,
    PAID_KIND,
    validateBannerContent,
    placeBanner,
    userOnCooldown
} from "./site-banner.js"

export const MIN_DAYS = BANNER_CONFIG.paid_min_days
export const MAX_DAYS = BANNER_CONFIG.paid_max_days
export const ORDER_CURRENCY = "usd"
export const FLOW_SOURCE = "mimita_banner"

let stripeClient = null

export function getStripe() {
    if (!process.env.STRIPE_SECRET_KEY) return null
    if (!stripeClient) {
        stripeClient = new Stripe(process.env.STRIPE_SECRET_KEY)
    }
    return stripeClient
}

export function validateDurationDays(value) {
    if (value === undefined || value === null || value === "") return null
    const n = Number(value)
    if (!Number.isInteger(n)) return null
    if (n < MIN_DAYS || n > MAX_DAYS) return null
    return n
}

export function amountCentsForDays(days) {
    return days * PRICE_CENTS_PER_DAY
}

function frontendUrl(req, path) {
    const origin = req.headers.origin || process.env.APP_ORIGIN || "http://localhost:5173"
    return `${origin}${path}`
}

function cleanOrderId(value) {
    const n = Number(value)
    return Number.isInteger(n) && n > 0 ? n : null
}

function createCheckoutSessionRouter(deps = {}) {
    const {
        stripe = getStripe(),
        query = (text, params) => pool.query(text, params),
        authenticateMw = authenticate,
        checkoutRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "banner_checkout" })
    } = deps

    const router = Router()

    router.post("/create-checkout-session", checkoutRateLimit, authenticateMw, async (req, res, next) => {
        try {
            if (!stripe) {
                return res.status(503).json({ success: false, message: "payments not configured" })
            }

            const durationDays = validateDurationDays(req.body?.duration_days)
            if (!durationDays) {
                return res.status(400).json({
                    success: false,
                    message: `duration_days must be an integer from ${MIN_DAYS} to ${MAX_DAYS}`
                })
            }

            const validation = validateBannerContent(req.body)
            if (!validation.ok) {
                return res.status(400).json({ success: false, message: validation.error })
            }
            const v = validation.value

            if (await userOnCooldown(query, req.user.id)) {
                return res.status(429).json({
                    success: false,
                    message: `you can create a banner once every ${BANNER_CONFIG.cooldown_minutes} minutes`
                })
            }

            // Price is always derived server-side; a browser-supplied price is never trusted.
            const amountCents = amountCentsForDays(durationDays)

            const bannerResult = await query(
                `INSERT INTO site_banners (user_id, kind, days, message, target_url, background_color, text_color, status)
                 VALUES ($1, $2, $3, $4, $5, $6, $7, 'draft')
                 RETURNING id`,
                [req.user.id, PAID_KIND, durationDays, v.message, v.target_url, v.background_color, v.text_color]
            )
            const bannerId = bannerResult.rows[0].id

            const orderResult = await query(
                `INSERT INTO banner_payment_orders (user_id, duration_days, amount_cents, currency, status)
                 VALUES ($1, $2, $3, $4, 'pending')
                 RETURNING id`,
                [req.user.id, durationDays, amountCents, ORDER_CURRENCY]
            )
            const orderId = orderResult.rows[0].id

            await query(
                `UPDATE site_banners
                 SET payment_order_id = $1, status = 'pending_payment', updated_at = NOW()
                 WHERE id = $2`,
                [orderId, bannerId]
            )

            const session = await stripe.checkout.sessions.create({
                mode: "payment",
                line_items: [{
                    price_data: {
                        currency: ORDER_CURRENCY,
                        unit_amount: amountCents,
                        product_data: {
                            name: "MiMITA banner time",
                            description: `${durationDays} banner day${durationDays === 1 ? "" : "s"}`
                        }
                    },
                    quantity: 1
                }],
                metadata: {
                    banner_order_id: String(orderId),
                    source: FLOW_SOURCE
                },
                success_url: frontendUrl(req, "/banner/success"),
                cancel_url: frontendUrl(req, "/banner")
            })

            await query(
                `UPDATE banner_payment_orders
                 SET stripe_checkout_session_id = $1, updated_at = NOW()
                 WHERE id = $2`,
                [session.id, orderId]
            )

            res.json({
                success: true,
                url: session.url,
                session_id: session.id,
                order_id: orderId,
                banner_id: bannerId,
                amount_cents: amountCents
            })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

function webhookLog(fields) {
    console.log("[BANNER WEBHOOK] " + JSON.stringify(fields))
}

function createWebhookRouter(deps = {}) {
    const {
        stripe = getStripe(),
        webhookSecret = process.env.STRIPE_WEBHOOK_SECRET || null,
        getClient = () => pool.connect()
    } = deps

    const router = Router()

    router.post("/", async (req, res) => {
        try {
            if (!stripe || !webhookSecret) {
                webhookLog({ signature: "unconfigured", result: "503" })
                return res.status(503).json({ success: false, message: "webhooks not configured" })
            }

            const signature = req.headers["stripe-signature"]

            let event
            try {
                event = stripe.webhooks.constructEvent(req.rawBody || req.body, signature, webhookSecret)
            }
            catch (error) {
                webhookLog({
                    signature: "fail",
                    reason: error?.message || "constructEvent threw",
                    result: "400"
                })
                return res.status(400).json({ success: false, message: "invalid signature" })
            }

            if (event.type !== "checkout.session.completed") {
                webhookLog({
                    event_id: event.id,
                    event_type: event.type,
                    signature: "ok",
                    result: "ignored_non_checkout"
                })
                return res.json({ success: true, received: event.type })
            }

            const session = event.data.object
            const base = {
                event_id: event.id,
                event_type: event.type,
                session_id: session.id,
                amount_cents: Number(session.amount_total),
                currency: session.currency,
                payment_status: session.payment_status,
                livemode: session.livemode,
                signature: "ok"
            }

            if (session.metadata?.source !== FLOW_SOURCE) {
                webhookLog({ ...base, result: "400_unrecognized_event" })
                return res.status(400).json({ success: false, message: "unrecognized event" })
            }

            const orderId = cleanOrderId(session.metadata?.banner_order_id)
            if (!orderId) {
                webhookLog({ ...base, result: "400_invalid_order_id" })
                return res.status(400).json({ success: false, message: "invalid order id" })
            }
            base.order_id = orderId

            if (session.payment_status !== "paid") {
                webhookLog({ ...base, result: "400_session_not_paid" })
                return res.status(400).json({ success: false, message: "session not paid" })
            }

            const sessionAmountCents = Number(session.amount_total)
            const sessionCurrency = String(session.currency || ORDER_CURRENCY).toLowerCase()

            if (!Number.isInteger(sessionAmountCents) || sessionAmountCents <= 0) {
                webhookLog({ ...base, result: "400_invalid_amount" })
                return res.status(400).json({ success: false, message: "invalid amount" })
            }

            const client = await getClient()
            try {
                await client.query("BEGIN")

                // Mark the order paid exactly once, guarded by status + amount/currency.
                const result = await client.query(
                    `UPDATE banner_payment_orders
                     SET status = 'paid',
                         paid_at = COALESCE(paid_at, NOW()),
                         stripe_event_id = $1,
                         stripe_checkout_session_id = COALESCE(NULLIF(stripe_checkout_session_id, ''), $2),
                         payment_intent_id = COALESCE(NULLIF($6, ''), payment_intent_id),
                         updated_at = NOW()
                     WHERE id = $3
                       AND status = 'pending'
                       AND amount_cents = $4
                       AND currency = $5
                     RETURNING id, duration_days, user_id`,
                    [event.id, session.id, orderId, sessionAmountCents, sessionCurrency, session.payment_intent || ""]
                )

                if (result.rowCount > 0) {
                    // Order transitioned pending -> paid in this call. Activate its banner
                    // in the same transaction. A duplicate delivery can never reach here.
                    const bannerResult = await client.query(
                        `SELECT id FROM site_banners
                         WHERE payment_order_id = $1 AND status = 'pending_payment'`,
                        [orderId]
                    )
                    if (bannerResult.rows.length) {
                        await placeBanner(client, bannerResult.rows[0].id)
                    }
                    await client.query("COMMIT")
                    webhookLog({ ...base, result: "200_paid_activated" })
                    return res.json({ success: true, order_id: orderId, status: "paid" })
                }

                // No pending order matched: duplicate, mismatch, or unknown.
                const check = await client.query(
                    `SELECT status, amount_cents, currency FROM banner_payment_orders WHERE id = $1`,
                    [orderId]
                )
                if (check.rows.length === 0) {
                    await client.query("ROLLBACK")
                    webhookLog({ ...base, result: "400_order_not_found" })
                    return res.status(400).json({ success: false, message: "order not found" })
                }

                const row = check.rows[0]

                if (row.status === "paid") {
                    await client.query("ROLLBACK")
                    webhookLog({ ...base, result: "200_duplicate" })
                    return res.json({ success: true, duplicate: true })
                }

                if (Number(row.amount_cents) !== sessionAmountCents || String(row.currency).toLowerCase() !== sessionCurrency) {
                    await client.query("ROLLBACK")
                    webhookLog({ ...base, result: "400_amount_or_currency_mismatch" })
                    return res.status(400).json({ success: false, message: "amount or currency mismatch" })
                }

                await client.query("ROLLBACK")
                webhookLog({ ...base, result: "400_cannot_be_paid" })
                return res.status(400).json({ success: false, message: "order cannot be paid" })
            }
            catch {
                await client.query("ROLLBACK").catch(() => {})
                webhookLog({ ...base, result: "500_processing_failed" })
                res.status(500).json({ success: false, message: "webhook processing failed" })
            }
            finally {
                client.release()
            }
        }
        catch {
            res.status(500).json({ success: false, message: "webhook processing failed" })
        }
    })

    return router
}

export { createCheckoutSessionRouter, createWebhookRouter }
