import assert from "node:assert/strict"
import test from "node:test"
import { pool, runMigrations } from "./db.js"

let adminToken = ""
let testUserId = 0

test("database migrations run", async () => {
    await runMigrations()
    const tables = await pool.query(`
        SELECT table_name FROM information_schema.tables
        WHERE table_schema = 'public'
        ORDER BY table_name
    `)
    const names = tables.rows.map(r => r.table_name)
    assert.ok(names.includes("users"), "users table exists")
    assert.ok(names.includes("feedback"), "feedback table exists")
    assert.ok(names.includes("analytics_events"), "analytics_events table exists")
    assert.ok(names.includes("analytics_metrics"), "analytics_metrics table exists")
})

test("can create admin user directly", async () => {
    await pool.query(
        `DELETE FROM users WHERE username = 'testadmin'`
    )
    const result = await pool.query(
        `
        INSERT INTO users (username, username_key, email, password_hash, role)
        VALUES ('testadmin', 'testadmin', 'testadmin@mimita.test',
                'scrypt:salt:a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6',
                'admin')
        RETURNING id, username, role
        `
    )
    assert.equal(result.rows[0].username, "testadmin")
    assert.equal(result.rows[0].role, "admin")
    testUserId = result.rows[0].id
})

test("can create regular user", async () => {
    const result = await pool.query(
        `
        INSERT INTO users (username, username_key, email, password_hash, role)
        VALUES ('testuser', 'testuser', 'testuser@mimita.test',
                'scrypt:salt:a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6a1b2c3d4e5f6',
                'user')
        RETURNING id, role
        `
    )
    assert.equal(result.rows[0].role, "user")
})

test("role column defaults to user", async () => {
    const result = await pool.query(
        `SELECT column_default FROM information_schema.columns
         WHERE table_name = 'users' AND column_name = 'role'`
    )
    assert.ok(result.rows[0]?.column_default?.includes("user"))
})

test("admin session can be inserted", async () => {
    const crypto = await import("crypto")
    const token = crypto.randomBytes(32).toString("base64url")
    const tokenHash = crypto.createHash("sha256").update("test-secret").update(":").update(token).digest("hex")

    await pool.query(
        `
        INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
        VALUES ($1, $2, 'test', '127.0.0.1', NOW() + INTERVAL '1 day')
        `,
        [testUserId, tokenHash]
    )

    adminToken = token
    const verify = await pool.query(
        `
        SELECT u.id, u.role FROM sessions s
        JOIN users u ON u.id = s.user_id
        WHERE s.token_hash = $1 AND s.revoked_at IS NULL
        `,
        [tokenHash]
    )
    assert.equal(verify.rows[0].role, "admin")
})

test("feedback can be submitted", async () => {
    const { submitFeedback } = await import("./feedback.js")
    const result = await submitFeedback({
        selectedPresets: ["Cool Site"],
        customFeedback: "test feedback",
        contactInfo: "test@test.com",
        pageUrl: "/test",
        userId: null
    })
    assert.ok(result.id > 0)
    assert.ok(result.created_at)
})

test("feedback can be retrieved", async () => {
    const { getFeedback } = await import("./feedback.js")
    const feedback = await getFeedback(10, 0)
    assert.ok(feedback.length >= 1)
    assert.ok(feedback[0].selected_presets)
})

test("trackEvent inserts analytics event", async () => {
    const { trackEvent } = await import("./analytics.js")
    await trackEvent("page_visit", {
        event_data: { page: "/test" },
        ip_address: "127.0.0.1",
        page_url: "/test"
    })
    const result = await pool.query(
        `SELECT COUNT(*) AS count FROM analytics_events WHERE event_name = 'page_visit'`
    )
    assert.ok(Number(result.rows[0].count) >= 1)
})

test("getMetrics returns expected structure", async () => {
    const { getMetrics } = await import("./analytics.js")
    const metrics = await getMetrics()
    assert.ok(typeof metrics.total_users === "number")
    assert.ok(typeof metrics.active_sessions === "number")
    assert.ok(typeof metrics.total_feedback === "number")
    assert.ok(Array.isArray(metrics.feedback_by_category))
    assert.ok("site_visitors_today" in metrics)
})

test("refreshMetrics populates metrics", async () => {
    const { refreshMetrics } = await import("./analytics.js")
    await refreshMetrics()
    const result = await pool.query(
        `SELECT COUNT(*) AS count FROM analytics_metrics WHERE metric_date = CURRENT_DATE`
    )
    assert.ok(Number(result.rows[0].count) > 0)
})

test("debug health check query", async () => {
    const result = await pool.query("SELECT 1 AS ping")
    assert.equal(result.rows[0].ping, 1)
})

test("debug error-catalog has expected sections", async () => {
    const { ERROR_CATALOG } = await import("./debug.js")
    assert.ok(ERROR_CATALOG.auth)
    assert.ok(ERROR_CATALOG.admin)
    assert.ok(ERROR_CATALOG.feedback)
    assert.ok(ERROR_CATALOG.analytics)
    assert.ok(ERROR_CATALOG.database)
})

test("can cleanup test data", async () => {
    await pool.query(`DELETE FROM feedback WHERE custom_feedback = 'test feedback'`)
    await pool.query(`DELETE FROM sessions WHERE user_id = $1`, [testUserId])
    await pool.query(`DELETE FROM users WHERE username IN ('testadmin', 'testuser')`)
})
