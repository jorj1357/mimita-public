// 09 06 2026, 14 40
/* purpose
* Inspect database schema and allowlisted aggregate progression values.
* Keep credentials on the host and support execution over SSH standard input.
* Enforce a bounded repeatable-read, read-only transaction.
* DOES NOT import application startup or execute migrations.
* DOES NOT select private records or accept arbitrary SQL arguments.
* DOES NOT write files, modify data, or change services.
*/
import dotenv from 'dotenv'
import pg from 'pg'
dotenv.config({ quiet: true })
const pool = new pg.Pool({
    connectionString: process.env.DATABASE_URL || undefined,
    ...(process.env.DATABASE_URL ? {} : {
        user: process.env.DB_USER, host: process.env.DB_HOST,
        database: process.env.DB_NAME, password: process.env.DB_PASSWORD,
        port: Number(process.env.DB_PORT || 5432)
    }),
    ssl: process.env.DB_SSL === 'true' ? { rejectUnauthorized: false } : undefined,
    max: 1, connectionTimeoutMillis: 5000,
    options: '-c default_transaction_read_only=on -c statement_timeout=10000 -c lock_timeout=2000'
})
let client
try {
    client = await pool.connect()
    await client.query('BEGIN ISOLATION LEVEL REPEATABLE READ READ ONLY')
    const report = { auditedAt: new Date().toISOString(), readOnly: true }
    report.tables = (await client.query("SELECT tablename FROM pg_tables WHERE schemaname='public' ORDER BY tablename")).rows.map(r => r.tablename)
    report.columns = (await client.query("SELECT table_name,column_name,data_type,is_nullable FROM information_schema.columns WHERE table_schema='public' ORDER BY table_name,ordinal_position")).rows
    report.constraints = (await client.query("SELECT table_name,constraint_name,constraint_type FROM information_schema.table_constraints WHERE table_schema='public' ORDER BY table_name,constraint_name")).rows
    report.indexes = (await client.query("SELECT tablename,indexname,indexdef FROM pg_indexes WHERE schemaname='public' ORDER BY tablename,indexname")).rows
    report.counts = {}
    for (const table of ['users', 'game_stats', 'processed_events', 'kill_events', 'player_kill_relationships', 'progression_sessions', 'progression_players', 'schema_migrations']) {
        report.counts[table] = report.tables.includes(table)
            ? (await client.query(`SELECT count(*) AS count FROM ${table}`)).rows[0].count : null
    }
    const columns = new Set(report.columns.filter(r => r.table_name === 'game_stats').map(r => r.column_name))
    report.progression = {}
    for (const field of ['gold', 'total_xp', 'lifetime_player_kills', 'lifetime_npc_kills', 'lifetime_deaths', 'playtime_seconds', 'playtime_ticks']) {
        report.progression[field] = columns.has(field)
            ? (await client.query(`SELECT coalesce(sum(${field}),0) AS total, count(*) FILTER (WHERE ${field}<0) AS negative_rows FROM game_stats`)).rows[0] : null
    }
    if (report.tables.includes('users') && columns.has('user_id')) {
        report.consistency = (await client.query(`SELECT
            (SELECT count(*) FROM users u LEFT JOIN game_stats g ON g.user_id=u.id WHERE g.user_id IS NULL) AS users_without_stats,
            (SELECT count(*) FROM game_stats g LEFT JOIN users u ON u.id=g.user_id WHERE u.id IS NULL) AS stats_without_users,
            (SELECT count(*) FROM (SELECT user_id FROM game_stats GROUP BY user_id HAVING count(*)>1) d) AS duplicate_stats_accounts`)).rows[0]
    }
    await client.query('ROLLBACK')
    console.log(JSON.stringify(report, null, 2))
} catch (error) {
    console.error(JSON.stringify({ success: false, error: 'database_audit_failed', code: /^[A-Z0-9_]{2,40}$/.test(error.code || '') ? error.code : 'UNKNOWN' }))
    process.exitCode = 1
} finally {
    client?.release()
    await pool.end()
}
