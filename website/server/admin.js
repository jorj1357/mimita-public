import { Router } from "express"
import { hashToken, getClientIp, verifyPassword, usernameKey, normalizeEmail } from "./authCore.js"
import { pool } from "./db.js"
import { getMetrics, refreshMetrics } from "./analytics.js"
import { getFeedback, FEEDBACK_PRESETS } from "./feedback.js"
import { getErrors, getErrorCount } from "./error-queue.js"
import {
    grantPrepaidEntitlement,
    getVipStateForUser,
    recomputeAndStoreVipForUser,
    ACTIVE_SUBSCRIPTION_STATUSES
} from "./vip-entitlements.js"
import {
    normalizeTier,
    tierRank,
    validateNameStyle
} from "./vip-config.js"
import {
    parseCookies,
    clearSessionCookie,
    createSession,
    sessionCookieName,
    sessionSecret
} from "./session.js"

const router = Router()

const ADMIN_ROLES = ["admin", "owner"]

async function requireAdmin(req, res, next) {
    try {
        const token = parseCookies(req)[sessionCookieName]

        if (!token) {
            console.log(`[ADMIN AUTH] 401 no token path=${req.path} ip=${getClientIp(req)}`)
            return res.status(401).json({
                success: false,
                message: "sign in required"
            })
        }

        const result = await pool.query(
            `
            SELECT
                u.id, u.username, u.email, u.role, u.bio
            FROM sessions s
            JOIN users u ON u.id = s.user_id
            WHERE s.token_hash = $1
              AND s.revoked_at IS NULL
              AND s.expires_at > NOW()
              AND u.deleted_at IS NULL
            LIMIT 1
            `,
            [hashToken(token, sessionSecret)]
        )

        if (!result.rowCount) {
            console.log(`[ADMIN AUTH] 401 session expired path=${req.path}`)
            clearSessionCookie(res)
            return res.status(401).json({
                success: false,
                message: "session expired"
            })
        }

        const user = result.rows[0]

        if (!ADMIN_ROLES.includes(user.role)) {
            console.log(`[ADMIN AUTH] 403 user=${user.username} role=${user.role} path=${req.path}`)
            return res.status(403).json({
                success: false,
                message: "admin access required"
            })
        }

        console.log(`[ADMIN AUTH] 200 user=${user.username} role=${user.role} path=${req.path}`)
        req.user = user
        next()
    }
    catch (error) {
        next(error)
    }
}

router.post("/login", async (req, res, next) => {
    try {
        console.log("[ADMIN LOGIN] received body keys:", Object.keys(req.body))
        console.log("[ADMIN LOGIN] username field present:", "username" in req.body)
        console.log("[ADMIN LOGIN] identifier field present:", "identifier" in req.body)

        const username = String(req.body.username || "").trim()
        const password = String(req.body.password || "")

        if (!username || !password) {
            return res.status(400).json({
                success: false,
                message: "username and password required"
            })
        }

        const result = await pool.query(
            `
            SELECT id, username, email, role, password_hash
            FROM users
            WHERE deleted_at IS NULL
              AND role = ANY($1)
              AND (
                  username_key = $2
                  OR email = $3
              )
            LIMIT 1
            `,
            [ADMIN_ROLES, usernameKey(username), normalizeEmail(username)]
        )

        const user = result.rows[0]

        if (!user || !(await verifyPassword(password, user.password_hash))) {
            console.log(`[ADMIN] failed login attempt from ${getClientIp(req)}`)
            return res.status(401).json({
                success: false,
                message: "invalid credentials or insufficient permissions"
            })
        }

        await createSession(user.id, req, res)
        console.log(`[ADMIN] login success user_id=${user.id} username=${user.username} from ${getClientIp(req)}`)

        res.json({
            success: true,
            user: {
                id: user.id,
                username: user.username,
                email: user.email,
                role: user.role
            }
        })
    }
    catch (error) {
        next(error)
    }
})

