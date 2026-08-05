// 08 04 2026, 22 45
/* purpose
* Tests the safe post-sign-in redirect helpers used by the sign-in flow.
* Proves unsafe external redirect values are rejected and safe ones are preserved.
* DOES NOT test authentication, sessions, or the server.
* DOES NOT render React or start a browser.
*/

import test from "node:test"
import assert from "node:assert/strict"

import {
    safeReturnTo,
    buildSigninPath,
    readReturnTo,
    redirectTarget
} from "./returnTo.js"

test("safeReturnTo keeps same-origin relative paths", () => {
    assert.equal(safeReturnTo("/vip"), "/vip")
    assert.equal(safeReturnTo("/account"), "/account")
    assert.equal(safeReturnTo("/vip?tab=styles"), "/vip?tab=styles")
    assert.equal(safeReturnTo("/users/some%20name"), "/users/some name")
})

test("safeReturnTo rejects unsafe external destinations", () => {
    for (const bad of [
        "https://evil.com",
        "http://evil.com",
        "//evil.com",
        "/\\evil.com",
        "javascript:alert(1)",
        "/path:80",
        "/vip\nnext",
        "/vip\rnext",
        "not-a-path",
        "",
        null,
        undefined,
        "%2F%2Fevil.com",
        "https:%2F%2Fevil.com"
    ]) {
        assert.equal(safeReturnTo(bad), null, `expected ${JSON.stringify(bad)} to be rejected`)
    }
})

test("safeReturnTo rejects malformed percent escapes", () => {
    assert.equal(safeReturnTo("/vip%zz"), null)
    assert.equal(safeReturnTo("%"), null)
})

test("buildSigninPath appends encoded returnTo only when safe", () => {
    assert.equal(buildSigninPath("/vip"), "/signin?returnTo=%2Fvip")
    assert.equal(buildSigninPath("/account"), "/signin?returnTo=%2Faccount")
    assert.equal(buildSigninPath("https://evil.com"), "/signin")
    assert.equal(buildSigninPath(null), "/signin")
    assert.equal(buildSigninPath(""), "/signin")
})

test("readReturnTo reads and validates the query parameter", () => {
    assert.equal(readReturnTo("?returnTo=/vip"), "/vip")
    assert.equal(readReturnTo("?returnTo=%2Fvip"), "/vip")
    assert.equal(readReturnTo("?returnTo=https://evil.com"), null)
    assert.equal(readReturnTo(""), null)
    assert.equal(readReturnTo("?other=1"), null)
})

test("redirectTarget falls back when no safe returnTo is present", () => {
    assert.equal(redirectTarget("?returnTo=/vip", "/profile"), "/vip")
    assert.equal(redirectTarget("?returnTo=https://evil.com", "/profile"), "/profile")
    assert.equal(redirectTarget("", "/profile"), "/profile")
    assert.equal(redirectTarget("?returnTo=", "/profile"), "/profile")
})
