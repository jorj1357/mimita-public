// 08 03 2026, 01 10
/* purpose
* Tests for the banner UI helpers: countdown formatting and local-only collapse.
* DOES NOT touch the server or the database.
*/

import test from "node:test"
import assert from "node:assert/strict"
import { formatCountdown, isBannerCollapsed, setBannerCollapsed } from "../src/lib/bannerUtils.js"

test("formatCountdown renders hh:mm:ss from expires_at", () => {
    const base = 1_000_000_000_000
    assert.equal(formatCountdown(base, base), "00:00:00")
    assert.equal(formatCountdown(base + 1_000, base), "00:00:01")
    assert.equal(formatCountdown(base + 61_000, base), "00:01:01")
    assert.equal(formatCountdown(base + 3_661_000, base), "01:01:01")
    assert.equal(formatCountdown(base + 86_400_000, base), "24:00:00")
    assert.equal(formatCountdown(base + 7 * 86_400_000, base), "168:00:00")
    assert.equal(formatCountdown(base - 5_000, base), "00:00:00")
})

test("formatCountdown is safe for invalid dates", () => {
    assert.equal(formatCountdown("not-a-date", Date.now()), "")
    assert.equal(formatCountdown(undefined, Date.now()), "")
})

test("collapse helpers are local-only and safe without a browser window", () => {
    // In Node there is no window.sessionStorage; helpers must not throw.
    assert.equal(isBannerCollapsed(1), false)
    setBannerCollapsed(1, true)
    setBannerCollapsed(null, true)
    assert.equal(isBannerCollapsed(null), false)
})
