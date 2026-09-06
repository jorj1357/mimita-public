// 08 03 2026, 17 20
/* purpose
* Owns server-side VIP entitlement calculation, calendar extension, and account cache updates.
* Converts persisted VIP purchases, subscriptions, and saved styles into one normalized VIP state.
* Provides database helpers for payment webhooks, website APIs, game APIs, and join tickets.
* DOES NOT create Stripe checkout sessions or verify webhook signatures.
* DOES NOT render website or game UI.
* DOES NOT trust client-supplied VIP tier or style data.
*/

import { pool } from "./db.js"
import {
    badgeForTier,
    defaultStyleForTier,
    normalizeStaffDisplay,
    normalizeTier,
    safeStyleForTier,
    staffDisplayColor,
    staffStyleForRole,
    tierIncludes,
    tierRank,
    VIP_STYLE_KINDS
} from "./vip-config.js"

export const ACTIVE_SUBSCRIPTION_STATUSES = new Set(["active", "trialing", "past_due"])

function queryFrom(clientOrQuery = pool) {
    if (typeof clientOrQuery === "function") return clientOrQuery
    return clientOrQuery.query.bind(clientOrQuery)
}

function toDate(value) {
    if (!value) return null
    const date = value instanceof Date ? value : new Date(value)
    return Number.isFinite(date.getTime()) ? date : null
}

function iso(value) {
    const date = toDate(value)
    return date ? date.toISOString() : null
}

function daysInUtcMonth(year, month) {
    return new Date(Date.UTC(year, month + 1, 0)).getUTCDate()
}

export function addUtcCalendarMonths(input, months) {
    const source = toDate(input) || new Date()
    const wholeMonths = Number(months)
    if (!Number.isInteger(wholeMonths) || wholeMonths <= 0) {
        throw new Error("months must be a positive integer")
    }

    const year = source.getUTCFullYear()
    const month = source.getUTCMonth()
    const targetMonthIndex = month + wholeMonths
    const targetYear = year + Math.floor(targetMonthIndex / 12)
    const targetMonth = ((targetMonthIndex % 12) + 12) % 12
    const targetDay = Math.min(source.getUTCDate(), daysInUtcMonth(targetYear, targetMonth))

    return new Date(Date.UTC(
        targetYear,
        targetMonth,
        targetDay,
        source.getUTCHours(),
        source.getUTCMinutes(),
        source.getUTCSeconds(),
        source.getUTCMilliseconds()
    ))
}

function currentEntitlement(row, now) {
    const startsAt = toDate(row.starts_at)
    const expiresAt = toDate(row.expires_at)
    return row.status === "active" &&
        startsAt && startsAt <= now &&
        expiresAt && expiresAt > now
}

function liveOrFutureEntitlement(row, now) {
    const expiresAt = toDate(row.expires_at)
    return row.status === "active" &&
        expiresAt && expiresAt > now
}

function activeSubscription(row, now) {
    const end = toDate(row.current_period_end)
    return ACTIVE_SUBSCRIPTION_STATUSES.has(String(row.status || "")) &&
        end && end > now
}

function unlockedStyles(activeTier) {
    return Object.entries(VIP_STYLE_KINDS)
        .filter(([, def]) => tierIncludes(activeTier, def.minTier))
        .map(([kind]) => kind)
}

function expirationWarnings(expiresAt, now) {
    const expires = toDate(expiresAt)
    if (!expires) return []
    const remainingMs = expires.getTime() - now.getTime()
    if (remainingMs <= 0) return ["expired"]
    const remainingDays = remainingMs / 86400000
    const warnings = []
    if (remainingDays <= 7) warnings.push("expires_7d")
    if (remainingDays <= 3) warnings.push("expires_3d")
    if (remainingDays <= 1) warnings.push("expires_1d")
    return warnings
}

