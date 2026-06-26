import dotenv from "dotenv"
dotenv.config()

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

const MIGRATION_STATEMENTS = [
    `CREATE TABLE IF NOT EXISTS newsletter (
        id BIGSERIAL PRIMARY KEY,
        email TEXT UNIQUE NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE TABLE IF NOT EXISTS users (
        id BIGSERIAL PRIMARY KEY,
        username TEXT NOT NULL,
        username_key TEXT NOT NULL UNIQUE,
        email TEXT NOT NULL UNIQUE,
        password_hash TEXT NOT NULL,
        bio TEXT NOT NULL DEFAULT '',
        avatar_url TEXT NOT NULL DEFAULT '',
        supporter_tier TEXT NOT NULL DEFAULT 'free'
            CHECK (supporter_tier IN ('free', 'vip', 'super_vip', 'ultra_vip', 'moderator', 'admin', 'owner')),
        role TEXT NOT NULL DEFAULT 'user'
            CHECK (role IN ('owner', 'admin', 'moderator', 'user')),
        email_notifications_enabled BOOLEAN NOT NULL DEFAULT TRUE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        deleted_at TIMESTAMPTZ
    )`,
    `CREATE TABLE IF NOT EXISTS sessions (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        token_hash TEXT NOT NULL UNIQUE,
        user_agent TEXT,
        ip_address TEXT,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL,
        revoked_at TIMESTAMPTZ
    )`,
    `CREATE INDEX IF NOT EXISTS sessions_user_id_idx ON sessions(user_id)`,
    `CREATE INDEX IF NOT EXISTS sessions_active_token_idx ON sessions(token_hash, expires_at) WHERE revoked_at IS NULL`,
    `CREATE TABLE IF NOT EXISTS password_change_codes (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        code_hash TEXT NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL,
        verified_at TIMESTAMPTZ,
        used_at TIMESTAMPTZ,
        request_ip TEXT,
        request_user_agent TEXT
    )`,
    `CREATE INDEX IF NOT EXISTS password_change_codes_user_id_idx ON password_change_codes(user_id, created_at DESC)`,
    `CREATE TABLE IF NOT EXISTS analytics_events (
        id BIGSERIAL PRIMARY KEY,
        event_name TEXT NOT NULL,
        event_data JSONB DEFAULT '{}',
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        ip_address TEXT,
        page_url TEXT,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS analytics_events_name_idx ON analytics_events(event_name, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS analytics_events_created_idx ON analytics_events(created_at DESC)`,
    `CREATE TABLE IF NOT EXISTS analytics_consent (
        id BIGSERIAL PRIMARY KEY,
        anonymous_id TEXT NOT NULL UNIQUE,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        username TEXT NOT NULL DEFAULT '',
        analytics_enabled BOOLEAN NOT NULL DEFAULT TRUE,
        permanently_disabled BOOLEAN NOT NULL DEFAULT FALSE,
        source TEXT NOT NULL DEFAULT 'game',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS analytics_consent_user_idx ON analytics_consent(user_id)`,
    `CREATE TABLE IF NOT EXISTS analytics_deletion_requests (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        anonymous_id TEXT,
        username TEXT NOT NULL DEFAULT '',
        email TEXT NOT NULL DEFAULT '',
        source TEXT NOT NULL DEFAULT 'game',
        status TEXT NOT NULL DEFAULT 'requested'
            CHECK (status IN ('requested', 'reviewing', 'completed', 'rejected')),
        audit JSONB NOT NULL DEFAULT '{}',
        requested_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS analytics_deletion_requests_status_idx ON analytics_deletion_requests(status, requested_at DESC)`,
    `CREATE TABLE IF NOT EXISTS analytics_audit_log (
        id BIGSERIAL PRIMARY KEY,
        action TEXT NOT NULL,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        anonymous_id TEXT,
        details JSONB NOT NULL DEFAULT '{}',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS analytics_audit_log_action_idx ON analytics_audit_log(action, created_at DESC)`,
    `CREATE TABLE IF NOT EXISTS analytics_metrics (
        id BIGSERIAL PRIMARY KEY,
        metric_date DATE NOT NULL DEFAULT CURRENT_DATE,
        metric_name TEXT NOT NULL,
        metric_value BIGINT NOT NULL DEFAULT 0,
        UNIQUE(metric_date, metric_name)
    )`,
    `CREATE INDEX IF NOT EXISTS analytics_metrics_date_idx ON analytics_metrics(metric_date DESC)`,
    `CREATE TABLE IF NOT EXISTS feedback (
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
    )`,
    `CREATE INDEX IF NOT EXISTS feedback_status_idx ON feedback(status, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS feedback_created_idx ON feedback(created_at DESC)`,
    `CREATE TABLE IF NOT EXISTS admin_sessions (
        id BIGSERIAL PRIMARY KEY,
        token_hash TEXT NOT NULL UNIQUE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL
    )`,
    `CREATE INDEX IF NOT EXISTS admin_sessions_token_idx ON admin_sessions(token_hash)`,
    `CREATE TABLE IF NOT EXISTS login_attempts (
        id BIGSERIAL PRIMARY KEY,
        identifier TEXT NOT NULL,
        ip_address TEXT NOT NULL DEFAULT '',
        attempted_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        success BOOLEAN NOT NULL DEFAULT FALSE
    )`,
    `CREATE INDEX IF NOT EXISTS login_attempts_identifier_idx ON login_attempts(identifier, attempted_at DESC)`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS email_verified_at TIMESTAMPTZ`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS email_verification_token TEXT`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_url TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS supporter_tier TEXT NOT NULL DEFAULT 'free' CHECK (supporter_tier IN ('free', 'vip', 'super_vip', 'ultra_vip', 'moderator', 'admin', 'owner'))`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_updated_at TIMESTAMPTZ`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS achievements TEXT[] NOT NULL DEFAULT '{}'`,
    `CREATE TABLE IF NOT EXISTS rate_limits (
        id BIGSERIAL PRIMARY KEY,
        key_name TEXT NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL
    )`,
    `CREATE INDEX IF NOT EXISTS rate_limits_key_idx ON rate_limits(key_name, created_at DESC)`
]

export async function runMigrations() {
    console.log("[DB] Running migrations...")
    for (const sql of MIGRATION_STATEMENTS) {
        try {
            await pool.query(sql)
        }
        catch (error) {
            console.log(`[DB] Migration step failed: ${error.message.substring(0, 100)}`)
            console.log(`[DB] SQL: ${sql.substring(0, 80)}...`)
        }
    }
    console.log("[DB] Migrations complete.")
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
            "analytics_consent",
            "analytics_deletion_requests",
            "analytics_audit_log",
            "analytics_metrics",
            "feedback",
            "admin_sessions"
        ]
    }
}