router.post("/logout", requireAdmin, async (req, res, next) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        const tokenHash = hashToken(token, sessionSecret)

        await pool.query(
            `UPDATE sessions SET revoked_at = NOW() WHERE token_hash = $1`,
            [tokenHash]
        )

        clearSessionCookie(res)
        console.log(`[ADMIN] logout user_id=${req.user.id}`)
        res.json({ success: true, message: "signed out" })
    }
    catch (error) {
        next(error)
    }
})

router.get("/me", requireAdmin, (req, res) => {
    res.json({
        success: true,
        user: {
            id: req.user.id,
            username: req.user.username,
            email: req.user.email,
            role: req.user.role,
            bio: req.user.bio
        }
    })
})

router.get("/dashboard", requireAdmin, async (req, res, next) => {
    try {
        const metrics = await getMetrics()
        res.json({ success: true, metrics })
    }
    catch (error) {
        next(error)
    }
})

router.post("/dashboard/refresh", requireAdmin, async (req, res, next) => {
    try {
        await refreshMetrics()
        const metrics = await getMetrics()
        res.json({ success: true, metrics })
    }
    catch (error) {
        next(error)
    }
})

router.get("/users", requireAdmin, async (req, res, next) => {
    try {
        const limit = Math.min(Number(req.query.limit) || 50, 200)
        const offset = Number(req.query.offset) || 0

        const result = await pool.query(
            `
            SELECT id, username, email, role, bio,
                   avatar_url, avatar_updated_at,
                   email_notifications_enabled,
                   created_at, updated_at, deleted_at
            FROM users
            ORDER BY created_at DESC
            LIMIT $1 OFFSET $2
            `,
            [limit, offset]
        )

        const countResult = await pool.query(
            `SELECT COUNT(*) AS count FROM users`
        )

        res.json({
            success: true,
            users: result.rows,
            total: Number(countResult.rows[0].count),
            limit,
            offset
        })
    }
    catch (error) {
        next(error)
    }
})

router.get("/feedback", requireAdmin, async (req, res, next) => {
    try {
        const limit = Math.min(Number(req.query.limit) || 20, 100)
        const offset = Number(req.query.offset) || 0
        const feedback = await getFeedback(limit, offset)
        res.json({ success: true, feedback })
    }
    catch (error) {
        next(error)
    }
})

router.get("/feedback/presets", (req, res) => {
    res.json({ success: true, presets: FEEDBACK_PRESETS })
})

router.get("/check", async (req, res) => {
    try {
        const token = parseCookies(req)[sessionCookieName]
        if (!token) {
            return res.json({ success: true, isAdmin: false })
        }

        const result = await pool.query(
            `
            SELECT u.role
            FROM sessions s
            JOIN users u ON u.id = s.user_id
            WHERE s.token_hash = $1
              AND s.revoked_at IS NULL
              AND s.expires_at > NOW()
              AND u.deleted_at IS NULL
            LIMIT 1
            `,
            [hashToken(token, sessionSecret)]
        )

        if (!result.rowCount) {
            return res.json({ success: true, isAdmin: false })
        }

        const isAdmin = ADMIN_ROLES.includes(result.rows[0].role)
        res.json({ success: true, isAdmin })
    }
    catch {
        res.json({ success: true, isAdmin: false })
    }
})

router.get("/admins", requireAdmin, async (req, res, next) => {
    try {
        const search = String(req.query.search || "").trim().toLowerCase()

        let sql = `
            SELECT id, username, email, role, avatar_url, avatar_updated_at,
                   created_at, email_verified_at, achievements
            FROM users
            WHERE deleted_at IS NULL
              AND role = ANY($1)
        `
        const params = [ADMIN_ROLES]

        if (search) {
            sql += ` AND (LOWER(username) LIKE $2 OR LOWER(email) LIKE $2)`
            params.push(`%${search}%`)
        }

        sql += ` ORDER BY
            CASE role
                WHEN 'owner' THEN 1
                WHEN 'admin' THEN 2
                ELSE 3
            END,
            username ASC
        `

        const result = await pool.query(sql, params)

        res.json({
            success: true,
            admins: result.rows
        })
    }
    catch (error) {
        next(error)
    }
})

