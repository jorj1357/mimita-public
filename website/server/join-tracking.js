// 09 09 2026
/* purpose
* Track join events for the MiMITA website.
* Record where users come from (website signup, game client, client login, link code).
* Aggregate daily join metrics for the admin dashboard.
* DOES NOT render analytics UI.
*/

import { pool } from "./db.js"

export async function trackJoinEvent(userId, source, req = null) {
    try {
        const validSources = ["website_signup", "game_client", "client_login", "link_code"]
        const cleanSource = validSources.includes(source) ? source : "unknown"

        let ipAddress = null
        let userAgent = null
        if (req) {
            ipAddress = req.get("x-forwarded-for")?.split(",")[0]?.trim()
                || req.socket?.remoteAddress
                || null
            userAgent = req.get("user-agent") || null
        }

        await pool.query(
            `INSERT INTO join_events (user_id, source, ip_address, user_agent)
             VALUES ($1, $2, $3, $4)`,
            [userId || null, cleanSource, ipAddress, userAgent]
        )
    }
    catch (error) {
        console.error(`[JOIN TRACKING] failed: ${error.message}`)
    }
}

export async function getJoinMetrics() {
    const today = new Date().toISOString().split("T")[0]

    // Joins by source today
    const todayBySource = await pool.query(
        `SELECT source, COUNT(*) AS count
         FROM join_events
         WHERE created_at >= CURRENT_DATE
         GROUP BY source
         ORDER BY count DESC`
    )

    // Joins by source last 7 days
    const weekBySource = await pool.query(
        `SELECT source, COUNT(*) AS count
         FROM join_events
         WHERE created_at >= CURRENT_DATE - INTERVAL '7 days'
         GROUP BY source
         ORDER BY count DESC`
    )

    // Joins by source last 30 days
    const monthBySource = await pool.query(
        `SELECT source, COUNT(*) AS count
         FROM join_events
         WHERE created_at >= CURRENT_DATE - INTERVAL '30 days'
         GROUP BY source
         ORDER BY count DESC`
    )

    // Total joins all time
    const allTime = await pool.query(
        `SELECT COUNT(*) AS count FROM join_events`
    )

    // Joins per day last 30 days (for chart)
    const dailyJoins = await pool.query(
        `SELECT
            DATE(created_at) AS date,
            source,
            COUNT(*) AS count
         FROM join_events
         WHERE created_at >= CURRENT_DATE - INTERVAL '30 days'
         GROUP BY DATE(created_at), source
         ORDER BY date DESC, source`
    )

    return {
        today: todayBySource.rows.reduce((acc, r) => { acc[r.source] = Number(r.count); return acc }, {}),
        thisWeek: weekBySource.rows.reduce((acc, r) => { acc[r.source] = Number(r.count); return acc }, {}),
        thisMonth: monthBySource.rows.reduce((acc, r) => { acc[r.source] = Number(r.count); return acc }, {}),
        allTime: Number(allTime.rows[0].count),
        daily: dailyJoins.rows
    }
}
