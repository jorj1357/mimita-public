// 08 03 2026, 17 20
/* purpose
* Owns VIP Stripe checkout-session creation and verified webhook processing.
* Validates server-selected tier, amount, currency, Price ID, account, and idempotent event IDs.
* Grants or updates VIP entitlements only after trusted Stripe webhook events.
* DOES NOT render VIP pages or expose customization APIs.
* DOES NOT store Stripe secrets in logs or responses.
* DOES NOT use real or hard-coded production Price IDs.
*/

import { Router } from "express"
import Stripe from "stripe"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { createRateLimit } from "./rateLimit.js"
import {
    getPurchaseDefinition,
    getStripePriceId,
    normalizeTier
} from "./vip-config.js"
import {
    grantPrepaidEntitlement,
    markVipOrderStatus,
    recomputeAndStoreVipForUser,
    upsertSubscriptionState
} from "./vip-entitlements.js"

export const VIP_FLOW_SOURCE = "mimita_vip"

const checkoutRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "vip_checkout" })

export function getStripe() {
    const key = process.env.STRIPE_SECRET_KEY
    if (!key) return null
    return new Stripe(key)
}

function queryFrom(clientOrQuery = pool) {
    if (typeof clientOrQuery === "function") return clientOrQuery
    return clientOrQuery.query.bind(clientOrQuery)
}

function vipLog(event, fields = {}) {
    const safe = {
        event,
        ...fields
    }
    console.log(`[VIP] ${JSON.stringify(safe)}`)
}

function appOrigin(req) {
    return process.env.APP_ORIGIN || `${req.protocol}://${req.get("host")}`
}

function cleanId(value) {
    const text = String(value || "").trim()
    return /^[A-Za-z0-9_:-]{1,160}$/.test(text) ? text : ""
}

function stripeUnixDate(value) {
    const seconds = Number(value || 0)
    if (!Number.isFinite(seconds) || seconds <= 0) return null
    return new Date(seconds * 1000)
}

async function recordStripeEvent(client, event) {
    const result = await client.query(
        `INSERT INTO vip_stripe_events (
            event_id, event_type, livemode, status, source,
            checkout_session_id, subscription_id, payment_intent_id
         )
         VALUES ($1, $2, $3, 'processing', $4, $5, $6, $7)
         ON CONFLICT (event_id) DO NOTHING
         RETURNING event_id`,
        [
            event.id,
            event.type,
            event.livemode === true,
            VIP_FLOW_SOURCE,
            event.data?.object?.id && String(event.data.object.object || "") === "checkout.session"
                ? event.data.object.id
                : "",
            cleanId(event.data?.object?.subscription),
            cleanId(event.data?.object?.payment_intent)
        ]
    )
    return result.rowCount > 0
}

async function finishStripeEvent(client, eventId, status, errorMessage = "") {
    await client.query(
        `UPDATE vip_stripe_events
         SET status = $1,
             error_message = $2,
             processed_at = NOW()
         WHERE event_id = $3`,
        [status, String(errorMessage || "").slice(0, 500), eventId]
    )
}

async function getSessionPriceId(stripe, session) {
    const inlinePrice = session.line_items?.data?.[0]?.price?.id
    if (inlinePrice) return inlinePrice
    if (!stripe?.checkout?.sessions?.listLineItems) return ""
    const lineItems = await stripe.checkout.sessions.listLineItems(session.id, { limit: 1 })
    return lineItems?.data?.[0]?.price?.id || ""
}

async function getStripeSubscription(stripe, subscriptionId) {
    const id = cleanId(subscriptionId)
    if (!id || !stripe?.subscriptions?.retrieve) return null
    return stripe.subscriptions.retrieve(id)
}

function checkoutLineItem(def, priceId) {
    if (priceId) return { price: priceId, quantity: 1 }
    const priceData = {
        currency: def.currency,
        unit_amount: def.amount_cents,
        product_data: {
            name: `MiMITA ${def.label}`
        }
    }
    if (def.mode === "subscription") {
        priceData.recurring = {
            interval: "month",
            interval_count: 1
        }
    }
    return { price_data: priceData, quantity: 1 }
}

async function storeStripeCustomerId(query, userId, customerId) {
    const cleanCustomerId = cleanId(customerId)
    if (!cleanCustomerId) return ""
    await query(
        `UPDATE users
         SET stripe_customer_id = $1, updated_at = NOW()
         WHERE id = $2
           AND COALESCE(stripe_customer_id, '') <> $1`,
        [cleanCustomerId, userId]
    )
    return cleanCustomerId
}