router.get("/error-log", requireAdmin, (req, res) => {
    const limit = Math.min(Number(req.query.limit) || 50, 200)
    res.json({
        success: true,
        errors: getErrors(limit),
        total: getErrorCount()
    })
})

router.get("/flagged-accounts", requireAdmin, async (req, res, next) => {
    try {
        const { getFlaggedAccounts } = await import("./authCore.js")
        const flagged = await getFlaggedAccounts()
        console.log(`[ADMIN] Flagged accounts: ${flagged.length}`)
        res.json({ success: true, flagged })
    } catch (error) {
        next(error)
    }
})

// ── Article management ──────────────────────────────────
import fs from "fs"
import path from "path"
import matter from "gray-matter"
import { fileURLToPath } from "url"

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const ARTICLES_DIR = path.resolve(__dirname, "..", "..", "content", "articles")
const JSON_OUTPUT = path.resolve(__dirname, "..", "public", "articles.generated.json")
const DIST_JSON_OUTPUT = path.resolve(__dirname, "..", "dist", "articles.generated.json")
const NEWS_JSON_OUTPUT = path.resolve(__dirname, "..", "public", "news.generated.json")
const DIST_NEWS_JSON_OUTPUT = path.resolve(__dirname, "..", "dist", "news.generated.json")

function getAllArticles() {
    if (!fs.existsSync(ARTICLES_DIR)) return []
    const files = fs.readdirSync(ARTICLES_DIR)
    const articles = []
    for (const file of files) {
        if (!file.endsWith(".md")) continue
        const filePath = path.join(ARTICLES_DIR, file)
        const slug = file.slice(0, -3)
        const raw = fs.readFileSync(filePath, "utf-8")
        const { data, content } = matter(raw)
        articles.push({
            slug,
            title: data.title || slug,
            description: data.description || "",
            date: data.date || "",
            author: data.author || "",
            tags: data.tags || [],
            published: data.published !== false,
            content
        })
    }
    return articles
}

function regenerateJson() {
    const articles = getAllArticles()
        .filter(a => a.published)
        .sort((a, b) => b.date.localeCompare(a.date))
        .map(a => {
            const rest = { ...a }
            delete rest.published
            return rest
        })
    const json = JSON.stringify(articles, null, 2)

    const dir = path.dirname(JSON_OUTPUT)
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(JSON_OUTPUT, json)

    // Also write to dist/ if it exists (production build)
    const distDir = path.dirname(DIST_JSON_OUTPUT)
    if (fs.existsSync(distDir)) {
        fs.writeFileSync(DIST_JSON_OUTPUT, json)
    }

    regenerateNews(articles)
}

function regenerateNews(articles) {
    const news = articles.map(a => ({
        type: "article",
        title: `New article: ${a.title}`,
        date: a.date,
        description: a.description,
        url: `/articles/${a.slug}`
    }))
    const json = JSON.stringify(news, null, 2)

    const dir = path.dirname(NEWS_JSON_OUTPUT)
    if (!fs.existsSync(dir)) fs.mkdirSync(dir, { recursive: true })
    fs.writeFileSync(NEWS_JSON_OUTPUT, json)

    const distDir = path.dirname(DIST_NEWS_JSON_OUTPUT)
    if (fs.existsSync(distDir)) {
        fs.writeFileSync(DIST_NEWS_JSON_OUTPUT, json)
    }
}

router.get("/articles", requireAdmin, (req, res) => {
    try {
        const all = getAllArticles()
        res.json({ success: true, articles: all })
    } catch (err) {
        res.status(500).json({ success: false, message: err.message })
    }
})

