import assert from "node:assert/strict"
import test from "node:test"

import {
    createSixDigitCode,
    hashPassword,
    hashToken,
    normalizeEmail,
    normalizeUsername,
    validatePassword,
    validateUsername,
    verifyPassword
} from "./authCore.js"

test("username normalization trims and removes internal whitespace", () => {
    assert.equal(normalizeUsername("  Mi mita  User "), "MimitaUser")
    assert.equal(normalizeUsername(" a b "), "ab")
    assert.equal(validateUsername(" a b ").ok, false)
})

test("email normalization trims and lowercases", () => {
    assert.equal(
        normalizeEmail("  Person@Example.COM "),
        "person@example.com"
    )
})

test("password policy enforces length, uppercase, and symbol", () => {
    assert.equal(validatePassword("shortA!").ok, false)
    assert.equal(validatePassword("lowercase!").ok, false)
    assert.equal(validatePassword("Uppercase1").ok, false)
    assert.equal(validatePassword("ValidPass!").ok, true)
})

test("password hashing verifies correct passwords only", async () => {
    const hash = await hashPassword("ValidPass!")

    assert.notEqual(hash, "ValidPass!")
    assert.equal(await verifyPassword("ValidPass!", hash), true)
    assert.equal(await verifyPassword("WrongPass!", hash), false)
})

test("token hashes are secret-dependent and deterministic", () => {
    assert.equal(hashToken("token", "secret"), hashToken("token", "secret"))
    assert.notEqual(
        hashToken("token", "secret"),
        hashToken("token", "another-secret")
    )
})

test("password change codes always contain six digits", () => {
    for (let index = 0; index < 100; index += 1) {
        assert.match(createSixDigitCode(), /^\d{6}$/)
    }
})
