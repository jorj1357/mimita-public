import { Router } from "express"
import { hashToken, getClientIp, verifyPassword, usernameKey, normalizeEmail } from "./authCore.js"
import { pool } from "./db.js"
import { getMetrics, refreshMetrics } from "./analytics.js"
import { submitFeedback, getFeedback, FEEDBACK_PRESETS } from "./feedback.js"

const router = Router()

const sessionCookieName =
    process.env.SESSION_COOKIE_NAME || "mimita_session"
const sessionSecret =
    process.env.SESSION_SECRET || "development-only-change-me"
const sessionDays = Number(process.env.SESSION_DAYS || 30)
const production = process.env.NODE_ENV === "production"

const ADMIN_ROLES = ["admin", "owner"]

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

function setAdminCookie(res, token) {
    res.cookie(sessionCookieName, token, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        maxAge: sessionDays * 24 * 60 * 60 * 1000,
        path: "/"
    })
}

function clearAdminCookie(res) {
    res.clearCookie(sessionCookieName, {
        httpOnly: true,
        secure: production,
        sameSite: "lax",
        path: "/"
    })
}

async function createAdminSession(userId, req, res) {
    const { createSecretToken } = await import("./authCore.js")
    const token = createSecretToken()
    const tokenHash = hashToken(token, sessionSecret)

    await pool.query(
        `
        INSERT INTO sessions (
            user_id, token_hash, user_agent, ip_address, expires_at
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

    setAdminCookie(res, token)
}

async function requireAdmin(req, res, next) {
    try {
        const token = parseCookies(req)[sessionCookieName]

        if (!token) {
            return res.status(401).json({
                success: false,
                message: "sign in required"
            })
        }

        const result = await pool.query(
            `
            SELECT
                u.id, u.username, u.email, u.role, u.bio
            FROM sessions s
            JOIN users u ON u.id = s.user_id
            WHERE s.token_hash = $1
              AND s.revoked_at IS NULL
              AND s.expires_at > NOW()
              AND u.deleted_at IS NULL
            LIMIT 1
            `,
            [hashToken(token, sessionSecret)]
        )

        if (!result.rowCount) {
            clearAdminCookie(res)
            return res.status(401).json({
                success: false,
                message: "session expired"
            })
        }

        const user = result.rows[0]

        if (!ADMIN_ROLES.includes(user.role)) {
            return res.status(403).json({
                success: false,
                message: "admin access required"
            })
        }

        req.user = user
        next()
    }
    catch (error) {
        next(error)
    }
}

router.post("/login", async (req, res, next) => {
    try {
        const identifier = String(req.body.identifier || "").trim()
        const password = String(req.body.password || "")

        if (!identifier || !password) {
            return res.status(400).json({
                success: false,
                message: "identifier and password required"
            })
        }

        const result = await pool.query(
            `
            SELECT id, username, email, role, password_hash
            FROM users
            WHERE deleted_at IS NULL
              AND role = ANY($1)
              AND (
                  username_key = $2
                  OR email = $3
              )
            LIMIT 1
            `,
            [ADMIN_ROLES, usernameKey(identifier), normalizeEmail(identifier)]
        )

        const user = result.rows[0]

        if (!user || !(await verifyPassword(password, user.password_hash))) {
            console.log(`[ADMIN] failed login attempt from ${getClientIp(req)}`)
            return res.status(401).json({
                success: false,
                message: "invalid credentials or insufficient permissions"
            })
        }

        await createAdminSession(user.id, req, res)
        console.log(`[ADMIN] login success user_id=${user.id} username=${user.username} from ${getClientIp(req)}`)

        res.json({
            success: true,
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                role: user.role
            }
        })
    }
    catch (error) {
        next(error)
    }
})

router.post("/logout", requireAdmin, async (req, res, next) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        const tokenHash = hashToken(token, sessionSecret)

        await pool.query(
            `UPDATE sessions SET revoked_at = NOW() WHERE token_hash = $1`,
            [tokenHash]
        )

        clearAdminCookie(res)
        console.log(`[ADMIN] logout user_id=${req.user.id}`)
        res.json({ success: true, message: "signed out" })
    }
    catch (error) {
        next(error)
    }
})

router.get("/me", requireAdmin, (req, res) => {
    res.json({
        success: true,
        user: {
            id: req.user.id,
            username: req.user.username,
            email: req.user.email,
            role: req.user.role,
            bio: req.user.bio
        }
    })
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

router.get("/users", requireAdmin, async (req, res, next) => {
    try {
        const limit = Math.min(Number(req.query.limit) || 50, 200)
        const offset = Number(req.query.offset) || 0

        const result = await pool.query(
            `
            SELECT id, username, email, role, bio,
                   email_notifications_enabled,
                   created_at, updated_at, deleted_at
            FROM users
            ORDER BY created_at DESC
            LIMIT $1 OFFSET $2
            `,
            [limit, offset]
        )

        const countResult = await pool.query(
            `SELECT COUNT(*) AS count FROM users`
        )

        res.json({
            success: true,
            users: result.rows,
            total: Number(countResult.rows[0].count),
            limit,
            offset
        })
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

export default router
export { requireAdmin }
