import crypto from "crypto"
import { pool } from "./db.js"

const LOCKOUT_WINDOW_MINUTES = 15
const MAX_FAILED = 5

export function normalizeUsername(value) {
    return String(value || "").trim().replace(/\s+/g, " ")
}

export function normalizeEmail(value) {
    return String(value || "").trim().toLowerCase()
}

export function usernameKey(value) {
    return normalizeUsername(value).toLowerCase()
}

export function validateUsername(value) {
    const v = normalizeUsername(value)
    return v.length >= 3 && v.length <= 32
}

export function validatePassword(value) {
    const v = String(value || "")
    return v.length >= 8 && /[A-Z]/.test(v) && /[!@#$%^&*(),.?":{}|<>]/.test(v)
}

export function validateEmail(value) {
    return /^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(String(value || "").trim())
}

export async function hashPassword(password) {
    return new Promise((resolve, reject) => {
        const salt = crypto.randomBytes(16).toString("hex")
        crypto.scrypt(password, salt, 64, (err, key) => {
            if (err) reject(err)
            else resolve("scrypt:" + salt + ":" + key.toString("hex"))
        })
    })
}

export async function verifyPassword(password, hash) {
    if (!hash || !hash.startsWith("scrypt:")) return false
    const parts = hash.split(":")
    if (parts.length !== 3) return false
    const [, salt, key] = parts
    return new Promise((resolve) => {
        crypto.scrypt(password, salt, 64, (err, derived) => {
            if (err) return resolve(false)
            const derivedHex = derived.toString("hex")
            if (derivedHex.length !== key.length) return resolve(false)
            resolve(crypto.timingSafeEqual(Buffer.from(derivedHex), Buffer.from(key)))
        })
    })
}

export function createSecretToken() {
    return crypto.randomBytes(32).toString("base64url")
}

export function hashToken(token, secret) {
    return crypto
        .createHash("sha256")
        .update(String(secret || ""))
        .update(":")
        .update(String(token || ""))
        .digest("hex")
}

export function createSixDigitCode() {
    return String(crypto.randomInt(0, 1000000)).padStart(6, "0")
}

export function getClientIp(req) {
    const forwarded = req.headers["x-forwarded-for"]
    if (forwarded) {
        const ip = String(forwarded).split(",")[0].trim()
        if (ip) return ip
    }
    return req.ip || req.socket?.remoteAddress || "unknown"
}

export function createCsrfToken() {
    return crypto.randomBytes(32).toString("base64url")
}

export function parseCookies(req) {
    const result = {}
    for (const pair of String(req.headers.cookie || "").split(";")) {
        const separator = pair.indexOf("=")
        if (separator === -1) continue
        const key = pair.slice(0, separator).trim()
        const value = pair.slice(separator + 1).trim()
        result[key] = decodeURIComponent(value)
    }
    return result
}

// ── DB-backed brute force protection ──────────────────────────

export async function checkBruteForce(identifier) {
    const key = identifier.toLowerCase()
    try {
        const windowAgo = new Date(Date.now() - LOCKOUT_WINDOW_MINUTES * 60 * 1000).toISOString()
        const result = await pool.query(
            `SELECT COUNT(*)::int AS cnt FROM login_attempts
             WHERE identifier = $1 AND success = false AND attempted_at > $2`,
            [key, windowAgo]
        )
        const count = result.rows[0]?.cnt || 0
        if (count >= MAX_FAILED) {
            const oldest = await pool.query(
                `SELECT attempted_at FROM login_attempts
                 WHERE identifier = $1 AND success = false
                 ORDER BY attempted_at ASC LIMIT 1`,
                [key]
            )
            const lockedUntil = new Date(
                new Date(oldest.rows[0]?.attempted_at || Date.now()).getTime() +
                LOCKOUT_WINDOW_MINUTES * 60 * 1000
            )
            return { locked: true, remaining: Math.ceil((lockedUntil - new Date()) / 1000) }
        }
        return { locked: false }
    } catch {
        return { locked: false }
    }
}

export async function recordFailedAttempt(identifier, ip) {
    const key = identifier.toLowerCase()
    try {
        await pool.query(
            `INSERT INTO login_attempts (identifier, ip_address, success) VALUES ($1, $2, false)`,
            [key, ip || ""]
        )
        console.log(`[AUTH] brute_force record_failed identifier=${key}`)
    } catch (e) {
        console.log(`[AUTH] brute_force record_failed error=${e.message}`)
    }
}

export async function resetFailedAttempts(identifier) {
    const key = identifier.toLowerCase()
    try {
        await pool.query(
            `DELETE FROM login_attempts WHERE identifier = $1 AND success = false`,
            [key]
        )
    } catch (e) {
        console.log(`[AUTH] brute_force reset error=${e.message}`)
    }
}

export async function getFlaggedAccounts() {
    try {
        const windowAgo = new Date(Date.now() - LOCKOUT_WINDOW_MINUTES * 60 * 1000).toISOString()
        const result = await pool.query(`
            SELECT la.identifier, COUNT(*)::int AS attempts,
                   MIN(la.attempted_at) AS first_attempt,
                   MAX(la.attempted_at) AS last_attempt
            FROM login_attempts la
            WHERE la.success = false AND la.attempted_at > $1
            GROUP BY la.identifier
            HAVING COUNT(*) >= $2
            ORDER BY MAX(la.attempted_at) DESC
        `, [windowAgo, MAX_FAILED])
        return result.rows
    } catch (e) {
        console.log(`[AUTH] getFlaggedAccounts error=${e.message}`)
        return []
    }
}

export { MAX_FAILED, LOCKOUT_WINDOW_MINUTES }
