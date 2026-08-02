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

export const MIN_DAYS = 1
export const MAX_DAYS = 7
export const PRICE_CENTS_PER_DAY = 100
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

function frontendUrl(path) {
    const origin = process.env.APP_ORIGIN || ""
    return origin ? `${origin}${path}` : path
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

            // Price is always derived server-side; a browser-supplied price is never trusted.
            const amountCents = amountCentsForDays(durationDays)

            const orderResult = await query(
                `INSERT INTO banner_payment_orders (user_id, duration_days, amount_cents, currency, status)
                 VALUES ($1, $2, $3, $4, 'pending')
                 RETURNING id`,
                [req.user.id, durationDays, amountCents, ORDER_CURRENCY]
            )
            const orderId = orderResult.rows[0].id

            const session = await stripe.checkout.sessions.create({
                mode: "payment",
                amount: amountCents,
                currency: ORDER_CURRENCY,
                metadata: {
                    banner_order_id: String(orderId),
                    source: FLOW_SOURCE
                },
                success_url: frontendUrl("/banner-pay-test?status=success"),
                cancel_url: frontendUrl("/banner-pay-test?status=cancelled")
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
                amount_cents: amountCents
            })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

function createWebhookRouter(deps = {}) {
    const {
        stripe = getStripe(),
        webhookSecret = process.env.STRIPE_WEBHOOK_SECRET || null,
        query = (text, params) => pool.query(text, params)
    } = deps

    const router = Router()

    router.post("/", async (req, res) => {
        try {
            if (!stripe || !webhookSecret) {
                return res.status(503).json({ success: false, message: "webhooks not configured" })
            }

            const signature = req.headers["stripe-signature"]

            let event
            try {
                event = stripe.webhooks.constructEvent(req.rawBody || req.body, signature, webhookSecret)
            }
            catch {
                return res.status(400).json({ success: false, message: "invalid signature" })
            }

            if (event.type !== "checkout.session.completed") {
                return res.json({ success: true, received: event.type })
            }

            const session = event.data.object

            if (session.metadata?.source !== FLOW_SOURCE) {
                return res.status(400).json({ success: false, message: "unrecognized event" })
            }

            const orderId = cleanOrderId(session.metadata?.banner_order_id)
            if (!orderId) {
                return res.status(400).json({ success: false, message: "invalid order id" })
            }

            if (session.payment_status !== "paid") {
                return res.status(400).json({ success: false, message: "session not paid" })
            }

            const sessionAmountCents = Number(session.amount_total)
            const sessionCurrency = String(session.currency || ORDER_CURRENCY).toLowerCase()

            if (!Number.isInteger(sessionAmountCents) || sessionAmountCents <= 0) {
                return res.status(400).json({ success: false, message: "invalid amount" })
            }

            const result = await query(
                `UPDATE banner_payment_orders
                 SET status = 'paid',
                     paid_at = COALESCE(paid_at, NOW()),
                     stripe_event_id = $1,
                     stripe_checkout_session_id = COALESCE(NULLIF(stripe_checkout_session_id, ''), $2),
                     updated_at = NOW()
                 WHERE id = $3
                   AND status = 'pending'
                   AND amount_cents = $4
                   AND currency = $5
                 RETURNING id, status`,
                [event.id, session.id, orderId, sessionAmountCents, sessionCurrency]
            )

            if (result.rowCount > 0) {
                return res.json({ success: true, order_id: orderId, status: "paid" })
            }

            // No pending order matched. Determine whether this is a duplicate,
            // a mismatch, or an unknown order.
            const check = await query(
                `SELECT status, amount_cents, currency FROM banner_payment_orders WHERE id = $1`,
                [orderId]
            )

            if (check.rows.length === 0) {
                return res.status(400).json({ success: false, message: "order not found" })
            }

            const row = check.rows[0]

            if (row.status === "paid") {
                // Duplicate delivery: already paid exactly once. Always 200.
                return res.json({ success: true, duplicate: true })
            }

            if (Number(row.amount_cents) !== sessionAmountCents || String(row.currency).toLowerCase() !== sessionCurrency) {
                return res.status(400).json({ success: false, message: "amount or currency mismatch" })
            }

            return res.status(400).json({ success: false, message: "order cannot be paid" })
        }
        catch {
            res.status(500).json({ success: false, message: "webhook processing failed" })
        }
    })

    return router
}

export { createCheckoutSessionRouter, createWebhookRouter }
