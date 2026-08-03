// 08 02 2026, 15 40
/* purpose
* Automated tests for the banner payment Stripe sandbox pipeline.
* Covers checkout-session creation (with banner content), signature verification,
* idempotent payment + banner activation in one transaction, and error handling.
* Uses injected mocks (no real Stripe or database required).
* DOES NOT contact Stripe or the database.
* DOES NOT test the banner display system.
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { createCheckoutSessionRouter, createWebhookRouter } from "./banner-payments.js"
import { clearRateLimitStores } from "./rateLimit.js"
import { makeStore, makeQuery, makeClient } from "./banner-test-helpers.js"

const VALID_BODY = {
    duration_days: 3,
    message: "check out my game",
    target_url: "https://example.com",
    background_color: "#000000",
    text_color: "#ffffff"
}

function makeApp({ stripe, store, query, currentUserRef, getClient }) {
    const deps = {
        stripe,
        query: query || makeQuery(store),
        getClient: getClient || (() => makeClient(store)),
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
    fake = {
        state: { sessionParams: null, event: null, validSignature: true, sessionsCreateError: null },
        stripe: {
            checkout: {
                sessions: {
                    async create(params) {
                        if (fake.state.sessionsCreateError) throw fake.state.sessionsCreateError
                        fake.state.sessionParams = params
                        return { id: "cs_test_123", url: "https://checkout.stripe.com/pay/cs_test_123" }
                    }
                }
            },
            webhooks: {
                constructEvent() {
                    if (!fake.state.validSignature) throw new Error("stripe signature verification failed")
                    if (!fake.state.event) throw new Error("no event configured")
                    return fake.state.event
                }
            }
        }
    }
    currentUserRef = { current: { id: 42, username: "tester" } }
    app = makeApp({ stripe: fake.stripe, store, currentUserRef })
})

async function createPendingOrder(days = 3) {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ ...VALID_BODY, duration_days: days })
    assert.equal(res.status, 200)
    return res.body
}

async function deliverWebhook(body = VALID_BODY) {
    const co = await createPendingOrder(body.duration_days || 3)
    fake.state.event = paidEvent(co.order_id)
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    return { res, orderId: co.order_id, bannerId: co.banner_id }
}

test("checkout returns 503 when Stripe is not configured", async () => {
    const noStripeApp = makeApp({ stripe: null, store, currentUserRef })
    const res = await request(noStripeApp)
        .post("/api/banner/payment/create-checkout-session")
        .send(VALID_BODY)
    assert.equal(res.status, 503)
    assert.equal(res.body.success, false)
})

test("webhook returns 503 when Stripe webhook secret is missing", async () => {
    const bare = express()
    bare.use(express.json({ verify: (req, res, buf) => { req.rawBody = buf } }))
    bare.use("/api/banner/payment/webhook", createWebhookRouter({ stripe: fake.stripe, webhookSecret: null, query: makeQuery(store), getClient: () => makeClient(store) }))
    const res = await request(bare)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "any")
        .send(JSON.stringify({}))
    assert.equal(res.status, 503)
})

test("checkout requires a signed-in user", async () => {
    currentUserRef.current = null
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send(VALID_BODY)
    assert.equal(res.status, 401)
})

test("checkout rejects invalid paid durations", async () => {
    for (const bad of [0, 1, 8, -1, 1.5, "three", null, undefined, []]) {
        const body = { message: "x", target_url: "", background_color: "#000000", text_color: "#ffffff" }
        if (bad !== undefined) body.duration_days = bad
        const res = await request(app)
            .post("/api/banner/payment/create-checkout-session")
            .send(body)
        assert.equal(res.status, 400, `duration_days=${JSON.stringify(bad)} should be rejected`)
    }
})

test("checkout rejects an empty message", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ ...VALID_BODY, message: "   " })
    assert.equal(res.status, 400)
})

test("checkout rejects an overlong message", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ ...VALID_BODY, message: "x".repeat(281) })
    assert.equal(res.status, 400)
})

test("checkout rejects an unsafe url", async () => {
    for (const bad of ["javascript:alert(1)", "file:///etc/passwd", "https://user:pass@example.com"]) {
        const res = await request(app)
            .post("/api/banner/payment/create-checkout-session")
            .send({ ...VALID_BODY, target_url: bad })
        assert.equal(res.status, 400, `url ${bad} should be rejected`)
    }
})

test("checkout rejects an invalid color", async () => {
    for (const bad of ["red", "url(javascript:alert(1))", "nothex"]) {
        const res = await request(app)
            .post("/api/banner/payment/create-checkout-session")
            .send({ ...VALID_BODY, background_color: bad })
        assert.equal(res.status, 400, `color ${bad} should be rejected`)
    }
})

test("browser-supplied price is ignored; server derives amount from days", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ ...VALID_BODY, amount_cents: 1, price: 1 })
    assert.equal(res.status, 200)
    assert.equal(fake.state.sessionParams.line_items[0].price_data.unit_amount, 300)
    const order = [...store.orders.values()][0]
    assert.equal(order.amount_cents, 300)
    assert.equal(order.status, "pending")
})

test("draft banner is linked to the correct user and order", async () => {
    const res = await request(app)
        .post("/api/banner/payment/create-checkout-session")
        .send({ ...VALID_BODY, duration_days: 7 })
    assert.equal(res.status, 200)
    const banner = [...store.banners.values()][0]
    const order = [...store.orders.values()][0]
    assert.equal(banner.user_id, 42)
    assert.equal(banner.kind, "paid")
    assert.equal(banner.days, 7)
    assert.equal(banner.status, "pending_payment")
    assert.equal(banner.payment_order_id, order.id)
    assert.equal(order.amount_cents, 700)
})

test("checkout returns 500 on database failure", async () => {
    const failingApp = makeApp({
        stripe: fake.stripe,
        store,
        currentUserRef,
        query: async () => { throw new Error("db down") },
        getClient: () => makeClient(store)
    })
    const res = await request(failingApp)
        .post("/api/banner/payment/create-checkout-session")
        .send(VALID_BODY)
    assert.equal(res.status, 500)
})

test("webhook rejects invalid signature with 400", async () => {
    await createPendingOrder()
    fake.state.validSignature = false
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "tampered")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "invalid signature")
})

test("paid webhook marks order paid and activates exactly one banner", async () => {
    const { res, orderId, bannerId } = await deliverWebhook()
    assert.equal(res.status, 200)
    assert.equal(store.orders.get(orderId).status, "paid")
    assert.equal(store.orders.get(orderId).stripe_event_id, "evt_1")
    const banner = store.banners.get(bannerId)
    assert.equal(banner.status, "active")
    assert.ok(banner.starts_at)
    assert.ok(banner.expires_at)
    const actives = [...store.banners.values()].filter(b => b.status === "active")
    assert.equal(actives.length, 1)
})

test("duplicate webhook does not reactivate or extend the banner", async () => {
    const { res: first, orderId, bannerId } = await deliverWebhook()
    assert.equal(first.status, 200)
    const bannerAfterFirst = store.banners.get(bannerId)
    const startsAfterFirst = bannerAfterFirst.starts_at.getTime()
    const expiresAfterFirst = bannerAfterFirst.expires_at.getTime()

    fake.state.event = paidEvent(orderId, { eventId: "evt_2" })
    const second = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(second.status, 200)
    assert.equal(second.body.duplicate, true)

    const bannerAfterSecond = store.banners.get(bannerId)
    assert.equal(bannerAfterSecond.status, "active")
    assert.equal(bannerAfterSecond.starts_at.getTime(), startsAfterFirst)
    assert.equal(bannerAfterSecond.expires_at.getTime(), expiresAfterFirst)
    assert.equal(store.orders.get(orderId).stripe_event_id, "evt_1")
})

test("duplicate checkout session after paid order returns 200", async () => {
    const { res, orderId, bannerId } = await deliverWebhook()
    assert.equal(res.status, 200)
    const bannerBefore = store.banners.get(bannerId).expires_at.getTime()

    fake.state.event = paidEvent(orderId, { eventId: "evt_3", session: { id: "cs_test_999" } })
    const second = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(second.status, 200)
    assert.equal(second.body.duplicate, true)
    assert.equal(store.banners.get(bannerId).expires_at.getTime(), bannerBefore)
})

test("webhook rejects an unpaid checkout session", async () => {
    const co = await createPendingOrder()
    fake.state.event = paidEvent(co.order_id, { session: { payment_status: "unpaid" } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(store.orders.get(co.order_id).status, "pending")
    assert.notEqual(store.banners.get(co.banner_id).status, "active")
})

test("webhook rejects mismatched amount", async () => {
    const co = await createPendingOrder()
    fake.state.event = paidEvent(co.order_id, { session: { amount_total: 500 } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "amount or currency mismatch")
    assert.equal(store.orders.get(co.order_id).status, "pending")
})

test("webhook rejects mismatched currency", async () => {
    const co = await createPendingOrder()
    fake.state.event = paidEvent(co.order_id, { session: { currency: "eur", amount_total: 300 } })
    const res = await request(app)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "amount or currency mismatch")
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

test("webhook rolls back transaction on database failure", async () => {
    const failingStore = makeStore()
    const failingApp = makeApp({
        stripe: fake.stripe,
        store: failingStore,
        currentUserRef,
        getClient: () => {
            const client = makeClient(failingStore)
            const original = client.query.bind(client)
            client.query = async (text, params) => {
                if (text.includes("SET status = 'active'")) throw new Error("db exploded")
                return original(text, params)
            }
            return client
        }
    })
    const co = await request(failingApp)
        .post("/api/banner/payment/create-checkout-session")
        .send(VALID_BODY)
    assert.equal(co.status, 200)
    fake.state.event = paidEvent(co.body.order_id)
    const res = await request(failingApp)
        .post("/api/banner/payment/webhook")
        .set("Content-Type", "application/json")
        .set("stripe-signature", "valid")
        .send(JSON.stringify({}))
    assert.equal(res.status, 500)
    assert.equal(failingStore.orders.get(co.body.order_id).status, "pending")
    assert.notEqual(failingStore.banners.get(co.body.banner_id).status, "active")
})

test("paid banner queues when an active paid banner exists", async () => {
    const first = await deliverWebhook()
    assert.equal(first.res.status, 200)
    assert.equal(store.banners.get(first.bannerId).status, "active")

    const second = await deliverWebhook()
    assert.equal(second.res.status, 200)
    assert.equal(store.banners.get(second.bannerId).status, "queued")
    const actives = [...store.banners.values()].filter(b => b.status === "active")
    assert.equal(actives.length, 1)
})
