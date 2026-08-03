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
import { addUtcCalendarMonths, computeVipState } from "./vip-entitlements.js"
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
    assert.equal(state.display.name_color_override, "#191919")
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
