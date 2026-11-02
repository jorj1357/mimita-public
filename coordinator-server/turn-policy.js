const fs = require("fs")
const path = require("path")
const crypto = require("crypto")

const defaultPolicyPath = path.join(__dirname, "turn-policy.json")

function readTurnPolicy(policyPath = defaultPolicyPath) {
    try {
        const value = JSON.parse(fs.readFileSync(policyPath, "utf8"))
        if (!value || typeof value.enabled !== "boolean") {
            return { enabled: false, valid: false }
        }
        return { enabled: value.enabled, valid: true }
    } catch {
        return { enabled: false, valid: false }
    }
}

function createTurnCredentials({ enabled, secret, nowSeconds = Math.floor(Date.now() / 1000) }) {
    if (enabled !== true) return { ok: false, enabled: false }
    if (!secret) return { ok: false, enabled: true }

    const host = "107.191.48.226"
    const port = 3478
    const expiresAt = nowSeconds + 3600
    const username = `${expiresAt}:mimita`
    const credential = crypto.createHmac("sha1", secret).update(username).digest("base64")
    return { ok: true, enabled: true, host, port, username, credential, expires_at: expiresAt }
}

module.exports = { readTurnPolicy, createTurnCredentials }
