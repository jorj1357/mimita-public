import { Router } from "express"
import { getClientIp, hashToken, createSecretToken } from "./authCore.js"
import { pool } from "./db.js"
import { authenticate, sessionSecret, sessionDays } from "./session.js"
import { createRateLimit } from "./rateLimit.js"
import { getVipStateForUser } from "./vip-entitlements.js"

const router = Router()
const clientLoginRateLimit = createRateLimit({ windowMs: 10 * 1000, max: 10, name: "client_login" })

router.post("/create-code", authenticate, async (req, res, next) => {
    try {
        const chars = "ABCDEFGHJKLMNPQRSTUVWXYZ"
        let code = ""
        for (let i = 0; i < 4; i++) {
            code += chars[Math.floor(Math.random() * chars.length)]
        }

        const codeHash = hashToken(code, sessionSecret)
        const expiresAt = new Date(Date.now() + 5 * 60 * 1000)

        await pool.query(
            `INSERT INTO client_login_codes (user_id, code_hash, expires_at, ip_address, user_agent)
             VALUES ($1, $2, $3, $4, $5)`,
            [req.user.id, codeHash, expiresAt, getClientIp(req), req.get("user-agent") || "unknown"]
        )

        res.json({ success: true, code, expires_at: expiresAt.toISOString() })
    }
    catch (error) {
        next(error)
    }
})

router.post("/preview", clientLoginRateLimit, async (req, res, next) => {
    try {
        const code = String(req.body.code || "").trim().toUpperCase()
        const codeHash = hashToken(code, sessionSecret)

        const result = await pool.query(
            `SELECT u.id, u.username, u.display_name, u.avatar_url, u.supporter_tier, u.role
             FROM client_login_codes c
             JOIN users u ON u.id = c.user_id
             WHERE c.code_hash = $1
               AND c.used_at IS NULL
               AND c.expires_at > NOW()
             LIMIT 1`,
            [codeHash]
        )

        if (!result.rowCount) {
            return res.json({ success: true, valid: false })
        }

        const vip = await getVipStateForUser(result.rows[0], pool)

        res.json({
            success: true,
            valid: true,
            username: result.rows[0].username,
            display_name: result.rows[0].display_name || result.rows[0].username,
            avatar_url: result.rows[0].avatar_url || "",
            supporter_tier: vip.active_tier,
            vip
        })
    }
    catch (error) {
        next(error)
    }
})

router.post("/confirm", clientLoginRateLimit, async (req, res, next) => {
    try {
        const code = String(req.body.code || "").trim().toUpperCase()
        const codeHash = hashToken(code, sessionSecret)

        const result = await pool.query(
            `SELECT c.id, c.user_id, u.username, u.display_name, u.role
             FROM client_login_codes c
             JOIN users u ON u.id = c.user_id
             WHERE c.code_hash = $1
               AND c.used_at IS NULL
               AND c.expires_at > NOW()
             LIMIT 1`,
            [codeHash]
        )

        if (!result.rowCount) {
            return res.status(404).json({ success: false, message: "invalid or expired code" })
        }

        await pool.query(
            `UPDATE client_login_codes SET used_at = NOW() WHERE id = $1`,
            [result.rows[0].id]
        )

        const token = createSecretToken()
        const tokenHash = hashToken(token, sessionSecret)
        await pool.query(
            `INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
             VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))`,
            [result.rows[0].user_id, tokenHash, "game-client", getClientIp(req), sessionDays]
        )

        const vip = await getVipStateForUser({
            id: result.rows[0].user_id,
            role: result.rows[0].role || "user"
        }, pool)

        res.json({
            success: true,
            session_token: token,
            account: {
                id: result.rows[0].user_id,
                username: result.rows[0].username,
                display_name: result.rows[0].display_name || result.rows[0].username,
                supporter_tier: vip.active_tier,
                vip
            }
        })
    }
    catch (error) {
        next(error)
    }
})

export default router
