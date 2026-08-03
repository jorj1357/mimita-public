// 08 03 2026, 03 10
/* purpose
* Single server-owned scheduling engine for the banner queue.
* Computes the active banner, the ordered queue, estimated start/end times,
* remaining purchased duration, priority amounts, and overwrite eligibility.
* Used by the public endpoint, order status, admin dashboard, user status page,
* and tests so all views agree.
* DOES NOT write to the database.
* DOES NOT talk to Stripe.
*/

const TIER = { admin: 3, paid: 2, free: 1 }
const DAY_MS = 86400000

export function bannerPriorityAmount(banner) {
    if (banner.kind === "admin") return Number.MAX_SAFE_INTEGER
    if (banner.kind === "paid") {
        const amount = Number(banner.amount_cents)
        if (Number.isFinite(amount) && amount > 0) return amount
        return Number(banner.days || 0) * 100
    }
    return 0
}

export function bannerRemainingDays(banner, now = Date.now()) {
    if (banner.status === "active" && banner.expires_at) {
        const remainingMs = new Date(banner.expires_at).getTime() - now
        return remainingMs > 0 ? remainingMs / DAY_MS : 0
    }
    if (banner.remaining_days !== undefined && banner.remaining_days !== null && Number(banner.remaining_days) > 0) {
        return Number(banner.remaining_days)
    }
    return Number(banner.days || 0)
}

export function compareQueueOrder(a, b) {
    const ta = TIER[a.kind] || 0
    const tb = TIER[b.kind] || 0
    if (ta !== tb) return tb - ta
    if (ta === 2 || ta === 3) {
        const pa = bannerPriorityAmount(a)
        const pb = bannerPriorityAmount(b)
        if (pa !== pb) return pb - pa
    }
    return new Date(a.created_at).getTime() - new Date(b.created_at).getTime()
}

export function canOverwrite(incoming, active) {
    if (!active) return true
    if (incoming.kind === "admin") return true
    if (incoming.kind === "free") return active.kind === "free"
    if (incoming.kind === "paid") {
        if (active.kind === "free") return true
        if (active.kind === "paid") return bannerPriorityAmount(incoming) > bannerPriorityAmount(active)
        return false
    }
    return false
}

function queueReason(banner, active) {
    if (!active) return "waiting for the active slot to free"
    if (banner.kind === "free") {
        if (active.kind === "paid") return "a paid banner is currently active"
        if (active.kind === "admin") return "an admin banner is currently active"
        return "a free banner is currently active"
    }
    if (banner.kind === "admin") return "waiting for the active slot to free"
    if (active.kind === "admin") return "an admin banner is currently active"
    if (active.kind === "paid") {
        return bannerPriorityAmount(banner) > bannerPriorityAmount(active)
            ? "waiting for the active slot to free"
            : "a paid banner with an equal or higher amount is currently active"
    }
    return "waiting for the active slot to free"
}

export function computeSchedule(rows, now = Date.now()) {
    const activeRow = rows.find(r => r.status === "active")
    let active = null
    let cursor = now
    if (activeRow) {
        const remaining = bannerRemainingDays(activeRow, now)
        if (remaining > 0 && activeRow.expires_at) {
            active = {
                id: activeRow.id,
                banner: activeRow,
                remaining_days: remaining,
                priority_amount_cents: bannerPriorityAmount(activeRow),
                estimated_end: activeRow.expires_at
            }
            cursor = new Date(activeRow.expires_at).getTime()
        }
    }

    const queued = rows.filter(r => r.status === "queued").sort(compareQueueOrder)

    const queue = queued.map((b, i) => {
        const durationDays = bannerRemainingDays(b, now)
        const start = new Date(cursor)
        const end = new Date(cursor + durationDays * DAY_MS)
        cursor = end.getTime()
        return {
            position: i + 1,
            banner: b,
            estimated_start: start,
            estimated_end: end,
            remaining_days: durationDays,
            priority_amount_cents: bannerPriorityAmount(b),
            overwrite_eligibility: active ? canOverwrite(b, active) : true,
            reason: queueReason(b, active ? active.banner : null)
        }
    })

    return { active, queue }
}
