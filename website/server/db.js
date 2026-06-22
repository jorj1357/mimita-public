import pg from "pg"

const { Pool } = pg

export const pool = new Pool({
    connectionString: process.env.DATABASE_URL || undefined,
    user: process.env.DATABASE_URL ? undefined : process.env.DB_USER,
    host: process.env.DATABASE_URL ? undefined : process.env.DB_HOST,
    database: process.env.DATABASE_URL ? undefined : process.env.DB_NAME,
    password: process.env.DATABASE_URL ? undefined : process.env.DB_PASSWORD,
    port: process.env.DATABASE_URL
        ? undefined
        : Number(process.env.DB_PORT || 5432),
    ssl: process.env.DB_SSL === "true"
        ? { rejectUnauthorized: false }
        : undefined
})

const LOG_ENABLED = process.env.DB_LOG !== "false"

function truncate(str, max = 150) {
    if (!str) return ""
    const s = typeof str === "string" ? str : String(str)
    if (s.length <= max) return s
    return s.slice(0, max) + "..."
}

function formatQuery(text) {
    return truncate(text.replace(/\s+/g, " ").trim())
}

// Wrap pool.query with logging
const originalQuery = pool.query.bind(pool)
pool.query = async function loggedQuery(text, params) {
    const start = Date.now()
    const queryLabel = formatQuery(text)
    try {
        const result = await originalQuery(text, params)
        const duration = Date.now() - start
        if (LOG_ENABLED) {
            const logData = {
                query: queryLabel,
                duration: `${duration}ms`,
                rows: result.rowCount ?? 0
            }
            console.log(`[DB QUERY] ${JSON.stringify(logData)}`)
        }
        return result
    }
    catch (error) {
        const duration = Date.now() - start
        const logData = {
            query: queryLabel,
            duration: `${duration}ms`,
            error: error.message,
            code: error.code || "unknown"
        }
        console.log(`[DB ERROR] ${JSON.stringify(logData)}`)

        if (error.code === "ECONNREFUSED") {
            console.log("[DB] Could not connect to PostgreSQL.")
            console.log("[DB] Possible causes:")
            console.log("[DB]   - PostgreSQL is not running")
            console.log("[DB]   - wrong DB_HOST or DB_PORT in .env")
            console.log("[DB]   - firewall blocking port 5432")
            console.log("[DB]   - invalid credentials (DB_USER / DB_PASSWORD)")
            console.log("[DB] Connection string expected:")
            console.log(`[DB]   DATABASE_URL=postgresql://${process.env.DB_USER || "mimita_user"}:${process.env.DB_PASSWORD ? "***" : ""}@${process.env.DB_HOST || "localhost"}:${process.env.DB_PORT || 5432}/${process.env.DB_NAME || "mimita_db"}`)
        }
        if (error.code === "42P01") {
            console.log("[DB] Table does not exist. Run migrations:")
            console.log("[DB]   npm run migrate")
        }
        if (error.code === "23505") {
            console.log("[DB] Duplicate key violation.")
        }
        if (error.code === "28P01") {
            console.log("[DB] Invalid database password.")
        }

        throw error
    }
}

const originalConnect = pool.connect.bind(pool)
pool.connect = async function loggedConnect() {
    try {
        const client = await originalConnect()
        return client
    }
    catch (error) {
        console.log(`[DB] Connection failed: ${error.message}`)
        throw error
    }
}

