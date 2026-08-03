// 08 03 2026, 05 30
/* purpose
* Tests for the server-owned banner scheduling engine.
* Verifies deterministic queue order, estimated timing, priority amounts,
* remaining duration, and overwrite eligibility.
* DOES NOT touch the database or Stripe.
*/

import test from "node:test"
import assert from "node:assert/strict"
import {
    computeSchedule,
    bannerRemainingDays,
    bannerPriorityAmount,
    compareQueueOrder,
    canOverwrite
} from "./banner-schedule.js"

const DAY = 86400000

function banner(overrides = {}) {
    const created = new Date(overrides.createdAt || Date.now())
    return {
        id: overrides.id || 1,
        kind: overrides.kind || "free",
        days: overrides.days ?? 1,
        remaining_days: overrides.remaining_days ?? null,
        status: overrides.status || "active",
        created_at: created,
        starts_at: overrides.starts_at || null,
        expires_at: overrides.expires_at || null,
        amount_cents: overrides.amount_cents ?? null
    }
}

test("priority amount uses order amount for paid and max for admin", () => {
    assert.equal(bannerPriorityAmount(banner({ kind: "paid", days: 7, amount_cents: 700 })), 700)
    assert.equal(bannerPriorityAmount(banner({ kind: "paid", days: 3, amount_cents: null })), 300)
    assert.equal(bannerPriorityAmount(banner({ kind: "free", days: 1 })), 0)
    assert.equal(bannerPriorityAmount(banner({ kind: "admin", days: 30 })), Number.MAX_SAFE_INTEGER)
})

test("remaining days comes from expires_at for active banners", () => {
    const now = Date.now()
    const b = banner({ kind: "paid", days: 7, status: "active", expires_at: new Date(now + 3.5 * DAY) })
    assert.ok(Math.abs(bannerRemainingDays(b, now) - 3.5) < 0.001)
})

test("remaining days falls back to preserved value or purchased days", () => {
    assert.equal(bannerRemainingDays(banner({ kind: "paid", days: 7, remaining_days: 2.5, status: "queued" })), 2.5)
    assert.equal(bannerRemainingDays(banner({ kind: "free", days: 1, status: "queued" })), 1)
})

test("queue order sorts by tier then paid amount then creation", () => {
    const paidLow = banner({ id: 1, kind: "paid", days: 3, amount_cents: 300, createdAt: Date.now() - 1000 })
    const paidHigh = banner({ id: 2, kind: "paid", days: 7, amount_cents: 700, createdAt: Date.now() - 500 })
    const free = banner({ id: 3, kind: "free", days: 1 })
    const sorted = [free, paidLow, paidHigh].sort(compareQueueOrder)
    assert.deepEqual(sorted.map(b => b.id), [2, 1, 3])
})

test("computeSchedule stacks estimated times after the active banner", () => {
    const now = Date.now()
    const active = banner({ id: 1, kind: "paid", days: 2, amount_cents: 200, status: "active", expires_at: new Date(now + 2 * DAY) })
    const q1 = banner({ id: 2, kind: "paid", days: 3, amount_cents: 300, status: "queued" })
    const q2 = banner({ id: 3, kind: "free", days: 1, status: "queued" })
    const schedule = computeSchedule([q2, q1, active], now)
    assert.equal(schedule.active.banner.id, 1)
    assert.equal(schedule.queue.length, 2)
    assert.equal(schedule.queue[0].banner.id, 2)
    assert.equal(schedule.queue[1].banner.id, 3)
    assert.equal(schedule.queue[0].estimated_start.getTime(), now + 2 * DAY)
    assert.equal(schedule.queue[0].estimated_end.getTime(), now + 5 * DAY)
    assert.equal(schedule.queue[1].estimated_start.getTime(), now + 5 * DAY)
    assert.equal(schedule.queue[1].estimated_end.getTime(), now + 6 * DAY)
})

test("computeSchedule treats an expired active as no active", () => {
    const now = Date.now()
    const expired = banner({ id: 1, kind: "free", days: 1, status: "active", expires_at: new Date(now - 1000) })
    const q = banner({ id: 2, kind: "free", days: 1, status: "queued" })
    const schedule = computeSchedule([expired, q], now)
    assert.equal(schedule.active, null)
    assert.equal(schedule.queue[0].position, 1)
    assert.equal(schedule.queue[0].estimated_start.getTime(), now)
})

test("canOverwrite matrix", () => {
    const free = banner({ kind: "free", days: 1 })
    const paid3 = banner({ kind: "paid", days: 3, amount_cents: 300 })
    const paid7 = banner({ kind: "paid", days: 7, amount_cents: 700 })
    const admin = banner({ kind: "admin", days: 30 })

    assert.equal(canOverwrite(free, free), true)
    assert.equal(canOverwrite(free, paid3), false)
    assert.equal(canOverwrite(paid3, free), true)
    assert.equal(canOverwrite(paid3, paid7), false)
    assert.equal(canOverwrite(paid7, paid3), true)
    assert.equal(canOverwrite(paid7, paid7), false)
    assert.equal(canOverwrite(admin, paid7), true)
    assert.equal(canOverwrite(paid3, admin), false)
    assert.equal(canOverwrite(paid3, null), true)
    assert.equal(canOverwrite(free, null), true)
})