router.get("/articles/:slug", requireAdmin, (req, res) => {
    try {
        const slug = req.params.slug
        const filePath = path.join(ARTICLES_DIR, slug + ".md")
        if (!fs.existsSync(filePath)) {
            return res.status(404).json({ success: false, message: "article not found" })
        }
        const raw = fs.readFileSync(filePath, "utf-8")
        const { data, content } = matter(raw)
        res.json({
            success: true,
            article: {
                slug,
                title: data.title || slug,
                description: data.description || "",
                date: data.date || "",
                author: data.author || "",
                tags: data.tags || [],
                published: data.published !== false,
                content
            }
        })
    } catch (err) {
        res.status(500).json({ success: false, message: err.message })
    }
})

router.post("/articles", requireAdmin, async (req, res) => {
    try {
        const { slug, title, description, date, author, tags, content, published } = req.body
        if (!slug || !title) {
            return res.status(400).json({ success: false, message: "slug and title required" })
        }

        if (!fs.existsSync(ARTICLES_DIR)) {
            fs.mkdirSync(ARTICLES_DIR, { recursive: true })
        }

        const dateStr = date || new Date().toISOString().split("T")[0]
        const authorStr = author || req.user.username
        const tagsArr = tags || []
        const publishedFlag = published !== false

        let frontmatter = "---\n"
        frontmatter += `title: "${title.replace(/"/g, '\\"')}"\n`
        frontmatter += `description: "${(description || "").replace(/"/g, '\\"')}"\n`
        frontmatter += `date: "${dateStr}"\n`
        frontmatter += `author: "${authorStr.replace(/"/g, '\\"')}"\n`
        frontmatter += `tags: [${tagsArr.map(t => `"${t.replace(/"/g, '\\"')}"`).join(", ")}]\n`
        frontmatter += `published: ${publishedFlag}\n`
        frontmatter += "---\n"

        const markdown = frontmatter + (content || "")
        const filePath = path.join(ARTICLES_DIR, slug + ".md")
        fs.writeFileSync(filePath, markdown, "utf-8")

        regenerateJson()

        console.log(`[ARTICLES] ${req.user.username} saved article: ${slug}`)
        res.json({ success: true, message: "article saved", slug })
    } catch (err) {
        res.status(500).json({ success: false, message: err.message })
    }
})

router.delete("/articles/:slug", requireAdmin, (req, res) => {
    try {
        const slug = req.params.slug
        const filePath = path.join(ARTICLES_DIR, slug + ".md")
        if (!fs.existsSync(filePath)) {
            return res.status(404).json({ success: false, message: "article not found" })
        }
        fs.unlinkSync(filePath)
        regenerateJson()
        console.log(`[ARTICLES] ${req.user.username} deleted article: ${slug}`)
        res.json({ success: true, message: "article deleted" })
    } catch (err) {
        res.status(500).json({ success: false, message: err.message })
    }
})

// ── VIP admin grants ─────────────────────────────────────────────────
// Manual entitlement grants/revokes for support and for testing the full
// signup -> tier -> styled-name journey without a live Stripe payment.
router.post("/vip/grant", requireAdmin, async (req, res, next) => {
    try {
        const userId = Number(req.body.user_id || 0)
        const tier = String(req.body.tier || "").trim().toLowerCase()
        const months = Math.floor(Number(req.body.months || 1))
        if (!Number.isInteger(userId) || userId <= 0) {
            return res.status(400).json({ success: false, message: "valid user_id required" })
        }
        if (!["vip", "super_vip", "ultra_vip"].includes(tier)) {
            return res.status(400).json({ success: false, message: "tier must be vip, super_vip, or ultra_vip" })
        }
        if (!Number.isInteger(months) || months < 1 || months > 24) {
            return res.status(400).json({ success: false, message: "months must be 1-24" })
        }

        const userResult = await pool.query(
            `SELECT id, role FROM users WHERE id = $1 AND deleted_at IS NULL LIMIT 1`,
            [userId]
        )
        if (!userResult.rowCount) {
            return res.status(404).json({ success: false, message: "user not found" })
        }

        await grantPrepaidEntitlement(pool, {
            userId,
            tier,
            purchaseType: "admin_grant",
            months,
            source: "admin",
            now: new Date()
        })
        const state = await getVipStateForUser(userResult.rows[0], pool)
        console.log(`[VIP GRANT] by=${req.user.username} user_id=${userId} tier=${tier} months=${months}`)
        res.json({ success: true, vip: state })
    } catch (error) {
        next(error)
    }
})

