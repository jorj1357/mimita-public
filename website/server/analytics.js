import { pool } from "./db.js"

const DEFAULT_METRICS = [
    "site_visitors_today",
    "site_visitors_7d",
    "site_visitors_30d",
    "site_visitors_all",
    "downloads_today",
    "downloads_7d",
    "downloads_30d",
    "downloads_all",
    "accounts_created_today",
    "accounts_created_7d",
    "accounts_created_30d",
    "accounts_created_all",
    "dau",
    "wau",
    "mau",
    "unique_players_today",
    "game_opens_today",
    "sessions_gt_1m",
    "sessions_gt_5m",
    "sessions_gt_10m",
    "sessions_gt_30m",
    "sessions_gt_1h",
    "avg_session_length_seconds",
    "matches_played",
    "avg_matches_per_user",
    "first_match_played_count",
    "retention_1d",
    "retention_7d",
    "retention_30d",
    "retention_90d",
    "retention_365d",
    "returning_users_today",
    "discord_joins",
    "messages_sent",
    "friend_requests",
    "profiles_viewed",
    "replays_saved",
    "replays_shared",
    "revenue_today",
    "revenue_30d",
    "revenue_all",
    "donation_page_visits",
    "donations_today",
    "donations_30d",
    "donations_all",
    "donors_count",
    "avg_donation",
    "donation_conversion_pct",
    "donation_page_exit_rate",
    "crashes_today",
    "disconnects_today",
    "failed_downloads",
    "failed_logins",
    "avg_ping",
    "avg_fps",
    "download_conversion_pct",
    "account_conversion_pct",
    "game_sessions_today",
    "game_sessions_all",
    "analytics_opt_out_rate",
    "analytics_deletion_requests_today",
    "analytics_deletion_requests_all"
]

const MOVEMENT_EVENTS = ["jump", "dash", "air_jump", "wall_jump"]
const FEATURE_EVENTS = [
    "settings_opened",
    "outfit_editor_opened",
    "replay_viewed",
    "map_loaded",
    "map_completed",
    "weapon_used"
]

export async function trackEvent(eventName, data = {}) {
    try {
        await pool.query(
            `
            INSERT INTO analytics_events (event_name, event_data, user_id, ip_address, page_url)
            VALUES ($1, $2, $3, $4, $5)
            `,
            [
                eventName,
                JSON.stringify(data.event_data || {}),
                data.user_id || null,
                data.ip_address || null,
                data.page_url || null
            ]
        )
    }
    catch (error) {
        console.error(`[ANALYTICS] trackEvent failed: ${eventName}`, error.message)
    }
}

export async function getMetrics() {
    const result = await pool.query(
        `
        SELECT metric_name, metric_value
        FROM analytics_metrics
        WHERE metric_date = CURRENT_DATE
        `
    )
    const today = new Map()
    for (const row of result.rows) {
        today.set(row.metric_name, Number(row.metric_value))
    }

    const allTimeResult = await pool.query(
        `
        SELECT metric_name, SUM(metric_value) AS metric_value
        FROM analytics_metrics
        GROUP BY metric_name
        `
    )
    const allTime = new Map()
    for (const row of allTimeResult.rows) {
        allTime.set(row.metric_name, Number(row.metric_value))
    }

    const metrics = {}
    for (const name of DEFAULT_METRICS) {
        metrics[name] = {
            today: today.get(name) || 0,
            allTime: allTime.get(name) || 0
        }
    }

    const userResult = await pool.query(
        `
        SELECT COUNT(*) AS count FROM users WHERE deleted_at IS NULL
        `
    )
    metrics.total_users = Number(userResult.rows[0].count)

    const sessionResult = await pool.query(
        `
        SELECT COUNT(*) AS count
        FROM sessions
        WHERE revoked_at IS NULL AND expires_at > NOW()
        `
    )
    metrics.active_sessions = Number(sessionResult.rows[0].count)

    const feedbackResult = await pool.query(
        `
        SELECT COUNT(*) AS count FROM feedback
        `
    )
    metrics.total_feedback = Number(feedbackResult.rows[0].count)

    const feedbackTodayResult = await pool.query(
        `
        SELECT COUNT(*) AS count
        FROM feedback
        WHERE created_at >= CURRENT_DATE
        `
    )
    metrics.feedback_today = Number(feedbackTodayResult.rows[0].count)

    const feedbackByCategoryResult = await pool.query(
        `
        SELECT category, COUNT(*) AS count
        FROM feedback
        GROUP BY category
        ORDER BY count DESC
        `
    )
    metrics.feedback_by_category = feedbackByCategoryResult.rows.map(r => ({
        category: r.category,
        count: Number(r.count)
    }))

    const presetCountsResult = await pool.query(
        `
        SELECT unnest(selected_presets) AS preset, COUNT(*) AS count
        FROM feedback
        WHERE cardinality(selected_presets) > 0
        GROUP BY preset
        ORDER BY count DESC
        LIMIT 1
        `
    )
    metrics.most_common_preset = presetCountsResult.rowCount
        ? presetCountsResult.rows[0].preset
        : null

    const topPageResult = await pool.query(
        `
        SELECT page_url, COUNT(*) AS count
        FROM feedback
        WHERE page_url != ''
        GROUP BY page_url
        ORDER BY count DESC
        LIMIT 1
        `
    )
    metrics.most_common_page = topPageResult.rowCount
        ? topPageResult.rows[0].page_url
        : null

    if (metrics.site_visitors_all.allTime > 0) {
        metrics.download_conversion_pct = Number(
            ((metrics.downloads_all.allTime / metrics.site_visitors_all.allTime) * 100).toFixed(2)
        )
    }
    if (metrics.downloads_all.allTime > 0) {
        metrics.account_conversion_pct = Number(
            ((metrics.accounts_created_all.allTime / metrics.downloads_all.allTime) * 100).toFixed(2)
        )
    }

    const gameSummary = await getGameAnalyticsSummary()
    Object.assign(metrics, gameSummary)

    return metrics
}

