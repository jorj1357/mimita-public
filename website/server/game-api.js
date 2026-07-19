// 07 19 2026, 12 00
/* purpose
* Expose game-client account data endpoints for the Mimita exe.
* Authenticate bearer/cookie sessions and return profile, stats, settings, and avatar data.
* Keep game-facing account bootstrap data in one backend owner.
* DOES NOT verify passwords or create login sessions.
* DOES NOT render website pages or admin dashboards.
* DOES NOT store client-side tokens.
*/

import { Router } from "express"
import { hashToken, getClientIp } from "./authCore.js"
import { pool } from "./db.js"
import { parseCookies, sessionCookieName, sessionSecret } from "./session.js"
import crypto from "crypto"

const router = Router()

function debugAim(...args) {
    console.log("[AimTrainerAPI]", ...args)
}

function makeDebugId() {
    return "aim_" + crypto.randomBytes(4).toString("hex")
}

function defaultStats() {
    return {
        wins: 0,
        losses: 0,
        kills: 0,
        deaths: 0,
        games_played: 0,
        playtime_seconds: 0,
        highest_mmr: 5000,
        current_mmr: 5000,
        accuracy: 0.0,
        headshots: 0,
        best_kill_streak: 0
    }
}

function profileFromUser(u) {
    return {
        id: u.id,
        username: u.username,
        display_name: u.display_name || u.username,
        email: u.email,
        bio: u.bio,
        avatar_url: u.avatar_url,
        avatar_data: u.avatar_data,
        supporter_tier: u.supporter_tier,
        role: u.role,
        created_at: u.created_at,
        email_verified: u.email_verified
    }
}

async function authenticateToken(req, res, next) {
    try {
        let token = parseCookies(req)[sessionCookieName]
        if (!token) {
            const authHeader = req.headers["authorization"]
            if (authHeader && authHeader.startsWith("Bearer ")) {
                token = authHeader.slice(7)
            }
        }
        if (!token) {
            return res.status(401).json({ success: false, error: "auth_required", message: "sign in required" })
        }

        const result = await pool.query(
            `SELECT u.id, u.username, u.display_name, u.email, u.bio,
                    u.avatar_url, u.avatar_data, u.supporter_tier, u.role, u.created_at,
                    u.email_verified_at IS NOT NULL AS email_verified
             FROM sessions s
             JOIN users u ON u.id = s.user_id
             WHERE s.token_hash = $1
               AND s.revoked_at IS NULL
               AND s.expires_at > NOW()
               AND u.deleted_at IS NULL
             LIMIT 1`,
            [hashToken(token, sessionSecret)]
        )

        if (!result.rowCount) {
            return res.status(401).json({ success: false, error: "session_expired", message: "session expired" })
        }

        req.user = result.rows[0]
        req.user.email_verified = result.rows[0].email_verified
        next()
    }
    catch (error) {
        next(error)
    }
}

router.get("/game/me", authenticateToken, async (req, res, next) => {
    try {
        const statsResult = await pool.query(
            `SELECT wins, losses, kills, deaths, games_played, playtime_seconds,
                    highest_mmr, current_mmr, accuracy, headshots, best_kill_streak
             FROM game_stats WHERE user_id = $1`,
            [req.user.id]
        )

        const settingsResult = await pool.query(
            `SELECT settings_json FROM user_settings WHERE user_id = $1`,
            [req.user.id]
        )

        res.json({
            success: true,
            account: {
                id: req.user.id,
                username: req.user.username,
                permissions: [req.user.role || "user"],
                supporter_tier: req.user.supporter_tier || "free"
            },
            profile: profileFromUser(req.user),
            stats: statsResult.rowCount ? statsResult.rows[0] : defaultStats(),
            settings: settingsResult.rowCount ? settingsResult.rows[0].settings_json : {},
            avatar: {
                avatar_url: req.user.avatar_url || "",
                avatar_data: req.user.avatar_data || null
            },
            inventory: {
                version: 1,
                items: [],
                equipped: {}
            },
            titles: {
                version: 1,
                unlocked: [],
                equipped: ""
            },
            loadout: {
                version: 1,
                weapons: {},
                cosmetics: {}
            }
        })
    }
    catch (error) {
        next(error)
    }
})

