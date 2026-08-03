// 08 03 2026, 00 10
/* purpose
* Shared in-memory fake database + transaction client for banner payment and
* site-banner tests.
* DOES NOT contact a real database.
* DOES NOT use real Stripe.
*/

export function makeStore() {
    const store = {
        users: new Map(),
        banners: new Map(),
        orders: new Map(),
        reports: [],
        support: [],
        adminActions: [],
        cooldownActive: false,
        nextBannerId: 1,
        nextOrderId: 1,
        nextUserId: 1,
        nextReportId: 1,
        nextSupportId: 1,
        nextAdminActionId: 1,
        addUser(username, email = "") {
            const id = store.nextUserId++
            store.users.set(id, { id, username, email })
            return id
        }
    }
    return store
}

function snapshotStore(store) {
    return {
        users: new Map([...store.users].map(([k, v]) => [k, structuredClone(v)])),
        banners: new Map([...store.banners].map(([k, v]) => [k, structuredClone(v)])),
        orders: new Map([...store.orders].map(([k, v]) => [k, structuredClone(v)])),
        reports: store.reports.map(r => structuredClone(r)),
        support: store.support.map(r => structuredClone(r)),
        adminActions: store.adminActions.map(r => structuredClone(r)),
        nextBannerId: store.nextBannerId,
        nextOrderId: store.nextOrderId,
        nextUserId: store.nextUserId,
        nextReportId: store.nextReportId,
        nextSupportId: store.nextSupportId,
        nextAdminActionId: store.nextAdminActionId
    }
}

function restoreStore(store, snap) {
    store.users = snap.users
    store.banners = snap.banners
    store.orders = snap.orders
    store.reports = snap.reports
    store.support = snap.support
    store.adminActions = snap.adminActions
    store.nextBannerId = snap.nextBannerId
    store.nextOrderId = snap.nextOrderId
    store.nextUserId = snap.nextUserId
    store.nextReportId = snap.nextReportId
    store.nextSupportId = snap.nextSupportId
    store.nextAdminActionId = snap.nextAdminActionId
}

function now() {
    return new Date()
}

function bannerFromParams(store, params) {
    const id = store.nextBannerId++
    return {
        id,
        user_id: params[0],
        kind: params[1],
        days: params[2],
        message: params[3],
        target_url: params[4],
        background_color: params[5],
        text_color: params[6],
        payment_order_id: null,
        status: "draft",
        starts_at: null,
        expires_at: null,
        remaining_days: null,
        moderation_state: "ok",
        refund_status: "",
        created_at: now(),
        updated_at: now()
    }
}

function orderFromParams(store, params) {
    const id = store.nextOrderId++
    return {
        id,
        user_id: params[0],
        duration_days: params[1],
        amount_cents: params[2],
        currency: params[3],
        status: "pending",
        stripe_checkout_session_id: "",
        stripe_event_id: "",
        payment_intent_id: "",
        created_at: now(),
        paid_at: null
    }
}

