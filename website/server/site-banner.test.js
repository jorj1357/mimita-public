// 08 03 2026, 00 40
/* purpose
* Automated tests for the community banner system: content validation, the
* overwrite/queue rules, the public banner endpoint, free submission, reports,
* and admin management. Uses injected mocks (no database or Stripe).
* DOES NOT contact a real database or Stripe.
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import {
    createSiteBannerPublicRouter,
    createSiteBannerUserRouter,
    createSiteBannerAdminRouter
} from "./site-banner.js"
import { validateBannerContent, decidePlacement, placeBanner, advanceBannerQueue } from "./site-banner.js"
import { makeStore, makeQuery, makeClient } from "./banner-test-helpers.js"
import { clearRateLimitStores } from "./rateLimit.js"

function seedBanner(store, overrides = {}) {
    const id = store.nextBannerId++
    const userId = overrides.user_id ?? store.addUser("user" + id, overrides.email || "")
    const days = overrides.days ?? 1
    const banner = {
        id,
        user_id: userId,
        kind: overrides.kind || "free",
        days,
        message: overrides.message || "m",
        target_url: overrides.target_url || "",
        background_color: "#000000",
        text_color: "#ffffff",
        payment_order_id: overrides.payment_order_id || null,
        status: overrides.status || "active",
        starts_at: new Date(),
        expires_at: overrides.expires_at || new Date(Date.now() + days * 86400000),
        remaining_days: overrides.remaining_days ?? null,
        moderation_state: overrides.moderation_state || "ok",
        refund_status: overrides.refund_status || "",
        created_at: new Date(),
        updated_at: new Date()
    }
    store.banners.set(id, banner)
    return banner
}

function makeSiteApp({ store, currentUserRef, currentAdminRef }) {
    const deps = {
        query: makeQuery(store),
        getClient: () => makeClient(store),
        authenticateMw: (req, res, next) => {
            if (!currentUserRef.current) {
                return res.status(401).json({ success: false, message: "sign in required" })
            }
            req.user = currentUserRef.current
            next()
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
    app.use(express.json({ verify: (req, res, buf) => { req.rawBody = buf } }))
    app.use("/api/site", createSiteBannerPublicRouter(deps))
    app.use("/api/banner", createSiteBannerUserRouter(deps))
    app.use("/api/admin/banners", createSiteBannerAdminRouter(deps))
    app.use((err, req, res, next) => {
        void next
        res.status(500).json({ success: false, message: "server error" })
    })
    return app
}

let store
let currentUserRef
let currentAdminRef
let app

beforeEach(() => {
    clearRateLimitStores()
    store = makeStore()
    currentUserRef = { current: { id: 1, username: "alice" } }
    currentAdminRef = { current: { id: 99, username: "admin", role: "admin" } }
    app = makeSiteApp({ store, currentUserRef, currentAdminRef })
})

const VALID_CONTENT = { message: "hello", target_url: "https://example.com", background_color: "#000000", text_color: "#ffffff" }

// ── validation ─────────────────────────────────────────────

test("validateBannerContent rejects empty and overlong messages", () => {
    assert.equal(validateBannerContent({ message: "   " }).ok, false)
    assert.equal(validateBannerContent({ message: "" }).ok, false)
    assert.equal(validateBannerContent({ message: "x".repeat(281) }).ok, false)
    const ok = validateBannerContent({ message: "x".repeat(280), target_url: "", background_color: "#fff", text_color: "#000000" })
    assert.equal(ok.ok, true)
    assert.equal(ok.value.message, "x".repeat(280))
})

test("validateBannerContent rejects unsafe urls", () => {
    for (const bad of ["javascript:alert(1)", "file:///etc/passwd", "https://user:pass@example.com", "not a url", "ftp://x"]) {
        assert.equal(validateBannerContent({ ...VALID_CONTENT, target_url: bad }).ok, false, bad)
    }
    const ok = validateBannerContent({ ...VALID_CONTENT, target_url: "https://example.com/page" })
    assert.equal(ok.ok, true)
    assert.equal(ok.value.target_url, "https://example.com/page")
})

test("validateBannerContent rejects non-hex colors", () => {
    for (const bad of ["red", "rgb(0,0,0)", "url(javascript:1)", "#zzzzzz", "gradient"]) {
        assert.equal(validateBannerContent({ ...VALID_CONTENT, background_color: bad }).ok, false, bad)
    }
    assert.equal(validateBannerContent({ ...VALID_CONTENT, background_color: "#a1b2c3" }).ok, true)
})

// ── decidePlacement rules ──────────────────────────────────

test("decidePlacement covers overwrite/queue matrix", () => {
    assert.equal(decidePlacement(null, { kind: "free", days: 1 }), "activate")
    assert.equal(decidePlacement({ kind: "free", days: 1 }, { kind: "free", days: 1 }), "overwrite")
    assert.equal(decidePlacement({ kind: "free", days: 1 }, { kind: "paid", days: 3 }), "overwrite")
    assert.equal(decidePlacement({ kind: "paid", days: 3 }, { kind: "free", days: 1 }), "queue")
    assert.equal(decidePlacement({ kind: "paid", days: 3 }, { kind: "paid", days: 5 }), "overwrite")
    assert.equal(decidePlacement({ kind: "paid", days: 5 }, { kind: "paid", days: 3 }), "queue")
    assert.equal(decidePlacement({ kind: "paid", days: 5 }, { kind: "paid", days: 5 }), "queue")
    assert.equal(decidePlacement({ kind: "paid", days: 7 }, { kind: "paid", days: 7 }), "queue")
    assert.equal(decidePlacement({ kind: "free", days: 1 }, { kind: "admin", days: 1 }), "overwrite")
    assert.equal(decidePlacement({ kind: "paid", days: 7 }, { kind: "admin", days: 30 }), "overwrite")
    assert.equal(decidePlacement({ kind: "admin", days: 10 }, { kind: "free", days: 1 }), "queue")
    assert.equal(decidePlacement({ kind: "admin", days: 10 }, { kind: "paid", days: 3 }), "queue")
})

// ── public endpoint ────────────────────────────────────────

test("public endpoint returns the active banner", async () => {
    seedBanner(store, { kind: "paid", days: 3 })
    const res = await request(app).get("/api/site/banner")
    assert.equal(res.status, 200)
    assert.ok(res.body.banner)
    assert.equal(res.body.banner.message, "m")
    assert.ok(res.body.banner.expires_at)
    assert.ok(res.body.banner.username)
})

test("public endpoint returns null when nothing is active", async () => {
    const res = await request(app).get("/api/site/banner")
    assert.equal(res.status, 200)
    assert.equal(res.body.banner, null)
})

test("public endpoint excludes expired banners", async () => {
    seedBanner(store, { kind: "free", days: 1, expires_at: new Date(Date.now() - 1000) })
    const res = await request(app).get("/api/site/banner")
    assert.equal(res.body.banner, null)
    assert.equal(store.banners.get(1).status, "expired")
})

test("public endpoint excludes disabled banners", async () => {
    seedBanner(store, { kind: "free", days: 1, status: "disabled" })
    const res = await request(app).get("/api/site/banner")
    assert.equal(res.body.banner, null)
})

test("pricing endpoint reports server config", async () => {
    const res = await request(app).get("/api/site/banner/pricing")
    assert.equal(res.status, 200)
    assert.equal(res.body.pricing.price_per_day_usd, 1)
    assert.equal(res.body.pricing.paid_min_days, 2)
    assert.equal(res.body.pricing.paid_max_days, 7)
})

// ── free submission ────────────────────────────────────────

test("free submission requires sign-in", async () => {
    currentUserRef.current = null
    const res = await request(app).post("/api/banner/free").send(VALID_CONTENT)
    assert.equal(res.status, 401)
})

test("free submission enforces the per-user cooldown", async () => {
    store.cooldownActive = true
    const res = await request(app).post("/api/banner/free").send(VALID_CONTENT)
    assert.equal(res.status, 429)
})

test("free submission validates content", async () => {
    const bad = await request(app).post("/api/banner/free").send({ ...VALID_CONTENT, message: "  " })
    assert.equal(bad.status, 400)
    const badUrl = await request(app).post("/api/banner/free").send({ ...VALID_CONTENT, target_url: "javascript:x" })
    assert.equal(badUrl.status, 400)
})

test("free banner overwrites an active free banner", async () => {
    seedBanner(store, { kind: "free", days: 1 })
    currentUserRef.current = { id: 2, username: "bob" }
    const res = await request(app).post("/api/banner/free").send({ ...VALID_CONTENT, message: "mine now" })
    assert.equal(res.status, 201)
    assert.equal(store.banners.get(1).status, "replaced")
    assert.equal(store.banners.get(2).status, "active")
})

test("free banner queues behind an active paid banner", async () => {
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 10 })
    currentUserRef.current = { id: 2, username: "bob" }
    const res = await request(app).post("/api/banner/free").send(VALID_CONTENT)
    assert.equal(res.status, 201)
    assert.equal(store.banners.get(1).status, "active")
    assert.equal(store.banners.get(2).status, "queued")
})

test("queued free banner activates with full duration after active ends", async () => {
    seedBanner(store, { kind: "free", days: 1, expires_at: new Date(Date.now() - 1000) })
    seedBanner(store, { kind: "free", days: 1, status: "queued" })
    const res = await request(app).get("/api/site/banner")
    assert.equal(res.status, 200)
    assert.ok(res.body.banner)
    assert.equal(res.body.banner.message, "m")
    const promoted = store.banners.get(2)
    assert.equal(promoted.status, "active")
    const duration = (promoted.expires_at - promoted.starts_at) / 86400000
    assert.ok(Math.abs(duration - 1) < 0.01)
})

// ── placeBanner overwrite rules at the DB level ────────────

test("paid overwrites a shorter paid banner and preserves its remaining time", async () => {
    const client = makeClient(store)
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 1, status: "active" })
    seedBanner(store, { kind: "paid", days: 5, payment_order_id: 2, status: "pending_payment" })
    const outcome = await placeBanner(client, 2)
    assert.equal(outcome.action, "overwrite")
    assert.equal(store.banners.get(1).status, "queued")
    assert.ok(store.banners.get(1).remaining_days > 0)
    assert.equal(store.banners.get(1).starts_at, null)
    assert.equal(store.banners.get(1).expires_at, null)
    assert.equal(store.banners.get(2).status, "active")

    seedBanner(store, { kind: "paid", days: 2, payment_order_id: 3, status: "pending_payment" })
    const second = await placeBanner(client, 3)
    assert.equal(second.action, "queued")
    assert.equal(store.banners.get(3).status, "queued")
})

test("a displaced paid banner is re-activated with its remaining time", async () => {
    const client = makeClient(store)
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 1, status: "active", expires_at: new Date(Date.now() + 2.5 * 86400000) })
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 2, status: "pending_payment" })
    await placeBanner(client, 2)
    assert.equal(store.banners.get(1).status, "queued")
    assert.ok(Math.abs(store.banners.get(1).remaining_days - 2.5) < 0.01)

    // expire the active 7-day so the preserved banner is promoted with its remaining time
    store.banners.get(2).expires_at = new Date(Date.now() - 1000)
    await advanceBannerQueue(client)
    const reActivated = store.banners.get(1)
    assert.equal(reActivated.status, "active")
    const duration = (reActivated.expires_at - reActivated.starts_at) / 86400000
    assert.ok(Math.abs(duration - 2.5) < 0.01)
})

test("a 7-day paid banner is never overwritten", async () => {
    const client = makeClient(store)
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 1, status: "active" })
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 2, status: "pending_payment" })
    const outcome = await placeBanner(client, 2)
    assert.equal(outcome.action, "queued")
    assert.equal(store.banners.get(1).status, "active")
    assert.equal(store.banners.get(2).status, "queued")
})

test("admin banner overwrites any banner and preserves displaced paid time", async () => {
    const client = makeClient(store)
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 1, status: "active" })
    seedBanner(store, { kind: "admin", days: 30, status: "draft" })
    const outcome = await placeBanner(client, 2)
    assert.equal(outcome.action, "overwrite")
    assert.equal(store.banners.get(1).status, "queued")
    assert.ok(store.banners.get(1).remaining_days > 0)
    assert.equal(store.banners.get(2).status, "active")
})

test("queue promotes paid before free", async () => {
    const client = makeClient(store)
    seedBanner(store, { kind: "free", days: 1, status: "queued" })
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 1, status: "queued" })
    await advanceBannerQueue(client)
    assert.equal(store.banners.get(2).status, "active")
    assert.equal(store.banners.get(1).status, "queued")
})

// ── report ─────────────────────────────────────────────────

test("report requires sign-in", async () => {
    currentUserRef.current = null
    const res = await request(app).post("/api/banner/report").send({ banner_id: 1 })
    assert.equal(res.status, 401)
})

test("report stores a row for an existing banner", async () => {
    seedBanner(store, {})
    const res = await request(app).post("/api/banner/report").send({ banner_id: 1 })
    assert.equal(res.status, 201)
    assert.equal(store.reports.length, 1)
    assert.equal(store.reports[0].banner_id, 1)
    assert.equal(store.reports[0].reporter_user_id, 1)
})

test("report returns 404 for unknown banner", async () => {
    const res = await request(app).post("/api/banner/report").send({ banner_id: 999 })
    assert.equal(res.status, 404)
})

// ── admin ──────────────────────────────────────────────────

test("admin routes reject non-admins", async () => {
    currentAdminRef.current = null
    const list = await request(app).get("/api/admin/banners")
    assert.equal(list.status, 403)
    const disable = await request(app).patch("/api/admin/banners/1/disable").send({})
    assert.equal(disable.status, 403)
})

test("admin can list all banners with owner and order info", async () => {
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 10 })
    seedBanner(store, { kind: "free", days: 1, status: "queued" })
    const res = await request(app).get("/api/admin/banners")
    assert.equal(res.status, 200)
    assert.equal(res.body.banners.length, 2)
    const paid = res.body.banners.find(b => b.kind === "paid")
    assert.equal(paid.owner_username, store.users.get(paid.user_id).username)
})

test("admin disable advances the queue with full duration", async () => {
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 10 })   // active
    seedBanner(store, { kind: "paid", days: 4, payment_order_id: 11, status: "queued" })
    seedBanner(store, { kind: "free", days: 1, status: "queued" })

    const res = await request(app).patch("/api/admin/banners/1/disable").send({ reason: "spam" })
    assert.equal(res.status, 200)
    assert.equal(store.banners.get(1).status, "disabled")
    assert.equal(store.banners.get(1).disabled_reason, "spam")
    assert.equal(store.banners.get(2).status, "active")
    assert.equal(store.banners.get(3).status, "queued")

    const promoted = store.banners.get(2)
    const duration = (promoted.expires_at - promoted.starts_at) / 86400000
    assert.ok(Math.abs(duration - 4) < 0.01)
})

test("admin delete soft-deletes and keeps history", async () => {
    seedBanner(store, {})
    const res = await request(app).delete("/api/admin/banners/1")
    assert.equal(res.status, 200)
    assert.equal(store.banners.get(1).status, "deleted")
    assert.ok(store.banners.get(1))
})

test("admin can edit a banner", async () => {
    seedBanner(store, {})
    const res = await request(app).patch("/api/admin/banners/1").send({ message: "edited", target_url: "", background_color: "#111111", text_color: "#eeeeee" })
    assert.equal(res.status, 200)
    assert.equal(store.banners.get(1).message, "edited")
    assert.equal(store.banners.get(1).background_color, "#111111")
})

test("admin can create an admin banner that takes the slot", async () => {
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 10 })
    const res = await request(app).post("/api/admin/banners").send({ ...VALID_CONTENT, days: 30 })
    assert.equal(res.status, 201)
    assert.equal(store.banners.get(1).status, "queued")
    assert.ok(store.banners.get(1).remaining_days > 0)
    assert.equal(store.banners.get(2).status, "active")
    assert.equal(store.banners.get(2).kind, "admin")
})

test("admin can re-enable a disabled banner", async () => {
    seedBanner(store, { kind: "free", days: 1, status: "disabled" })
    const res = await request(app).patch("/api/admin/banners/1/re-enable").send({})
    assert.equal(res.status, 200)
    assert.equal(store.banners.get(1).status, "active")
})

test("admin actions are logged", async () => {
    seedBanner(store, { kind: "free", days: 1 })
    await request(app).patch("/api/admin/banners/1/disable").send({ reason: "spam" })
    assert.ok(store.adminActions.length >= 1)
    const action = store.adminActions[store.adminActions.length - 1]
    assert.equal(action.action, "disable")
    assert.equal(action.banner_id, 1)
    assert.equal(action.admin_user_id, 99)
})

test("public schedule endpoint returns active and ordered queue", async () => {
    seedBanner(store, { kind: "paid", days: 7, payment_order_id: 10 })
    seedBanner(store, { kind: "paid", days: 5, payment_order_id: 11, status: "queued" })
    seedBanner(store, { kind: "free", days: 1, status: "queued" })
    const res = await request(app).get("/api/site/banner/schedule")
    assert.equal(res.status, 200)
    assert.ok(res.body.schedule.active)
    assert.equal(res.body.schedule.active.id, 1)
    assert.equal(res.body.schedule.queue.length, 2)
    assert.equal(res.body.schedule.queue[0].banner.id, 2)
    assert.equal(res.body.schedule.queue[1].banner.id, 3)
    assert.equal(res.body.schedule.queue[0].position, 1)
    assert.equal(res.body.schedule.queue[1].position, 2)
    assert.ok(res.body.schedule.queue[1].estimated_start > res.body.schedule.queue[0].estimated_start)
})

test("my banners endpoint returns own banners and schedule", async () => {
    seedBanner(store, { kind: "free", days: 1, user_id: 1 })
    seedBanner(store, { kind: "paid", days: 5, payment_order_id: 10, user_id: 2, status: "queued" })
    const res = await request(app).get("/api/banner/mine")
    assert.equal(res.status, 200)
    assert.equal(res.body.banners.length, 1)
    assert.equal(res.body.banners[0].id, 1)
    assert.equal(res.body.schedule.queue.length, 1)
})

test("order status endpoint is owner-only", async () => {
    store.orders.set(1, { id: 1, user_id: 1, duration_days: 3, amount_cents: 300, currency: "usd", status: "paid", stripe_checkout_session_id: "", stripe_event_id: "", payment_intent_id: "", created_at: new Date(), paid_at: new Date() })
    seedBanner(store, { kind: "paid", days: 3, payment_order_id: 1 })
    const res = await request(app).get("/api/banner/orders/1")
    assert.equal(res.status, 200)
    assert.equal(res.body.order.id, 1)
    assert.equal(res.body.banner.id, 1)

    currentUserRef.current = { id: 7, username: "other" }
    const denied = await request(app).get("/api/banner/orders/1")
    assert.equal(denied.status, 403)
})

test("paid banner maximum is enforced server-side", async () => {
    const res = await request(app)
        .post("/api/admin/banners")
        .send({ ...VALID_CONTENT, days: 366 })
    assert.equal(res.status, 400)
})