router.get("/profile", authenticateToken, (req, res) => {
    const u = req.user
    res.json({
        success: true,
        profile: profileFromUser(u)
    })
})

router.patch("/profile", authenticateToken, async (req, res, next) => {
    try {
        const allowed = ["bio", "display_name"]
        const updates = []
        const params = []
        let idx = 1

        for (const field of allowed) {
            if (req.body[field] !== undefined) {
                updates.push(`${field} = $${idx}`)
                params.push(req.body[field])
                idx++
            }
        }

        if (updates.length === 0) {
            return res.json({ success: true })
        }

        params.push(req.user.id)
        await pool.query(
            `UPDATE users SET ${updates.join(", ")}, updated_at = NOW() WHERE id = $${idx}`,
            params
        )

        res.json({ success: true })
    }
    catch (error) {
        next(error)
    }
})

router.get("/avatar/data", authenticateToken, (req, res) => {
    res.json({
        success: true,
        avatar_data: req.user.avatar_data || null
    })
})

router.put("/avatar/data", authenticateToken, async (req, res, next) => {
    try {
        const avatarData = req.body.avatar_data || null
        await pool.query(
            `UPDATE users SET avatar_data = $1, updated_at = NOW() WHERE id = $2`,
            [avatarData ? JSON.stringify(avatarData) : null, req.user.id]
        )
        res.json({ success: true })
    }
    catch (error) {
        next(error)
    }
})

router.get("/stats", authenticateToken, async (req, res, next) => {
    try {
        const result = await pool.query(
            `SELECT wins, losses, kills, deaths, games_played, playtime_seconds,
                    highest_mmr, current_mmr, accuracy, headshots, best_kill_streak
             FROM game_stats WHERE user_id = $1`,
            [req.user.id]
        )

        if (!result.rowCount) {
            return res.json({
                success: true,
                stats: {
                    ...defaultStats()
                }
            })
        }

        res.json({ success: true, stats: result.rows[0] })
    }
    catch (error) {
        next(error)
    }
})

router.post("/stats", authenticateToken, async (req, res, next) => {
    try {
        const { match_id, map_name, game_mode, duration_seconds, winner_id,
                kills, deaths, accuracy, headshots, damage_dealt, won, team,
                mmr_before, mmr_after } = req.body

        const client = await pool.connect()
        try {
            await client.query("BEGIN")

            await client.query(
                `INSERT INTO match_history (match_id, map_name, game_mode, duration_seconds, winner_id)
                 VALUES ($1, $2, $3, $4, $5)
                 ON CONFLICT (match_id) DO NOTHING`,
                [match_id, map_name || "", game_mode || "", duration_seconds || 0, winner_id || null]
            )

            await client.query(
                `INSERT INTO match_participants (match_id, user_id, username, kills, deaths, accuracy, headshots, damage_dealt, team, won, mmr_before, mmr_after)
                 VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12)
                 ON CONFLICT (match_id, user_id) DO NOTHING`,
                [match_id, req.user.id, req.user.username,
                 kills || 0, deaths || 0, accuracy || 0.0, headshots || 0,
                 damage_dealt || 0, team || 0, won || false,
                 mmr_before || 5000, mmr_after || 5000]
            )

            await client.query(
                `INSERT INTO game_stats (user_id, wins, losses, kills, deaths, games_played, playtime_seconds,
                                         highest_mmr, current_mmr, accuracy, headshots, best_kill_streak)
                 VALUES ($1, $2, $3, $4, $5, 1, $6, $7, $8, $9, $10, $11)
                 ON CONFLICT (user_id) DO UPDATE SET
                     wins = game_stats.wins + $2,
                     losses = game_stats.losses + $3,
                     kills = game_stats.kills + $4,
                     deaths = game_stats.deaths + $5,
                     games_played = game_stats.games_played + 1,
                     playtime_seconds = game_stats.playtime_seconds + $6,
                     highest_mmr = GREATEST(game_stats.highest_mmr, $7),
                     current_mmr = $8,
                     accuracy = $9,
                     headshots = game_stats.headshots + $10,
                     best_kill_streak = GREATEST(game_stats.best_kill_streak, $11),
                     updated_at = NOW()`,
                [req.user.id,
                 won ? 1 : 0, won ? 0 : 1,
                 kills || 0, deaths || 0, duration_seconds || 0,
                 mmr_after || 5000, mmr_after || 5000,
                 accuracy || 0.0, headshots || 0, kills || 0]
            )

            await client.query("COMMIT")
            res.json({ success: true })
        }
        catch (error) {
            await client.query("ROLLBACK")
            throw error
        }
        finally {
            client.release()
        }
    }
    catch (error) {
        next(error)
    }
})

