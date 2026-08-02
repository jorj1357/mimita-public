// 08 02 2026, 16 00
/* purpose
* Automated tests for the forgot-password reset flow router.
* Covers request (code issuance, no account enumeration) and reset
* (valid/invalid code, password policy, mismatched passwords).
* Uses an in-memory fake pool; no real database required.
* DOES NOT contact the database or email servers.
* DOES NOT test signed-in password changes.
*/

import test, { beforeEach } from "node:test"
import assert from "node:assert/strict"
import express from "express"
import request from "supertest"

import { createForgotPasswordRouter } from "./forgot-password.js"
import { hashToken } from "./authCore.js"
import { clearRateLimitStores } from "./rateLimit.js"

const SESSION_SECRET = "test-secret"

function makePool() {
    const users = new Map()
    const codes = new Map()
    const sessions = new Map()
    let nextUserId = 1
    let nextCodeId = 1
    let nextSessionId = 1

    function findUser(params) {
        for (const user of users.values()) {
            if (
                user.deleted_at == null &&
                (user.username_key === params[0] || user.email === params[1])
            ) {
                return user
            }
        }
        return null
    }

    const query = async (text, params = []) => {
        const q = text.replace(/\s+/g, " ").trim()

        if (q.includes("SELECT id, email, email_notifications_enabled")) {
            const user = findUser(params)
            return { rows: user ? [{ id: user.id, email: user.email, email_notifications_enabled: user.email_notifications_enabled }] : [], rowCount: user ? 1 : 0 }
        }
        if (q.includes("SELECT id, email")) {
            const user = findUser(params)
            return { rows: user ? [{ id: user.id, email: user.email }] : [], rowCount: user ? 1 : 0 }
        }
        if (q.includes("UPDATE password_reset_codes") && q.includes("WHERE user_id")) {
            for (const code of codes.values()) {
                if (code.user_id === params[0] && code.used_at == null) {
                    code.used_at = new Date()
                }
            }
            return { rows: [], rowCount: 0 }
        }
        if (q.includes("INSERT INTO password_reset_codes")) {
            const id = nextCodeId++
            codes.set(id, {
                id,
                user_id: params[0],
                code_hash: params[1],
                created_at: new Date(),
                expires_at: new Date(Date.now() + 10 * 60 * 1000),
                used_at: null
            })
            return { rows: [{ id }], rowCount: 1 }
        }
        if (q.includes("FROM password_reset_codes") && q.includes("FOR UPDATE")) {
            const [userId, codeHash] = params
            const now = Date.now()
            const matches = []
            for (const code of codes.values()) {
                if (
                    code.user_id === userId &&
                    code.code_hash === codeHash &&
                    code.used_at == null &&
                    code.expires_at.getTime() > now
                ) {
                    matches.push(code)
                }
            }
            matches.sort((a, b) => b.created_at - a.created_at)
            return {
                rows: matches.length ? [{ id: matches[0].id }] : [],
                rowCount: matches.length ? 1 : 0
            }
        }
        if (q.includes("UPDATE users") && q.includes("password_hash")) {
            const user = users.get(params[1])
            if (user) user.password_hash = params[0]
            return { rows: [], rowCount: user ? 1 : 0 }
        }
        if (q.includes("UPDATE password_reset_codes") && q.includes("WHERE id")) {
            const code = codes.get(params[0])
            if (code) code.used_at = new Date()
            return { rows: [], rowCount: code ? 1 : 0 }
        }
        if (q.includes("UPDATE sessions SET revoked_at")) {
            for (const session of sessions.values()) {
                if (session.user_id === params[0] && session.revoked_at == null) {
                    session.revoked_at = new Date()
                }
            }
            return { rows: [], rowCount: 0 }
        }
        if (q === "BEGIN" || q === "COMMIT" || q === "ROLLBACK") {
            return { rows: [], rowCount: 0 }
        }
        throw new Error("unhandled query: " + text.slice(0, 120))
    }

    const pool = {
        query,
        async connect() {
            return { query, release() {} }
        }
    }

    return {
        pool,
        addUser({ username, email }) {
            const id = nextUserId++
            const user = {
                id,
                username,
                username_key: username.toLowerCase(),
                email,
                password_hash: "old-hash",
                deleted_at: null,
                email_notifications_enabled: true
            }
            users.set(id, user)
            return user
        },
        addSession(userId) {
            const id = nextSessionId++
            sessions.set(id, { id, user_id: userId, revoked_at: null })
            return { id, user_id: userId }
        },
        seedCode(userId, code) {
            const id = nextCodeId++
            codes.set(id, {
                id,
                user_id: userId,
                code_hash: hashToken(code, SESSION_SECRET),
                created_at: new Date(),
                expires_at: new Date(Date.now() + 10 * 60 * 1000),
                used_at: null
            })
            return id
        },
        seedExpiredCode(userId, code) {
            const id = nextCodeId++
            codes.set(id, {
                id,
                user_id: userId,
                code_hash: hashToken(code, SESSION_SECRET),
                created_at: new Date(Date.now() - 30 * 60 * 1000),
                expires_at: new Date(Date.now() - 10 * 60 * 1000),
                used_at: null
            })
            return id
        },
        usedCodes() {
            return [...codes.values()].filter((code) => code.used_at != null)
        },
        getUser(userId) {
            return users.get(userId)
        }
    }
}

