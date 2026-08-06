// 08 03 2026, 17 20
/* purpose
* Exposes VIP status, customization, presets, subscription-management, and join-ticket APIs.
* Uses the centralized entitlement and style validation services for all user-facing VIP state.
* Issues short-lived opaque multiplayer VIP tickets that dedicated servers can verify.
* DOES NOT create Stripe checkout sessions or process Stripe webhooks.
* DOES NOT render React pages or game UI.
* DOES NOT trust client-supplied VIP tier, role, badge, or style data.
*/

import { Router } from "express"
import { pool } from "./db.js"
import { authenticate, sessionSecret } from "./session.js"
import { hashToken, createSecretToken } from "./authCore.js"
import { getStripe, reconcilePendingCheckoutOrders, getVipOrders, refundOrder } from "./vip-payments.js"
import {
    publicVipConfig,
    safeStyleForTier,
    validateNameStyle,
    VIP_STYLE_LIMITS
} from "./vip-config.js"
import {
    getVipStateForUser,
    recordVipNotification,
    recomputeAndStoreVipForUser
} from "./vip-entitlements.js"

function queryFrom(clientOrQuery = pool) {
    if (typeof clientOrQuery === "function") return clientOrQuery
    return clientOrQuery.query.bind(clientOrQuery)
}

function vipApiLog(event, fields = {}) {
    console.log(`[VIP API] ${JSON.stringify({ event, ...fields })}`)
}

function getPortalStripe() {
    return getStripe()
}

function appOrigin(req) {
    return process.env.APP_ORIGIN || `${req.protocol}://${req.get("host")}`
}

function cleanPresetName(value) {
    return String(value || "").trim().slice(0, VIP_STYLE_LIMITS.maxPresetNameLength)
}

async function getPresetCount(query, userId) {
    const result = await query(
        `SELECT COUNT(*)::int AS count FROM vip_name_presets WHERE user_id = $1`,
        [userId]
    )
    return result.rows[0]?.count || 0
}