router.get("/leaderboard", async (req, res, next) => {
    try {
        const type = req.query.type || "mmr"
        const limit = Math.min(Number(req.query.limit) || 50, 200)

        let orderBy
        switch (type) {
            case "wins": orderBy = "gs.wins DESC"; break
            case "kills": orderBy = "gs.kills DESC"; break
            case "kill_streak": orderBy = "gs.best_kill_streak DESC"; break
            default: orderBy = "gs.current_mmr DESC"
        }

        const result = await pool.query(
            `SELECT
                ROW_NUMBER() OVER (ORDER BY ${orderBy}) AS rank,
                u.id, u.username, u.avatar_url, u.supporter_tier,
                gs.wins, gs.losses, gs.kills, gs.deaths,
                gs.games_played, gs.playtime_seconds,
                gs.highest_mmr, gs.current_mmr,
                gs.accuracy, gs.headshots, gs.best_kill_streak
             FROM game_stats gs
             JOIN users u ON u.id = gs.user_id
             WHERE u.deleted_at IS NULL
             ORDER BY ${orderBy}
             LIMIT $1`,
            [limit]
        )

        res.json({ success: true, leaderboard: result.rows })
    }
    catch (error) {
        next(error)
    }
})

router.get("/match-history", authenticateToken, async (req, res, next) => {
    try {
        const page = Math.max(1, Number(req.query.page) || 1)
        const limit = Math.min(Number(req.query.limit) || 20, 100)
        const offset = (page - 1) * limit

        const countResult = await pool.query(
            `SELECT COUNT(*) FROM match_participants WHERE user_id = $1`,
            [req.user.id]
        )
        const total = Number(countResult.rows[0].count)

        const result = await pool.query(
            `SELECT mh.match_id, mh.map_name, mh.game_mode, mh.duration_seconds, mh.created_at,
                    mp.kills, mp.deaths, mp.accuracy, mp.headshots, mp.damage_dealt,
                    mp.won, mp.mmr_before, mp.mmr_after
             FROM match_participants mp
             JOIN match_history mh ON mh.match_id = mp.match_id
             WHERE mp.user_id = $1
             ORDER BY mh.created_at DESC
             LIMIT $2 OFFSET $3`,
            [req.user.id, limit, offset]
        )

        res.json({
            success: true,
            matches: result.rows,
            total,
            page,
            pages: Math.ceil(total / limit)
        })
    }
    catch (error) {
        next(error)
    }
})

router.get("/settings", authenticateToken, async (req, res, next) => {
    try {
        const result = await pool.query(
            `SELECT settings_json FROM user_settings WHERE user_id = $1`,
            [req.user.id]
        )

        res.json({
            success: true,
            settings: result.rowCount ? result.rows[0].settings_json : {}
        })
    }
    catch (error) {
        next(error)
    }
})

router.put("/settings", authenticateToken, async (req, res, next) => {
    try {
        const settings = req.body.settings || {}
        await pool.query(
            `INSERT INTO user_settings (user_id, settings_json, updated_at)
             VALUES ($1, $2, NOW())
             ON CONFLICT (user_id) DO UPDATE SET
                 settings_json = $2, updated_at = NOW()`,
            [req.user.id, JSON.stringify(settings)]
        )
        res.json({ success: true })
    }
    catch (error) {
        next(error)
    }
})

// ── Game Scores (aim-test-v1, reaction-test, etc.) ──