export async function runMigrations() {
    console.log("[DB] Running migrations...")
    try {
        await pool.query(`
            CREATE TABLE IF NOT EXISTS newsletter (
                id BIGSERIAL PRIMARY KEY,
                email TEXT UNIQUE NOT NULL,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE TABLE IF NOT EXISTS users (
                id BIGSERIAL PRIMARY KEY,
                username TEXT NOT NULL,
                username_key TEXT NOT NULL UNIQUE,
                email TEXT NOT NULL UNIQUE,
                password_hash TEXT NOT NULL,
                bio TEXT NOT NULL DEFAULT '',
                role TEXT NOT NULL DEFAULT 'user'
                    CHECK (role IN ('owner', 'admin', 'moderator', 'user')),
                email_notifications_enabled BOOLEAN NOT NULL DEFAULT TRUE,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                deleted_at TIMESTAMPTZ
            );

            CREATE TABLE IF NOT EXISTS sessions (
                id BIGSERIAL PRIMARY KEY,
                user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                token_hash TEXT NOT NULL UNIQUE,
                user_agent TEXT,
                ip_address TEXT,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                expires_at TIMESTAMPTZ NOT NULL,
                revoked_at TIMESTAMPTZ
            );

            CREATE INDEX IF NOT EXISTS sessions_user_id_idx
                ON sessions(user_id);

            CREATE INDEX IF NOT EXISTS sessions_active_token_idx
                ON sessions(token_hash, expires_at)
                WHERE revoked_at IS NULL;

            CREATE TABLE IF NOT EXISTS password_change_codes (
                id BIGSERIAL PRIMARY KEY,
                user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                code_hash TEXT NOT NULL,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                expires_at TIMESTAMPTZ NOT NULL,
                verified_at TIMESTAMPTZ,
                used_at TIMESTAMPTZ,
                request_ip TEXT,
                request_user_agent TEXT
            );

            CREATE INDEX IF NOT EXISTS password_change_codes_user_id_idx
                ON password_change_codes(user_id, created_at DESC);

            CREATE TABLE IF NOT EXISTS analytics_events (
                id BIGSERIAL PRIMARY KEY,
                event_name TEXT NOT NULL,
                event_data JSONB DEFAULT '{}',
                user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
                ip_address TEXT,
                page_url TEXT,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE INDEX IF NOT EXISTS analytics_events_name_idx
                ON analytics_events(event_name, created_at DESC);

            CREATE INDEX IF NOT EXISTS analytics_events_created_idx
                ON analytics_events(created_at DESC);

            CREATE TABLE IF NOT EXISTS analytics_metrics (
                id BIGSERIAL PRIMARY KEY,
                metric_date DATE NOT NULL DEFAULT CURRENT_DATE,
                metric_name TEXT NOT NULL,
                metric_value BIGINT NOT NULL DEFAULT 0,
                UNIQUE(metric_date, metric_name)
            );

            CREATE INDEX IF NOT EXISTS analytics_metrics_date_idx
                ON analytics_metrics(metric_date DESC);

            CREATE TABLE IF NOT EXISTS feedback (
                id BIGSERIAL PRIMARY KEY,
                selected_presets TEXT[] NOT NULL DEFAULT '{}',
                custom_feedback TEXT NOT NULL DEFAULT '',
                contact_info TEXT NOT NULL DEFAULT '',
                page_url TEXT NOT NULL DEFAULT '',
                user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
                status TEXT NOT NULL DEFAULT 'new'
                    CHECK (status IN ('new', 'reviewed', 'completed', 'ignored')),
                category TEXT NOT NULL DEFAULT 'general',
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
            );

            CREATE INDEX IF NOT EXISTS feedback_status_idx
                ON feedback(status, created_at DESC);

            CREATE INDEX IF NOT EXISTS feedback_created_idx
                ON feedback(created_at DESC);

            CREATE TABLE IF NOT EXISTS admin_sessions (
                id BIGSERIAL PRIMARY KEY,
                token_hash TEXT NOT NULL UNIQUE,
                created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
                expires_at TIMESTAMPTZ NOT NULL
            );

            CREATE INDEX IF NOT EXISTS admin_sessions_token_idx
                ON admin_sessions(token_hash)
                WHERE expires_at > CURRENT_TIMESTAMP;
        `)
        console.log("[DB] Migrations complete.")
    }
    catch (error) {
        console.log(`[DB] Migration failed: ${error.message}`)
        throw error
    }
}

export function getDbConfig() {
    return {
        host: process.env.DB_HOST || "localhost",
        port: Number(process.env.DB_PORT || 5432),
        database: process.env.DB_NAME || "mimita_db",
        user: process.env.DB_USER || "mimita_user",
        hasPassword: Boolean(process.env.DB_PASSWORD),
        connectionString: process.env.DATABASE_URL ? "configured" : "not set",
        expectedTables: [
            "newsletter",
            "users",
            "sessions",
            "password_change_codes",
            "analytics_events",
            "analytics_metrics",
            "feedback",
            "admin_sessions"
        ]
    }
}