export async function refreshMetrics() {
    const today = new Date().toISOString().split("T")[0]

    // site visitors
    await updateMetric("site_visitors_today", today, await countEvents("page_visit", "day"))

    const days7Ago = new Date(Date.now() - 7 * 86400000).toISOString()
    await updateMetric("site_visitors_7d", today, await countEventsSince("page_visit", days7Ago))
    await updateMetric("site_visitors_30d", today, await countEventsSince("page_visit", new Date(Date.now() - 30 * 86400000).toISOString()))
    await updateMetric("site_visitors_all", today, await countEvents("page_visit", "all"))

    // downloads
    await updateMetric("downloads_today", today, await countEvents("download", "day"))
    await updateMetric("downloads_all", today, await countEvents("download", "all"))

    // accounts
    const accountsToday = await pool.query(
        `SELECT COUNT(*) AS count FROM users WHERE created_at >= CURRENT_DATE AND deleted_at IS NULL`
    )
    await updateMetric("accounts_created_today", today, Number(accountsToday.rows[0].count))

    const accountsAll = await pool.query(
        `SELECT COUNT(*) AS count FROM users WHERE deleted_at IS NULL`
    )
    await updateMetric("accounts_created_all", today, Number(accountsAll.rows[0].count))

    // feedback
    const feedbackToday = await pool.query(
        `SELECT COUNT(*) AS count FROM feedback WHERE created_at >= CURRENT_DATE`
    )
    await updateMetric("feedback_today", today, Number(feedbackToday.rows[0].count))

    await updateMetric("game_sessions_today", today, await countEvents("session_start", "day"))
    await updateMetric("game_sessions_all", today, await countEvents("session_start", "all"))
    await updateMetric("game_opens_today", today, await countEvents("session_start", "day"))
    await updateMetric("crashes_today", today, await countEvents("crash_detected", "day"))
    await updateMetric("disconnects_today", today, await countEvents("disconnect", "day"))
    await updateMetric("failed_logins", today, await countEvents("failed_login", "all"))

    const avgDuration = await pool.query(`
        SELECT COALESCE(AVG((event_data->>'duration_seconds')::numeric), 0) AS value
        FROM analytics_events
        WHERE event_name = 'session_duration'
          AND created_at >= CURRENT_DATE
          AND (event_data->>'duration_seconds') ~ '^[0-9]+(\\.[0-9]+)?$'
    `)
    await updateMetric(
        "avg_session_length_seconds",
        today,
        Math.round(Number(avgDuration.rows[0].value || 0))
    )

    for (const seconds of [60, 300, 600, 1800, 3600]) {
        const result = await pool.query(
            `
            SELECT COUNT(*) AS count
            FROM analytics_events
            WHERE event_name = 'session_duration'
              AND created_at >= CURRENT_DATE
              AND (event_data->>'duration_seconds') ~ '^[0-9]+(\\.[0-9]+)?$'
              AND (event_data->>'duration_seconds')::numeric >= $1
            `,
            [seconds]
        )
        const name = seconds === 60 ? "sessions_gt_1m"
            : seconds === 300 ? "sessions_gt_5m"
            : seconds === 600 ? "sessions_gt_10m"
            : seconds === 1800 ? "sessions_gt_30m"
            : "sessions_gt_1h"
        await updateMetric(name, today, Number(result.rows[0].count))
    }
}