async function ensureStripeCustomer(query, stripe, user) {
    const existing = cleanId(user?.stripe_customer_id)
    if (existing) return existing
    if (!stripe?.customers?.create) return ""

    const customer = await stripe.customers.create({
        email: user.email || undefined,
        metadata: { user_id: String(user.id) }
    })
    return storeStripeCustomerId(query, user.id, customer?.id)
}

function subscriptionUserId(subscription, fallback = "") {
    return Number(subscription?.metadata?.user_id || fallback || 0)
}

function subscriptionTier(subscription, fallback = "") {
    return normalizeTier(subscription?.metadata?.tier || fallback)
}

async function processCheckoutCompleted({ client, stripe, session, event }) {
    if (session.metadata?.source !== VIP_FLOW_SOURCE) {
        return "ignored_foreign_source"
    }

    const orderId = Number(session.metadata?.vip_order_id || 0)
    if (!Number.isInteger(orderId) || orderId <= 0) {
        throw new Error("invalid VIP order metadata")
    }

    const orderResult = await client.query(
        `SELECT * FROM vip_orders WHERE id = $1 FOR UPDATE`,
        [orderId]
    )
    if (!orderResult.rowCount) throw new Error("VIP order not found")

    const order = orderResult.rows[0]
    if (order.status === "paid") return "duplicate_paid_order"
    if (order.status !== "pending") throw new Error(`VIP order cannot be paid from ${order.status}`)

    const def = getPurchaseDefinition(order.tier, order.purchase_type)
    if (!def) throw new Error("VIP order has invalid tier or purchase type")

    const priceId = await getSessionPriceId(stripe, session)
    if (order.stripe_price_id && (!priceId || priceId !== order.stripe_price_id)) {
        throw new Error("Stripe Price ID mismatch")
    }
    if (Number(session.amount_total) !== Number(order.amount_cents)) {
        throw new Error("Stripe amount mismatch")
    }
    if (String(session.currency || "").toLowerCase() !== String(order.currency || "").toLowerCase()) {
        throw new Error("Stripe currency mismatch")
    }
    if (String(session.metadata?.user_id || "") !== String(order.user_id)) {
        throw new Error("Stripe user metadata mismatch")
    }
    if (normalizeTier(session.metadata?.tier) !== order.tier ||
        String(session.metadata?.purchase_type || "") !== order.purchase_type) {
        throw new Error("Stripe tier metadata mismatch")
    }
    if (session.payment_status && session.payment_status !== "paid" && session.payment_status !== "no_payment_required") {
        throw new Error("Stripe session is not paid")
    }

    await markVipOrderStatus(client, order.id, "paid", {
        stripeEventId: event.id,
        stripePaymentIntentId: cleanId(session.payment_intent),
        stripeSubscriptionId: cleanId(session.subscription),
        stripeCustomerId: cleanId(session.customer)
    })
    await storeStripeCustomerId(client.query.bind(client), order.user_id, session.customer)

    if (def.mode === "subscription") {
        const sub = await getStripeSubscription(stripe, session.subscription)
        const periodStart = stripeUnixDate(sub?.current_period_start) || stripeUnixDate(session.subscription_current_period_start)
        const periodEnd = stripeUnixDate(sub?.current_period_end) || stripeUnixDate(session.subscription_current_period_end)
        await upsertSubscriptionState(client, {
            userId: order.user_id,
            tier: order.tier,
            stripeCustomerId: cleanId(sub?.customer || session.customer),
            stripeSubscriptionId: cleanId(session.subscription),
            status: sub?.status || "active",
            currentPeriodStart: periodStart || new Date(),
            currentPeriodEnd: periodEnd,
            cancelAtPeriodEnd: sub?.cancel_at_period_end === true,
            canceledAt: stripeUnixDate(sub?.canceled_at),
            latestInvoiceId: cleanId(sub?.latest_invoice),
            latestPaymentIntentId: cleanId(session.payment_intent),
            now: new Date()
        })
    }
    else {
        await grantPrepaidEntitlement(client, {
            userId: order.user_id,
            orderId: order.id,
            tier: order.tier,
            purchaseType: order.purchase_type,
            months: def.calendar_months,
            stripeCheckoutSessionId: cleanId(session.id),
            stripePaymentIntentId: cleanId(session.payment_intent),
            stripeCustomerId: cleanId(session.customer),
            now: new Date()
        })
    }

    return "processed_checkout"
}

