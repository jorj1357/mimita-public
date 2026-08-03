// 08 02 2026, 23 10
/* purpose
* Own the community banner system for the MiMITA website.
* Validate banner content, run the overwrite/queue rules, expose the public
* banner, the free/report endpoints, and the admin banner management routes.
* DOES NOT create Stripe payment sessions or process webhooks (see banner-payments.js).
* DOES NOT render the banner UI (that lives in the React frontend).
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate } from "./session.js"
import { requireAdmin } from "./admin.js"
import { createRateLimit } from "./rateLimit.js"

export const BANNER_CONFIG = {
    price_per_day_usd: Number(process.env.BANNER_PRICE_PER_DAY_USD || 1),
    free_days: Number(process.env.BANNER_FREE_DAYS || 1),
    paid_min_days: Number(process.env.BANNER_PAID_MIN_DAYS || 2),
    paid_max_days: Number(process.env.BANNER_PAID_MAX_DAYS || 7),
    cooldown_minutes: Number(process.env.BANNER_USER_COOLDOWN_MINUTES || 5),
    admin_max_days: Number(process.env.BANNER_ADMIN_MAX_DAYS || 365)
}

export const PRICE_CENTS_PER_DAY = Math.round(BANNER_CONFIG.price_per_day_usd * 100)
export const FREE_KIND = "free"
export const PAID_KIND = "paid"
export const ADMIN_KIND = "admin"

const BANNER_ADVISORY_LOCK = 9271
const HEX_COLOR = /^#([0-9a-f]{3}|[0-9a-f]{6})$/i

export function validateBannerContent({ message, target_url, background_color, text_color } = {}) {
    const cleanMessage = String(message || "").trim()
    if (!cleanMessage) {
        return { ok: false, error: "message is required" }
    }
    if (cleanMessage.length > 280) {
        return { ok: false, error: "message must be 280 characters or fewer" }
    }

    let cleanUrl = ""
    if (target_url !== undefined && target_url !== null && String(target_url).trim() !== "") {
        const raw = String(target_url).trim()
        if (raw.length > 2048) {
            return { ok: false, error: "link is too long" }
        }
        let parsed
        try {
            parsed = new URL(raw)
        }
        catch {
            return { ok: false, error: "link must be a valid http(s) url" }
        }
        if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
            return { ok: false, error: "link must use http or https" }
        }
        if (parsed.username || parsed.password) {
            return { ok: false, error: "link cannot contain credentials" }
        }
        cleanUrl = parsed.href
    }

    const cleanBackground = String(background_color || "#000000").trim()
    const cleanText = String(text_color || "#ffffff").trim()
    if (!HEX_COLOR.test(cleanBackground)) {
        return { ok: false, error: "background color must be a hex color like #ff0000" }
    }
    if (!HEX_COLOR.test(cleanText)) {
        return { ok: false, error: "text color must be a hex color like #ffffff" }
    }

    return {
        ok: true,
        value: {
            message: cleanMessage,
            target_url: cleanUrl,
            background_color: cleanBackground,
            text_color: cleanText
        }
    }
}

const KIND_PRIORITY = { [ADMIN_KIND]: 3, [PAID_KIND]: 2, [FREE_KIND]: 1 }

export function decidePlacement(active, incoming) {
    if (!active) return "activate"
    const activePriority = KIND_PRIORITY[active.kind] || 0
    const incomingPriority = KIND_PRIORITY[incoming.kind] || 0
    if (incomingPriority > activePriority) return "overwrite"
    if (incomingPriority < activePriority) return "queue"
    if (incoming.kind === PAID_KIND) {
        return incoming.days > active.days ? "overwrite" : "queue"
    }
    return "overwrite"
}

export async function userOnCooldown(query, userId, minutes = BANNER_CONFIG.cooldown_minutes) {
    const result = await query(
        `SELECT 1 FROM site_banners
         WHERE user_id = $1
           AND created_at > NOW() - ($2 * INTERVAL '1 minute')
         LIMIT 1`,
        [userId, minutes]
    )
    return result.rows.length > 0
}

async function loadBannerForPlacement(client, bannerId) {
    const result = await client.query(
        `SELECT id, kind, days, status, payment_order_id
         FROM site_banners
         WHERE id = $1`,
        [bannerId]
    )
    return result.rows[0] || null
}

async function currentActive(client) {
    const result = await client.query(
        `SELECT id, kind, days FROM site_banners WHERE status = 'active' ORDER BY starts_at ASC LIMIT 1`
    )
    return result.rows[0] || null
}

async function makeActive(client, bannerId, days) {
    await client.query(
        `UPDATE site_banners
         SET status = 'active',
             starts_at = NOW(),
             expires_at = NOW() + ($2 * INTERVAL '1 day'),
             updated_at = NOW()
         WHERE id = $1
           AND status IN ('draft', 'pending_payment', 'queued')`,
        [bannerId, days]
    )
}

export async function placeBanner(client, bannerId) {
    const banner = await loadBannerForPlacement(client, bannerId)
    if (!banner) return { action: "missing" }

    const active = await currentActive(client)
    const action = decidePlacement(
        active ? { kind: active.kind, days: active.days } : null,
        { kind: banner.kind, days: banner.days }
    )

    if (action === "overwrite" && active) {
        await client.query(
            `UPDATE site_banners SET status = 'replaced', updated_at = NOW() WHERE id = $1`,
            [active.id]
        )
    }

    if (action === "queue") {
        await client.query(
            `UPDATE site_banners SET status = 'queued', updated_at = NOW()
             WHERE id = $1 AND status IN ('draft', 'pending_payment')`,
            [bannerId]
        )
        return { action: "queued" }
    }

    await makeActive(client, bannerId, banner.days)
    return { action: action === "overwrite" ? "overwrite" : "activated" }
}

export async function advanceBannerQueue(client) {
    await client.query(
        `UPDATE site_banners SET status = 'expired', updated_at = NOW()
         WHERE status = 'active' AND expires_at <= NOW()`
    )

    const active = await currentActive(client)
    if (active) return "already_active"

    const paidCandidate = await client.query(
        `SELECT id, days FROM site_banners
         WHERE status = 'queued' AND kind IN ('paid', 'admin')
         ORDER BY created_at ASC LIMIT 1`
    )
    if (paidCandidate.rows.length) {
        const row = paidCandidate.rows[0]
        const updated = await client.query(
            `UPDATE site_banners
             SET status = 'active',
                 starts_at = NOW(),
                 expires_at = NOW() + ($2 * INTERVAL '1 day'),
                 updated_at = NOW()
             WHERE id = $1 AND status = 'queued'`,
            [row.id, row.days]
        )
        if (updated.rowCount > 0) return "promoted"
    }

    const freeCandidate = await client.query(
        `SELECT id, days FROM site_banners
         WHERE status = 'queued' AND kind = 'free'
         ORDER BY created_at ASC LIMIT 1`
    )
    if (freeCandidate.rows.length) {
        const row = freeCandidate.rows[0]
        const updated = await client.query(
            `UPDATE site_banners
             SET status = 'active',
                 starts_at = NOW(),
                 expires_at = NOW() + ($2 * INTERVAL '1 day'),
                 updated_at = NOW()
             WHERE id = $1 AND status = 'queued'`,
            [row.id, row.days]
        )
        if (updated.rowCount > 0) return "promoted"
    }

    return "empty"
}

function cleanPositiveId(value) {
    const n = Number(value)
    return Number.isInteger(n) && n > 0 ? n : null
}

function publicBannerShape(row) {
    if (!row) return null
    return {
        id: row.id,
        message: row.message,
        target_url: row.target_url,
        background_color: row.background_color,
        text_color: row.text_color,
        username: row.username,
        created_at: row.created_at,
        expires_at: row.expires_at
    }
}

async function selectActiveBanner(query) {
    const result = await query(
        `SELECT b.id, b.message, b.target_url, b.background_color, b.text_color,
                u.username, b.created_at, b.expires_at
         FROM site_banners b
         JOIN users u ON u.id = b.user_id
         WHERE b.status = 'active' AND b.expires_at > NOW()
         ORDER BY b.starts_at ASC
         LIMIT 1`
    )
    return result.rows[0] || null
}

// ── Public router: GET /api/site/banner, GET /api/site/banner/pricing ──

function createSiteBannerPublicRouter(deps = {}) {
    const {
        getClient = () => pool.connect(),
        query = (text, params) => pool.query(text, params)
    } = deps

    const router = Router()

    router.get("/banner/pricing", (req, res) => {
        res.json({
            success: true,
            pricing: {
                price_per_day_usd: BANNER_CONFIG.price_per_day_usd,
                free_days: BANNER_CONFIG.free_days,
                paid_min_days: BANNER_CONFIG.paid_min_days,
                paid_max_days: BANNER_CONFIG.paid_max_days,
                admin_max_days: BANNER_CONFIG.admin_max_days,
                cooldown_minutes: BANNER_CONFIG.cooldown_minutes
            }
        })
    })

    router.get("/banner", async (req, res, next) => {
        try {
            const row = await selectActiveBanner(query)
            if (row) {
                return res.json({ success: true, banner: publicBannerShape(row) })
            }

            const client = await getClient()
            try {
                await client.query("BEGIN")
                await client.query("SELECT pg_advisory_xact_lock($1)", [BANNER_ADVISORY_LOCK])
                await advanceBannerQueue(client)
                const refreshed = await selectActiveBanner(t => client.query(t))
                await client.query("COMMIT")
                res.json({ success: true, banner: publicBannerShape(refreshed) })
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

    return router
}

// ── User router: POST /api/banner/free, POST /api/banner/report ──

function createSiteBannerUserRouter(deps = {}) {
    const {
        getClient = () => pool.connect(),
        query = (text, params) => pool.query(text, params),
        authenticateMw = authenticate
    } = deps

    const router = Router()
    const reportRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 5, name: "banner_report" })
    const freeRateLimit = createRateLimit({ windowMs: 60 * 1000, max: 3, name: "banner_free" })

    router.post("/free", freeRateLimit, authenticateMw, async (req, res, next) => {
        try {
            if (await userOnCooldown(query, req.user.id)) {
                return res.status(429).json({
                    success: false,
                    message: `you can create a banner once every ${BANNER_CONFIG.cooldown_minutes} minutes`
                })
            }

            const validation = validateBannerContent(req.body)
            if (!validation.ok) {
                return res.status(400).json({ success: false, message: validation.error })
            }
            const v = validation.value

            const client = await getClient()
            try {
                await client.query("BEGIN")
                const inserted = await client.query(
                    `INSERT INTO site_banners (user_id, kind, days, message, target_url, background_color, text_color, status)
                     VALUES ($1, $2, $3, $4, $5, $6, $7, 'draft')
                     RETURNING id`,
                    [req.user.id, FREE_KIND, BANNER_CONFIG.free_days, v.message, v.target_url, v.background_color, v.text_color]
                )
                const bannerId = inserted.rows[0].id
                const outcome = await placeBanner(client, bannerId)
                await client.query("COMMIT")
                res.status(201).json({ success: true, banner_id: bannerId, status: outcome.action })
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

    router.post("/report", reportRateLimit, authenticateMw, async (req, res, next) => {
        try {
            const bannerId = cleanPositiveId(req.body?.banner_id)
            if (!bannerId) {
                return res.status(400).json({ success: false, message: "banner_id required" })
            }

            const exists = await query(
                `SELECT 1 FROM site_banners WHERE id = $1`,
                [bannerId]
            )
            if (!exists.rows.length) {
                return res.status(404).json({ success: false, message: "banner not found" })
            }

            await query(
                `INSERT INTO banner_reports (banner_id, reporter_user_id)
                 VALUES ($1, $2)`,
                [bannerId, req.user.id]
            )
            res.status(201).json({ success: true, message: "banner reported" })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

// ── Admin router: mounted at /api/admin/banners ──

function createSiteBannerAdminRouter(deps = {}) {
    const {
        getClient = () => pool.connect(),
        query = (text, params) => pool.query(text, params),
        requireAdminMw = requireAdmin
    } = deps

    const router = Router()
    router.use(requireAdminMw)

    function listQuery() {
        return `
            SELECT b.*,
                   u.username AS owner_username,
                   o.duration_days AS order_duration_days,
                   o.amount_cents AS order_amount_cents,
                   o.currency AS order_currency,
                   o.status AS order_status,
                   o.stripe_checkout_session_id,
                   (SELECT COUNT(*) FROM banner_reports r WHERE r.banner_id = b.id) AS report_count
            FROM site_banners b
            JOIN users u ON u.id = b.user_id
            LEFT JOIN banner_payment_orders o ON o.id = b.payment_order_id
            ORDER BY b.created_at DESC`
    }

    router.get("/", async (req, res, next) => {
        try {
            const result = await query(listQuery())
            res.json({ success: true, banners: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/", async (req, res, next) => {
        try {
            const days = cleanPositiveId(req.body?.days)
            if (!days || days > BANNER_CONFIG.admin_max_days) {
                return res.status(400).json({ success: false, message: `days must be an integer from 1 to ${BANNER_CONFIG.admin_max_days}` })
            }
            const validation = validateBannerContent(req.body)
            if (!validation.ok) {
                return res.status(400).json({ success: false, message: validation.error })
            }
            const v = validation.value

            const client = await getClient()
            try {
                await client.query("BEGIN")
                await client.query("SELECT pg_advisory_xact_lock($1)", [BANNER_ADVISORY_LOCK])
                const inserted = await client.query(
                    `INSERT INTO site_banners (user_id, kind, days, message, target_url, background_color, text_color, status)
                     VALUES ($1, $2, $3, $4, $5, $6, $7, 'draft')
                     RETURNING id`,
                    [req.user.id, ADMIN_KIND, days, v.message, v.target_url, v.background_color, v.text_color]
                )
                const bannerId = inserted.rows[0].id
                const outcome = await placeBanner(client, bannerId)
                await client.query("COMMIT")
                res.status(201).json({ success: true, banner_id: bannerId, status: outcome.action })
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

    router.patch("/:id", async (req, res, next) => {
        try {
            const bannerId = cleanPositiveId(req.params.id)
            if (!bannerId) {
                return res.status(400).json({ success: false, message: "invalid banner id" })
            }
            const validation = validateBannerContent(req.body)
            if (!validation.ok) {
                return res.status(400).json({ success: false, message: validation.error })
            }
            const v = validation.value
            await query(
                `UPDATE site_banners
                 SET message = $1, target_url = $2, background_color = $3, text_color = $4, updated_at = NOW()
                 WHERE id = $5`,
                [v.message, v.target_url, v.background_color, v.text_color, bannerId]
            )
            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    router.patch("/:id/disable", async (req, res, next) => {
        try {
            const bannerId = cleanPositiveId(req.params.id)
            if (!bannerId) {
                return res.status(400).json({ success: false, message: "invalid banner id" })
            }
            const reason = String(req.body?.reason || "disabled by admin").trim().slice(0, 200)

            const client = await getClient()
            try {
                await client.query("BEGIN")
                await client.query("SELECT pg_advisory_xact_lock($1)", [BANNER_ADVISORY_LOCK])
                await client.query(
                    `UPDATE site_banners
                     SET status = 'disabled', disabled_by_admin_id = $1, disabled_reason = $2, updated_at = NOW()
                     WHERE id = $3`,
                    [req.user.id, reason, bannerId]
                )
                await advanceBannerQueue(client)
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

    router.delete("/:id", async (req, res, next) => {
        try {
            const bannerId = cleanPositiveId(req.params.id)
            if (!bannerId) {
                return res.status(400).json({ success: false, message: "invalid banner id" })
            }

            const client = await getClient()
            try {
                await client.query("BEGIN")
                await client.query("SELECT pg_advisory_xact_lock($1)", [BANNER_ADVISORY_LOCK])
                await client.query(
                    `UPDATE site_banners
                     SET status = 'deleted', disabled_by_admin_id = $1, disabled_reason = 'deleted by admin', updated_at = NOW()
                     WHERE id = $2`,
                    [req.user.id, bannerId]
                )
                await advanceBannerQueue(client)
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

    router.post("/advance", async (req, res, next) => {
        try {
            const client = await getClient()
            let active = null
            try {
                await client.query("BEGIN")
                await client.query("SELECT pg_advisory_xact_lock($1)", [BANNER_ADVISORY_LOCK])
                await advanceBannerQueue(client)
                const activeResult = await client.query(
                    `SELECT b.id, b.message, u.username FROM site_banners b
                     JOIN users u ON u.id = b.user_id
                     WHERE b.status = 'active' ORDER BY b.starts_at ASC LIMIT 1`
                )
                active = activeResult.rows[0] || null
                await client.query("COMMIT")
            }
            catch (error) {
                await client.query("ROLLBACK")
                throw error
            }
            finally {
                client.release()
            }
            res.json({ success: true, active })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}

export {
    createSiteBannerPublicRouter,
    createSiteBannerUserRouter,
    createSiteBannerAdminRouter
}
