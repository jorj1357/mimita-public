import crypto from "crypto"
import { Router } from "express"
import { getClientIp } from "./authCore.js"
import { pool } from "./db.js"
import { trackEvent } from "./analytics.js"
import { sendDataDeletionRequestEmail } from "./mail.js"

const router = Router()

const EVENT_CATALOG = new Map([
    ["account_created", "account"],
    ["login", "account"],
    ["logout", "account"],
    ["failed_login", "errors"],
    ["session_start", "gameplay"],
    ["session_end", "gameplay"],
    ["session_duration", "gameplay"],
    ["jump", "movement"],
    ["dash", "movement"],
    ["air_jump", "movement"],
    ["wall_jump", "movement"],
    ["map_loaded", "engagement"],
    ["map_completed", "engagement"],
    ["weapon_used", "engagement"],
    ["settings_opened", "ui"],
    ["outfit_editor_opened", "ui"],
    ["replay_viewed", "ui"],
    ["first_launch", "retention"],
    ["day_1_return", "retention"],
    ["day_7_return", "retention"],
    ["day_30_return", "retention"],
    ["crash_detected", "errors"],
    ["disconnect", "errors"],
    ["page_visit", "engagement"],
    ["page_view", "engagement"],
    ["download", "conversion"],
    ["game_open", "gameplay"],
    ["feedback_submit", "engagement"],
    ["donation", "conversion"],
    ["replay_save", "engagement"],
    ["replay_share", "social"],
    ["friend_request", "social"],
    ["discord_join", "social"]
])

const BLOCKED_KEYS = new Set([
    "password",
    "pass",
    "token",
    "session",
    "cookie",
    "chat",
    "message",
    "text"
])

function cleanText(value, max = 160) {
    return String(value || "").trim().slice(0, max)
}

function cleanEmail(value) {
    return cleanText(value, 254).toLowerCase()
}

function cleanUserId(value) {
    const n = Number(value)
    return Number.isInteger(n) && n > 0 ? n : null
}

function requestIpHash(req) {
    const secret = process.env.ANALYTICS_PRIVACY_SALT ||
        process.env.SESSION_SECRET ||
        "development-analytics-salt"
    return crypto
        .createHash("sha256")
        .update(secret)
        .update(":")
        .update(getClientIp(req))
        .digest("hex")
}

function sanitizeProperties(value, depth = 0) {
    if (!value || typeof value !== "object" || Array.isArray(value) || depth > 2) {
        return {}
    }

    const out = {}
    for (const [rawKey, rawValue] of Object.entries(value)) {
        const key = cleanText(rawKey, 48)
        if (!key || BLOCKED_KEYS.has(key.toLowerCase())) continue

        if (rawValue === null || rawValue === undefined) {
            continue
        }
        if (typeof rawValue === "number" && Number.isFinite(rawValue)) {
            out[key] = rawValue
        }
        else if (typeof rawValue === "boolean") {
            out[key] = rawValue
        }
        else if (typeof rawValue === "string") {
            out[key] = cleanText(rawValue, 240)
        }
        else if (typeof rawValue === "object" && !Array.isArray(rawValue)) {
            out[key] = sanitizeProperties(rawValue, depth + 1)
        }
    }
    return out
}

function normalizeGameEvent(raw) {
    const eventName = cleanText(raw.event_name || raw.name, 80)
    if (!EVENT_CATALOG.has(eventName)) return null

    const data = {
        source: "game",
        category: cleanText(raw.event_category || EVENT_CATALOG.get(eventName), 48),
        anonymous_id: cleanText(raw.anonymous_id, 96),
        session_id: cleanText(raw.session_id, 96),
        client_event_id: cleanText(raw.client_event_id, 128),
        username: cleanText(raw.username, 64),
        app_version: cleanText(raw.app_version, 64),
        occurred_at: cleanText(raw.occurred_at, 64),
        ...sanitizeProperties(raw.properties || raw.event_data || {})
    }

    return {
        eventName,
        userId: cleanUserId(raw.account_id || raw.user_id),
        data
    }
}

