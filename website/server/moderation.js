// 09 09 2026
/* purpose
* Moderation system for the MiMITA website.
* Report, block, and mute functionality.
* Reports go to an admin queue with email notification.
* Blocks hide the user's profile, DMs, forum posts, and in-game chat.
* Mutes hide their messages everywhere.
* DOES NOT render the moderation UI.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { requireAdmin } from "./admin.js"
import { createRateLimit } from "./rateLimit.js"
import { sendReportNotificationEmail } from "./mail.js"

function cleanText(value, max) {
    return String(value || "").trim().slice(0, max)
}

function createModerationRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params),
        mailFn = sendReportNotificationEmail
    } = deps

    const router = Router()
    router.use(authenticate)

    const reportRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "moderation_report" })

    // Submit a report
    router.post("/report", reportRateLimit, async (req, res, next) => {
        try {
            const reportedId = Number(req.body.reportedId)
            const reason = cleanText(req.body.reason, 2000)

            if (!reportedId || reportedId === req.user.id) {
                return res.status(400).json({ success: false, message: "invalid user to report" })
            }
            if (!reason) {
                return res.status(400).json({ success: false, message: "reason is required" })
            }

            // Check reported user exists
            const reported = await query(
                `SELECT id, username FROM users WHERE id = $1 AND deleted_at IS NULL`,
                [reportedId]
            )
            if (!reported.rowCount) {
                return res.status(404).json({ success: false, message: "user not found" })
            }

            // Prevent duplicate reports from same user within 24h
            const recent = await query(
                `SELECT 1 FROM reports
                 WHERE reporter_id = $1 AND reported_id = $2
                   AND created_at > NOW() - INTERVAL '24 hours'`,
                [req.user.id, reportedId]
            )
            if (recent.rowCount) {
                return res.status(429).json({ success: false, message: "you already reported this user recently" })
            }

            const evidence = {
                serverId: cleanText(req.body.serverId, 100),
                matchId: cleanText(req.body.matchId, 100),
                reporterUsername: req.user.username,
                reportedUsername: reported.rows[0].username
            }

            const result = await query(
                `INSERT INTO reports (reporter_id, reported_id, reason, evidence, server_id, match_id)
                 VALUES ($1, $2, $3, $4, $5, $6)
                 RETURNING id, created_at`,
                [req.user.id, reportedId, reason, JSON.stringify(evidence), evidence.serverId, evidence.matchId]
            )

            console.log(`[MODERATION] report created id=${result.rows[0].id} reporter=${req.user.id} reported=${reportedId}`)

            res.status(201).json({
                success: true,
                reportId: result.rows[0].id
            })

            // Send email notification (non-blocking)
            mailFn({
                reportId: result.rows[0].id,
                reporterUsername: req.user.username,
                reportedUsername: reported.rows[0].username,
                reason,
                createdAt: result.rows[0].created_at
            })
                .then(() => console.log(`[MODERATION] report email sent id=${result.rows[0].id}`))
                .catch(err => console.log(`[MODERATION] report email failed id=${result.rows[0].id} error=${err?.message}`))
        }
        catch (error) {
            next(error)
        }
    })

    // Block a user
    router.post("/block/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId || targetUserId === req.user.id) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            const target = await query(
                `SELECT id FROM users WHERE id = $1 AND deleted_at IS NULL`,
                [targetUserId]
            )
            if (!target.rowCount) {
                return res.status(404).json({ success: false, message: "user not found" })
            }

            await query(
                `INSERT INTO blocks (blocker_id, blocked_id)
                 VALUES ($1, $2)
                 ON CONFLICT (blocker_id, blocked_id) DO NOTHING`,
                [req.user.id, targetUserId]
            )

            // Remove any existing friendship
            await query(
                `DELETE FROM friendships
                 WHERE ((requester_id = $1 AND addressee_id = $2)
                     OR (requester_id = $2 AND addressee_id = $1))`,
                [req.user.id, targetUserId]
            )

            // Remove any existing DM conversation membership
            await query(
                `DELETE FROM conversation_members
                 WHERE user_id = $1
                   AND conversation_id IN (
                       SELECT cm2.conversation_id FROM conversation_members cm2
                       JOIN conversations c ON c.id = cm2.conversation_id AND c.type = 'dm'
                       WHERE cm2.user_id = $2
                   )`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Unblock a user
    router.delete("/block/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            await query(
                `DELETE FROM blocks WHERE blocker_id = $1 AND blocked_id = $2`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Mute a user
    router.post("/mute/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId || targetUserId === req.user.id) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            await query(
                `INSERT INTO mutes (muter_id, muted_id)
                 VALUES ($1, $2)
                 ON CONFLICT (muter_id, muted_id) DO NOTHING`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Unmute a user
    router.delete("/mute/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            await query(
                `DELETE FROM mutes WHERE muter_id = $1 AND muted_id = $2`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // List blocked users
    router.get("/blocks", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT u.id, u.username, u.avatar_url, b.created_at
                 FROM blocks b
                 JOIN users u ON u.id = b.blocked_id
                 WHERE b.blocker_id = $1
                 ORDER BY b.created_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, blocked: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // List muted users
    router.get("/mutes", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT u.id, u.username, u.avatar_url, m.created_at
                 FROM mutes m
                 JOIN users u ON u.id = m.muted_id
                 WHERE m.muter_id = $1
                 ORDER BY m.created_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, muted: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // Check if user is blocked/muted by current user
    router.get("/status/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.json({ success: true, blocked: false, muted: false })
            }

            const blocked = await query(
                `SELECT 1 FROM blocks WHERE blocker_id = $1 AND blocked_id = $2 LIMIT 1`,
                [req.user.id, targetUserId]
            )
            const muted = await query(
                `SELECT 1 FROM mutes WHERE muter_id = $1 AND muted_id = $2 LIMIT 1`,
                [req.user.id, targetUserId]
            )

            res.json({
                success: true,
                blocked: blocked.rowCount > 0,
                muted: muted.rowCount > 0
            })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

// Admin moderation queue router
function createModerationAdminRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params),
        requireAdminMw = requireAdmin
    } = deps

    const router = Router()
    router.use(requireAdminMw)

    // List reports
    router.get("/reports", async (req, res, next) => {
        try {
            const limit = Math.min(Number(req.query.limit) || 50, 200)
            const status = cleanText(req.query.status, 20)

            let sql = `
                SELECT r.id, r.reporter_id, r.reported_id, r.reason, r.evidence,
                       r.status, r.server_id, r.match_id, r.created_at, r.reviewed_at,
                       reporter.username AS reporter_username,
                       reported.username AS reported_username
                FROM reports r
                JOIN users reporter ON reporter.id = r.reporter_id
                JOIN users reported ON reported.id = r.reported_id
            `
            const params = [limit]
            if (status) {
                sql += ` WHERE r.status = $2`
                params.push(status)
            }
            sql += ` ORDER BY r.created_at DESC LIMIT $1`

            const result = await query(sql, params)
            res.json({ success: true, reports: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // Update report status
    router.patch("/reports/:id", async (req, res, next) => {
        try {
            const reportId = Number(req.params.id)
            if (!reportId) {
                return res.status(400).json({ success: false, message: "invalid report" })
            }

            const validStatuses = ["new", "reviewing", "action_taken", "no_action", "duplicate", "escalated"]
            const status = cleanText(req.body.status, 20)
            if (!validStatuses.includes(status)) {
                return res.status(400).json({ success: false, message: "invalid status" })
            }

            await query(
                `UPDATE reports
                 SET status = $1, reviewed_by = $2, reviewed_at = NOW()
                 WHERE id = $3`,
                [status, req.user.id, reportId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Get report count by status
    router.get("/reports/counts", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT status, COUNT(*) AS count FROM reports GROUP BY status`
            )
            const counts = {}
            for (const row of result.rows) {
                counts[row.status] = Number(row.count)
            }
            res.json({ success: true, counts })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export { createModerationRouter, createModerationAdminRouter }
