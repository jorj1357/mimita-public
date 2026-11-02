const test = require("node:test")
const assert = require("node:assert/strict")
const fs = require("node:fs")
const os = require("node:os")
const path = require("node:path")
const { readTurnPolicy, createTurnCredentials } = require("./turn-policy")

test("reads the global relay switch from JSON on every call", (t) => {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), "mimita-turn-policy-"))
    const policyPath = path.join(directory, "turn-policy.json")
    t.after(() => fs.rmSync(directory, { recursive: true, force: true }))

    fs.writeFileSync(policyPath, JSON.stringify({ enabled: false }))
    assert.deepEqual(readTurnPolicy(policyPath), { enabled: false, valid: true })
    fs.writeFileSync(policyPath, JSON.stringify({ enabled: true }))
    assert.deepEqual(readTurnPolicy(policyPath), { enabled: true, valid: true })
})

test("fails closed when the relay policy is missing, malformed, or incomplete", (t) => {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), "mimita-turn-policy-"))
    const policyPath = path.join(directory, "turn-policy.json")
    t.after(() => fs.rmSync(directory, { recursive: true, force: true }))

    assert.deepEqual(readTurnPolicy(policyPath), { enabled: false, valid: false })
    fs.writeFileSync(policyPath, "not-json")
    assert.deepEqual(readTurnPolicy(policyPath), { enabled: false, valid: false })
    fs.writeFileSync(policyPath, JSON.stringify({ enabled: "false" }))
    assert.deepEqual(readTurnPolicy(policyPath), { enabled: false, valid: false })
})

test("disabled policy returns no relay credential", () => {
    assert.deepEqual(createTurnCredentials({ enabled: false, secret: "secret" }), {
        ok: false,
        enabled: false
    })
})

test("enabled policy requires the server secret and issues expiring credentials", () => {
    assert.deepEqual(createTurnCredentials({ enabled: true, secret: "" }), {
        ok: false,
        enabled: true
    })
    const result = createTurnCredentials({ enabled: true, secret: "test-secret", nowSeconds: 1000 })
    assert.equal(result.ok, true)
    assert.equal(result.enabled, true)
    assert.equal(result.host, "107.191.48.226")
    assert.equal(result.port, 3478)
    assert.equal(result.username, "4600:mimita")
    assert.ok(result.credential)
    assert.equal(result.expires_at, 4600)
})
