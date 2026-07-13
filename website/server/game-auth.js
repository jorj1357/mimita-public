import { Router } from "express"
import crypto from "crypto"
import { pool } from "./db.js"
import { hashToken, createSecretToken, verifyPassword, getClientIp,
         checkBruteForce, recordFailedAttempt, resetFailedAttempts,
         usernameKey, normalizeEmail } from "./authCore.js"
import { sessionSecret, sessionDays } from "./session.js"
import { createRateLimit } from "./rateLimit.js"

const router = Router()
const loginRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "game_auth_login" })

function hashRefreshToken(token) {
    return crypto.createHash("sha256").update("mimita_refresh:").update(token).digest("hex")
}

// POST /api/game/auth/login
router.post("/login", loginRateLimit, async (req, res, next) => {
    try {
        const identifier = String(req.body.identifier || "").trim()
        const password = String(req.body.password || "")
        const rememberMe = req.body.remember_me === true
        const device = req.body.device || {}

        if (!identifier || !password) {
            return res.status(400).json({
                ok: false,
                error: { code: "INVALID_CREDENTIALS", message: "The username/email or password was incorrect." }
            })
        }

        const bruteForce = await checkBruteForce(identifier)
        if (bruteForce.locked) {
            console.log(`[GAME AUTH] login locked identifier=${identifier}`)
            return res.status(429).json({
                ok: false,
                error: { code: "RATE_LIMITED", message: "Too many attempts. Try again later." }
            })
        }

        const result = await pool.query(
            `SELECT id, username, email, password_hash, supporter_tier, role,
                    deleted_at, email_verified_at
             FROM users
             WHERE deleted_at IS NULL
               AND (username_key = $1 OR email = $2)
             LIMIT 1`,
            [usernameKey(identifier), normalizeEmail(identifier)]
        )
        const user = result.rows[0]

        if (!user || !(await verifyPassword(password, user.password_hash))) {
            console.log(`[GAME AUTH] login failed identifier=${identifier}`)
            await recordFailedAttempt(identifier, getClientIp(req))
            return res.status(401).json({
                ok: false,
                error: { code: "INVALID_CREDENTIALS", message: "The username/email or password was incorrect." }
            })
        }

        const accessToken = createSecretToken()
        const rawRefreshToken = createSecretToken()
        const refreshTokenHash = hashRefreshToken(rawRefreshToken)

        const deviceId = String(device.device_id || "unknown")
        const deviceName = String(device.device_name || "")
        const platform = String(device.platform || "unknown")
        const clientBuild = String(device.client_build || "")

        const accessExpiresAt = new Date(Date.now() + 15 * 60 * 1000)
        const refreshExpiresAt = new Date(Date.now() + sessionDays * 24 * 60 * 60 * 1000)

        await pool.query(
            `INSERT INTO sessions (user_id, token_hash, refresh_token_hash, device_id, device_name,
                                    platform, client_build, user_agent, ip_address, expires_at)
             VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)`,
            [user.id, hashToken(accessToken, sessionSecret), refreshTokenHash,
             deviceId, deviceName, platform, clientBuild,
             req.get("user-agent") || "game-client", getClientIp(req), refreshExpiresAt]
        )

        await resetFailedAttempts(identifier)

        console.log(`[GAME AUTH] login success user_id=${user.id} username=${user.username} rememberMe=${rememberMe}`)

        res.json({
            ok: true,
            account: {
                id: user.id,
                username: user.username,
                permissions: [user.role || "player"],
                supporter_tier: user.supporter_tier || "free"
            },
            session: {
                access_token: accessToken,
                access_expires_at: accessExpiresAt.toISOString(),
                refresh_token: rawRefreshToken,
                refresh_expires_at: refreshExpiresAt.toISOString()
            },
            remember_me: rememberMe
        })
    } catch (error) {
        console.log(`[GAME AUTH] login error: ${error.message}`)
        next(error)
    }
})

