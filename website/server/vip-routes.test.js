// 08 03 2026, 18 40
/* purpose
* Tests VIP route-level ticket verification behavior with injected Express dependencies.
* Covers one-time-use join-ticket replay protection without real Postgres or Stripe.
* Keeps multiplayer VIP authority tests focused on website-issued opaque tickets.
* DOES NOT contact Stripe, send email, or start the production server.
* DOES NOT render React or game UI.
*/

import test from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"
import { hashToken } from "./authCore.js"
import { sessionSecret } from "./session.js"
import { createVipRouter } from "./vip-routes.js"

function rows(result) {
    return { rows: result, rowCount: result.length }
}

function makeApp(store) {
    const app = express()
    app.use(express.json())
    app.use("/api/vip", createVipRouter({
        authenticateMw: (req, res, next) => {
            req.user = store.user
            next()
        },
        stripeFactory: () => store.stripe || null,
        query: async (rawText, params = []) => {
            const text = String(rawText).replace(/\s+/g, " ").trim()

            if (text.startsWith("SELECT t.token_hash")) {
                const ticket = store.tickets.get(params[0])
                if (!ticket || ticket.used_at) return rows([])
                return rows([{
                    token_hash: params[0],
                    user_id: ticket.user_id,
                    room_code: ticket.room_code,
                    username: store.user.username,
                    display_name: store.user.display_name,
                    role: store.user.role
                }])
            }

            if (text.startsWith("UPDATE vip_join_tickets")) {
                const ticket = store.tickets.get(params[1])
                if (ticket) {
                    ticket.used_at = new Date("2026-08-03T12:01:00.000Z")
                    ticket.last_result = JSON.parse(params[0])
                }
                return rows([])
            }

            if (text.startsWith("SELECT tier, source, status")) {
                return rows(store.entitlements)
            }

            if (text.startsWith("SELECT tier, status, current_period_start")) {
                return rows([])
            }

            if (text.startsWith("SELECT style_json")) {
                return rows([])
            }

            if (text.startsWith("SELECT COUNT(*)::int")) {
                return rows([{ count: 0 }])
            }

            if (text.startsWith("UPDATE vip_orders SET refund_status = 'requested'")) {
                if (!store.refundOrder || store.refundOrder.id !== params[0] || store.refundOrder.user_id !== params[1]) return rows([])
                store.refundOrder.refund_status = "requested"
                return rows([store.refundOrder])
            }

            if (text.startsWith("UPDATE vip_orders SET stripe_refund_id")) {
                store.refundOrder.stripe_refund_id = params[0]
                return rows([])
            }

            throw new Error(`unexpected query: ${text}`)
        }
    }))
    return app
}

test("verified VIP join tickets are consumed and cannot be replayed", async () => {
    const token = "opaque-ticket"
    const tokenHash = hashToken(token, sessionSecret)
    const store = {
        user: {
            id: 42,
            username: "tester",
            display_name: "Tester",
            role: "user"
        },
        tickets: new Map([[tokenHash, {
            user_id: 42,
            room_code: "ROOM-1",
            used_at: null,
            last_result: null
        }]]),
        entitlements: [{
            tier: "super_vip",
            source: "stripe",
            status: "active",
            starts_at: "2026-08-01T00:00:00.000Z",
            expires_at: "2026-09-01T00:00:00.000Z",
            stripe_subscription_id: "",
            stripe_checkout_session_id: "cs_test"
        }]
    }

    const app = makeApp(store)
    const first = await request(app)
        .post("/api/vip/verify-join-ticket")
        .send({ join_ticket: token, room_code: "ROOM-1" })
        .expect(200)

    assert.equal(first.body.success, true)
    assert.equal(first.body.verified, true)
    assert.equal(first.body.vip.active_tier, "super_vip")
    assert.ok(store.tickets.get(tokenHash).used_at)

    const second = await request(app)
        .post("/api/vip/verify-join-ticket")
        .send({ join_ticket: token, room_code: "ROOM-1" })
        .expect(200)

    assert.equal(second.body.success, true)
    assert.equal(second.body.verified, false)
    assert.equal(second.body.reason, "invalid_or_expired")
})

test("owned refundable VIP order starts one-time Stripe refund", async () => {
    const store = {
        user: { id: 42, username: "tester", display_name: "Tester", role: "user" },
        refundOrder: {
            id: 7,
            user_id: 42,
            tier: "vip",
            purchase_type: "prepaid",
            amount_cents: 1998,
            currency: "usd",
            stripe_payment_intent_id: "pi_test",
            paid_at: new Date("2026-09-01T00:00:00.000Z"),
            refund_status: ""
        },
        stripe: {
            refunds: {
                create: async params => {
                    assert.deepEqual(params, {
                        payment_intent: "pi_test",
                        amount: 1998,
                        metadata: {
                            source: "mimita_vip",
                            vip_order_id: "7",
                            user_id: "42",
                            reason: "customer_requested_within_30_days"
                        }
                    })
                    return { id: "re_test", status: "pending" }
                }
            }
        }
    }
    const response = await request(makeApp(store))
        .post("/api/vip/orders/7/refund")
        .expect(200)

    assert.equal(response.body.status, "pending")
    assert.equal(store.refundOrder.refund_status, "requested")
    assert.equal(store.refundOrder.stripe_refund_id, "re_test")
})
