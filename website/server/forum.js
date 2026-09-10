// 09 09 2026
/* purpose
* Forum system for the MiMITA website.
* Public by default with privacy hooks for future private categories.
* Built on top of the generalized messaging foundation.
* DOES NOT render the forum UI.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { createRateLimit } from "./rateLimit.js"

function cleanText(value, max) {
    return String(value || "").trim().slice(0, max)
}

function slugify(text) {
    return String(text || "")
        .trim()
        .toLowerCase()
        .replace(/[^a-z0-9]+/g, "-")
        .replace(/^-+|-+$/g, "")
        .slice(0, 100) || "untitled"
}

function createForumRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params)
    } = deps

    const router = Router()

    const postRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 10, name: "forum_post" })
    const threadRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "forum_thread" })

    // Public: list categories
    router.get("/categories", async (req, res, next) => {
        try {
            const result = await query(
                `SELECT fc.id, fc.name, fc.slug, fc.description, fc.sort_order,
                        (SELECT COUNT(*) FROM forum_threads ft WHERE ft.category_id = fc.id) AS thread_count
                 FROM forum_categories fc
                 ORDER BY fc.sort_order ASC`
            )
            res.json({ success: true, categories: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    // Public: list threads in a category
    router.get("/categories/:slug/threads", async (req, res, next) => {
        try {
            const category = await query(
                `SELECT id, name, slug, description FROM forum_categories WHERE slug = $1`,
                [req.params.slug]
            )
            if (!category.rowCount) {
                return res.status(404).json({ success: false, message: "category not found" })
            }

            const limit = Math.min(Number(req.query.limit) || 30, 100)
            const page = Math.max(Number(req.query.page) || 1, 1)
            const offset = (page - 1) * limit

            const result = await query(
                `SELECT ft.id, ft.title, ft.reply_count, ft.pinned, ft.locked,
                        ft.last_reply_at, ft.created_at,
                        u.id AS author_id, u.username, u.avatar_url, u.supporter_tier, u.role
                 FROM forum_threads ft
                 JOIN users u ON u.id = ft.author_id
                 WHERE ft.category_id = $1
                 ORDER BY ft.pinned DESC, ft.last_reply_at DESC NULLS LAST, ft.created_at DESC
                 LIMIT $2 OFFSET $3`,
                [category.rows[0].id, limit, offset]
            )

            res.json({
                success: true,
                category: category.rows[0],
                threads: result.rows,
                page,
                limit
            })
        }
        catch (error) {
            next(error)
        }
    })

    // Public: get thread + posts
    router.get("/threads/:id", async (req, res, next) => {
        try {
            const threadId = Number(req.params.id)
            if (!threadId) {
                return res.status(400).json({ success: false, message: "invalid thread" })
            }

            const threadResult = await query(
                `SELECT ft.id, ft.title, ft.reply_count, ft.pinned, ft.locked,
                        ft.created_at,
                        u.id AS author_id, u.username, u.avatar_url, u.supporter_tier, u.role,
                        fc.name AS category_name, fc.slug AS category_slug
                 FROM forum_threads ft
                 JOIN users u ON u.id = ft.author_id
                 JOIN forum_categories fc ON fc.id = ft.category_id
                 WHERE ft.id = $1`,
                [threadId]
            )
            if (!threadResult.rowCount) {
                return res.status(404).json({ success: false, message: "thread not found" })
            }

            const limit = Math.min(Number(req.query.limit) || 50, 200)
            const postsResult = await query(
                `SELECT fp.id, fp.body, fp.edited_at, fp.created_at,
                        u.id AS author_id, u.username, u.avatar_url, u.supporter_tier, u.role
                 FROM forum_posts fp
                 JOIN users u ON u.id = fp.author_id
                 WHERE fp.thread_id = $1 AND fp.deleted_at IS NULL
                 ORDER BY fp.created_at ASC
                 LIMIT $2`,
                [threadId, limit]
            )

            // Get reactions for each post
            const postIds = postsResult.rows.map(p => p.id)
            let reactions = { rows: [] }
            if (postIds.length) {
                reactions = await query(
                    `SELECT post_id, user_id, emoji
                     FROM forum_reactions
                     WHERE post_id = ANY($1)`,
                    [postIds]
                )
            }

            const reactionsByPost = {}
            for (const r of reactions.rows) {
                if (!reactionsByPost[r.post_id]) reactionsByPost[r.post_id] = []
                reactionsByPost[r.post_id].push({ userId: r.user_id, emoji: r.emoji })
            }

            const posts = postsResult.rows.map(p => ({
                ...p,
                reactions: reactionsByPost[p.id] || []
            }))

            res.json({
                success: true,
                thread: threadResult.rows[0],
                posts
            })
        }
        catch (error) {
            next(error)
        }
    })

    // Create thread (auth required)
    router.post("/threads", authenticate, threadRateLimit, async (req, res, next) => {
        try {
            const categoryId = Number(req.body.categoryId)
            const title = cleanText(req.body.title, 200)
            const body = cleanText(req.body.body, 10000)

            if (!categoryId) {
                return res.status(400).json({ success: false, message: "category is required" })
            }
            if (!title) {
                return res.status(400).json({ success: false, message: "title is required" })
            }
            if (!body) {
                return res.status(400).json({ success: false, message: "body is required" })
            }

            const category = await query(
                `SELECT id FROM forum_categories WHERE id = $1`,
                [categoryId]
            )
            if (!category.rowCount) {
                return res.status(404).json({ success: false, message: "category not found" })
            }

            const threadResult = await query(
                `INSERT INTO forum_threads (category_id, author_id, title, last_reply_at)
                 VALUES ($1, $2, $3, NOW())
                 RETURNING id, title, created_at`,
                [categoryId, req.user.id, title]
            )
            const thread = threadResult.rows[0]

            await query(
                `INSERT INTO forum_posts (thread_id, author_id, body)
                 VALUES ($1, $2, $3)`,
                [thread.id, req.user.id, body]
            )

            res.status(201).json({ success: true, thread })
        }
        catch (error) {
            next(error)
        }
    })

    // Reply to thread (auth required)
    router.post("/threads/:id/posts", authenticate, postRateLimit, async (req, res, next) => {
        try {
            const threadId = Number(req.params.id)
            if (!threadId) {
                return res.status(400).json({ success: false, message: "invalid thread" })
            }

            const thread = await query(
                `SELECT id, locked FROM forum_threads WHERE id = $1`,
                [threadId]
            )
            if (!thread.rowCount) {
                return res.status(404).json({ success: false, message: "thread not found" })
            }
            if (thread.rows[0].locked) {
                return res.status(403).json({ success: false, message: "thread is locked" })
            }

            const body = cleanText(req.body.body, 10000)
            if (!body) {
                return res.status(400).json({ success: false, message: "body is required" })
            }

            const post = await query(
                `INSERT INTO forum_posts (thread_id, author_id, body)
                 VALUES ($1, $2, $3)
                 RETURNING id, body, created_at`,
                [threadId, req.user.id, body]
            )

            await query(
                `UPDATE forum_threads
                 SET reply_count = reply_count + 1, last_reply_at = NOW()
                 WHERE id = $1`,
                [threadId]
            )

            const result = {
                ...post.rows[0],
                author_id: req.user.id,
                username: req.user.username,
                avatar_url: req.user.avatar_url,
                supporter_tier: req.user.supporter_tier,
                role: req.user.role,
                reactions: []
            }

            res.status(201).json({ success: true, post: result })
        }
        catch (error) {
            next(error)
        }
    })

    // Add/toggle reaction on a post (auth required)
    router.post("/posts/:id/react", authenticate, async (req, res, next) => {
        try {
            const postId = Number(req.params.id)
            const emoji = cleanText(req.body.emoji, 10)
            if (!postId || !emoji) {
                return res.status(400).json({ success: false, message: "post id and emoji required" })
            }

            // Check if reaction exists
            const existing = await query(
                `SELECT id FROM forum_reactions WHERE post_id = $1 AND user_id = $2 AND emoji = $3`,
                [postId, req.user.id, emoji]
            )

            if (existing.rowCount) {
                // Remove reaction (toggle off)
                await query(
                    `DELETE FROM forum_reactions WHERE post_id = $1 AND user_id = $2 AND emoji = $3`,
                    [postId, req.user.id, emoji]
                )
                return res.json({ success: true, action: "removed" })
            }

            // Add reaction
            await query(
                `INSERT INTO forum_reactions (post_id, user_id, emoji)
                 VALUES ($1, $2, $3)`,
                [postId, req.user.id, emoji]
            )
            res.status(201).json({ success: true, action: "added" })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export { createForumRouter }