router.get("/games/:gameId/scores", authenticateToken, async (req, res, next) => {
    try {
        const { gameId } = req.params
        const userId = req.user.id

        debugAim("[Scores] GET request", JSON.stringify({ gameId, userId, method: req.method, path: req.originalUrl, origin: req.get("origin") || "none", ip: getClientIp(req) }))

        const bestResult = await pool.query(
            `SELECT score_value, created_at FROM game_scores
             WHERE user_id = $1 AND game_id = $2 AND deleted_at IS NULL
             ORDER BY score_value ASC LIMIT 1`,
            [userId, gameId]
        )
        const recentResult = await pool.query(
            `SELECT score_value, created_at FROM game_scores
             WHERE user_id = $1 AND game_id = $2 AND deleted_at IS NULL
             ORDER BY created_at DESC LIMIT 10`,
            [userId, gameId]
        )

        const response = {
            success: true,
            best: bestResult.rowCount ? bestResult.rows[0].score_value : null,
            recent: recentResult.rows.map(r => ({ time: r.score_value, date: r.created_at })),
        }
        debugAim("[Scores] GET response", JSON.stringify({ userId, gameId, best: response.best, recentCount: response.recent.length }))
        res.json(response)
    } catch (error) {
        next(error)
    }
})

router.post("/games/:gameId/scores", authenticateToken, async (req, res, next) => {
    const debugId = makeDebugId()
    try {
        const { gameId } = req.params
        const scoreValue = req.body.time
        const userId = req.user.id
        const username = req.user.username

        debugAim("[Submit] request received", JSON.stringify({
            debugId,
            method: req.method,
            path: req.originalUrl,
            origin: req.get("origin") || "none",
            referer: req.get("referer") || "none",
            ip: getClientIp(req),
            contentType: req.get("content-type"),
            userAgent: (req.get("user-agent") || "").slice(0, 80),
            body: JSON.stringify(req.body),
            gameId,
            userId,
            username,
        }))

        if (typeof scoreValue !== "number" || scoreValue <= 0) {
            debugAim("[Submit] rejected", JSON.stringify({ debugId, reason: "invalid_payload", details: `scoreValue=${scoreValue} type=${typeof scoreValue}` }))
            return res.status(400).json({ success: false, error: "invalid_payload", message: "time must be a positive number", debugId })
        }

        const result = await pool.query(
            `INSERT INTO game_scores (user_id, game_id, score_value, created_at)
             VALUES ($1, $2, $3, NOW())
             RETURNING id, created_at`,
            [userId, gameId, scoreValue]
        )

        debugAim("[Submit] inserted score", JSON.stringify({
            debugId,
            scoreId: result.rows[0].id,
            userId,
            username,
            gameId,
            timeMs: scoreValue,
            createdAt: result.rows[0].created_at,
        }))

        res.json({ success: true, scoreId: result.rows[0].id, stored: true })
    } catch (error) {
        debugAim("[Submit] rejected", JSON.stringify({ debugId, reason: "db_error", details: error.message }))
        next(error)
    }
})

// Public leaderboard for mini games
const HIGH_SCORE_GAMES = new Set(["rhythm-test-v1"])
router.get("/games/:gameId/leaderboard", async (req, res, next) => {
    try {
        const { gameId } = req.params
        const limit = Math.min(Number(req.query.limit) || 10, 100)
        const desc = HIGH_SCORE_GAMES.has(gameId)

        debugAim("[Leaderboard] request received", JSON.stringify({
            method: req.method,
            path: req.originalUrl,
            origin: req.get("origin") || "none",
            limit,
            gameId,
            ordering: desc ? "DESC" : "ASC",
        }))

        const result = await pool.query(
            `SELECT
                ROW_NUMBER() OVER (ORDER BY gs.score_value ${desc ? "DESC" : "ASC"}) AS rank,
                u.id, u.username, u.avatar_url, u.supporter_tier,
                gs.score_value, gs.created_at
             FROM game_scores gs
             JOIN users u ON u.id = gs.user_id
             WHERE gs.game_id = $1 AND gs.deleted_at IS NULL AND u.deleted_at IS NULL
             ORDER BY gs.score_value ${desc ? "DESC" : "ASC"}
             LIMIT $2`,
            [gameId, limit]
        )

        const rows = result.rows
        debugAim("[Leaderboard] response", JSON.stringify({
            entries: rows.length,
            ordering: desc ? "DESC" : "ASC",
            firstEntry: rows.length > 0 ? { rank: rows[0].rank, username: rows[0].username, score: rows[0].score_value } : null,
            lastEntry: rows.length > 0 ? { rank: rows[rows.length - 1].rank, username: rows[rows.length - 1].username, score: rows[rows.length - 1].score_value } : null,
        }))

        res.json({ success: true, leaderboard: rows })
    } catch (error) {
        next(error)
    }
})

export default router