router.post("/vip/revoke", requireAdmin, async (req, res, next) => {
    try {
        const userId = Number(req.body.user_id || 0)
        if (!Number.isInteger(userId) || userId <= 0) {
            return res.status(400).json({ success: false, message: "valid user_id required" })
        }
        await pool.query(
            `UPDATE vip_entitlements
             SET status = 'expired', updated_at = NOW()
             WHERE user_id = $1 AND status = 'active'`,
            [userId]
        )
        await recomputeAndStoreVipForUser(pool, userId)
        const userResult = await pool.query(
            `SELECT id, role FROM users WHERE id = $1 LIMIT 1`,
            [userId]
        )
        const state = userResult.rowCount
            ? await getVipStateForUser(userResult.rows[0], pool)
            : null
        console.log(`[VIP REVOKE] by=${req.user.username} user_id=${userId}`)
        res.json({ success: true, vip: state })
    } catch (error) {
        next(error)
    }
})

export function computeVipAdminFlags({ subscriptions = [], activeTier = "free", now = new Date() } = {}) {
    const current = now instanceof Date ? now : new Date(now)
    const active = (subscriptions || []).filter(s =>
        ACTIVE_SUBSCRIPTION_STATUSES.has(String(s.status)) &&
        s.current_period_end &&
        new Date(s.current_period_end) > current
    )
    let subscriptionTier = "free"
    for (const s of active) {
        const tier = normalizeTier(s.tier)
        if (tierRank(tier) > tierRank(subscriptionTier)) subscriptionTier = tier
    }
    const hasActiveSubscription = active.length > 0
    return {
        has_active_subscription: hasActiveSubscription,
        subscription_tier: subscriptionTier,
        desync: hasActiveSubscription && tierRank(subscriptionTier) > tierRank(activeTier)
    }
}

router.get("/vip/lookup", requireAdmin, async (req, res, next) => {
    try {
        const queryText = String(req.query.query || "").trim()
        if (!queryText) {
            return res.status(400).json({ success: false, message: "query required" })
        }

        const numericId = /^\d+$/.test(queryText) ? Number(queryText) : 0
        let sql = `SELECT id, username, email, role, avatar_url, avatar_updated_at
                   FROM users WHERE deleted_at IS NULL`
        const params = []
        if (numericId > 0) {
            sql += ` AND id = $1`
            params.push(numericId)
        }
        else {
            sql += ` AND (LOWER(username) = LOWER($1) OR LOWER(email) = LOWER($1))`
            params.push(queryText)
        }
        sql += ` LIMIT 1`

        const userResult = await pool.query(sql, params)
        if (!userResult.rowCount) {
            return res.status(404).json({ success: false, message: "user not found" })
        }
        const row = userResult.rows[0]
        const now = new Date()

        const state = await getVipStateForUser(row, pool, now)
        const [entitlements, subscriptions] = await Promise.all([
            pool.query(
                `SELECT id, tier, source, status, starts_at, expires_at
                 FROM vip_entitlements
                 WHERE user_id = $1 AND status = 'active'
                 ORDER BY expires_at DESC`,
                [row.id]
            ),
            pool.query(
                `SELECT id, tier, status, current_period_start, current_period_end,
                        cancel_at_period_end, stripe_subscription_id
                 FROM vip_subscriptions
                 WHERE user_id = $1
                 ORDER BY current_period_end DESC NULLS LAST`,
                [row.id]
            )
        ])

        res.json({
            success: true,
            user: {
                id: row.id,
                username: row.username,
                email: row.email,
                role: row.role,
                avatar_url: row.avatar_url
            },
            state,
            entitlements: entitlements.rows,
            subscriptions: subscriptions.rows,
            ...computeVipAdminFlags({ subscriptions: subscriptions.rows, activeTier: state.active_tier, now })
        })
    } catch (error) {
        next(error)
    }
})

