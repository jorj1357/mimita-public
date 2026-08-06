// 08 03 2026, 17 20
/* purpose
* Tests VIP tier calculation, calendar extension, style validation, and staff precedence.
* Covers pure entitlement behavior without requiring Stripe or Postgres.
* Protects the server-authoritative VIP rules shared by website and game APIs.
* DOES NOT contact Stripe, send email, or start the Express server.
* DOES NOT render React or game UI.
*/

import test from "node:test"
import assert from "node:assert/strict"
import { addUtcCalendarMonths, computeVipState, grantPrepaidEntitlement } from "./vip-entitlements.js"
import {
    defaultStyleForTier,
    staffStyleForRole,
    validateNameStyle,
    VIP_STYLE_LIMITS
} from "./vip-config.js"

const NOW = new Date("2026-08-03T12:00:00.000Z")

test("calendar month arithmetic clamps to valid UTC month days", () => {
    assert.equal(
        addUtcCalendarMonths(new Date("2026-01-31T10:15:00.000Z"), 1).toISOString(),
        "2026-02-28T10:15:00.000Z"
    )
    assert.equal(
        addUtcCalendarMonths(new Date("2024-02-29T00:00:00.000Z"), 12).toISOString(),
        "2025-02-28T00:00:00.000Z"
    )
})

test("highest active tier wins while lower entitlements remain ignored until later", () => {
    const state = computeVipState({
        user: { role: "user" },
        now: NOW,
        entitlements: [
            {
                tier: "vip",
                status: "active",
                starts_at: "2026-08-01T00:00:00.000Z",
                expires_at: "2026-12-01T00:00:00.000Z"
            },
            {
                tier: "ultra_vip",
                status: "active",
                starts_at: "2026-08-03T00:00:00.000Z",
                expires_at: "2026-09-03T00:00:00.000Z"
            }
        ]
    })

    assert.equal(state.active_tier, "ultra_vip")
    assert.equal(state.expires_at, "2026-09-03T00:00:00.000Z")
    assert.equal(state.badge_url, "/assets/images/mimita%20ultra%20vip.png")
})

test("same-tier overlapping rows expose the latest effective expiration", () => {
    const state = computeVipState({
        now: NOW,
        entitlements: [
            {
                tier: "super_vip",
                status: "active",
                starts_at: "2026-07-01T00:00:00.000Z",
                expires_at: "2026-08-15T00:00:00.000Z"
            },
            {
                tier: "super_vip",
                status: "active",
                starts_at: "2026-08-15T00:00:00.000Z",
                expires_at: "2026-09-15T00:00:00.000Z"
            }
        ]
    })

    assert.equal(state.active_tier, "super_vip")
    assert.equal(state.expires_at, "2026-09-15T00:00:00.000Z")
})

test("subscription period grants visible tier through current period end", () => {
    const state = computeVipState({
        now: NOW,
        subscriptions: [
            {
                tier: "vip",
                status: "past_due",
                current_period_start: "2026-08-01T00:00:00.000Z",
                current_period_end: "2026-09-01T00:00:00.000Z",
                cancel_at_period_end: false,
                stripe_subscription_id: "sub_1"
            }
        ]
    })

    assert.equal(state.active_tier, "vip")
    assert.equal(state.subscription.status, "past_due")
    assert.equal(state.expires_at, "2026-09-01T00:00:00.000Z")
})

test("expired entitlements fall back to free gray styling and lock controls", () => {
    const state = computeVipState({
        now: NOW,
        entitlements: [
            {
                tier: "ultra_vip",
                status: "active",
                starts_at: "2026-07-01T00:00:00.000Z",
                expires_at: "2026-08-01T00:00:00.000Z"
            }
        ],
        style: {
            kind: "per_letter",
            colors: ["#ff0001"]
        }
    })

    assert.equal(state.active_tier, "free")
    assert.equal(state.controls_unlocked, false)
    assert.deepEqual(state.name_style, defaultStyleForTier("free"))
})

test("staff role colors override VIP name color but preserve VIP badge state", () => {
    const state = computeVipState({
        user: { role: "admin" },
        now: NOW,
        entitlements: [
            {
                tier: "ultra_vip",
                status: "active",
                starts_at: "2026-08-01T00:00:00.000Z",
                expires_at: "2026-09-01T00:00:00.000Z"
            }
        ],
        style: {
            kind: "solid",
            solid_color: "#44aaff"
        }
    })

    assert.equal(state.active_tier, "ultra_vip")
    assert.equal(state.badge_url, "/assets/images/mimita%20ultra%20vip.png")
    assert.deepEqual(state.staff_style, staffStyleForRole("admin"))
    assert.equal(state.display.name_color_override, "#000000")
})