export function computeVipState({
    user = {},
    entitlements = [],
    subscriptions = [],
    style = null,
    presetCount = 0,
    now = new Date()
} = {}) {
    const current = toDate(now) || new Date()
    const visibleRows = []
    const extensionRows = []

    for (const row of entitlements || []) {
        if (liveOrFutureEntitlement(row, current)) {
            extensionRows.push({
                tier: normalizeTier(row.tier),
                expires_at: toDate(row.expires_at),
                source: row.source || "stripe"
            })
        }
        if (currentEntitlement(row, current)) {
            visibleRows.push({
                tier: normalizeTier(row.tier),
                expires_at: toDate(row.expires_at),
                source: row.source || "stripe"
            })
        }
    }

    for (const row of subscriptions || []) {
        if (activeSubscription(row, current)) {
            visibleRows.push({
                tier: normalizeTier(row.tier),
                expires_at: toDate(row.current_period_end),
                source: "subscription"
            })
            extensionRows.push({
                tier: normalizeTier(row.tier),
                expires_at: toDate(row.current_period_end),
                source: "subscription"
            })
        }
    }

    let activeTier = "free"
    for (const row of visibleRows) {
        if (tierRank(row.tier) > tierRank(activeTier)) activeTier = row.tier
    }

    let expiresAt = null
    for (const row of extensionRows) {
        if (row.tier !== activeTier) continue
        if (!expiresAt || row.expires_at > expiresAt) expiresAt = row.expires_at
    }

    const staff = staffStyleForRole(user.role)
    const storedStyle = style?.style_json || style || null
    const nameStyle = safeStyleForTier(storedStyle, {
        activeTier,
        role: user.role || "user"
    })
    const staffDisplay = normalizeStaffDisplay(storedStyle?.staff_display, user.role || "user")
    const staffDisplayColorValue = staffDisplayColor(staffDisplay)
    const useStaffColor = Boolean(staffDisplayColorValue)
    nameStyle.staff_display = staffDisplay

    let subscription = null
    for (const row of subscriptions || []) {
        const candidate = {
            tier: normalizeTier(row.tier),
            status: row.status || "none",
            current_period_start: iso(row.current_period_start),
            current_period_end: iso(row.current_period_end),
            cancel_at_period_end: row.cancel_at_period_end === true,
            stripe_subscription_id: row.stripe_subscription_id || ""
        }
        if (!subscription ||
            tierRank(candidate.tier) > tierRank(subscription.tier) ||
            toDate(candidate.current_period_end) > toDate(subscription.current_period_end)) {
            subscription = candidate
        }
    }

    return {
        active: activeTier !== "free",
        active_tier: activeTier,
        badge_url: badgeForTier(activeTier),
        expires_at: iso(expiresAt),
        server_time: current.toISOString(),
        controls_unlocked: activeTier !== "free",
        allowed_styles: unlockedStyles(activeTier),
        name_style: nameStyle,
        default_style: defaultStyleForTier(activeTier),
        staff_style: staff,
        display: {
            name_color_override: useStaffColor ? staffDisplayColorValue : "",
            staff_overrides_vip_name: useStaffColor
        },
        subscription,
        preset_count: Number(presetCount) || 0,
        warnings_due: expirationWarnings(expiresAt, current),
        style_revision: style?.style_revision || 1
    }
}

export async function getVipStateForUser(user, clientOrQuery = pool, now = new Date()) {
    const query = queryFrom(clientOrQuery)
    const batch = Array.isArray(user)
    if (batch && !user.length) return []
    const userId = typeof user === "object" ? user.id : user
    const role = typeof user === "object" ? user.role : "user"
    const ids = batch ? [...new Set(user.map(row => String(row.id)))] : userId
    const selectUser = batch ? "user_id, " : ""
    const predicate = batch ? "user_id = ANY($1::bigint[])" : "user_id = $1"

    const [entitlements, subscriptions, style, presets] = await Promise.all([
        query(
            `SELECT ${selectUser}tier, source, status, starts_at, expires_at,
                    stripe_subscription_id, stripe_checkout_session_id
             FROM vip_entitlements
             WHERE ${predicate}`,
            [ids]
        ),
        query(
            `SELECT ${selectUser}tier, status, current_period_start, current_period_end,
                    cancel_at_period_end, stripe_subscription_id
             FROM vip_subscriptions
             WHERE ${predicate}`,
            [ids]
        ),
        query(
            `SELECT ${selectUser}style_json, style_revision FROM vip_name_styles WHERE ${predicate}`,
            [ids]
        ),
        query(
            `SELECT ${selectUser}COUNT(*)::int AS count FROM vip_name_presets WHERE ${predicate}${batch ? " GROUP BY user_id" : ""}`,
            [ids]
        )
    ])

    if (batch) {
        const groups = [entitlements, subscriptions, style, presets].map(result => {
            const grouped = new Map()
            for (const row of result.rows) {
                const id = String(row.user_id)
                if (!grouped.has(id)) grouped.set(id, [])
                grouped.get(id).push(row)
            }
            return grouped
        })
        return user.map(row => {
            const id = String(row.id)
            return computeVipState({ user: row, now,
                entitlements: groups[0].get(id) || [], subscriptions: groups[1].get(id) || [],
                style: groups[2].get(id)?.[0] || null, presetCount: groups[3].get(id)?.[0]?.count || 0 })
        })
    }
    return computeVipState({
        user: { id: userId, role },
        entitlements: entitlements.rows,
        subscriptions: subscriptions.rows,
        style: style.rows[0] || null,
        presetCount: presets.rows[0]?.count || 0,
        now
    })
}

