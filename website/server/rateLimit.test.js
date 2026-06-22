import assert from "node:assert/strict"
import test from "node:test"
import { createRateLimit, clearRateLimitStores } from "./rateLimit.js"

test("rate limiter allows requests under limit", () => {
    clearRateLimitStores()
    const limiter = createRateLimit({ windowMs: 1000, max: 5, name: "test1" })
    let calls = 0
    const mockNext = () => { calls++ }

    const mockReq = (ip) => ({
        ip,
        connection: { remoteAddress: ip }
    })
    const mockRes = {
        _headers: {},
        set(key, value) { this._headers[key] = value },
        status() { return this },
        json() {}
    }

    for (let i = 0; i < 5; i++) {
        limiter(mockReq("1.2.3.4"), mockRes, mockNext)
    }
    assert.equal(calls, 5)
})

test("rate limiter blocks over-limit requests", () => {
    clearRateLimitStores()
    const limiter = createRateLimit({ windowMs: 10000, max: 3, name: "test2" })
    const blocked = []
    const mockNext = () => {}

    const mockReq = (ip) => ({
        ip,
        connection: { remoteAddress: ip }
    })
    const mockRes = {
        _headers: {},
        _status: 0,
        _body: null,
        set(key, value) { this._headers[key] = value },
        status(code) { this._status = code; return this },
        json(body) { this._body = body; blocked.push({ status: this._status, body }) }
    }

    for (let i = 0; i < 5; i++) {
        limiter(mockReq("5.6.7.8"), mockRes, mockNext)
    }
    assert.equal(blocked.length, 2)
    assert.equal(blocked[0]._status || 429, 429)
    assert.ok(blocked[0].body?.message?.includes("too many requests"))
})

test("rate limiter resets after window", async () => {
    clearRateLimitStores()
    const limiter = createRateLimit({ windowMs: 100, max: 2, name: "test3" })
    let calls = 0
    const mockNext = () => { calls++ }
    const mockRes = {
        _headers: {},
        set(key, value) { this._headers[key] = value },
        status() { return this },
        json() {}
    }

    limiter({ ip: "9.9.9.9", connection: { remoteAddress: "9.9.9.9" } }, mockRes, mockNext)
    limiter({ ip: "9.9.9.9", connection: { remoteAddress: "9.9.9.9" } }, mockRes, mockNext)
    assert.equal(calls, 2)

    limiter({ ip: "9.9.9.9", connection: { remoteAddress: "9.9.9.9" } }, mockRes, mockNext)
    assert.equal(calls, 2)

    await new Promise(r => setTimeout(r, 150))

    limiter({ ip: "9.9.9.9", connection: { remoteAddress: "9.9.9.9" } }, mockRes, mockNext)
    assert.equal(calls, 3)
})

test("rate limiter tracks different IPs separately", () => {
    clearRateLimitStores()
    const limiter = createRateLimit({ windowMs: 10000, max: 2, name: "test4" })
    let callsA = 0
    let callsB = 0

    const resA = { set() { return this }, status() { return this }, json() {} }
    const resB = { set() { return this }, status() { return this }, json() {} }

    limiter({ ip: "10.0.0.1", connection: { remoteAddress: "10.0.0.1" } }, resA, () => callsA++)
    limiter({ ip: "10.0.0.2", connection: { remoteAddress: "10.0.0.2" } }, resB, () => callsB++)
    limiter({ ip: "10.0.0.1", connection: { remoteAddress: "10.0.0.1" } }, resA, () => callsA++)
    limiter({ ip: "10.0.0.2", connection: { remoteAddress: "10.0.0.2" } }, resB, () => callsB++)

    assert.equal(callsA, 2)
    assert.equal(callsB, 2)
})

test("rate limiter sets correct headers", () => {
    clearRateLimitStores()
    const limiter = createRateLimit({ windowMs: 5000, max: 10, name: "test5" })
    const headers = {}
    const mockNext = () => {}

    limiter(
        { ip: "1.1.1.1", connection: { remoteAddress: "1.1.1.1" } },
        { set(k, v) { headers[k] = v }, status() { return this }, json() {} },
        mockNext
    )
    assert.equal(headers["X-RateLimit-Limit"], "10")
    assert.equal(headers["X-RateLimit-Remaining"], "9")
    assert.ok(headers["X-RateLimit-Reset"])
})
