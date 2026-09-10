// 09 09 2026
/* purpose
* Generalized messaging function for the MiMITA website.
* Supports DMs, forum replies, moderator messages, and any future channel.
* The foundation that unlocks forum, DMs, and notification systems.
* DOES NOT render the messaging UI.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { createRateLimit } from "./rateLimit.js"

function cleanText(value, max) {
    return String(value || "").trim().slice(0, max)
}

function createMessagesRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params)
    } = deps

    const router = Router()
    router.use(authenticate)

    const sendRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 30, name: "messages_send" })

    // Create or get DM conversation with another user
    router.post("/conversations/dm", async (req, res, next) => {
        try {
            const otherUserId = Number(req.body.userId)
            if (!otherUserId || otherUserId === req.user.id) {
                return res.status(400).json({ success: false, message: "invalid user" })
            }

            // Check if blocked
            const blocked = await query(
                `SELECT 1 FROM blocks
                 WHERE (blocker_id = $1 AND blocked_id = $2)
                    OR (blocker_id = $2 AND blocked_id = $1)
                 LIMIT 1`,
                [req.user.id, otherUserId]
            )
            if (blocked.rowCount) {
                return res.status(403).json({ success: false, message: "cannot message this user" })
            }

            // Check other user exists
            const targetUser = await query(
                `SELECT id, username FROM users WHERE id = $1 AND deleted_at IS NULL`,
                [otherUserId]
            )
            if (!targetUser.rowCount) {
                return res.status(404).json({ success: false, message: "user not found" })
            }

            // Find existing DM conversation
            const existing = await query(
                `SELECT c.id FROM conversations c
                 JOIN conversation_members m1 ON m1.conversation_id = c.id AND m1.user_id = $1
                 JOIN conversation_members m2 ON m2.conversation_id = c.id AND m2.user_id = $2
                 WHERE c.type = 'dm'`,
                [req.user.id, otherUserId]
            )

            if (existing.rowCount) {
                return res.json({ success: true, conversationId: existing.rows[0].id })
            }

            // Create new DM conversation
            const conv = await query(
                `INSERT INTO conversations (type) VALUES ('dm') RETURNING id`
            )
            const conversationId = conv.rows[0].id

            await query(
                `INSERT INTO conversation_members (conversation_id, user_id) VALUES ($1, $2), ($1, $3)`,
                [conversationId, req.user.id, otherUserId]
            )

            res.status(201).json({ success: true, conversationId })
        }
        catch (error) {
            next(error)
        }
    })

    // List user's conversations
    router.get("/conversations", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT c.id, c.type, c.created_at,
                        last_msg.body AS last_message,
                        last_msg.created_at AS last_message_at,
                        last_msg.sender_id AS last_message_sender_id,
                        (SELECT COUNT(*) FROM messages m
                         WHERE m.conversation_id = c.id
                           AND m.sender_id != $1
                           AND m.read_at IS NULL) AS unread_count
                 FROM conversations c
                 JOIN conversation_members cm ON cm.conversation_id = c.id AND cm.user_id = $1
                 LEFT JOIN LATERAL (
                     SELECT m.body, m.created_at, m.sender_id
                     FROM messages m
                     WHERE m.conversation_id = c.id
                     ORDER BY m.created_at DESC
                     LIMIT 1
                 ) last_msg ON TRUE
                 ORDER BY COALESCE(last_msg.created_at, c.created_at) DESC`,
                [req.user.id]
            )

            // For DMs, attach the other user's info
            const conversations = await Promise.all(result.rows.map(async (conv) => {
                if (conv.type === "dm") {
                    const other = await query(
                        `SELECT u.id, u.username, u.avatar_url, u.supporter_tier
                         FROM conversation_members cm
                         JOIN users u ON u.id = cm.user_id
                         WHERE cm.conversation_id = $1 AND cm.user_id != $2`,
                        [conv.id, req.user.id]
                    )
                    conv.otherUser = other.rows[0] || null
                }
                return conv
            }))

            res.json({ success: true, conversations })
        }
        catch (error) {
            next(error)
        }
    })

    // Get messages in a conversation
    router.get("/conversations/:id", async (req, res, next) => {
        try {
            const conversationId = Number(req.params.id)
            if (!conversationId) {
                return res.status(400).json({ success: false, message: "invalid conversation" })
            }

            // Verify membership
            const member = await query(
                `SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2`,
                [conversationId, req.user.id]
            )
            if (!member.rowCount) {
                return res.status(403).json({ success: false, message: "not a member" })
            }

            const limit = Math.min(Number(req.query.limit) || 50, 200)
            const before = req.query.before

            let sql = `
                SELECT m.id, m.sender_id, m.body, m.read_at, m.created_at,
                       u.username, u.avatar_url, u.supporter_tier, u.role
                FROM messages m
                JOIN users u ON u.id = m.sender_id
                WHERE m.conversation_id = $1
            `
            const params = [conversationId]

            if (before) {
                params.push(Number(before))
                sql += ` AND m.id < $2`
            }

            sql += ` ORDER BY m.created_at DESC LIMIT $${params.length + 1}`
            params.push(limit)

            const result = await query(sql, params)

            // Mark as read
            await query(
                `UPDATE conversation_members
                 SET last_read_at = NOW()
                 WHERE conversation_id = $1 AND user_id = $2`,
                [conversationId, req.user.id]
            )
            await query(
                `UPDATE messages
                 SET read_at = NOW()
                 WHERE conversation_id = $1 AND sender_id != $2 AND read_at IS NULL`,
                [conversationId, req.user.id]
            )

            res.json({ success: true, messages: result.rows.reverse() })
        }
        catch (error) {
            next(error)
        }
    })

    // Send a message
    router.post("/conversations/:id", sendRateLimit, async (req, res, next) => {
        try {
            const conversationId = Number(req.params.id)
            if (!conversationId) {
                return res.status(400).json({ success: false, message: "invalid conversation" })
            }

            const body = cleanText(req.body.body, 2000)
            if (!body) {
                return res.status(400).json({ success: false, message: "message is required" })
            }

            // Verify membership
            const member = await query(
                `SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2`,
                [conversationId, req.user.id]
            )
            if (!member.rowCount) {
                return res.status(403).json({ success: false, message: "not a member" })
            }

            // Check if blocked by other member
            const otherMembers = await query(
                `SELECT user_id FROM conversation_members WHERE conversation_id = $1 AND user_id != $2`,
                [conversationId, req.user.id]
            )
            for (const other of otherMembers.rows) {
                const blocked = await query(
                    `SELECT 1 FROM blocks WHERE blocker_id = $1 AND blocked_id = $2 LIMIT 1`,
                    [other.user_id, req.user.id]
                )
                if (blocked.rowCount) {
                    return res.status(403).json({ success: false, message: "cannot send message" })
                }
            }

            const result = await query(
                `INSERT INTO messages (conversation_id, sender_id, body)
                 VALUES ($1, $2, $3)
                 RETURNING id, sender_id, body, read_at, created_at`,
                [conversationId, req.user.id, body]
            )

            const message = result.rows[0]
            message.username = req.user.username
            message.avatar_url = req.user.avatar_url
            message.supporter_tier = req.user.supporter_tier
            message.role = req.user.role

            res.status(201).json({ success: true, message })
        }
        catch (error) {
            next(error)
        }
    })

    // Get unread count (for nav badge)
    router.get("/unread-count", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT COUNT(*) AS count
                 FROM messages m
                 JOIN conversation_members cm ON cm.conversation_id = m.conversation_id
                 WHERE cm.user_id = $1
                   AND m.sender_id != $1
                   AND m.read_at IS NULL`,
                [req.user.id]
            )
            res.json({ success: true, count: Number(result.rows[0].count) })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export { createMessagesRouter }
