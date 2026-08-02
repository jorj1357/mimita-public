// 08 02 2026, 15 40
/* purpose
* Automated tests for the banner payment Stripe sandbox pipeline.
* Covers checkout-session creation, signature verification, idempotent payment,
* and error handling using injected mocks (no real Stripe or database required).
* DOES NOT contact Stripe or the database.
* DOES NOT test the banner display system.
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { createCheckoutSessionRouter, createWebhookRouter } from "./banner-payments.js"
import { clearRateLimitStores } from "./rateLimit.js"

function makeStore() {
    const orders = new Map()
    let nextId = 1
    return {
        orders,
        next() { return nextId++ },
        get(id) { return orders.get(id) },
        set(id, order) { orders.set(id, order) }
    }
}

function makeQuery(store) {
    return async (text, params) => {
        if (text.includes("INSERT INTO banner_payment_orders")) {
            const id = store.next()
            store.set(id, {
                id,
                user_id: params[0],
                duration_days: params[1],
                amount_cents: params[2],
                currency: params[3],
                status: "pending",
                stripe_checkout_session_id: "",
                stripe_event_id: ""
            })
            return { rows: [{ id }], rowCount: 1 }
        }
        if (text.includes("stripe_event_id = $1")) {
            const [eventId, sessionId, orderId, amountCents, currency] = params
            const order = store.get(orderId)
            if (
                order &&
                order.status === "pending" &&
                order.amount_cents === amountCents &&
                order.currency.toLowerCase() === String(currency).toLowerCase()
            ) {
                order.status = "paid"
                order.paid_at = new Date()
                order.stripe_event_id = eventId
                if (!order.stripe_checkout_session_id) order.stripe_checkout_session_id = sessionId
                return { rows: [{ id: orderId, status: "paid" }], rowCount: 1 }
            }
            return { rows: [], rowCount: 0 }
        }
        if (text.includes("stripe_checkout_session_id = $1")) {
            const order = store.get(params[1])
            if (order) order.stripe_checkout_session_id = params[0]
            return { rows: [], rowCount: order ? 1 : 0 }
        }
        if (text.includes("SELECT status, amount_cents, currency")) {
            const order = store.get(params[0])
            return {
                rows: order
                    ? [{ status: order.status, amount_cents: order.amount_cents, currency: order.currency }]
                    : [],
                rowCount: order ? 1 : 0
            }
        }
        throw new Error("unhandled query: " + text.slice(0, 80))
    }
}

function makeFakeStripe() {
    const state = {
        sessionParams: null,
        event: null,
        validSignature: true,
        sessionsCreateError: null
    }
    const stripe = {
        checkout: {
            sessions: {
                async create(params) {
                    if (state.sessionsCreateError) throw state.sessionsCreateError
                    state.sessionParams = params
                    return { id: "cs_test_123", url: "https://checkout.stripe.com/pay/cs_test_123" }
                }
            }
        },
        webhooks: {
            constructEvent() {
                if (!state.validSignature) throw new Error("stripe signature verification failed")
                if (!state.event) throw new Error("no event configured")
                return state.event
            }
        }
    }
    return { stripe, state }
}

function makeApp({ stripe, store, query, currentUserRef }) {
    const deps = {
        stripe,
        query: query || makeQuery(store),
        checkoutRateLimit: (req, res, next) => next(),
        authenticateMw: (req, res, next) => {
            if (!currentUserRef.current) {
                return res.status(401).json({ success: false, message: "sign in required" })
            }
            req.user = currentUserRef.current
            next()
        }
    }
    const app = express()
    app.use(express.json({ verify: (req, res, buf) => { req.rawBody = buf } }))
    app.use("/api/banner/payment", createCheckoutSessionRouter(deps))
    app.use("/api/banner/payment/webhook", createWebhookRouter(deps))
    app.use((err, req, res, next) => {
        void next
        res.status(500).json({ success: false, message: "server error" })
    })
    return app
}

function paidEvent(orderId, overrides = {}) {
    return {
        type: "checkout.session.completed",
        id: overrides.eventId || "evt_1",
        data: {
            object: {
                id: "cs_test_123",
                payment_status: "paid",
                amount_total: 300,
                currency: "usd",
                metadata: { banner_order_id: String(orderId), source: "mimita_banner" },
                ...overrides.session
            }
        }
    }
}

let store
let fake
let currentUserRef
let app

beforeEach(() => {
    clearRateLimitStores()
    store = makeStore()
    fake = makeFakeStripe()
    currentUserRef = { current: { id: 42, username: "tester" } }
    app = makeApp({ stripe: fake.stripe, state: fake.state, store, currentUserRef })
})

test("checkout returns 503 when Stripe is not configured", async () => {
    const noStripeApp = makeApp({ stripe: null, state: fake.state, store, currentUserRef })
    const res = await request(noStripeApp)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 3 })
    assert.equal(res.status, 503)
    assert.equal(res.body.success, false)
})

test("webhook returns 503 when Stripe webhook secret is missing", async () => {
    const bareWebhookApp = express()
    bareWebhookApp.use(express.json({ verify: (req, res, buf) => { req.rawBody = buf } }))
    bareWebhookApp.use("/api/banner/payment/webhook", createWebhookRouter({ stripe: fake.stripe, webhookSecret: null, query: makeQuery(store) }))
    const res = await request(bareWebhookApp)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "any")
        .send(JSON.stringify({}))
    assert.equal(res.status, 503)
    assert.equal(res.body.success, false)
})

test("checkout requires a signed-in user", async () => {
    currentUserRef.current = null
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 3 })
    assert.equal(res.status, 401)
})

test("checkout rejects invalid duration values", async () => {
    for (const bad of [0, 8, -1, 1.5, "three", null, undefined, []]) {
        const body = {}
        if (bad !== undefined) body.duration_days = bad
        const res = await request(app)
            .post("/api/banner/payment/create-checkout-session")
            .send(body)
        assert.equal(res.status, 400, `duration_days=${JSON.stringify(bad)} should be rejected`)
    }
})

test("browser-supplied price is ignored; server derives amount from days", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 3, amount_cents: 1, price: 1 })
    assert.equal(res.status, 200)
    assert.equal(fake.state.sessionParams.amount, 300)
    assert.equal(fake.state.sessionParams.currency, "usd")
    assert.equal(store.get(1).amount_cents, 300)
    assert.equal(store.get(1).status, "pending")
})

test("checkout session is created and returns only session info", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 7 })
    assert.equal(res.status, 200)
    assert.equal(res.body.success, true)
    assert.equal(res.body.order_id, 1)
    assert.equal(res.body.session_id, "cs_test_123")
    assert.equal(res.body.amount_cents, 700)
    assert.ok(res.body.url.startsWith("https://"))
    assert.equal(fake.state.sessionParams.metadata.banner_order_id, "1")
    assert.equal(fake.state.sessionParams.metadata.source, "mimita_banner")
    assert.equal(fake.state.sessionParams.mode, "payment")
})

test("checkout returns 500 on database failure", async () => {
    const failingApp = makeApp({
        stripe: fake.stripe,
        state: fake.state,
        store,
        currentUserRef,
        query: async () => { throw new Error("db down") }
    })
    const res = await request(failingApp)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 3 })
    assert.equal(res.status, 500)
})

test("webhook rejects invalid signature with 400", async () => {
    fake.state.validSignature = false
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "tampered")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "invalid signature")
})

test("webhook marks a valid paid order as paid exactly once", async () => {
    await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ duration_days: 3 })
    fake.state.event = paidEvent(1)
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 200)
    assert.equal(store.get(1).status, "paid")
    assert.equal(store.get(1).stripe_event_id, "evt_1")
    assert.equal(store.get(1).stripe_checkout_session_id, "cs_test_123")
    assert.ok(store.get(1).paid_at)
})

test("duplicate webhook delivery returns 200 and does not apply payment twice", async () => {
    await request(app).post("/api/banner/payment/create-checkout-session").send({ duration_days: 3 })
    fake.state.event = paidEvent(1)
    await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    const paidAt = store.get(1).paid_at

    fake.state.event = paidEvent(1, { eventId: "evt_2" })
    const second = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(second.status, 200)
    assert.equal(second.body.duplicate, true)
    assert.equal(store.get(1).status, "paid")
    assert.equal(store.get(1).stripe_event_id, "evt_1")
    assert.equal(store.get(1).paid_at.getTime(), paidAt.getTime())
})

test("duplicate checkout session after paid order returns 200", async () => {
    await request(app).post("/api/banner/payment/create-checkout-session").send({ duration_days: 3 })
    fake.state.event = paidEvent(1)
    await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    fake.state.event = paidEvent(1, { eventId: "evt_3", session: { id: "cs_test_999" } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 200)
    assert.equal(res.body.duplicate, true)
})

test("webhook rejects an unpaid checkout session", async () => {
    fake.state.event = paidEvent(1, { session: { payment_status: "unpaid" } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "session not paid")
})

test("webhook rejects mismatched amount", async () => {
    await request(app).post("/api/banner/payment/create-checkout-session").send({ duration_days: 3 })
    fake.state.event = paidEvent(1, { session: { amount_total: 500 } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "amount or currency mismatch")
    assert.equal(store.get(1).status, "pending")
})

test("webhook rejects mismatched currency", async () => {
    await request(app).post("/api/banner/payment/create-checkout-session").send({ duration_days: 3 })
    fake.state.event = paidEvent(1, { session: { currency: "eur", amount_total: 300 } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "amount or currency mismatch")
    assert.equal(store.get(1).status, "pending")
})

test("webhook rejects an unknown order id", async () => {
    fake.state.event = paidEvent(9999)
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "order not found")
})

test("webhook rejects an event that is not from the banner flow", async () => {
    fake.state.event = {
        type: "checkout.session.completed",
        id: "evt_9",
        data: {
            object: {
                id: "cs_test_other",
                payment_status: "paid",
                amount_total: 100,
                currency: "usd",
                metadata: { source: "somewhere_else" }
            }
        }
    }
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "unrecognized event")
})

test("webhook ignores non-checkout events", async () => {
    fake.state.event = { type: "checkout.session.async_payment_failed", id: "evt_10", data: { object: {} } }
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 200)
    assert.equal(res.body.received, "checkout.session.async_payment_failed")
})

test("webhook returns 500 on database failure", async () => {
    const failingApp = makeApp({
        stripe: fake.stripe,
        state: fake.state,
        store,
        currentUserRef,
        query: async () => { throw new Error("db down") }
    })
    fake.state.event = paidEvent(1)
    const res = await request(failingApp)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 500)
    assert.equal(res.body.success, false)
})