export function createVipRouter(deps = {}) {
    const router = Router()
    const query = queryFrom(deps.query || pool)
    const authenticateMw = deps.authenticateMw || authenticate
    const env = deps.env || process.env
    const stripeFactory = deps.stripeFactory || getPortalStripe
    const reconcileOrders = deps.reconcileOrders || reconcilePendingCheckoutOrders

    router.get("/config", (req, res) => {
        res.json({
            success: true,
            config: publicVipConfig(env)
        })
    })

    router.get("/me", authenticateMw, async (req, res, next) => {
        try {
            await reconcileOrders({ stripe: stripeFactory(), userId: req.user.id })
            await recomputeAndStoreVipForUser(query, req.user.id)
            const state = await getVipStateForUser(req.user, query)
            res.json({ success: true, vip: state })
        }
        catch (error) {
            next(error)
        }
    })

    router.get("/orders", authenticateMw, async (req, res, next) => {
        try {
            const orders = await getVipOrders({ query, userId: req.user.id })
            res.json({ success: true, orders })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/refund", authenticateMw, async (req, res) => {
        try {
            const orderId = Number(req.body.order_id || 0)
            const result = await refundOrder({
                stripe: stripeFactory(),
                getClient: () => pool.connect(),
                query,
                userId: req.user.id,
                orderId
            })
            res.json({ success: true, ...result })
        }
        catch (error) {
            res.status(400).json({ success: false, message: error.message })
        }
    })

    router.patch("/style", authenticateMw, async (req, res, next) => {
        try {
            const state = await getVipStateForUser(req.user, query)
            if (!state.controls_unlocked) {
                return res.status(403).json({ success: false, message: "VIP is not active" })
            }

            const validation = validateNameStyle(req.body.style || {}, {
                activeTier: state.active_tier,
                role: req.user.role || "user"
            })
            if (!validation.ok) {
                vipApiLog("style_rejected", {
                    user_id: req.user.id,
                    tier: state.active_tier,
                    role: req.user.role || "user",
                    reason: validation.message
                })
                return res.status(400).json({ success: false, message: validation.message })
            }

            await query(
                `INSERT INTO vip_name_styles (user_id, style_json, updated_at)
                 VALUES ($1, $2, NOW())
                 ON CONFLICT (user_id) DO UPDATE SET style_json = $2, updated_at = NOW()`,
                [req.user.id, JSON.stringify(validation.style)]
            )
            const nextState = await getVipStateForUser(req.user, query)
            vipApiLog("style_saved", {
                user_id: req.user.id,
                tier: nextState.active_tier,
                kind: validation.style.kind
            })
            res.json({ success: true, vip: nextState })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/style/reset", authenticateMw, async (req, res, next) => {
        try {
            const state = await getVipStateForUser(req.user, query)
            const style = safeStyleForTier(null, {
                activeTier: state.active_tier,
                role: req.user.role || "user"
            })
            await query(
                `INSERT INTO vip_name_styles (user_id, style_json, updated_at)
                 VALUES ($1, $2, NOW())
                 ON CONFLICT (user_id) DO UPDATE SET style_json = $2, updated_at = NOW()`,
                [req.user.id, JSON.stringify(style)]
            )
            res.json({ success: true, vip: await getVipStateForUser(req.user, query) })
        }
        catch (error) {
            next(error)
        }
    })

    router.get("/presets", authenticateMw, async (req, res, next) => {
        try {
            const result = await query(
                `SELECT id, name, style_json, created_at, updated_at
                 FROM vip_name_presets
                 WHERE user_id = $1
                 ORDER BY created_at DESC`,
                [req.user.id]
            )
            res.json({ success: true, presets: result.rows })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/presets", authenticateMw, async (req, res, next) => {
        try {
            const state = await getVipStateForUser(req.user, query)
            if (state.active_tier !== "ultra_vip") {
                return res.status(403).json({ success: false, message: "presets are for Ultra VIP" })
            }
            const count = await getPresetCount(query, req.user.id)
            if (count >= VIP_STYLE_LIMITS.maxPresetCount) {
                return res.status(400).json({ success: false, message: "preset limit reached" })
            }
            const name = cleanPresetName(req.body.name)
            if (!name) return res.status(400).json({ success: false, message: "preset name required" })
            const validation = validateNameStyle(req.body.style || {}, {
                activeTier: state.active_tier,
                role: req.user.role || "user"
            })
            if (!validation.ok) return res.status(400).json({ success: false, message: validation.message })

            const result = await query(
                `INSERT INTO vip_name_presets (user_id, name, style_json)
                 VALUES ($1, $2, $3)
                 RETURNING id, name, style_json, created_at, updated_at`,
                [req.user.id, name, JSON.stringify(validation.style)]
            )
            res.status(201).json({ success: true, preset: result.rows[0] })
        }
        catch (error) {
            next(error)
        }
    })

    router.patch("/presets/:id", authenticateMw, async (req, res, next) => {
        try {
            const state = await getVipStateForUser(req.user, query)
            if (state.active_tier !== "ultra_vip") {
                return res.status(403).json({ success: false, message: "presets are for Ultra VIP" })
            }
            const presetId = Number(req.params.id)
            if (!Number.isInteger(presetId) || presetId <= 0) {
                return res.status(400).json({ success: false, message: "invalid preset" })
            }
            const validation = validateNameStyle(req.body.style || {}, {
                activeTier: state.active_tier,
                role: req.user.role || "user"
            })
            if (!validation.ok) return res.status(400).json({ success: false, message: validation.message })
            const name = cleanPresetName(req.body.name)

            const result = await query(
                `UPDATE vip_name_presets
                 SET name = COALESCE(NULLIF($1, ''), name),
                     style_json = $2,
                     updated_at = NOW()
                 WHERE id = $3 AND user_id = $4
                 RETURNING id, name, style_json, created_at, updated_at`,
                [name, JSON.stringify(validation.style), presetId, req.user.id]
            )
            if (!result.rowCount) return res.status(404).json({ success: false, message: "preset not found" })
            res.json({ success: true, preset: result.rows[0] })
        }
        catch (error) {
            next(error)
        }
    })

    router.delete("/presets/:id", authenticateMw, async (req, res, next) => {
        try {
            const presetId = Number(req.params.id)
            await query(
                `DELETE FROM vip_name_presets WHERE id = $1 AND user_id = $2`,
                [presetId, req.user.id]
            )
            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/manage-subscription", authenticateMw, async (req, res, next) => {
        try {
            const stripe = stripeFactory()
            if (!stripe?.billingPortal?.sessions?.create) {
                return res.status(503).json({ success: false, message: "subscription portal is not configured" })
            }
            const result = await query(
                `SELECT COALESCE(
                    NULLIF(u.stripe_customer_id, ''),
                    (
                        SELECT NULLIF(s.stripe_customer_id, '')
                        FROM vip_subscriptions s
                        WHERE s.user_id = u.id
                          AND s.stripe_customer_id <> ''
                        ORDER BY s.updated_at DESC
                        LIMIT 1
                    )
                 ) AS stripe_customer_id
                 FROM users u
                 WHERE u.id = $1
                 LIMIT 1`,
                [req.user.id]
            )
            if (!result.rowCount || !result.rows[0].stripe_customer_id) {
                return res.status(404).json({ success: false, message: "you have no active subscription to manage" })
            }
            const session = await stripe.billingPortal.sessions.create({
                customer: result.rows[0].stripe_customer_id,
                return_url: `${appOrigin(req)}/vip`
            })
            res.json({ success: true, url: session.url })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/join-ticket", authenticateMw, async (req, res, next) => {
        try {
            const roomCode = String(req.body.room_code || "").trim().slice(0, 32)
            const token = createSecretToken()
            const tokenHash = hashToken(token, sessionSecret)
            await query(
                `INSERT INTO vip_join_tickets (
                    token_hash, user_id, room_code, expires_at
                 )
                 VALUES ($1, $2, $3, NOW() + INTERVAL '5 minutes')`,
                [tokenHash, req.user.id, roomCode]
            )
            vipApiLog("join_ticket_issued", {
                user_id: req.user.id,
                room_code: roomCode ? "present" : "empty",
            })
            res.json({
                success: true,
                join_ticket: token,
                expires_in_seconds: 300
            })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/verify-join-ticket", async (req, res, next) => {
        try {
            const token = String(req.body.join_ticket || "").trim()
            const roomCode = String(req.body.room_code || "").trim().slice(0, 32)
            if (!token) {
                return res.json({ success: true, verified: false, vip: null, reason: "missing_ticket" })
            }
            const tokenHash = hashToken(token, sessionSecret)
            const result = await query(
                `SELECT t.token_hash, t.user_id, t.room_code,
                        u.username, u.display_name, u.role
                 FROM vip_join_tickets t
                 JOIN users u ON u.id = t.user_id
                 WHERE t.token_hash = $1
                   AND t.expires_at > NOW()
                   AND t.used_at IS NULL
                   AND u.deleted_at IS NULL
                 LIMIT 1`,
                [tokenHash]
            )
            if (!result.rowCount) {
                return res.json({ success: true, verified: false, vip: null, reason: "invalid_or_expired" })
            }

            const row = result.rows[0]
            if ((row.room_code || roomCode) && row.room_code !== roomCode) {
                const payload = { verified: false, reason: "room_mismatch" }
                await query(
                    `UPDATE vip_join_tickets
                     SET used_at = NOW(), last_result = $1
                     WHERE token_hash = $2`,
                    [JSON.stringify(payload), tokenHash]
                )
                return res.json({ success: true, verified: false, vip: null, reason: "room_mismatch" })
            }

            const state = await getVipStateForUser({
                id: row.user_id,
                role: row.role
            }, query)
            const payload = {
                account_id: row.user_id,
                username: row.username,
                display_name: row.display_name || row.username,
                role: row.role || "user",
                vip: state
            }
            await query(
                `UPDATE vip_join_tickets
                 SET used_at = NOW(), last_result = $1
                 WHERE token_hash = $2`,
                [JSON.stringify(payload), tokenHash]
            )
            res.json({ success: true, verified: true, ...payload })
        }
        catch (error) {
            next(error)
        }
    })

    router.post("/notifications/ack", authenticateMw, async (req, res, next) => {
        try {
            const type = String(req.body.notification_type || "")
            const key = String(req.body.entitlement_key || "")
            const allowedTypes = new Set(["expires_7d", "expires_3d", "expires_1d", "expired"])
            if (!allowedTypes.has(type) || !key) {
                return res.status(400).json({ success: false, message: "invalid notification acknowledgement" })
            }
            await recordVipNotification(query, {
                userId: req.user.id,
                notificationType: type,
                entitlementKey: key,
                channel: "website"
            })
            res.json({ success: true })
        }
        catch (error) {
            next(error)
        }
    })

    return router
}
