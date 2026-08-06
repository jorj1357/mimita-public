// 08 06 2026, 17 30
/* purpose
* Tests the admin VIP desync/override flag computation.
* Covers the "has an active subscription but is not getting VIP" detection used by the admin dashboard.
* DOES NOT contact Stripe, hit Postgres, or start the production server.
*/

import test from "node:test"
import assert from "node:assert/strict"
import { computeVipAdminFlags } from "./admin.js"

function activeSub(tier, status = "active", periodEndOffsetHours = 72) {
    return {
        tier,
        status,
        current_period_end: new Date(Date.now() + periodEndOffsetHours * 60 * 60 * 1000).toISOString()
    }
}

test("flags desync when a user pays for a tier higher than what is showing", () => {
    const flags = computeVipAdminFlags({
        subscriptions: [activeSub("ultra_vip")],
        activeTier: "free",
        now: new Date()
    })
    assert.equal(flags.has_active_subscription, true)
    assert.equal(flags.subscription_tier, "ultra_vip")
    assert.equal(flags.desync, true)
})

test("no desync when the displayed tier matches the subscription tier", () => {
    const flags = computeVipAdminFlags({
        subscriptions: [activeSub("vip")],
        activeTier: "vip",
        now: new Date()
    })
    assert.equal(flags.desync, false)
})

test("no desync when the displayed tier is above the subscription tier", () => {
    const flags = computeVipAdminFlags({
        subscriptions: [activeSub("vip")],
        activeTier: "ultra_vip",
        now: new Date()
    })
    assert.equal(flags.has_active_subscription, true)
    assert.equal(flags.desync, false)
})

test("expired or cancelled subscriptions are ignored", () => {
    const flags = computeVipAdminFlags({
        subscriptions: [
            { ...activeSub("vip"), status: "canceled", current_period_end: new Date(Date.now() - 60 * 60 * 1000).toISOString() },
            { ...activeSub("super_vip"), status: "incomplete", current_period_end: new Date(Date.now() + 60 * 60 * 1000).toISOString() }
        ],
        activeTier: "free",
        now: new Date()
    })
    assert.equal(flags.has_active_subscription, false)
    assert.equal(flags.desync, false)
})

test("no subscriptions means no flags", () => {
    const flags = computeVipAdminFlags({ subscriptions: [], activeTier: "free", now: new Date() })
    assert.equal(flags.has_active_subscription, false)
    assert.equal(flags.subscription_tier, "free")
    assert.equal(flags.desync, false)
})

test("highest active subscription tier wins", () => {
    const flags = computeVipAdminFlags({
        subscriptions: [activeSub("vip"), activeSub("ultra_vip")],
        activeTier: "super_vip",
        now: new Date()
    })
    assert.equal(flags.subscription_tier, "ultra_vip")
    assert.equal(flags.desync, true)
})