router.post("/vip/style", requireAdmin, async (req, res, next) => {
    try {
        const userId = Number(req.body.user_id || 0)
        if (!Number.isInteger(userId) || userId <= 0) {
            return res.status(400).json({ success: false, message: "valid user_id required" })
        }
        const userResult = await pool.query(
            `SELECT id, role FROM users WHERE id = $1 AND deleted_at IS NULL LIMIT 1`,
            [userId]
        )
        if (!userResult.rowCount) {
            return res.status(404).json({ success: false, message: "user not found" })
        }
        const user = userResult.rows[0]
        const state = await getVipStateForUser(user, pool)

        const validation = validateNameStyle(req.body.style || {}, {
            activeTier: state.active_tier,
            role: user.role || "user"
        })
        if (!validation.ok) {
            return res.status(400).json({ success: false, message: validation.message })
        }

        await pool.query(
            `INSERT INTO vip_name_styles (user_id, style_json, updated_at)
             VALUES ($1, $2, NOW())
             ON CONFLICT (user_id) DO UPDATE SET style_json = $2, updated_at = NOW()`,
            [userId, JSON.stringify(validation.style)]
        )
        await recomputeAndStoreVipForUser(pool, userId)
        const nextState = await getVipStateForUser(user, pool)
        console.log(`[VIP STYLE] by=${req.user.username} user_id=${userId} kind=${validation.style.kind}`)
        res.json({ success: true, vip: nextState })
    } catch (error) {
        next(error)
    }
})

router.post("/vip/resync", requireAdmin, async (req, res, next) => {
    try {
        const userId = Number(req.body.user_id || 0)
        if (!Number.isInteger(userId) || userId <= 0) {
            return res.status(400).json({ success: false, message: "valid user_id required" })
        }

        const inserted = await pool.query(
            `INSERT INTO vip_entitlements (
                user_id, tier, source, status, starts_at, expires_at,
                stripe_subscription_id, stripe_customer_id, stripe_payment_intent_id
             )
             SELECT s.user_id, s.tier, 'subscription', 'active',
                    s.current_period_start, s.current_period_end,
                    s.stripe_subscription_id, s.stripe_customer_id, s.latest_payment_intent_id
             FROM vip_subscriptions s
             WHERE s.user_id = $1
               AND s.status = ANY($2)
               AND s.current_period_end > NOW()
               AND NOT EXISTS (
                   SELECT 1 FROM vip_entitlements e
                   WHERE e.user_id = s.user_id
                     AND e.status = 'active'
                     AND e.stripe_subscription_id = s.stripe_subscription_id
               )
             RETURNING id`,
            [userId, [...ACTIVE_SUBSCRIPTION_STATUSES]]
        )

        await recomputeAndStoreVipForUser(pool, userId)
        const userResult = await pool.query(
            `SELECT id, role FROM users WHERE id = $1 LIMIT 1`,
            [userId]
        )
        const state = userResult.rowCount
            ? await getVipStateForUser(userResult.rows[0], pool)
            : null
        console.log(`[VIP RESYNC] by=${req.user.username} user_id=${userId} created=${inserted.rowCount}`)
        res.json({ success: true, vip: state, created: inserted.rowCount })
    } catch (error) {
        next(error)
    }
})

export default router
export { requireAdmin, regenerateJson }
