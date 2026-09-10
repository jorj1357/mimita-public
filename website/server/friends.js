// 09 09 2026
/* purpose
* Friends system for the MiMITA website.
* Friend requests, accept/reject, removal, mutual friends, and friend stats.
* Built on top of the generalized messaging foundation.
* DOES NOT render the friends UI.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { createRateLimit } from "./rateLimit.js"

function createFriendsRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params)
    } = deps

    const router = Router()
    router.use(authenticate)

    const requestRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "friend_request" })

    // List friends (accepted)
    router.get("/", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT u.id, u.username, u.display_name, u.avatar_url, u.supporter_tier, u.role,
                        f.accepted_at,
                        gs.total_xp, gs.gold, gs.lifetime_player_kills, gs.lifetime_deaths
                 FROM friendships f
                 JOIN users u ON (
                     (u.id = f.addressee_id AND f.requester_id = $1)
                     OR (u.id = f.requester_id AND f.addressee_id = $1)
                 )
                 LEFT JOIN game_stats gs ON gs.user_id = u.id
                 WHERE f.status = 'accepted'
                   AND u.deleted_at IS NULL
                 ORDER BY f.accepted_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, friends: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // List pending requests (received)
    router.get("/requests", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT f.id, f.requester_id, f.created_at,
                        u.username, u.display_name, u.avatar_url, u.supporter_tier, u.role
                 FROM friendships f
                 JOIN users u ON u.id = f.requester_id
                 WHERE f.addressee_id = $1 AND f.status = 'pending'
                 ORDER BY f.created_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, requests: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // List sent requests (pending outbound)
    router.get("/sent", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT f.id, f.addressee_id, f.created_at,
                        u.username, u.display_name, u.avatar_url, u.supporter_tier, u.role
                 FROM friendships f
                 JOIN users u ON u.id = f.addressee_id
                 WHERE f.requester_id = $1 AND f.status = 'pending'
                 ORDER BY f.created_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, sent: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // Send friend request
    router.post("/request/:userId", requestRateLimit, async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId || targetUserId === req.user.id) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            // Check target exists
            const target = await query(
                `SELECT id, username FROM users WHERE id = $1 AND deleted_at IS NULL`,
                [targetUserId]
            )
            if (!target.rowCount) {
                return res.status(404).json({ success: false, message: "user not found" })
            }

            // Check blocked
            const blocked = await query(
                `SELECT 1 FROM blocks
                 WHERE (blocker_id = $1 AND blocked_id = $2)
                    OR (blocker_id = $2 AND blocked_id = $1)
                 LIMIT 1`,
                [req.user.id, targetUserId]
            )
            if (blocked.rowCount) {
                return res.status(403).json({ success: false, message: "cannot add this user" })
            }

            // Check existing friendship (any direction)
            const existing = await query(
                `SELECT id, status FROM friendships
                 WHERE (requester_id = $1 AND addressee_id = $2)
                    OR (requester_id = $2 AND addressee_id = $1)`,
                [req.user.id, targetUserId]
            )

            if (existing.rowCount) {
                const s = existing.rows[0].status
                if (s === "accepted") {
                    return res.status(409).json({ success: false, message: "already friends" })
                }
                if (s === "pending") {
                    // Auto-accept if the other person already sent a request
                    if (existing.rows[0].id) {
                        const reverse = await query(
                            `SELECT id FROM friendships WHERE requester_id = $1 AND addressee_id = $2 AND status = 'pending'`,
                            [targetUserId, req.user.id]
                        )
                        if (reverse.rowCount) {
                            await query(
                                `UPDATE friendships SET status = 'accepted', accepted_at = NOW() WHERE id = $1`,
                                [reverse.rows[0].id]
                            )
                            return res.json({ success: true, action: "accepted", friendshipId: reverse.rows[0].id })
                        }
                    }
                    return res.status(409).json({ success: false, message: "request already pending" })
                }
                if (s === "blocked") {
                    return res.status(403).json({ success: false, message: "cannot add this user" })
                }
            }

            const result = await query(
                `INSERT INTO friendships (requester_id, addressee_id, status)
                 VALUES ($1, $2, 'pending')
                 RETURNING id, created_at`,
                [req.user.id, targetUserId]
            )

            res.status(201).json({
                success: true,
                action: "requested",
                friendshipId: result.rows[0].id
            })
        }
        catch (error) {
            next(error)
        }
    })

    // Accept friend request
    router.post("/accept/:friendshipId", async (req, res, next) => {
        try {
            const friendshipId = Number(req.params.friendshipId)
            if (!friendshipId) {
                return res.status(400).json({ success: false, message: "invalid request" })
            }

            const result = await query(
                `UPDATE friendships
                 SET status = 'accepted', accepted_at = NOW()
                 WHERE id = $1 AND addressee_id = $2 AND status = 'pending'
                 RETURNING id`,
                [friendshipId, req.user.id]
            )
            if (!result.rowCount) {
                return res.status(404).json({ success: false, message: "request not found" })
            }

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Reject friend request
    router.post("/reject/:friendshipId", async (req, res, next) => {
        try {
            const friendshipId = Number(req.params.friendshipId)
            if (!friendshipId) {
                return res.status(400).json({ success: false, message: "invalid request" })
            }

            const result = await query(
                `DELETE FROM friendships
                 WHERE id = $1 AND addressee_id = $2 AND status = 'pending'`,
                [friendshipId, req.user.id]
            )
            if (!result.rowCount) {
                return res.status(404).json({ success: false, message: "request not found" })
            }

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Remove friend
    router.delete("/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            await query(
                `DELETE FROM friendships
                 WHERE status = 'accepted'
                   AND ((requester_id = $1 AND addressee_id = $2)
                     OR (requester_id = $2 AND addressee_id = $1))`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    // Get mutual friends with a user
    router.get("/mutual/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            const result = await query(
                `SELECT u.id, u.username, u.avatar_url, u.supporter_tier
                 FROM friendships f1
                 JOIN users u ON (
                     (u.id = f1.addressee_id AND f1.requester_id = $1)
                     OR (u.id = f1.requester_id AND f1.addressee_id = $1)
                 )
                 WHERE f1.status = 'accepted'
                   AND u.deleted_at IS NULL
                   AND u.id IN (
                       SELECT friend_id FROM (
                           SELECT addressee_id AS friend_id FROM friendships WHERE requester_id = $2 AND status = 'accepted'
                           UNION
                           SELECT requester_id AS friend_id FROM friendships WHERE addressee_id = $2 AND status = 'accepted'
                       ) my_friends
                       WHERE friend_id IN (
                           SELECT addressee_id FROM friendships WHERE requester_id = $2 AND status = 'accepted'
                           UNION
                           SELECT requester_id FROM friendships WHERE addressee_id = $2 AND status = 'accepted'
                       )
                   )
                 LIMIT 20`,
                [req.user.id, targetUserId]
            )

            res.json({ success: true, mutualFriends: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // Check friendship status with a user
    router.get("/status/:userId", async (req, res, next) => {
        try {
            const targetUserId = Number(req.params.userId)
            if (!targetUserId) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            const result = await query(
                `SELECT status, requester_id FROM friendships
                 WHERE (requester_id = $1 AND addressee_id = $2)
                    OR (requester_id = $2 AND addressee_id = $1)
                 LIMIT 1`,
                [req.user.id, targetUserId]
            )

            if (!result.rowCount) {
                return res.json({ success: true, status: "none" })
            }

            const row = result.rows[0]
            res.json({
                success: true,
                status: row.status,
                direction: row.requester_id === req.user.id ? "outgoing" : "incoming"
            })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export { createFriendsRouter }