export async function updateSupporterTierCache(clientOrQuery, userId, state) {
    const query = queryFrom(clientOrQuery)
    await query(
        `UPDATE users SET supporter_tier = $1, updated_at = NOW() WHERE id = $2`,
        [state.active_tier, userId]
    )
}

export async function recomputeAndStoreVipForUser(clientOrQuery, userId, now = new Date()) {
    const query = queryFrom(clientOrQuery)
    const userResult = await query(
        `SELECT id, role FROM users WHERE id = $1 AND deleted_at IS NULL LIMIT 1`,
        [userId]
    )
    if (!userResult.rowCount) return null
    const state = await getVipStateForUser(userResult.rows[0], query, now)
    await updateSupporterTierCache(query, userId, state)
    return state
}

export async function expirePastEntitlements(clientOrQuery, now = new Date()) {
    const query = queryFrom(clientOrQuery)
    const expired = await query(
        `UPDATE vip_entitlements
         SET status = 'expired', updated_at = NOW()
         WHERE status = 'active' AND expires_at <= $1
         RETURNING id, user_id, expires_at`,
        [now]
    )
    const userIds = new Set(expired.rows?.map(row => row.user_id).filter(Boolean) || [])
    for (const row of expired.rows || []) {
        await recordVipNotification(query, {
            userId: row.user_id,
            notificationType: "expired",
            entitlementKey: `${row.id}:${iso(row.expires_at)}`,
            channel: "website"
        })
    }
    for (const userId of userIds) {
        await recomputeAndStoreVipForUser(query, userId, now)
    }
    return expired.rowCount || 0
}

export async function recordDueVipNotifications(clientOrQuery, now = new Date()) {
    const query = queryFrom(clientOrQuery)
    const active = await query(
        `SELECT id, user_id, expires_at
         FROM vip_entitlements
         WHERE status = 'active'
           AND expires_at > $1
           AND expires_at <= ($1::timestamptz + INTERVAL '7 days')`,
        [now]
    )
    let recorded = 0
    for (const row of active.rows || []) {
        const key = `${row.id}:${iso(row.expires_at)}`
        for (const warning of expirationWarnings(row.expires_at, toDate(now) || new Date())) {
            if (warning === "expired") continue
            await recordVipNotification(query, {
                userId: row.user_id,
                notificationType: warning,
                entitlementKey: key,
                channel: "website"
            })
            recorded++
        }
    }
    return recorded
}

export async function runVipReconcile(clientOrQuery = pool, now = new Date()) {
    const query = queryFrom(clientOrQuery)
    const expired = await expirePastEntitlements(query, now)
    const notifications = await recordDueVipNotifications(query, now)
    return { expired, notifications }
}

export async function grantPrepaidEntitlement(clientOrQuery, {
    userId,
    orderId = null,
    tier,
    purchaseType,
    months,
    source = "stripe",
    stripeCheckoutSessionId = "",
    stripePaymentIntentId = "",
    stripeCustomerId = "",
    now = new Date()
}) {
    const query = queryFrom(clientOrQuery)
    const normalizedTier = normalizeTier(tier)
    const latest = await query(
        `SELECT MAX(expires_at) AS expires_at
         FROM vip_entitlements
         WHERE user_id = $1
           AND tier = $2
           AND status = 'active'
           AND expires_at > $3`,
        [userId, normalizedTier, now]
    )

    const latestExpires = toDate(latest.rows[0]?.expires_at)
    const startsAt = latestExpires && latestExpires > now ? latestExpires : now
    const expiresAt = addUtcCalendarMonths(startsAt, months)

    const inserted = await query(
        `INSERT INTO vip_entitlements (
            user_id, order_id, tier, source, status, starts_at, expires_at,
            stripe_checkout_session_id, stripe_payment_intent_id, stripe_customer_id
         )
         VALUES ($1, $2, $3, $4, 'active', $5, $6, $7, $8, $9)
         RETURNING id, starts_at, expires_at`,
        [
            userId,
            orderId,
            normalizedTier,
            source,
            startsAt,
            expiresAt,
            stripeCheckoutSessionId,
            stripePaymentIntentId,
            stripeCustomerId
        ]
    )

    await recomputeAndStoreVipForUser(query, userId, now)
    return {
        ...inserted.rows[0],
        tier: normalizedTier,
        purchase_type: purchaseType
    }
}

