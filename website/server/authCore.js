import crypto from "crypto"
import { promisify } from "util"

const scryptAsync = promisify(crypto.scrypt)

const PASSWORD_HASH_PREFIX = "scrypt"
const PASSWORD_KEY_LENGTH = 64

export function normalizeUsername(username) {
    return String(username || "")
        .trim()
        .replace(/\s+/g, "")
}

export function normalizeEmail(email) {
    return String(email || "")
        .trim()
        .toLowerCase()
}

export function usernameKey(username) {
    return normalizeUsername(username).toLowerCase()
}

export function validateUsername(username) {
    const cleaned = normalizeUsername(username)

    if (cleaned.length < 3) {
        return {
            ok: false,
            message: "username must be at least 3 characters"
        }
    }

    return {
        ok: true,
        value: cleaned
    }
}

export function validatePassword(password) {
    const value = String(password || "")

    if (value.length < 8) {
        return {
            ok: false,
            message: "password must be at least 8 characters"
        }
    }

    if (!/[A-Z]/.test(value)) {
        return {
            ok: false,
            message: "password must include 1 uppercase letter"
        }
    }

    if (!/[^A-Za-z0-9]/.test(value)) {
        return {
            ok: false,
            message: "password must include 1 symbol"
        }
    }

    return {
        ok: true
    }
}

export function validateEmail(email) {
    const cleaned = normalizeEmail(email)

    if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(cleaned)) {
        return {
            ok: false,
            message: "valid email required"
        }
    }

    return {
        ok: true,
        value: cleaned
    }
}

export async function hashPassword(password) {
    const salt = crypto.randomBytes(16).toString("hex")
    const key = await scryptAsync(
        String(password),
        salt,
        PASSWORD_KEY_LENGTH
    )

    return [
        PASSWORD_HASH_PREFIX,
        salt,
        key.toString("hex")
    ].join(":")
}

export async function verifyPassword(password, storedHash) {
    const parts = String(storedHash || "").split(":")

    if (
        parts.length !== 3 ||
        parts[0] !== PASSWORD_HASH_PREFIX
    ) {
        return false
    }

    const [, salt, keyHex] = parts
    const expected = Buffer.from(keyHex, "hex")
    const actual = await scryptAsync(
        String(password),
        salt,
        expected.length
    )

    if (actual.length !== expected.length) {
        return false
    }

    return crypto.timingSafeEqual(actual, expected)
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
        return String(forwarded).split(",")[0].trim()
    }

    return req.ip || req.socket?.remoteAddress || "unknown"
}