async function processSubscriptionLikeEvent({ client, stripe, object }) {
    const sub = object.object === "subscription"
        ? object
        : await getStripeSubscription(stripe, object.subscription)
    if (!sub) return "ignored_missing_subscription"

    const userId = subscriptionUserId(sub, object.metadata?.user_id)
    const tier = subscriptionTier(sub, object.metadata?.tier)
    if (!userId || tier === "free") {
        const lookup = await client.query(
            `SELECT user_id, tier FROM vip_subscriptions WHERE stripe_subscription_id = $1 LIMIT 1`,
            [cleanId(sub.id || object.subscription)]
        )
        if (!lookup.rowCount) return "ignored_unknown_subscription"
        return upsertSubscriptionState(client, {
            userId: lookup.rows[0].user_id,
            tier: lookup.rows[0].tier,
            stripeCustomerId: cleanId(sub.customer),
            stripeSubscriptionId: cleanId(sub.id || object.subscription),
            status: sub.status || object.status || "active",
            currentPeriodStart: stripeUnixDate(sub.current_period_start),
            currentPeriodEnd: stripeUnixDate(sub.current_period_end),
            cancelAtPeriodEnd: sub.cancel_at_period_end === true,
            canceledAt: stripeUnixDate(sub.canceled_at),
            latestInvoiceId: cleanId(object.id || sub.latest_invoice),
            latestPaymentIntentId: cleanId(object.payment_intent)
        }).then(() => "processed_subscription_lookup")
    }

    await upsertSubscriptionState(client, {
        userId,
        tier,
        stripeCustomerId: cleanId(sub.customer),
        stripeSubscriptionId: cleanId(sub.id || object.subscription),
        status: sub.status || object.status || "active",
        currentPeriodStart: stripeUnixDate(sub.current_period_start),
        currentPeriodEnd: stripeUnixDate(sub.current_period_end),
        cancelAtPeriodEnd: sub.cancel_at_period_end === true,
        canceledAt: stripeUnixDate(sub.canceled_at),
        latestInvoiceId: cleanId(object.id || sub.latest_invoice),
        latestPaymentIntentId: cleanId(object.payment_intent)
    })
    return "processed_subscription"
}

async function processRefundOrDispute({ client, object, status }) {
    const paymentIntent = cleanId(object.payment_intent)
    if (!paymentIntent) return "ignored_missing_payment_intent"

    const orderResult = await client.query(
        `UPDATE vip_orders
         SET status = $1, updated_at = NOW()
         WHERE stripe_payment_intent_id = $2
         RETURNING id, user_id`,
        [status, paymentIntent]
    )

    for (const order of orderResult.rows) {
        await client.query(
            `UPDATE vip_entitlements
             SET status = $1, updated_at = NOW()
             WHERE order_id = $2`,
            [status === "disputed" ? "disputed" : "refunded", order.id]
        )
        await recomputeAndStoreVipForUser(client, order.user_id)
    }

    return orderResult.rowCount ? `processed_${status}` : `ignored_unknown_${status}`
}