function makeApp(state, deps = {}) {
    const emails = []
    const router = createForgotPasswordRouter({
        pool: state.pool,
        rateLimit: (req, res, next) => next(),
        sessionSecret: SESSION_SECRET,
        logAuth: () => {},
        sendResetCodeEmail: async (email, code) => {
            emails.push({ type: "reset", email, code })
        },
        sendPasswordChangedEmail: async (email) => {
            emails.push({ type: "changed", email })
        },
        ...deps
    })
    const app = express()
    app.use(express.json())
    app.use("/api/auth/forgot-password", router)
    app.use((err, req, res, next) => {
        void next
        res.status(500).json({ success: false, message: "server error" })
    })
    return { app, emails }
}

let state
let app
let emails

beforeEach(() => {
    clearRateLimitStores()
    state = makePool()
    const built = makeApp(state)
    app = built.app
    emails = built.emails
})

test("request requires an identifier", async () => {
    const res = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "" })
    assert.equal(res.status, 400)
    assert.equal(res.body.success, false)
})

test("request sends a code for a known username", async () => {
    state.addUser({ username: "alice", email: "alice@example.com" })
    const res = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "alice" })
    assert.equal(res.status, 200)
    assert.equal(res.body.success, true)
    assert.equal(emails.length, 1)
    assert.equal(emails[0].type, "reset")
    assert.equal(emails[0].email, "alice@example.com")
    assert.match(emails[0].code, /^\d{6}$/)
})

test("request sends a code for a known email", async () => {
    state.addUser({ username: "alice", email: "alice@example.com" })
    const res = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "ALICE@example.com" })
    assert.equal(res.status, 200)
    assert.equal(emails.length, 1)
})

test("request does not enumerate unknown identifiers", async () => {
    const res = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "ghost@example.com" })
    assert.equal(res.status, 200)
    assert.equal(res.body.success, true)
    assert.equal(res.body.message, "if an account exists, a reset code was sent")
    assert.equal(emails.length, 0)
})

test("request for unknown user sends no email but same generic response", async () => {
    const known = state.addUser({ username: "alice", email: "alice@example.com" })
    const knownRes = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "alice" })
    const unknownRes = await request(app)
        .post("/api/auth/forgot-password/request")
        .send({ identifier: "nobody" })
    assert.equal(knownRes.body.message, unknownRes.body.message)
    assert.equal(emails.length, 1)
    assert.equal(emails[0].email, known.email)
})

test("reset requires a 6-digit code", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "abc", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(res.status, 400)
})

test("reset enforces password policy", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "weak", confirmNewPassword: "weak" })
    assert.equal(res.status, 400)
    assert.equal(res.body.success, false)
})

test("reset requires matching passwords", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "Different!1" })
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "new passwords do not match")
})

test("reset rejects an invalid code", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "999999", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "invalid or expired code")
    assert.equal(state.getUser(user.id).password_hash, "old-hash")
})

test("reset rejects an expired code", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedExpiredCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(res.status, 400)
    assert.equal(res.body.message, "invalid or expired code")
})

test("reset updates password, consumes code, and revokes sessions", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.addSession(user.id)
    state.addSession(user.id)
    state.seedCode(user.id, "123456")

    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })

    assert.equal(res.status, 200)
    assert.equal(res.body.success, true)
    assert.equal(res.body.message, "password reset")
    assert.notEqual(state.getUser(user.id).password_hash, "old-hash")
    assert.equal(state.usedCodes().length, 1)
    assert.equal(emails.some((mail) => mail.type === "changed"), true)
})

test("reset works with email identifier", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const res = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice@example.com", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(res.status, 200)
    assert.notEqual(state.getUser(user.id).password_hash, "old-hash")
})

test("a used code cannot reset a second time", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")

    const first = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(first.status, 200)

    const second = await request(app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "AnotherPass!2", confirmNewPassword: "AnotherPass!2" })
    assert.equal(second.status, 400)
    assert.equal(second.body.message, "invalid or expired code")
})

test("reset returns 500 on database failure", async () => {
    const user = state.addUser({ username: "alice", email: "alice@example.com" })
    state.seedCode(user.id, "123456")
    const failing = makeApp({
        pool: {
            async connect() {
                throw new Error("db down")
            }
        }
    })
    const res = await request(failing.app)
        .post("/api/auth/forgot-password/reset")
        .send({ identifier: "alice", code: "123456", newPassword: "NewPassword!1", confirmNewPassword: "NewPassword!1" })
    assert.equal(res.status, 500)
})
