import crypto from "crypto"
import { Router } from "express"
import { createSecretToken, getClientIp, hashToken } from "./authCore.js"
import { pool } from "./db.js"
import { authenticate, sessionSecret, sessionDays } from "./session.js"

const router = Router()

const exchangeTokens = new Map()

router.post("/token-exchange", (req, res) => {
    const token = crypto.randomBytes(32).toString("hex")
    exchangeTokens.set(token, {
        userId: req.user.id,
        username: req.user.username,
        createdAt: Date.now()
    })
    setTimeout(() => exchangeTokens.delete(token), 2 * 60 * 1000)
    res.json({ success: true, exchange_token: token })
})

router.post("/exchange-session", async (req, res, next) => {
    try {
        const exchangeToken = String(req.body.exchange_token || "").trim()
        const entry = exchangeTokens.get(exchangeToken)
        if (!entry) {
            return res.status(404).json({ success: false, message: "invalid or expired token" })
        }
        exchangeTokens.delete(exchangeToken)

        const token = createSecretToken()
        const tokenHash = hashToken(token, sessionSecret)
        await pool.query(
            `INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
             VALUES ($1, $2, $3, $4, NOW() + ($5 * INTERVAL '1 day'))`,
            [entry.userId, tokenHash, "game-client", getClientIp(req), sessionDays]
        )

        res.json({ success: true, session_token: token })
    }
    catch (error) {
        next(error)
    }
})

export default router