export function makeDispatch(store) {
    return function dispatch(rawText, params = []) {
        const text = String(rawText).replace(/\s+/g, " ").trim()
        if (text.includes("pg_advisory_xact_lock")) {
            return { rows: [], rowCount: 0 }
        }

        if (text.includes("SELECT 1 FROM site_banners WHERE user_id") && text.includes("LIMIT 1")) {
            return { rows: store.cooldownActive ? [{ x: 1 }] : [] }
        }

        if (text.includes("SELECT 1 FROM site_banners WHERE id")) {
            const found = store.banners.has(params[0])
            return { rows: found ? [{ x: 1 }] : [] }
        }

        if (text.includes("SELECT 1 FROM banner_payment_orders WHERE id")) {
            const found = store.orders.has(params[0])
            return { rows: found ? [{ x: 1 }] : [] }
        }

        if (text.includes("INSERT INTO admin_actions")) {
            store.adminActions.push({ id: store.nextAdminActionId++, admin_user_id: params[0], action: params[1], banner_id: params[2], previous_state: params[3], new_state: params[4] })
            return { rows: [], rowCount: 1 }
        }

        if (text.includes("INSERT INTO support_requests")) {
            const rec = { id: store.nextSupportId++, user_id: params[0], email: params[1], topic: params[2], subject: params[3], message: params[4], url: params[5], banner_order_id: params[6], status: "new", created_at: now() }
            store.support.push(rec)
            return { rows: [{ id: rec.id, created_at: rec.created_at }], rowCount: 1 }
        }

        if (text.includes("INSERT INTO banner_reports")) {
            store.reports.push({ id: store.nextReportId++, banner_id: params[0], reporter_user_id: params[1] })
            return { rows: [], rowCount: 1 }
        }

        if (text.includes("INSERT INTO site_banners")) {
            const banner = bannerFromParams(store, params)
            store.banners.set(banner.id, banner)
            return { rows: [{ id: banner.id }], rowCount: 1 }
        }

        if (text.includes("INSERT INTO banner_payment_orders")) {
            const order = orderFromParams(store, params)
            store.orders.set(order.id, order)
            return { rows: [{ id: order.id }], rowCount: 1 }
        }

        if (text.includes("UPDATE site_banners SET payment_order_id") && text.includes("pending_payment")) {
            const banner = store.banners.get(params[1])
            if (banner) {
                banner.payment_order_id = params[0]
                banner.status = "pending_payment"
            }
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("stripe_checkout_session_id = $1") && !text.includes("status = 'paid'")) {
            const order = store.orders.get(params[1])
            if (order) order.stripe_checkout_session_id = params[0]
            return { rows: [], rowCount: order ? 1 : 0 }
        }

        if (text.includes("stripe_event_id = $1") && text.includes("status = 'paid'")) {
            const [eventId, sessionId, orderId, amountCents, currency, paymentIntent] = params
            const order = store.orders.get(orderId)
            if (
                order &&
                order.status === "pending" &&
                order.amount_cents === amountCents &&
                order.currency.toLowerCase() === String(currency).toLowerCase()
            ) {
                order.status = "paid"
                order.paid_at = now()
                order.stripe_event_id = eventId
                order.payment_intent_id = paymentIntent || order.payment_intent_id || ""
                if (!order.stripe_checkout_session_id) order.stripe_checkout_session_id = sessionId
                return { rows: [{ id: orderId, duration_days: order.duration_days, user_id: order.user_id }], rowCount: 1 }
            }
            return { rows: [], rowCount: 0 }
        }

        if (text.includes("SELECT status, amount_cents, currency FROM banner_payment_orders")) {
            const order = store.orders.get(params[0])
            return {
                rows: order ? [{ status: order.status, amount_cents: order.amount_cents, currency: order.currency }] : [],
                rowCount: order ? 1 : 0
            }
        }

        if (text.includes("SELECT id, kind, days, remaining_days, status, payment_order_id FROM site_banners")) {
            const banner = store.banners.get(params[0])
            return {
                rows: banner
                    ? [{ id: banner.id, kind: banner.kind, days: banner.days, remaining_days: banner.remaining_days, status: banner.status, payment_order_id: banner.payment_order_id }]
                    : []
            }
        }

        if (text.includes("SELECT id FROM site_banners WHERE payment_order_id")) {
            const banner = [...store.banners.values()].find(b => b.payment_order_id === params[0] && b.status === "pending_payment")
            return { rows: banner ? [{ id: banner.id }] : [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SELECT id, kind, days FROM site_banners") && text.includes("status = 'active'")) {
            const active = [...store.banners.values()].find(b => b.status === "active")
            return { rows: active ? [{ id: active.id, kind: active.kind, days: active.days }] : [] }
        }

        if (text.includes("SELECT status FROM site_banners WHERE id")) {
            const banner = store.banners.get(params[0])
            return { rows: banner ? [{ status: banner.status }] : [] }
        }

        if (text.includes("SET status = 'replaced'")) {
            const banner = store.banners.get(params[0])
            if (banner) banner.status = "replaced"
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET status = 'queued', remaining_days")) {
            const banner = store.banners.get(params[0])
            if (banner) {
                const remaining = banner.expires_at ? Math.max((banner.expires_at.getTime() - Date.now()) / 86400000, 0) : banner.remaining_days
                banner.status = "queued"
                banner.remaining_days = remaining
                banner.starts_at = null
                banner.expires_at = null
            }
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET status = 'queued'") && text.includes("IN ('draft', 'pending_payment')")) {
            const banner = store.banners.get(params[0])
            if (banner && ["draft", "pending_payment"].includes(banner.status)) banner.status = "queued"
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET status = 'queued'") && text.includes("disabled_by_admin_id = NULL")) {
            const banner = store.banners.get(params[0])
            if (banner && banner.status === "disabled") banner.status = "queued"
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET status = 'active'") && text.includes("IN ('draft', 'pending_payment', 'queued')")) {
            const banner = store.banners.get(params[0])
            if (banner && ["draft", "pending_payment", "queued"].includes(banner.status)) {
                banner.status = "active"
                banner.starts_at = now()
                banner.expires_at = new Date(Date.now() + params[1] * 86400000)
                banner.remaining_days = null
                banner.updated_at = now()
                return { rows: [], rowCount: 1 }
            }
            return { rows: [], rowCount: 0 }
        }

        if (text.includes("SET status = 'expired'")) {
            for (const banner of store.banners.values()) {
                if (banner.status === "active" && banner.expires_at <= now()) banner.status = "expired"
            }
            return { rows: [], rowCount: 0 }
        }

        if (text.includes("LEFT JOIN banner_payment_orders") && text.includes("b.status = 'queued'")) {
            const rows = [...store.banners.values()]
                .filter(b => b.status === "queued")
                .map(b => {
                    const order = b.payment_order_id ? store.orders.get(b.payment_order_id) : null
                    return { id: b.id, kind: b.kind, days: b.days, remaining_days: b.remaining_days, created_at: b.created_at, amount_cents: order ? order.amount_cents : null }
                })
            return { rows }
        }

        if (text.includes("SET status = 'active'") && text.includes("status = 'queued'")) {
            const banner = store.banners.get(params[0])
            if (banner && banner.status === "queued") {
                banner.status = "active"
                banner.starts_at = now()
                banner.expires_at = new Date(Date.now() + params[1] * 86400000)
                banner.remaining_days = null
                banner.updated_at = now()
                return { rows: [], rowCount: 1 }
            }
            return { rows: [], rowCount: 0 }
        }

        if (text.includes("SET created_at = $2")) {
            const banner = store.banners.get(params[0])
            if (banner) banner.created_at = params[1]
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("u.username AS owner_username")) {
            const rows = [...store.banners.values()].map(b => {
                const order = b.payment_order_id ? store.orders.get(b.payment_order_id) : null
                const user = store.users.get(b.user_id)
                const reportCount = store.reports.filter(r => r.banner_id === b.id).length
                return {
                    ...b,
                    owner_username: user ? user.username : "",
                    owner_email: user ? user.email : "",
                    order_duration_days: order ? order.duration_days : null,
                    order_amount_cents: order ? order.amount_cents : null,
                    order_currency: order ? order.currency : null,
                    order_status: order ? order.status : null,
                    order_paid_at: order ? order.paid_at : null,
                    stripe_checkout_session_id: order ? order.stripe_checkout_session_id : "",
                    stripe_event_id: order ? order.stripe_event_id : "",
                    payment_intent_id: order ? order.payment_intent_id : "",
                    report_count: reportCount
                }
            })
            return { rows }
        }

        if (text.includes("b.status IN ('active', 'queued')")) {
            const rows = [...store.banners.values()]
                .filter(b => b.status === "active" || b.status === "queued")
                .map(b => {
                    const order = b.payment_order_id ? store.orders.get(b.payment_order_id) : null
                    const user = store.users.get(b.user_id)
                    return {
                        id: b.id, user_id: b.user_id, kind: b.kind, days: b.days, remaining_days: b.remaining_days,
                        status: b.status, message: b.message, target_url: b.target_url,
                        background_color: b.background_color, text_color: b.text_color,
                        starts_at: b.starts_at, expires_at: b.expires_at, created_at: b.created_at,
                        amount_cents: order ? order.amount_cents : null,
                        username: user ? user.username : ""
                    }
                })
            return { rows }
        }

        if (text.includes("FROM site_banners b") && text.includes("LEFT JOIN banner_payment_orders") && text.includes("b.user_id = $1")) {
            const rows = [...store.banners.values()]
                .filter(b => b.user_id === params[0])
                .map(b => {
                    const order = b.payment_order_id ? store.orders.get(b.payment_order_id) : null
                    return {
                        id: b.id, kind: b.kind, days: b.days, remaining_days: b.remaining_days, status: b.status,
                        message: b.message, target_url: b.target_url, background_color: b.background_color,
                        text_color: b.text_color, starts_at: b.starts_at, expires_at: b.expires_at,
                        created_at: b.created_at, payment_order_id: b.payment_order_id,
                        moderation_state: b.moderation_state, refund_status: b.refund_status,
                        amount_cents: order ? order.amount_cents : null,
                        currency: order ? order.currency : null,
                        order_status: order ? order.status : null
                    }
                })
            return { rows }
        }

        if (text.includes("FROM banner_payment_orders WHERE id = $1")) {
            const order = store.orders.get(params[0])
            return { rows: order ? [{ ...order }] : [] }
        }

        if (text.includes("FROM site_banners") && text.includes("WHERE payment_order_id = $1")) {
            const banner = [...store.banners.values()].find(b => b.payment_order_id === params[0])
            return { rows: banner ? [{ ...banner }] : [] }
        }

        if (text.includes("u.username") && text.includes("b.status = 'active'") && text.includes("b.expires_at > NOW()")) {
            const active = [...store.banners.values()].find(b => b.status === "active" && b.expires_at > now())
            if (!active) return { rows: [] }
            const user = store.users.get(active.user_id)
            return { rows: [{ id: active.id, message: active.message, target_url: active.target_url, background_color: active.background_color, text_color: active.text_color, username: user ? user.username : "", created_at: active.created_at, expires_at: active.expires_at }] }
        }

        if (text.includes("u.username") && text.includes("b.status = 'active'")) {
            const active = [...store.banners.values()].find(b => b.status === "active")
            const user = active ? store.users.get(active.user_id) : null
            return { rows: active ? [{ id: active.id, message: active.message, username: user ? user.username : "" }] : [] }
        }

        if (text.includes("SET status = 'disabled'")) {
            const banner = store.banners.get(params[2])
            if (banner) {
                banner.status = "disabled"
                banner.disabled_by_admin_id = params[0]
                banner.disabled_reason = params[1]
            }
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET status = 'deleted'")) {
            const banner = store.banners.get(params[1])
            if (banner) {
                banner.status = "deleted"
                banner.disabled_by_admin_id = params[0]
            }
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("SET message = $1")) {
            const banner = store.banners.get(params[4])
            if (banner) {
                banner.message = params[0]
                banner.target_url = params[1]
                banner.background_color = params[2]
                banner.text_color = params[3]
            }
            return { rows: [], rowCount: banner ? 1 : 0 }
        }

        if (text.includes("FROM support_requests")) {
            const rows = store.support.map(r => ({
                ...r,
                username: r.user_id ? (store.users.get(r.user_id)?.username || "") : ""
            }))
            return { rows }
        }

        if (text.includes("UPDATE support_requests SET status")) {
            const rec = store.support.find(r => r.id === params[1])
            if (rec) rec.status = params[0]
            return { rows: [], rowCount: rec ? 1 : 0 }
        }

        throw new Error("unhandled query: " + text.replace(/\s+/g, " ").slice(0, 120))
    }
}

export function makeQuery(store) {
    const dispatch = makeDispatch(store)
    return (text, params) => Promise.resolve(dispatch(text, params))
}

export function makeClient(store) {
    const dispatch = makeDispatch(store)
    let snapshot = null
    return {
        began: 0,
        committed: 0,
        rolledBack: 0,
        released: false,
        async query(text, params) {
            if (text === "BEGIN") {
                this.began++
                snapshot = snapshotStore(store)
                return { rows: [], rowCount: 0 }
            }
            if (text === "COMMIT") {
                this.committed++
                snapshot = null
                return { rows: [], rowCount: 0 }
            }
            if (text === "ROLLBACK") {
                this.rolledBack++
                if (snapshot) restoreStore(store, snapshot)
                snapshot = null
                return { rows: [], rowCount: 0 }
            }
            return dispatch(text, params)
        },
        async begin() {
            this.began++
            snapshot = snapshotStore(store)
        },
        async commit() {
            this.committed++
            snapshot = null
        },
        async rollback() {
            this.rolledBack++
            if (snapshot) restoreStore(store, snapshot)
            snapshot = null
        },
        async release() {
            this.released = true
        }
    }
}