router.post("/events", async (req, res, next) => {
    try {
        const rawEvents = Array.isArray(req.body.events)
            ? req.body.events
            : [req.body]
        const events = rawEvents.slice(0, 64)
            .map(normalizeGameEvent)
            .filter(Boolean)

        for (const event of events) {
            await trackEvent(event.eventName, {
                event_data: event.data,
                user_id: event.userId,
                ip_address: null,
                page_url: null
            })
        }

        res.status(202).json({
            success: true,
            accepted: events.length,
            rejected: rawEvents.length - events.length
        })
    }
    catch (error) {
        next(error)
    }
})

router.post("/consent", async (req, res, next) => {
    try {
        const anonymousId = cleanText(req.body.anonymous_id, 96)
        if (!anonymousId) {
            return res.status(400).json({
                success: false,
                message: "anonymous_id required"
            })
        }

        const enabled = req.body.analytics_enabled !== false
        const permanentlyDisabled = req.body.permanently_disabled === true
        const userId = cleanUserId(req.body.account_id || req.body.user_id)
        const username = cleanText(req.body.username, 64)

        await pool.query(
            `
            INSERT INTO analytics_consent (
                anonymous_id, user_id, username, analytics_enabled,
                permanently_disabled, source, updated_at
            )
            VALUES ($1, $2, $3, $4, $5, 'game', NOW())
            ON CONFLICT (anonymous_id)
            DO UPDATE SET
                user_id = COALESCE(EXCLUDED.user_id, analytics_consent.user_id),
                username = EXCLUDED.username,
                analytics_enabled = EXCLUDED.analytics_enabled,
                permanently_disabled = EXCLUDED.permanently_disabled,
                updated_at = NOW()
            `,
            [anonymousId, userId, username, enabled, permanentlyDisabled]
        )

        await pool.query(
            `
            INSERT INTO analytics_audit_log (action, user_id, anonymous_id, details)
            VALUES ('consent_updated', $1, $2, $3)
            `,
            [
                userId,
                anonymousId,
                JSON.stringify({
                    analytics_enabled: enabled,
                    permanently_disabled: permanentlyDisabled,
                    source: "game",
                    ip_hash: requestIpHash(req)
                })
            ]
        )

        res.json({ success: true })
    }
    catch (error) {
        next(error)
    }
})

router.post("/deletion-request", async (req, res, next) => {
    try {
        const requestedAt = new Date().toISOString()
        const userId = cleanUserId(req.body.account_id || req.body.user_id)
        const anonymousId = cleanText(req.body.anonymous_id, 96)
        const username = cleanText(req.body.username, 64)
        const email = cleanEmail(req.body.email)
        const audit = {
            source: "game",
            ip_hash: requestIpHash(req),
            user_agent: cleanText(req.get("user-agent") || "", 180)
        }

        const result = await pool.query(
            `
            INSERT INTO analytics_deletion_requests (
                user_id, anonymous_id, username, email, source, audit
            )
            VALUES ($1, $2, $3, $4, 'game', $5)
            RETURNING id
            `,
            [userId, anonymousId || null, username, email, JSON.stringify(audit)]
        )

        await pool.query(
            `
            INSERT INTO analytics_audit_log (action, user_id, anonymous_id, details)
            VALUES ('deletion_requested', $1, $2, $3)
            `,
            [
                userId,
                anonymousId || null,
                JSON.stringify({ request_id: result.rows[0].id, ...audit })
            ]
        )

        await sendDataDeletionRequestEmail({
            accountId: userId,
            anonymousId,
            username,
            email,
            requestedAt
        })

        res.status(201).json({
            success: true,
            requestId: result.rows[0].id,
            message: "data deletion request received"
        })
    }
    catch (error) {
        next(error)
    }
})

export default router
export { EVENT_CATALOG }