test("normal users cannot select exact reserved staff colors", () => {
    const result = validateNameStyle(
        { kind: "solid", solid_color: "#ff0000" },
        { activeTier: "super_vip", role: "user" }
    )
    assert.equal(result.ok, false)
    assert.match(result.message, /reserved/)
})

test("tier-locked and unsafe animated styles are rejected", () => {
    assert.equal(
        validateNameStyle({ kind: "per_letter", colors: ["#ffffff"] }, { activeTier: "super_vip" }).ok,
        false
    )
    assert.equal(
        validateNameStyle({ kind: "animated_rainbow", colors: ["#ffffff", "#000001"], rainbow_speed: 99 }, { activeTier: "ultra_vip" }).ok,
        false
    )
    assert.equal(
        validateNameStyle({
            kind: "per_letter",
            colors: Array(VIP_STYLE_LIMITS.maxPerLetterColors + 1).fill("#00ff00")
        }, { activeTier: "ultra_vip" }).ok,
        false
    )
})

test("pure black and pure red are reserved for staff", () => {
    for (const color of ["#000000", "#ff0000"]) {
        const result = validateNameStyle(
            { kind: "solid", solid_color: color },
            { activeTier: "ultra_vip", role: "user" }
        )
        assert.equal(result.ok, false, `expected ${color} to be rejected`)
        assert.match(result.message, /reserved/)
    }
})

test("staff roles can use their own reserved colors", () => {
    const result = validateNameStyle(
        { kind: "solid", solid_color: "#000000" },
        { activeTier: "super_vip", role: "admin" }
    )
    assert.equal(result.ok, true)
})

test("color_cycle is no longer a supported style kind", () => {
    const result = validateNameStyle(
        { kind: "color_cycle", colors: ["#ff0000", "#00ff00"] },
        { activeTier: "ultra_vip", role: "user" }
    )
    assert.equal(result.ok, false)
    assert.match(result.message, /unsupported/)
})

test("admin grant records the admin source on the entitlement row", async () => {
    const inserted = []
    const fakeQuery = async (rawText, params = []) => {
        const text = String(rawText).replace(/\s+/g, " ").trim()
        if (text.startsWith("SELECT MAX(expires_at)")) {
            return { rows: [{ expires_at: null }], rowCount: 1 }
        }
        if (text.startsWith("INSERT INTO vip_entitlements")) {
            inserted.push(params)
            return {
                rows: [{ id: 1, starts_at: params[4], expires_at: params[5] }],
                rowCount: 1
            }
        }
        if (text.startsWith("SELECT id, role FROM users")) {
            return { rows: [{ id: 42, role: "user" }], rowCount: 1 }
        }
        if (text.startsWith("SELECT tier, source, status")) {
            return { rows: [{ tier: "super_vip", source: "admin", status: "active", starts_at: "2026-08-03T12:00:00.000Z", expires_at: "2026-09-03T12:00:00.000Z", stripe_subscription_id: "", stripe_checkout_session_id: "" }], rowCount: 1 }
        }
        if (text.startsWith("SELECT tier, status, current_period_start")) {
            return { rows: [], rowCount: 0 }
        }
        if (text.startsWith("SELECT style_json")) {
            return { rows: [], rowCount: 0 }
        }
        if (text.startsWith("SELECT COUNT(*)::int")) {
            return { rows: [{ count: 0 }], rowCount: 1 }
        }
        if (text.startsWith("UPDATE users SET supporter_tier")) {
            return { rows: [], rowCount: 0 }
        }
        throw new Error(`unexpected query: ${text}`)
    }

    const result = await grantPrepaidEntitlement(fakeQuery, {
        userId: 42,
        tier: "super_vip",
        purchaseType: "admin_grant",
        months: 1,
        source: "admin",
        now: new Date("2026-08-03T12:00:00.000Z")
    })

    assert.equal(result.tier, "super_vip")
    assert.equal(inserted.length, 1)
    // params: [user_id, order_id(null), tier, source, starts_at, expires_at, ...]
    assert.equal(inserted[0][3], "admin")
    assert.equal(result.expires_at.toISOString(), "2026-09-03T12:00:00.000Z")
})
