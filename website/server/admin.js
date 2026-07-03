import { Router } from "express"
import { hashToken, getClientIp, verifyPassword, usernameKey, normalizeEmail } from "./authCore.js"
import { pool } from "./db.js"
import { getMetrics, refreshMetrics } from "./analytics.js"
import { getFeedback, FEEDBACK_PRESETS } from "./feedback.js"
import {
    parseCookies,
    clearSessionCookie,
    createSession,
    sessionCookieName,
    sessionSecret
} from "./session.js"

const router = Router()

const ADMIN_ROLES = ["admin", "owner"]

async function requireAdmin(req, res, next) {
    try {
        const token = parseCookies(req)[sessionCookieName]

        if (!token) {
            console.log(`[ADMIN AUTH] 401 no token path=${req.path} ip=${getClientIp(req)}`)
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
            console.log(`[ADMIN AUTH] 401 session expired path=${req.path}`)
            clearSessionCookie(res)
            return res.status(401).json({
                success: false,
                message: "session expired"
            })
        }

        const user = result.rows[0]

        if (!ADMIN_ROLES.includes(user.role)) {
            console.log(`[ADMIN AUTH] 403 user=${user.username} role=${user.role} path=${req.path}`)
            return res.status(403).json({
                success: false,
                message: "admin access required"
            })
        }

        console.log(`[ADMIN AUTH] 200 user=${user.username} role=${user.role} path=${req.path}`)
        req.user = user
        next()
    }
    catch (error) {
        next(error)
    }
}

router.post("/login", async (req, res, next) => {
    try {
        console.log("[ADMIN LOGIN] received body keys:", Object.keys(req.body))
        console.log("[ADMIN LOGIN] username field present:", "username" in req.body)
        console.log("[ADMIN LOGIN] identifier field present:", "identifier" in req.body)

        const username = String(req.body.username || "").trim()
        const password = String(req.body.password || "")

        if (!username || !password) {
            return res.status(400).json({
                success: false,
                message: "username and password required"
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
            [ADMIN_ROLES, usernameKey(username), normalizeEmail(username)]
        )

        const user = result.rows[0]

        if (!user || !(await verifyPassword(password, user.password_hash))) {
            console.log(`[ADMIN] failed login attempt from ${getClientIp(req)}`)
            return res.status(401).json({
                success: false,
                message: "invalid credentials or insufficient permissions"
            })
        }

        await createSession(user.id, req, res)
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

        clearSessionCookie(res)
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
                   avatar_url, avatar_updated_at,
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

router.get("/check", async (req, res) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        if (!token) {
            return res.json({ success: true, isAdmin: false })
        }

        const result = await pool.query(
            `
            SELECT u.role
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
            return res.json({ success: true, isAdmin: false })
        }

        const isAdmin = ADMIN_ROLES.includes(result.rows[0].role)
        res.json({ success: true, isAdmin })
    }
    catch {
        res.json({ success: true, isAdmin: false })
    }
})

router.get("/admins", requireAdmin, async (req, res, next) => {
    try {
        const search = String(req.query.search || "").trim().toLowerCase()

        let sql = `
            SELECT id, username, email, role, avatar_url, avatar_updated_at,
                   created_at, email_verified_at, achievements
            FROM users
            WHERE deleted_at IS NULL
              AND role = ANY($1)
        `
        const params = [ADMIN_ROLES]

        if (search) {
            sql += ` AND (LOWER(username) LIKE $2 OR LOWER(email) LIKE $2)`
            params.push(`%${search}%`)
        }

        sql += ` ORDER BY
            CASE role
                WHEN 'owner' THEN 1
                WHEN 'admin' THEN 2
                ELSE 3
            END,
            username ASC
        `

        const result = await pool.query(sql, params)

        res.json({
            success: true,
            admins: result.rows
        })
    }
    catch (error) {
        next(error)
    }
})

router.get("/flagged-accounts", requireAdmin, async (req, res, next) => {
    try {
        const { getFlaggedAccounts } = await import("./authCore.js")
        const flagged = await getFlaggedAccounts()
        console.log(`[ADMIN] Flagged accounts: ${flagged.length}`)
        res.json({ success: true, flagged })
    } catch (error) {
        next(error)
    }
})

export default router
export { requireAdmin }
