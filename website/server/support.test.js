// 08 03 2026, 05 10
/* purpose
* Tests for the support request system: validation, topics, saving, admin
* visibility, authorization, and email notification behavior.
* Uses injected mocks (no database, no real email).
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { createSupportRouter, createSupportAdminRouter, SUPPORT_TOPICS } from "./support.js"
import { makeStore, makeQuery } from "./banner-test-helpers.js"
import { clearRateLimitStores } from "./rateLimit.js"

const VALID = { email: "tester@example.com", topic: "game_issue", subject: "cant join a room", message: "it failed" }

function makeSupportApp({ store, currentAdminRef, mailCalls, mailError }) {
    const deps = {
        query: makeQuery(store),
        mailFn: async (payload) => {
            if (mailError) throw mailError
            mailCalls.push(payload)
        },
        requireAdminMw: (req, res, next) => {
            if (!currentAdminRef.current) {
                return res.status(403).json({ success: false, message: "admin access required" })
            }
            req.user = currentAdminRef.current
            next()
        }
    }
    const app = express()
    app.use(express.json())
    app.use("/api/support", createSupportRouter(deps))
    app.use("/api/admin/support", createSupportAdminRouter(deps))
    app.use((err, req, res, next) => {
        void next
        res.status(500).json({ success: false, message: "server error" })
    })
    return app
}

let store
let currentAdminRef
let mailCalls
let mailError
let app

beforeEach(() => {
    clearRateLimitStores()
    store = makeStore()
    currentAdminRef = { current: { id: 99, username: "admin", role: "admin" } }
    mailCalls = []
    mailError = null
    app = makeSupportApp({ store, currentAdminRef, mailCalls, mailError })
})

test("topic is required", async () => {
    const res = await request(app).post("/api/support").send({ ...VALID, topic: "" })
    assert.equal(res.status, 400)
})

test("all topics are accepted", async () => {
    for (const topic of SUPPORT_TOPICS.map(t => t.value)) {
        clearRateLimitStores()
        const freshApp = makeSupportApp({ store, currentAdminRef, mailCalls, mailError })
        const res = await request(freshApp).post("/api/support").send({ ...VALID, topic })
        assert.equal(res.status, 201, topic)
    }
})

test("request is saved and a notification email is attempted", async () => {
    const res = await request(app).post("/api/support").send(VALID)
    assert.equal(res.status, 201)
    assert.equal(store.support.length, 1)
    assert.equal(store.support[0].topic, "game_issue")
    assert.equal(store.support[0].message, "it failed")
    assert.equal(mailCalls.length, 1)
    assert.equal(mailCalls[0].requestId, store.support[0].id)
    assert.equal(mailCalls[0].topic, "game_issue")
})

test("email failure does not delete the request", async () => {
    mailError = new Error("smtp down")
    const res = await request(app).post("/api/support").send(VALID)
    assert.equal(res.status, 201)
    assert.equal(store.support.length, 1)
})

test("payment topic may link a banner order", async () => {
    store.orders.set(5, { id: 5, user_id: 1, duration_days: 3, amount_cents: 300, currency: "usd", status: "paid", stripe_checkout_session_id: "", stripe_event_id: "", payment_intent_id: "", created_at: new Date(), paid_at: new Date() })
    const res = await request(app).post("/api/support").send({ ...VALID, topic: "payment_finance", banner_order_id: 5 })
    assert.equal(res.status, 201)
    assert.equal(store.support[0].banner_order_id, 5)
    assert.equal(mailCalls[0].bannerOrderId, 5)
})

test("security topic is stored clearly", async () => {
    const res = await request(app).post("/api/support").send({ ...VALID, topic: "security" })
    assert.equal(res.status, 201)
    assert.equal(store.support[0].topic, "security")
})

test("invalid inputs are rejected", async () => {
    assert.equal((await request(app).post("/api/support").send({ ...VALID, email: "not-an-email" })).status, 400)
    assert.equal((await request(app).post("/api/support").send({ ...VALID, subject: "" })).status, 400)
    assert.equal((await request(app).post("/api/support").send({ ...VALID, message: "  " })).status, 400)
    assert.equal((await request(app).post("/api/support").send({ ...VALID, url: "javascript:x" })).status, 400)
    assert.equal((await request(app).post("/api/support").send({ ...VALID, banner_order_id: 999 })).status, 400)
})

test("admin support view lists requests", async () => {
    await request(app).post("/api/support").send(VALID)
    await request(app).post("/api/support").send({ ...VALID, topic: "security", message: "someone tried to hack" })
    const res = await request(app).get("/api/admin/support")
    assert.equal(res.status, 200)
    assert.equal(res.body.requests.length, 2)
})

test("non-admins cannot access admin support", async () => {
    currentAdminRef.current = null
    const res = await request(app).get("/api/admin/support")
    assert.equal(res.status, 403)
})

test("guests can submit without an account", async () => {
    const res = await request(app).post("/api/support").send(VALID)
    assert.equal(res.status, 201)
    assert.equal(store.support[0].user_id, null)
})