export function createVipCheckoutRouter(deps = {}) {
    const router = Router()
    const stripe = deps.stripe === undefined ? getStripe() : deps.stripe
    const query = queryFrom(deps.query || pool)
    const authenticateMw = deps.authenticateMw || authenticate
    const rateLimitMw = deps.checkoutRateLimit || checkoutRateLimit
    const env = deps.env || process.env

    router.post("/checkout", authenticateMw, rateLimitMw, async (req, res, next) => {
        try {
            if (!stripe) {
                return res.status(503).json({ success: false, message: "Stripe is not configured" })
            }

            const tier = normalizeTier(req.body.tier)
            const purchaseType = String(req.body.purchase_type || "").trim()
            const def = getPurchaseDefinition(tier, purchaseType, env)
            if (!def) {
                return res.status(400).json({ success: false, message: "invalid VIP purchase option" })
            }

            const priceId = getStripePriceId(tier, purchaseType, env)
            const customerId = await ensureStripeCustomer(query, stripe, req.user)

            const order = await query(
                `INSERT INTO vip_orders (
                    user_id, tier, purchase_type, amount_cents, currency, stripe_price_id
                 )
                 VALUES ($1, $2, $3, $4, $5, $6)
                 RETURNING id`,
                [req.user.id, tier, purchaseType, def.amount_cents, def.currency, priceId]
            )
            const orderId = order.rows[0].id
            const origin = appOrigin(req)
            const metadata = {
                source: VIP_FLOW_SOURCE,
                vip_order_id: String(orderId),
                user_id: String(req.user.id),
                tier,
                purchase_type: purchaseType
            }
            const params = {
                mode: def.mode,
                line_items: [checkoutLineItem(def, priceId)],
                metadata,
                client_reference_id: `vip_order:${orderId}`,
                success_url: `${origin}/vip/success?order_id=${orderId}`,
                cancel_url: `${origin}/vip?checkout=cancelled&order_id=${orderId}`
            }
            if (customerId) params.customer = customerId
            else if (req.user.email) params.customer_email = req.user.email
            if (def.mode === "subscription") params.subscription_data = { metadata }
            else params.payment_intent_data = { metadata }

            const session = await stripe.checkout.sessions.create(params)
            await query(
                `UPDATE vip_orders
                 SET stripe_checkout_session_id = $1,
                     updated_at = NOW()
                 WHERE id = $2`,
                [session.id || "", orderId]
            )

            vipLog("checkout_created", {
                user_id: req.user.id,
                order_id: orderId,
                tier,
                purchase_type: purchaseType,
                mode: def.mode
            })

            res.json({
                success: true,
                order_id: orderId,
                checkout_url: session.url,
                tier,
                purchase_type: purchaseType
            })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export function createVipWebhookRouter(deps = {}) {
    const router = Router()
    const stripe = deps.stripe === undefined ? getStripe() : deps.stripe
    const webhookSecret = deps.webhookSecret === undefined
        ? process.env.STRIPE_VIP_WEBHOOK_SECRET || process.env.STRIPE_WEBHOOK_SECRET
        : deps.webhookSecret
    const getClient = deps.getClient || (() => pool.connect())

    router.post("/", async (req, res) => {
        if (!stripe) {
            vipLog("webhook_config_error", { reason: "stripe_missing" })
            return res.status(503).json({ success: false, message: "Stripe is not configured" })
        }
        if (!webhookSecret) {
            vipLog("webhook_config_error", { reason: "secret_missing" })
            return res.status(503).json({ success: false, message: "Stripe webhook is not configured" })
        }

        let event
        try {
            event = stripe.webhooks.constructEvent(
                req.rawBody || req.body,
                req.headers["stripe-signature"],
                webhookSecret
            )
        }
        catch (error) {
            vipLog("webhook_invalid_signature", { reason: error.message })
            return res.status(400).json({ success: false, message: "invalid signature" })
        }

        const client = await getClient()
        try {
            await client.query("BEGIN")
            const firstDelivery = await recordStripeEvent(client, event)
            if (!firstDelivery) {
                await client.query("COMMIT")
                vipLog("webhook_duplicate", { event_id: event.id, type: event.type })
                return res.json({ success: true, duplicate: true })
            }

            let result = "ignored"
            const object = event.data?.object || {}
            if (event.type === "checkout.session.completed") {
                result = await processCheckoutCompleted({ client, stripe, session: object, event })
            }
            else if (event.type === "invoice.payment_succeeded" ||
                     event.type === "invoice.payment_failed" ||
                     event.type === "customer.subscription.created" ||
                     event.type === "customer.subscription.updated" ||
                     event.type === "customer.subscription.deleted") {
                result = await processSubscriptionLikeEvent({ client, stripe, object })
            }
            else if (event.type === "charge.refunded") {
                result = await processRefundOrDispute({ client, object, status: "refunded" })
            }
            else if (event.type === "charge.dispute.created") {
                result = await processRefundOrDispute({ client, object, status: "disputed" })
            }

            await finishStripeEvent(client, event.id, result.startsWith("ignored") ? "ignored" : "processed")
            await client.query("COMMIT")
            vipLog("webhook_processed", { event_id: event.id, type: event.type, result })
            res.json({ success: true, result })
        }
        catch (error) {
            try {
                await client.query("ROLLBACK")
                await finishStripeEvent(client, event.id, "failed", error.message)
            }
            catch {
                // best effort after rollback
            }
            vipLog("webhook_failed", { event_id: event.id, type: event.type, reason: error.message })
            res.status(400).json({ success: false, message: error.message })
        }
        finally {
            client.release()
        }
    })

    return router
}
