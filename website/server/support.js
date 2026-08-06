// 08 03 2026, 04 10
/* purpose
* Support request system for the MiMITA website.
* Accept a support request with a required topic, save it, notify hello@mimita.fun,
* and expose an admin support view.
* DOES NOT render the support UI.
* DOES NOT require an account for guests.
* DOES NOT delete the request if the notification email fails.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { validateEmail, hashToken } from "./authCore.js"
import { requireAdmin } from "./admin.js"
import { createRateLimit } from "./rateLimit.js"
import { parseCookies, sessionCookieName, sessionSecret } from "./session.js"
import { sendSupportNotificationEmail } from "./mail.js"

export const SUPPORT_TOPICS = [
    { value: "user_issue", label: "Issue with another user" },
    { value: "game_issue", label: "Issue with the MiMITA game" },
    { value: "payment_finance", label: "Issue with payment or finance" },
    { value: "vip_purchase", label: "Issue with VIP purchase" },
    { value: "security", label: "Issue with security, hacks, or attempted hacks" },
    { value: "other", label: "Other" }
]

const TOPIC_VALUES = SUPPORT_TOPICS.map(t => t.value)

function cleanText(value, max) {
    return String(value || "").trim().slice(0, max)
}

function cleanPositiveId(value) {
    const n = Number(value)
    return Number.isInteger(n) && n > 0 ? n : null
}

export function validateSupportInput({ email, topic, subject, message, url, banner_order_id } = {}) {
    const cleanEmail = String(email || "").trim().toLowerCase()
    if (!validateEmail(cleanEmail).ok) {
        return { ok: false, error: "a valid email is required" }
    }

    if (!TOPIC_VALUES.includes(topic)) {
        return { ok: false, error: "topic is required" }
    }

    const cleanSubject = cleanText(subject, 150)
    if (!cleanSubject) {
        return { ok: false, error: "subject is required" }
    }

    const cleanMessage = cleanText(message, 5000)
    if (!cleanMessage) {
        return { ok: false, error: "message is required" }
    }

    let cleanUrl = ""
    if (url !== undefined && url !== null && String(url).trim() !== "") {
        const raw = String(url).trim()
        if (raw.length > 2048) {
            return { ok: false, error: "url is too long" }
        }
        try {
            const parsed = new URL(raw)
            if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
                return { ok: false, error: "url must use http or https" }
            }
            cleanUrl = parsed.href
        }
        catch {
            return { ok: false, error: "url must be a valid http(s) url" }
        }
    }

    const cleanOrderId = banner_order_id === undefined || banner_order_id === null || banner_order_id === ""
        ? null
        : cleanPositiveId(banner_order_id)

    return {
        ok: true,
        value: {
            email: cleanEmail,
            topic,
            subject: cleanSubject,
            message: cleanMessage,
            url: cleanUrl,
            banner_order_id: cleanOrderId
        }
    }
}

function createSupportRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params),
        mailFn = sendSupportNotificationEmail
    } = deps

    const router = Router()
    const supportRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "support" })

    async function optionalAuth(req, res, next) {
        const token = parseCookies(req)[sessionCookieName]
        if (!token) return next()
        try {
            const result = await query(
                `SELECT u.id, u.username, u.email
                 FROM sessions s
                 JOIN users u ON u.id = s.user_id
                 WHERE s.token_hash = $1
                   AND s.revoked_at IS NULL
                   AND s.expires_at > NOW()
                   AND u.deleted_at IS NULL
                 LIMIT 1`,
                [hashToken(token, sessionSecret)]
            )
            if (result.rows.length) req.user = result.rows[0]
        }
        catch {
            // guest submission still allowed
        }
        next()
    }

    router.post("/", supportRateLimit, optionalAuth, async (req, res, next) => {
        try {
            const validation = validateSupportInput(req.body)
            if (!validation.ok) {
                return res.status(400).json({ success: false, message: validation.error })
            }
            const v = validation.value

            if (v.banner_order_id) {
                const orderCheck = await query(
                    `SELECT 1 FROM banner_payment_orders WHERE id = $1`,
                    [v.banner_order_id]
                )
                if (!orderCheck.rows.length) {
                    return res.status(400).json({ success: false, message: "related banner order not found" })
                }
            }

            const email = req.user?.email || v.email
            const saved = await query(
                `INSERT INTO support_requests (user_id, email, topic, subject, message, url, banner_order_id)
                 VALUES ($1, $2, $3, $4, $5, $6, $7)
                 RETURNING id, created_at`,
                [req.user?.id || null, email, v.topic, v.subject, v.message, v.url, v.banner_order_id]
            )
            const requestId = saved.rows[0].id

            console.log(`[SUPPORT] support request created id=${requestId} topic=${v.topic} user=${req.user?.id || "guest"}`)

            res.status(201).json({
                success: true,
                message: "support request received",
                request_id: requestId
            })

            mailFn({
                requestId,
                topic: v.topic,
                username: req.user?.username || "guest",
                email,
                subject: v.subject,
                message: v.message,
                bannerOrderId: v.banner_order_id,
                createdAt: saved.rows[0].created_at
            })
                .then(() => console.log(`[SUPPORT] support email sent id=${requestId}`))
                .catch(err => console.log(`[SUPPORT] support email failed id=${requestId} error=${err?.message}`))
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

function createSupportAdminRouter(deps = {}) {
    const {
        query = (text, params) => pool.query(text, params),
        requireAdminMw = requireAdmin
    } = deps

    const router = Router()
    router.use(requireAdminMw)

    router.get("/", async (req, res, next) => {
        try {
            const topic = String(req.query.topic || "").trim()
            const limit = Math.min(Number(req.query.limit) || 50, 200)
            const params = [limit]
            let where = ""
            if (TOPIC_VALUES.includes(topic)) {
                where = "WHERE s.topic = $2"
                params.push(topic)
            }
            const result = await query(
                `SELECT s.id, s.user_id, s.email, s.topic, s.subject, s.message, s.url,
                        s.banner_order_id, s.status, s.priority, s.created_at, s.updated_at,
                        u.username
                 FROM support_requests s
                 LEFT JOIN users u ON u.id = s.user_id
                 ${where}
                 ORDER BY s.created_at DESC
                 LIMIT $1`,
                params
            )
            res.json({ success: true, requests: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    router.patch("/:id", async (req, res, next) => {
        try {
            const id = cleanPositiveId(req.params.id)
            if (!id) {
                return res.status(400).json({ success: false, message: "invalid request id" })
            }
            const validStatus = ["new", "open", "in_progress", "resolved"]
            const status = String(req.body?.status || "").trim()
            if (!validStatus.includes(status)) {
                return res.status(400).json({ success: false, message: "invalid status" })
            }
            await query(
                `UPDATE support_requests SET status = $1, updated_at = NOW() WHERE id = $2`,
                [status, id]
            )
            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export { createSupportRouter, createSupportAdminRouter }