export async function upsertSubscriptionState(clientOrQuery, {
    userId,
    tier,
    stripeCustomerId = "",
    stripeSubscriptionId,
    status,
    currentPeriodStart,
    currentPeriodEnd,
    billingCycleAnchor = null,
    cancelAtPeriodEnd = false,
    canceledAt = null,
    latestInvoiceId = "",
    latestPaymentIntentId = "",
    now = new Date()
}) {
    const query = queryFrom(clientOrQuery)
    const normalizedTier = normalizeTier(tier)
    const start = toDate(currentPeriodStart) || toDate(billingCycleAnchor)
    let end = toDate(currentPeriodEnd)
    if (!end && start) {
        // Stripe occasionally omits period fields (observed with test-mode
        // subscriptions). Fall back to a 1-month interval from the period start
        // (or billing cycle anchor) so an active monthly subscription always
        // yields an entitlement.
        end = addUtcCalendarMonths(start, 1)
    }

    await query(
        `INSERT INTO vip_subscriptions (
            user_id, tier, stripe_customer_id, stripe_subscription_id, status,
            current_period_start, current_period_end, cancel_at_period_end,
            canceled_at, latest_invoice_id, latest_payment_intent_id, updated_at
         )
         VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, NOW())
         ON CONFLICT (stripe_subscription_id) DO UPDATE SET
            user_id = EXCLUDED.user_id,
            tier = EXCLUDED.tier,
            stripe_customer_id = EXCLUDED.stripe_customer_id,
            status = EXCLUDED.status,
            current_period_start = EXCLUDED.current_period_start,
            current_period_end = EXCLUDED.current_period_end,
            cancel_at_period_end = EXCLUDED.cancel_at_period_end,
            canceled_at = EXCLUDED.canceled_at,
            latest_invoice_id = EXCLUDED.latest_invoice_id,
            latest_payment_intent_id = EXCLUDED.latest_payment_intent_id,
            updated_at = NOW()`,
        [
            userId,
            normalizedTier,
            stripeCustomerId,
            stripeSubscriptionId,
            status,
            start,
            end,
            cancelAtPeriodEnd === true,
            canceledAt ? toDate(canceledAt) : null,
            latestInvoiceId,
            latestPaymentIntentId
        ]
    )

    if (ACTIVE_SUBSCRIPTION_STATUSES.has(String(status || "")) && end && end > now) {
        await query(
            `INSERT INTO vip_entitlements (
                user_id, tier, source, status, starts_at, expires_at,
                stripe_subscription_id, stripe_customer_id, stripe_payment_intent_id
             )
             VALUES ($1, $2, 'subscription', 'active', $3, $4, $5, $6, $7)
             ON CONFLICT (stripe_subscription_id) WHERE stripe_subscription_id <> ''
             DO UPDATE SET
                tier = EXCLUDED.tier,
                status = EXCLUDED.status,
                starts_at = EXCLUDED.starts_at,
                expires_at = EXCLUDED.expires_at,
                stripe_customer_id = EXCLUDED.stripe_customer_id,
                stripe_payment_intent_id = EXCLUDED.stripe_payment_intent_id,
                updated_at = NOW()`,
            [
                userId,
                normalizedTier,
                start || now,
                end,
                stripeSubscriptionId,
                stripeCustomerId,
                latestPaymentIntentId
            ]
        )
    }
    else if (stripeSubscriptionId) {
        await query(
            `UPDATE vip_entitlements
             SET status = CASE WHEN expires_at <= $2 THEN 'expired' ELSE status END,
                 updated_at = NOW()
             WHERE stripe_subscription_id = $1`,
            [stripeSubscriptionId, now]
        )
    }

    return recomputeAndStoreVipForUser(query, userId, now)
}

export async function markVipOrderStatus(clientOrQuery, orderId, status, fields = {}) {
    const query = queryFrom(clientOrQuery)
    const result = await query(
        `UPDATE vip_orders
         SET status = $1,
             stripe_event_id = COALESCE(NULLIF($2, ''), stripe_event_id),
             stripe_payment_intent_id = COALESCE(NULLIF($3, ''), stripe_payment_intent_id),
             stripe_subscription_id = COALESCE(NULLIF($4, ''), stripe_subscription_id),
             stripe_customer_id = COALESCE(NULLIF($5, ''), stripe_customer_id),
             paid_at = CASE WHEN $1 = 'paid' THEN COALESCE(paid_at, NOW()) ELSE paid_at END,
             updated_at = NOW()
         WHERE id = $6
         RETURNING *`,
        [
            status,
            fields.stripeEventId || "",
            fields.stripePaymentIntentId || "",
            fields.stripeSubscriptionId || "",
            fields.stripeCustomerId || "",
            orderId
        ]
    )
    return result.rows[0] || null
}

export async function recordVipNotification(clientOrQuery, {
    userId,
    notificationType,
    entitlementKey,
    channel = "website"
}) {
    const query = queryFrom(clientOrQuery)
    await query(
        `INSERT INTO vip_notifications (user_id, notification_type, entitlement_key, channel)
         VALUES ($1, $2, $3, $4)
         ON CONFLICT (user_id, notification_type, entitlement_key, channel) DO NOTHING`,
        [userId, notificationType, entitlementKey || "", channel]
    )
}