// POST /api/game/auth/refresh
router.post("/refresh", async (req, res, next) => {
    try {
        const refreshToken = String(req.body.refresh_token || "")
        const deviceId = String(req.body.device_id || "")

        if (!refreshToken) {
            return res.status(401).json({
                ok: false,
                error: { code: "INVALID_REFRESH_TOKEN", message: "Refresh token is required." }
            })
        }

        const refreshHash = hashRefreshToken(refreshToken)

        const result = await pool.query(
            `SELECT s.id AS session_id, s.user_id, u.deleted_at,
                    u.supporter_tier, u.role, u.username,
                    s.device_id, s.revoked_at, s.expires_at
             FROM sessions s
             JOIN users u ON u.id = s.user_id
             WHERE s.refresh_token_hash = $1
             LIMIT 1`,
            [refreshHash]
        )

        const session = result.rows[0]
        if (!session) {
            console.log(`[GAME AUTH] refresh failed: token not found`)
            return res.status(401).json({
                ok: false,
                error: { code: "INVALID_REFRESH_TOKEN", message: "Session not found." }
            })
        }

        if (session.revoked_at) {
            console.log(`[GAME AUTH] refresh failed: session revoked user_id=${session.user_id}`)
            return res.status(401).json({
                ok: false,
                error: { code: "SESSION_REVOKED", message: "Session has been revoked." }
            })
        }

        if (new Date(session.expires_at) < new Date()) {
            console.log(`[GAME AUTH] refresh failed: session expired user_id=${session.user_id}`)
            return res.status(401).json({
                ok: false,
                error: { code: "SESSION_EXPIRED", message: "Session has expired. Sign in again." }
            })
        }

        if (session.deleted_at) {
            console.log(`[GAME AUTH] refresh failed: account deleted user_id=${session.user_id}`)
            return res.status(401).json({
                ok: false,
                error: { code: "ACCOUNT_DISABLED", message: "Account is no longer available." }
            })
        }

        if (deviceId && session.device_id && session.device_id !== "unknown" && session.device_id !== deviceId) {
            console.log(`[GAME AUTH] refresh device mismatch session_device=${session.device_id} request_device=${deviceId}`)
            return res.status(401).json({
                ok: false,
                error: { code: "DEVICE_MISMATCH", message: "Session does not match this device." }
            })
        }

        const newAccessToken = createSecretToken()
        const newAccessHash = hashToken(newAccessToken, sessionSecret)

        await pool.query(
            `UPDATE sessions
             SET token_hash = $1, last_used_at = NOW()
             WHERE id = $2`,
            [newAccessHash, session.session_id]
        )

        console.log(`[GAME AUTH] refresh success user_id=${session.user_id}`)

        res.json({
            ok: true,
            access_token: newAccessToken,
            access_expires_at: new Date(Date.now() + 15 * 60 * 1000).toISOString(),
            refresh_token: refreshToken,
            refresh_expires_at: session.expires_at
        })
    } catch (error) {
        console.log(`[GAME AUTH] refresh error: ${error.message}`)
        next(error)
    }
})

// POST /api/game/auth/logout
router.post("/logout", async (req, res, next) => {
    try {
        const refreshToken = String(req.body.refresh_token || "")
        const deviceId = String(req.body.device_id || "")

        if (!refreshToken) {
            return res.json({ ok: true })
        }

        const refreshHash = hashRefreshToken(refreshToken)

        let query
        let params

        if (deviceId) {
            query = `UPDATE sessions SET revoked_at = NOW() WHERE refresh_token_hash = $1 AND device_id = $2`
            params = [refreshHash, deviceId]
        } else {
            query = `UPDATE sessions SET revoked_at = NOW() WHERE refresh_token_hash = $1`
            params = [refreshHash]
        }

        const result = await pool.query(query, params)
        console.log(`[GAME AUTH] logout: revoked ${result.rowCount} session(s)`)

        res.json({ ok: true })
    } catch (error) {
        console.log(`[GAME AUTH] logout error: ${error.message}`)
        next(error)
    }
})

export default router