async function getGameAnalyticsSummary() {
    const summary = {}

    const sessions = await pool.query(`
        SELECT
            COUNT(*) FILTER (WHERE created_at >= CURRENT_DATE) AS today,
            COUNT(*) AS all_time
        FROM analytics_events
        WHERE event_name = 'session_start'
    `)
    summary.game_sessions_today = {
        today: Number(sessions.rows[0].today),
        allTime: Number(sessions.rows[0].all_time)
    }
    summary.game_sessions_all = {
        today: Number(sessions.rows[0].all_time),
        allTime: Number(sessions.rows[0].all_time)
    }

    const uniquePlayers = await pool.query(`
        SELECT COUNT(DISTINCT event_data->>'anonymous_id') AS today
        FROM analytics_events
        WHERE created_at >= CURRENT_DATE
          AND event_data ? 'anonymous_id'
    `)
    summary.unique_players_today = {
        today: Number(uniquePlayers.rows[0].today),
        allTime: Number(uniquePlayers.rows[0].today)
    }

    summary.top_maps = await topProperty(
        ["map_loaded", "map_completed"],
        ["map", "map_name", "map_path"],
        8
    )
    summary.top_weapons = await topProperty(["weapon_used"], ["weapon", "weapon_id"], 8)

    const featureResult = await pool.query(
        `
        SELECT event_name, COUNT(*) AS count
        FROM analytics_events
        WHERE event_name = ANY($1)
        GROUP BY event_name
        ORDER BY count DESC
        LIMIT 10
        `,
        [FEATURE_EVENTS]
    )
    summary.top_features = featureResult.rows.map(row => ({
        name: row.event_name,
        count: Number(row.count)
    }))

    const movementResult = await pool.query(
        `
        SELECT event_name, COUNT(*) AS count
        FROM analytics_events
        WHERE event_name = ANY($1)
        GROUP BY event_name
        ORDER BY count DESC
        `,
        [MOVEMENT_EVENTS]
    )
    summary.movement_stats = movementResult.rows.map(row => ({
        name: row.event_name,
        count: Number(row.count)
    }))

    const retentionResult = await pool.query(
        `
        SELECT event_name, COUNT(*) AS count
        FROM analytics_events
        WHERE event_name = ANY($1)
        GROUP BY event_name
        `,
        [["day_1_return", "day_7_return", "day_30_return"]]
    )
    summary.retention_returns = retentionResult.rows.map(row => ({
        name: row.event_name,
        count: Number(row.count)
    }))

    const consentResult = await pool.query(`
        SELECT
            COUNT(*) AS total,
            COUNT(*) FILTER (WHERE analytics_enabled = FALSE OR permanently_disabled = TRUE) AS opted_out
        FROM analytics_consent
    `)
    const totalConsent = Number(consentResult.rows[0].total)
    const optedOut = Number(consentResult.rows[0].opted_out)
    summary.analytics_opt_out_rate = totalConsent > 0
        ? Number(((optedOut / totalConsent) * 100).toFixed(2))
        : 0

    const deletionResult = await pool.query(`
        SELECT
            COUNT(*) FILTER (WHERE requested_at >= CURRENT_DATE) AS today,
            COUNT(*) AS all_time,
            COUNT(*) FILTER (WHERE status = 'requested') AS pending
        FROM analytics_deletion_requests
    `)
    summary.analytics_deletion_requests = {
        today: Number(deletionResult.rows[0].today),
        allTime: Number(deletionResult.rows[0].all_time),
        pending: Number(deletionResult.rows[0].pending)
    }

    return summary
}

async function topProperty(eventNames, keys, limit) {
    const expressions = keys.map(key => `event_data->>'${key}'`).join(", ")
    const result = await pool.query(
        `
        SELECT COALESCE(${expressions}) AS name, COUNT(*) AS count
        FROM analytics_events
        WHERE event_name = ANY($1)
        GROUP BY name
        HAVING COALESCE(${expressions}) IS NOT NULL
           AND COALESCE(${expressions}) <> ''
        ORDER BY count DESC
        LIMIT $2
        `,
        [eventNames, limit]
    )
    return result.rows.map(row => ({
        name: row.name,
        count: Number(row.count)
    }))
}

async function countEvents(eventName, range) {
    let query
    if (range === "day") {
        query = `SELECT COUNT(*) AS count FROM analytics_events WHERE event_name = $1 AND created_at >= CURRENT_DATE`
    }
    else {
        query = `SELECT COUNT(*) AS count FROM analytics_events WHERE event_name = $1`
    }
    const result = await pool.query(query, [eventName])
    return Number(result.rows[0].count)
}

async function countEventsSince(eventName, since) {
    const result = await pool.query(
        `SELECT COUNT(*) AS count FROM analytics_events WHERE event_name = $1 AND created_at >= $2`,
        [eventName, since]
    )
    return Number(result.rows[0].count)
}

async function updateMetric(name, date, value) {
    if (value === undefined || value === null) return
    await pool.query(
        `
        INSERT INTO analytics_metrics (metric_date, metric_name, metric_value)
        VALUES ($1, $2, $3)
        ON CONFLICT (metric_date, metric_name)
        DO UPDATE SET metric_value = EXCLUDED.metric_value
        `,
        [date, name, value]
    )
}

export { DEFAULT_METRICS }
