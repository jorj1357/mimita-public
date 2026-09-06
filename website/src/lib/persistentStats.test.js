// 09 06 2026, 14 43
/* purpose
* Verify exact formatting of persisted BIGINT strings and tick durations.
* Cover zero, missing data, invalid totals, and time unit boundaries.
* DOES NOT claim live database or browser acceptance.
*/
import test from "node:test"
import assert from "node:assert/strict"
import { formatPersistentStat } from "./persistentStats.js"

test("persistent totals preserve decimal strings beyond Number precision", () => {
    assert.equal(formatPersistentStat("9223372036854775807"), "9,223,372,036,854,775,807")
    assert.equal(formatPersistentStat("001250"), "1,250")
    assert.equal(formatPersistentStat(1250), "1,250")
    assert.equal(formatPersistentStat("0"), "0")
    assert.equal(formatPersistentStat(0), "0")
})

test("missing or invalid values are not presented as saved zero totals", () => {
    for (const value of [null, undefined, "", "abc", "-1", "1.5", true, NaN, Infinity, Number.MAX_SAFE_INTEGER + 1]) {
        assert.equal(formatPersistentStat(value), "—")
        assert.equal(formatPersistentStat(value, true), "—")
    }
})

test("playtime converts 60 ticks per second with integer precision", () => {
    assert.equal(formatPersistentStat("0", true), "0s")
    assert.equal(formatPersistentStat("59", true), "0s")
    assert.equal(formatPersistentStat("60", true), "1s")
    assert.equal(formatPersistentStat("3599", true), "59s")
    assert.equal(formatPersistentStat("3600", true), "1m")
    assert.equal(formatPersistentStat("216000", true), "1h 0m")
    assert.equal(formatPersistentStat("5184000", true), "1d 0h 0m")
    assert.equal(formatPersistentStat("109792800", true), "21d 4h 18m")
    assert.equal(formatPersistentStat("9223372036854775807", true), "1,779,199,852,788d 8h 15m")
})
