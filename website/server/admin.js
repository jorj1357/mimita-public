import { Router } from "express"
import { createSecretToken, hashToken, getClientIp } from "./authCore.js"
import { pool } from "./db.js"
import { getMetrics, refreshMetrics } from "./analytics.js"
import { submitFeedback, getFeedback, FEEDBACK_PRESETS } from "./feedback.js"

const router = Router()

const ADMIN_COOKIE_NAME = "mimita_admin_session"
const ADMIN_SECRET = process.env.ADMIN_SECRET || "development-admin-secret-change-me"
const ADMIN_SESSION_DAYS = Number(process.env.ADMIN_SESSION_DAYS || 1)
const production = process.env.NODE_ENV === "production"

/*
  TODO: Replace hardcoded admin credentials with proper database-backed authentication.
  TODO: Implement password hashing.
  TODO: Implement session expiration.
  TODO: Implement role-based permissions.
*/
const HARDCODED_ADMIN_USERNAME = "admin"
const HARDCODED_ADMIN_PASSWORD = "admin"

function setAdminCookie(res, token) {
    res.cookie(ADMIN_COOKIE_NAME, token, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        maxAge: ADMIN_SESSION_DAYS * 24 * 60 * 60 * 1000,
        path: "/"
    })
}

function clearAdminCookie(res) {
    res.clearCookie(ADMIN_COOKIE_NAME, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        path: "/"
    })
}

function parseCookies(req) {
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

async function requireAdmin(req, res, next) {
    try {
        const token = parseCookies(req)[ADMIN_COOKIE_NAME]
        if (!token) {
            return res.status(401).json({ success: false, message: "admin sign in required" })
        }
        const tokenHash = hashToken(token, ADMIN_SECRET)
        const result = await pool.query(
            `
            SELECT id FROM admin_sessions
            WHERE token_hash = $1 AND expires_at > NOW()
            LIMIT 1
            `,
            [tokenHash]
        )
        if (!result.rowCount) {
            clearAdminCookie(res)
            return res.status(401).json({ success: false, message: "admin session expired" })
        }
        next()
    }
    catch (error) {
        next(error)
    }
}

router.post("/login", async (req, res, next) => {
    try {
        const username = String(req.body.username || "").trim()
        const password = String(req.body.password || "")

        if (username !== HARDCODED_ADMIN_USERNAME || password !== HARDCODED_ADMIN_PASSWORD) {
            console.log(`[ADMIN] failed login attempt from ${getClientIp(req)}`)
            return res.status(401).json({ success: false, message: "invalid admin credentials" })
        }

        const token = createSecretToken()
        const tokenHash = hashToken(token, ADMIN_SECRET)

        await pool.query(
            `
            INSERT INTO admin_sessions (token_hash, expires_at)
            VALUES ($1, NOW() + ($2 * INTERVAL '1 day'))
            `,
            [tokenHash, ADMIN_SESSION_DAYS]
        )

        setAdminCookie(res, token)
        console.log(`[ADMIN] login success from ${getClientIp(req)}`)

        res.json({ success: true, message: "admin authenticated" })
    }
    catch (error) {
        next(error)
    }
})

router.post("/logout", requireAdmin, async (req, res, next) => {
    try {
        const token = parseCookies(req)[ADMIN_COOKIE_NAME]
        const tokenHash = hashToken(token, ADMIN_SECRET)
        await pool.query(`DELETE FROM admin_sessions WHERE token_hash = $1`, [tokenHash])
        clearAdminCookie(res)
        res.json({ success: true, message: "admin signed out" })
    }
    catch (error) {
        next(error)
    }
})

router.get("/check", requireAdmin, (req, res) => {
    res.json({ success: true, admin: true })
})

router.get("/dashboard", requireAdmin, async (req, res, next) => {
    try {
        const metrics = await getMetrics()
        res.json({ success: true, metrics })
    }
    catch (error) {
        next(error)
    }
})

router.post("/dashboard/refresh", requireAdmin, async (req, res, next) => {
    try {
        await refreshMetrics()
        const metrics = await getMetrics()
        res.json({ success: true, metrics })
    }
    catch (error) {
        next(error)
    }
})

router.get("/feedback", requireAdmin, async (req, res, next) => {
    try {
        const limit = Math.min(Number(req.query.limit) || 20, 100)
        const offset = Number(req.query.offset) || 0
        const feedback = await getFeedback(limit, offset)
        res.json({ success: true, feedback })
    }
    catch (error) {
        next(error)
    }
})

router.get("/feedback/presets", (req, res) => {
    res.json({ success: true, presets: FEEDBACK_PRESETS })
})

router.post("/feedback", async (req, res, next) => {
    try {
        const result = await submitFeedback({
            selectedPresets: req.body.selectedPresets,
            customFeedback: req.body.customFeedback,
            contactInfo: req.body.contactInfo,
            pageUrl: req.body.pageUrl,
            userId: req.body.userId
        })
        res.status(201).json({ success: true, feedback: result })
    }
    catch (error) {
        next(error)
    }
})

export default router
export { requireAdmin }
