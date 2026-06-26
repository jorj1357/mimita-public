import { hashToken, createSecretToken, createCsrfToken, getClientIp } from "./authCore.js"
import { pool } from "./db.js"

export const sessionCookieName = process.env.SESSION_COOKIE_NAME || "mimita_session"
export const sessionSecret = process.env.SESSION_SECRET || "development-only-change-me"
export const sessionDays = Number(process.env.SESSION_DAYS || 30)
export const production = process.env.NODE_ENV === "production"

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

export function setSessionCookie(res, token) {
    res.cookie(sessionCookieName, token, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        maxAge: sessionDays * 24 * 60 * 60 * 1000,
        path: "/"
    })
}

export function clearSessionCookie(res) {
    res.clearCookie(sessionCookieName, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        path: "/"
    })
}

export function setCsrfCookie(res) {
    const csrfToken = createCsrfToken()
    res.cookie("csrf_token", csrfToken, {
        httpOnly: false,
        secure: production,
        sameSite: "strict",
        maxAge: sessionDays * 24 * 60 * 60 * 1000,
        path: "/"
    })
}

export function csrfProtection(req, res, next) {
    if (["GET", "HEAD", "OPTIONS"].includes(req.method)) return next()
    const cookies = parseCookies(req)
    const cookieToken = cookies["csrf_token"]
    const headerToken = req.headers["x-csrf-token"]
    if (!cookieToken || !headerToken || cookieToken !== headerToken) {
        return res.status(403).json({
            success: false,
            message: "invalid csrf token"
        })
    }
    next()
}

export async function createSession(userId, req, res) {
    const token = createSecretToken()
    const tokenHash = hashToken(token, sessionSecret)

    await pool.query(
        `
        INSERT INTO sessions (
            user_id,
            token_hash,
            user_agent,
            ip_address,
            expires_at
        )
        VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))
        `,
        [
            userId,
            tokenHash,
            req.get("user-agent") || "unknown",
            getClientIp(req),
            sessionDays
        ]
    )

    setSessionCookie(res, token)
    setCsrfCookie(res)
}
