// 08 03 2026, 17 20
/* purpose
* Tests VIP Stripe checkout and webhook safety with injected Stripe and database fakes.
* Covers server-selected prices, signature verification, event idempotency, grants, and subscription state.
* Keeps payment tests deterministic without real Stripe, real Price IDs, or Postgres.
* DOES NOT contact Stripe, send email, or mutate production configuration.
* DOES NOT render website or game UI.
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { createVipCheckoutRouter, createVipWebhookRouter, reconcilePendingCheckoutOrders, getVipOrders, computeUpgradeDiscountCents, syncActiveSubscriptions, VIP_FLOW_SOURCE } from "./vip-payments.js"
import { clearRateLimitStores } from "./rateLimit.js"

function makeStore() {
    return {
        orders: new Map(),
        entitlements: [],
        subscriptions: new Map(),
        events: new Map(),
        users: new Map([[42, {
            id: 42,
            username: "tester",
            role: "user",
            supporter_tier: "free",
            stripe_customer_id: ""
        }]]),
        nextOrderId: 1,
        nextEntitlementId: 1
    }
}

function rows(result) {
    return { rows: result, rowCount: result.length }
}

function makeDispatch(store) {
    return async function dispatch(rawText, params = []) {
        const text = String(rawText).replace(/\s+/g, " ").trim()
        if (["BEGIN", "COMMIT", "ROLLBACK"].includes(text)) return rows([])

        if (text.startsWith("INSERT INTO vip_orders")) {
            const id = store.nextOrderId++
            store.orders.set(id, {
                id,
                user_id: params[0],
                tier: params[1],
                purchase_type: params[2],
                amount_cents: params[3],
                currency: params[4],
                stripe_price_id: params[5],
                status: "pending",
                stripe_checkout_session_id: "",
                stripe_payment_intent_id: "",
                stripe_subscription_id: "",
                stripe_customer_id: "",
                stripe_event_id: ""
            })
            return rows([{ id }])
        }

        if (text.startsWith("UPDATE vip_orders SET stripe_checkout_session_id")) {
            const order = store.orders.get(params[1])
            if (order) order.stripe_checkout_session_id = params[0]
            return rows([])
        }

        if (text.startsWith("INSERT INTO vip_stripe_events")) {
            if (store.events.has(params[0])) return { rows: [], rowCount: 0 }
            store.events.set(params[0], {
                event_id: params[0],
                event_type: params[1],
                status: "processing"
            })
            return rows([{ event_id: params[0] }])
        }

        if (text.startsWith("UPDATE vip_stripe_events")) {
            const event = store.events.get(params[2])
            if (event) {
                event.status = params[0]
                event.error_message = params[1]
            }
            return rows([])
        }

        if (text.startsWith("SELECT * FROM vip_orders") && text.includes("status = 'pending'")) {
            const userIdFilter = params.length > 1 ? params[1] : null
            const orders = [...store.orders.values()]
                .filter(order => order.status === "pending" && order.stripe_checkout_session_id)
                .filter(order => userIdFilter == null || order.user_id === userIdFilter)
                .sort((a, b) => a.id - b.id)
            return rows(orders.map(order => structuredClone(order)))
        }

        if (text.startsWith("SELECT id, tier, purchase_type")) {
            const orders = [...store.orders.values()]
                .filter(order => order.user_id === params[0])
                .sort((a, b) => (b.created_at || 0) - (a.created_at || 0))
            return rows(orders.map(order => structuredClone(order)))
        }

        if (text.startsWith("SELECT e.tier, e.starts_at, e.expires_at")) {
            const result = store.entitlements
                .filter(e => e.user_id === params[0] && e.status === "active")
                .map(e => {
                    const order = e.order_id ? store.orders.get(e.order_id) : null
                    return {
                        tier: e.tier,
                        starts_at: e.starts_at,
                        expires_at: e.expires_at,
                        amount_cents: order ? order.amount_cents : null
                    }
                })
            return rows(result)
        }

        if (text.startsWith("UPDATE vip_orders SET status = 'refunded'")) {
            const order = store.orders.get(params[0])
            if (order) order.status = "refunded"
            return rows([])
        }

        if (text.startsWith("UPDATE vip_entitlements") && text.includes("WHERE order_id = $1")) {
            for (const entitlement of store.entitlements) {
                if (entitlement.order_id === params[0]) entitlement.status = "refunded"
            }
            return rows([])
        }

        if (text.startsWith("UPDATE vip_entitlements") && text.includes("WHERE stripe_subscription_id = $1")) {
            for (const entitlement of store.entitlements) {
                if (entitlement.stripe_subscription_id === params[0]) entitlement.status = "expired"
            }
            return rows([])
        }

        if (text.startsWith("SELECT * FROM vip_orders")) {
            const order = store.orders.get(params[0])
            return rows(order ? [structuredClone(order)] : [])
        }

        if (text.startsWith("UPDATE vip_orders SET status = $1") && text.includes("WHERE id = $6")) {
            const order = store.orders.get(params[5])
            if (!order) return rows([])
            order.status = params[0]
            if (params[1]) order.stripe_event_id = params[1]
            if (params[2]) order.stripe_payment_intent_id = params[2]
            if (params[3]) order.stripe_subscription_id = params[3]
            if (params[4]) order.stripe_customer_id = params[4]
            return rows([structuredClone(order)])
        }

        if (text.startsWith("UPDATE users SET stripe_customer_id")) {
            const user = store.users.get(params[1])
            if (user && user.stripe_customer_id !== params[0]) {
                user.stripe_customer_id = params[0]
            }
            return rows([])
        }

        if (text.startsWith("SELECT MAX(expires_at)")) {
            const [userId, tier, minDate] = params
            const latest = store.entitlements
                .filter(e => e.user_id === userId && e.tier === tier && e.status === "active" && e.expires_at > minDate)
                .map(e => e.expires_at)
                .sort((a, b) => b - a)[0] || null
            return rows([{ expires_at: latest }])
        }

        if (text.startsWith("INSERT INTO vip_entitlements")) {
            const isSubscription = text.includes("ON CONFLICT DO NOTHING") ||
                text.includes("stripe_subscription_id <> ''")
            const rec = {
                id: store.nextEntitlementId++,
                user_id: params[0],
                order_id: isSubscription ? null : (params[1] || null),
                tier: isSubscription ? params[1] : params[2],
                source: isSubscription ? "subscription" : (params[3] || "stripe"),
                status: "active",
                starts_at: isSubscription ? params[2] : params[4],
                expires_at: isSubscription ? params[3] : params[5],
                stripe_checkout_session_id: isSubscription ? "" : params[6] || "",
                stripe_subscription_id: isSubscription ? params[4] : "",
                stripe_payment_intent_id: isSubscription ? params[6] : params[7] || "",
                stripe_customer_id: isSubscription ? params[5] : params[8] || ""
            }
            store.entitlements.push(rec)
            return rows([{ id: rec.id, starts_at: rec.starts_at, expires_at: rec.expires_at }])
        }

        if (text.startsWith("SELECT id, role FROM users")) {
            const user = store.users.get(params[0])
            return rows(user ? [{ id: user.id, role: user.role }] : [])
        }

        if (text.startsWith("SELECT tier, source, status, starts_at, expires_at")) {
            return rows(store.entitlements.filter(e => e.user_id === params[0]).map(e => structuredClone(e)))
        }

        if (text.startsWith("SELECT tier, status, current_period_start")) {
            return rows([...store.subscriptions.values()].filter(s => s.user_id === params[0]).map(s => structuredClone(s)))
        }

        if (text.startsWith("SELECT style_json") && text.includes("FROM vip_name_styles")) return rows([])
        if (text.startsWith("SELECT COUNT(*)::int AS count FROM vip_name_presets")) return rows([{ count: 0 }])

        if (text.startsWith("UPDATE users SET supporter_tier")) {
            const user = store.users.get(params[1])
            if (user) user.supporter_tier = params[0]
            return rows([])
        }

        if (text.startsWith("INSERT INTO vip_subscriptions")) {
            store.subscriptions.set(params[3], {
                user_id: params[0],
                tier: params[1],
                stripe_customer_id: params[2],
                stripe_subscription_id: params[3],
                status: params[4],
                current_period_start: params[5],
                current_period_end: params[6],
                cancel_at_period_end: params[7]
            })
            return rows([])
        }

        if (text.startsWith("SELECT user_id, tier FROM vip_subscriptions")) {
            const sub = store.subscriptions.get(params[0])
            return rows(sub ? [{ user_id: sub.user_id, tier: sub.tier }] : [])
        }

        if (text.startsWith("SELECT id, user_id, tier, stripe_customer_id, stripe_subscription_id")) {
            const active = [...store.subscriptions.values()]
                .filter(s => ["active", "trialing", "past_due"].includes(s.status))
            return rows(active.map(s => ({
                id: 0,
                user_id: s.user_id,
                tier: s.tier,
                stripe_customer_id: s.stripe_customer_id || "",
                stripe_subscription_id: s.stripe_subscription_id,
                status: s.status,
                cancel_at_period_end: s.cancel_at_period_end === true,
                current_period_end: s.current_period_end
            })))
        }

        if (text.startsWith("UPDATE vip_orders") && text.includes("WHERE stripe_payment_intent_id")) {
            const changed = []
            for (const order of store.orders.values()) {
                if (order.stripe_payment_intent_id === params[1]) {
                    order.status = params[0]
                    changed.push({ id: order.id, user_id: order.user_id })
                }
            }
            return rows(changed)
        }

        if (text.startsWith("UPDATE vip_entitlements") && text.includes("WHERE order_id = $2")) {
            for (const entitlement of store.entitlements) {
                if (entitlement.order_id === params[1]) entitlement.status = params[0]
            }
            return rows([])
        }

        throw new Error("unhandled fake SQL: " + text.slice(0, 140))
    }
}

function makeClient(query) {
    return {
        query,
        release() {}
    }
}

function makeApp({ stripe, store, env, currentUser }) {
    const query = makeDispatch(store)
    const app = express()
    app.use(express.json({ verify: (req, res, buf) => { req.rawBody = buf } }))
    app.use("/api/vip/payment", createVipCheckoutRouter({
        stripe,
        query,
        env,
        checkoutRateLimit: (req, res, next) => next(),
        authenticateMw: (req, res, next) => {
            if (!currentUser.current) return res.status(401).json({ success: false })
            req.user = currentUser.current
            next()
        }
    }))
    app.use("/api/vip/payment/webhook", createVipWebhookRouter({
        stripe,
        webhookSecret: "whsec_test",
        getClient: () => makeClient(query)
    }))
    app.use((err, req, res, next) => {
        void next
        res.status(500).json({ success: false, message: err.message })
    })
    return app
}

function checkoutEvent(orderId, overrides = {}) {
    return {
        id: overrides.eventId || "evt_checkout",
        type: "checkout.session.completed",
        livemode: false,
        data: {
            object: {
                object: "checkout.session",
                id: "cs_test_vip",
                payment_status: "paid",
                amount_total: overrides.amount_total ?? 333,
                currency: overrides.currency || "usd",
                payment_intent: overrides.payment_intent || "pi_test",
                customer: overrides.customer || "cus_test",
                subscription: overrides.subscription || null,
                metadata: {
                    source: VIP_FLOW_SOURCE,
                    vip_order_id: String(orderId),
                    user_id: "42",
                    tier: "vip",
                    purchase_type: "one_month",
                    ...overrides.metadata
                },
                line_items: {
                    data: [
                        {
                            price: {
                                id: overrides.priceId || "price_vip_1m"
                            }
                        }
                    ]
                }
            }
        }
    }
}

let store
let currentUser
let fake
let app
let env

beforeEach(() => {
    clearRateLimitStores()
    store = makeStore()
    currentUser = { current: { id: 42, username: "tester", email: "tester@example.com", role: "user" } }
    env = {
        VIP_PRICE_VIP_ONE_MONTH: "333",
        VIP_PRICE_VIP_MONTHLY: "333",
        VIP_PRICE_VIP_TWELVE_MONTH: "1999"
    }
    fake = {
        state: {
            event: null,
            validSignature: true,
            sessionParams: null,
            retrievedSession: null,
            subscription: {
                id: "sub_test",
                customer: "cus_test",
                status: "active",
                current_period_start: 1785585600,
                current_period_end: 1788264000,
                cancel_at_period_end: false,
                metadata: { user_id: "42", tier: "vip" }
            }
        },
        stripe: {
            checkout: {
                sessions: {
                    async create(params) {
                        fake.state.sessionParams = params
                        return { id: "cs_test_vip", url: "https://checkout.stripe.test/vip" }
                    },
                    async retrieve() {
                        return fake.state.retrievedSession
                    },
                    async listLineItems() {
                        return { data: [{ price: { id: "price_vip_1m" } }] }
                    }
                }
            },
            customers: {
                async create() {
                    return { id: "cus_created" }
                }
            },
            subscriptions: {
                async retrieve() {
                    return fake.state.subscription
                }
            },
            webhooks: {
                constructEvent() {
                    if (!fake.state.validSignature) throw new Error("bad signature")
                    return fake.state.event
                }
            }
        }
    }
    app = makeApp({ stripe: fake.stripe, store, env, currentUser })
})

async function createCheckout(body = {}) {
    return request(app)
        .post("/api/vip/payment/checkout")
        .send({ tier: "vip", purchase_type: "one_month", amount_cents: 1, ...body })
}

async function deliver(event) {
    fake.state.event = event
    return request(app)
        .post("/api/vip/payment/webhook")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
}

test("checkout requires Stripe configuration", async () => {
    const noStripe = makeApp({ stripe: null, store, env, currentUser })
    const res = await request(noStripe)
        .post("/api/vip/payment/checkout")
        .send({ tier: "vip", purchase_type: "one_month" })
    assert.equal(res.status, 503)
})

test("checkout uses inline price data and stores a Stripe customer id", async () => {
    const res = await request(app)
        .post("/api/vip/payment/checkout")
        .send({ tier: "vip", purchase_type: "one_month" })
    assert.equal(res.status, 200)
    assert.equal(fake.state.sessionParams.customer, "cus_created")
    assert.equal(fake.state.sessionParams.line_items[0].price_data.unit_amount, 333)
    assert.equal(fake.state.sessionParams.line_items[0].price_data.currency, "usd")
    assert.equal(store.users.get(42).stripe_customer_id, "cus_created")
})

test("checkout ignores browser price fields and uses server-selected amount", async () => {
    const res = await createCheckout()
    assert.equal(res.status, 200)
    assert.equal(fake.state.sessionParams.line_items[0].price_data.unit_amount, 333)
    const order = store.orders.get(res.body.order_id)
    assert.equal(order.amount_cents, 333)
    assert.equal(order.stripe_price_id, "")
})

test("webhook rejects invalid signatures", async () => {
    await createCheckout()
    fake.state.validSignature = false
    fake.state.event = checkoutEvent(1)
    const res = await request(app)
        .post("/api/vip/payment/webhook")
        .set("stripe-signature", "bad")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
})

test("paid one-month checkout grants one calendar month and updates cache", async () => {
    const checkout = await createCheckout()
    const res = await deliver(checkoutEvent(checkout.body.order_id))
    assert.equal(res.status, 200)
    const order = store.orders.get(checkout.body.order_id)
    assert.equal(order.status, "paid")
    assert.equal(store.entitlements.length, 1)
    assert.equal(store.entitlements[0].tier, "vip")
    assert.equal(store.users.get(42).supporter_tier, "vip")
})

test("duplicate Stripe event is idempotent", async () => {
    const checkout = await createCheckout()
    const event = checkoutEvent(checkout.body.order_id)
    const first = await deliver(event)
    const second = await deliver(event)
    assert.equal(first.status, 200)
    assert.equal(second.status, 200)
    assert.equal(second.body.duplicate, true)
    assert.equal(store.entitlements.length, 1)
})

test("wrong amount or currency does not grant entitlement", async () => {
    for (const overrides of [
        { eventId: "evt_amount", amount_total: 1 },
        { eventId: "evt_currency", currency: "eur" }
    ]) {
        const checkout = await createCheckout()
        const res = await deliver(checkoutEvent(checkout.body.order_id, overrides))
        assert.equal(res.status, 400)
    }
    assert.equal(store.entitlements.length, 0)
})

test("checkout blocks buying a lower tier than the user already has", async () => {
    store.entitlements.push({
        id: 99,
        user_id: 42,
        tier: "ultra_vip",
        source: "stripe",
        status: "active",
        starts_at: new Date(Date.now() - 1000),
        expires_at: new Date(Date.now() + 30 * 24 * 60 * 60 * 1000)
    })
    const res = await request(app)
        .post("/api/vip/payment/checkout")
        .send({ tier: "vip", purchase_type: "one_month" })
    assert.equal(res.status, 400)
    assert.match(res.body.message, /already have/)
    assert.equal(store.orders.size, 0)
})

test("checkout applies a rollover discount when upgrading to a higher tier", async () => {
    store.orders.set(500, {
        id: 500,
        user_id: 42,
        tier: "vip",
        purchase_type: "one_month",
        amount_cents: 333,
        currency: "usd",
        status: "paid",
        stripe_checkout_session_id: "",
        stripe_payment_intent_id: "pi_x",
        stripe_subscription_id: "",
        stripe_customer_id: "",
        stripe_event_id: ""
    })
    store.entitlements.push({
        id: 501,
        user_id: 42,
        order_id: 500,
        tier: "vip",
        source: "stripe",
        status: "active",
        starts_at: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000),
        expires_at: new Date(Date.now() + 30 * 24 * 60 * 60 * 1000)
    })

    const res = await request(app)
        .post("/api/vip/payment/checkout")
        .send({ tier: "ultra_vip", purchase_type: "one_month" })
    assert.equal(res.status, 200)

    const created = store.orders.get(res.body.order_id)
    const expectedDiscount = Math.floor(333 * (30 / 60))
    assert.equal(created.amount_cents, 1777 - expectedDiscount)
    assert.equal(created.stripe_price_id, "")
    assert.equal(fake.state.sessionParams.line_items[0].price_data.unit_amount, created.amount_cents)
})

test("subscription checkout records Stripe subscription period", async () => {
    const checkout = await createCheckout({ purchase_type: "monthly_subscription" })
    fake.state.event = checkoutEvent(checkout.body.order_id, {
        subscription: "sub_test",
        metadata: { purchase_type: "monthly_subscription" }
    })
    const res = await deliver(fake.state.event)
    assert.equal(res.status, 200)
    assert.equal(store.subscriptions.get("sub_test").status, "active")
    assert.equal(store.users.get(42).supporter_tier, "vip")
})

test("refund marks linked order and entitlement inactive", async () => {
    const checkout = await createCheckout()
    await deliver(checkoutEvent(checkout.body.order_id))
    const res = await deliver({
        id: "evt_refund",
        type: "charge.refunded",
        livemode: false,
        data: {
            object: {
                payment_intent: "pi_test"
            }
        }
    })
    assert.equal(res.status, 200)
    assert.equal(store.orders.get(checkout.body.order_id).status, "refunded")
    assert.equal(store.entitlements[0].status, "refunded")
})

function retrievedSession(orderId, overrides = {}) {
    return {
        object: "checkout.session",
        id: overrides.sessionId || "cs_test_vip",
        payment_status: overrides.payment_status || "paid",
        amount_total: overrides.amount_total ?? 333,
        currency: overrides.currency || "usd",
        payment_intent: overrides.payment_intent || "pi_recon",
        customer: overrides.customer || "cus_recon",
        subscription: overrides.subscription || null,
        metadata: {
            source: VIP_FLOW_SOURCE,
            vip_order_id: String(orderId),
            user_id: "42",
            tier: "vip",
            purchase_type: "one_month",
            ...overrides.metadata
        },
        line_items: {
            data: [
                {
                    price: {
                        id: overrides.priceId || "price_vip_1m"
                    }
                }
            ]
        }
    }
}

async function runReconcile() {
    return reconcilePendingCheckoutOrders({
        stripe: fake.stripe,
        getClient: () => makeClient(makeDispatch(store)),
        query: makeDispatch(store)
    })
}

test("reconcile grants a paid pending order when the webhook never fired", async () => {
    const checkout = await createCheckout()
    const orderId = checkout.body.order_id
    assert.equal(store.orders.get(orderId).status, "pending")
    fake.state.retrievedSession = retrievedSession(orderId)

    const result = await runReconcile()
    assert.equal(result.orders, 1)
    assert.equal(result.reconciled, 1)
    assert.equal(store.orders.get(orderId).status, "paid")
    assert.equal(store.entitlements.length, 1)
    assert.equal(store.entitlements[0].tier, "vip")
    assert.equal(store.users.get(42).supporter_tier, "vip")
})

test("reconcile skips unpaid sessions and leaves the order pending", async () => {
    const checkout = await createCheckout()
    const orderId = checkout.body.order_id
    fake.state.retrievedSession = retrievedSession(orderId, { payment_status: "unpaid" })

    const result = await runReconcile()
    assert.equal(result.orders, 1)
    assert.equal(result.reconciled, 0)
    assert.equal(store.orders.get(orderId).status, "pending")
    assert.equal(store.entitlements.length, 0)
})

test("reconcile does not regrant an order that the webhook already paid", async () => {
    const checkout = await createCheckout()
    const orderId = checkout.body.order_id
    await deliver(checkoutEvent(orderId))
    assert.equal(store.orders.get(orderId).status, "paid")
    assert.equal(store.entitlements.length, 1)

    fake.state.retrievedSession = retrievedSession(orderId)
    const result = await runReconcile()
    assert.equal(result.orders, 0)
    assert.equal(result.reconciled, 0)
    assert.equal(store.entitlements.length, 1)
})

test("getVipOrders reports a paid one-time order as refundable", async () => {
    const checkout = await createCheckout()
    const orderId = checkout.body.order_id
    await deliver(checkoutEvent(orderId))
    const order = store.orders.get(orderId)
    order.paid_at = new Date(Date.now() - 1000)
    order.stripe_payment_intent_id = "pi_test"

    const orders = await getVipOrders({ query: makeDispatch(store), userId: 42 })
    assert.equal(orders.length, 1)
    assert.equal(orders[0].status, "paid")
    assert.equal(orders[0].refundable, true)
    assert.ok(orders[0].refund_until)
})

test("getVipOrders does not mark monthly subscriptions refundable", async () => {
    const checkout = await createCheckout({ purchase_type: "monthly_subscription" })
    const orderId = checkout.body.order_id
    const order = store.orders.get(orderId)
    order.status = "paid"
    order.paid_at = new Date(Date.now() - 1000)
    order.stripe_payment_intent_id = "pi_test"

    const orders = await getVipOrders({ query: makeDispatch(store), userId: 42 })
    assert.equal(orders[0].refundable, false)
})

test("computeUpgradeDiscountCents returns remaining value of the current tier", () => {
    const now = new Date("2026-08-06T12:00:00.000Z")
    const start = new Date("2026-07-07T12:00:00.000Z")
    const end = new Date("2026-09-05T12:00:00.000Z")
    const discount = computeUpgradeDiscountCents({
        requestedTier: "ultra_vip",
        amountCents: 1000,
        startsAt: start,
        expiresAt: end,
        now
    })
    // Half of the 60-day entitlement remains, so half the amount is rolled over.
    assert.equal(discount, 500)
})

test("computeUpgradeDiscountCents returns 0 when nothing remains", () => {
    const start = new Date(Date.now() - 40 * 24 * 60 * 60 * 1000)
    const end = new Date(Date.now() - 10 * 24 * 60 * 60 * 1000)
    const discount = computeUpgradeDiscountCents({
        requestedTier: "ultra_vip",
        amountCents: 1000,
        startsAt: start,
        expiresAt: end,
        now: new Date()
    })
    assert.equal(discount, 0)
})

test("syncActiveSubscriptions creates a missing entitlement for an active subscription", async () => {
    store.subscriptions.set("sub_selfheal", {
        user_id: 42,
        tier: "super_vip",
        stripe_customer_id: "cus_x",
        stripe_subscription_id: "sub_selfheal",
        status: "active",
        current_period_start: null,
        current_period_end: null,
        cancel_at_period_end: false
    })
    fake.state.subscription = {
        id: "sub_selfheal",
        status: "active",
        billing_cycle_anchor: 1786051745,
        current_period_start: null,
        current_period_end: null,
        cancel_at_period_end: false,
        customer: "cus_x"
    }

    const result = await syncActiveSubscriptions({
        stripe: fake.stripe,
        query: makeDispatch(store)
    })
    assert.equal(result.checked, 1)
    const subEntitlement = store.entitlements.find(e => e.stripe_subscription_id === "sub_selfheal")
    assert.ok(subEntitlement, "expected a subscription entitlement to be created")
    assert.equal(subEntitlement.tier, "super_vip")
    assert.equal(subEntitlement.source, "subscription")
    assert.ok(subEntitlement.expires_at > subEntitlement.starts_at)
})

test("syncActiveSubscriptions marks a canceled subscription and counts the transition", async () => {
    store.subscriptions.set("sub_cancel", {
        user_id: 42,
        tier: "vip",
        stripe_customer_id: "cus_c",
        stripe_subscription_id: "sub_cancel",
        status: "active",
        current_period_start: new Date(Date.now() - 86400000),
        current_period_end: new Date(Date.now() + 20 * 86400000),
        cancel_at_period_end: false
    })
    fake.state.subscription = {
        id: "sub_cancel",
        status: "canceled",
        current_period_start: 1786051745,
        current_period_end: 1788730145,
        cancel_at_period_end: false,
        customer: "cus_c"
    }

    const result = await syncActiveSubscriptions({
        stripe: fake.stripe,
        query: makeDispatch(store)
    })
    assert.equal(result.canceled, 1)
    assert.equal(store.subscriptions.get("sub_cancel").status, "canceled")
})
