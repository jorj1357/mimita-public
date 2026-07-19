// 07 19 2026, 12 00
/* purpose
* Configure the website Postgres connection and bootstrap database schema.
* Create account, analytics, auth, game, email, and session tables locally or on VPS.
* Keep schema setup centralized for server startup and tests.
* DOES NOT render website pages or game UI.
* DOES NOT store client-side auth tokens.
* DOES NOT implement request handlers directly.
*/

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

function logMigration(sql) {
    console.log(`[DB] SQL: ${sql.substring(0, 120)}`)
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
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS email_visible BOOLEAN NOT NULL DEFAULT FALSE`,
    `CREATE TABLE IF NOT EXISTS rate_limits (
        id BIGSERIAL PRIMARY KEY,
        key_name TEXT NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL
    )`,
    `CREATE INDEX IF NOT EXISTS rate_limits_key_idx ON rate_limits(key_name, created_at DESC)`,

    // ── Account System v2 ───────────────────────────────────────────────

    `ALTER TABLE users ADD COLUMN IF NOT EXISTS avatar_data JSONB`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS display_name TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS last_login_at TIMESTAMPTZ`,

    `CREATE TABLE IF NOT EXISTS game_stats (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE UNIQUE,
        wins INT NOT NULL DEFAULT 0,
        losses INT NOT NULL DEFAULT 0,
        kills INT NOT NULL DEFAULT 0,
        deaths INT NOT NULL DEFAULT 0,
        games_played INT NOT NULL DEFAULT 0,
        playtime_seconds BIGINT NOT NULL DEFAULT 0,
        highest_mmr INT NOT NULL DEFAULT 5000,
        current_mmr INT NOT NULL DEFAULT 5000,
        accuracy REAL NOT NULL DEFAULT 0.0,
        headshots INT NOT NULL DEFAULT 0,
        best_kill_streak INT NOT NULL DEFAULT 0,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS game_stats_mmr_idx ON game_stats(current_mmr DESC)`,
    `CREATE INDEX IF NOT EXISTS game_stats_wins_idx ON game_stats(wins DESC)`,

    `CREATE TABLE IF NOT EXISTS match_history (
        id BIGSERIAL PRIMARY KEY,
        match_id TEXT NOT NULL UNIQUE,
        map_name TEXT NOT NULL DEFAULT '',
        game_mode TEXT NOT NULL DEFAULT '',
        duration_seconds INT NOT NULL DEFAULT 0,
        winner_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS match_history_created_idx ON match_history(created_at DESC)`,

    `CREATE TABLE IF NOT EXISTS match_participants (
        id BIGSERIAL PRIMARY KEY,
        match_id TEXT NOT NULL REFERENCES match_history(match_id) ON DELETE CASCADE,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        username TEXT NOT NULL DEFAULT '',
        mmr_before INT NOT NULL DEFAULT 0,
        mmr_after INT NOT NULL DEFAULT 0,
        kills INT NOT NULL DEFAULT 0,
        deaths INT NOT NULL DEFAULT 0,
        accuracy REAL NOT NULL DEFAULT 0.0,
        headshots INT NOT NULL DEFAULT 0,
        damage_dealt INT NOT NULL DEFAULT 0,
        team INT NOT NULL DEFAULT 0,
        won BOOLEAN NOT NULL DEFAULT FALSE,
        UNIQUE(match_id, user_id)
    )`,
    `CREATE INDEX IF NOT EXISTS match_participants_user_idx ON match_participants(user_id, created_at DESC)`,

    `CREATE TABLE IF NOT EXISTS user_settings (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE UNIQUE,
        settings_json JSONB NOT NULL DEFAULT '{}',
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    `CREATE TABLE IF NOT EXISTS user_inventory (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE UNIQUE,
        inventory_json JSONB NOT NULL DEFAULT '{"version":1,"items":[],"equipped":{}}',
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    `CREATE TABLE IF NOT EXISTS user_loadouts (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE UNIQUE,
        loadout_json JSONB NOT NULL DEFAULT '{"version":1,"weapons":{},"cosmetics":{}}',
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    `CREATE TABLE IF NOT EXISTS user_titles (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE UNIQUE,
        titles_json JSONB NOT NULL DEFAULT '{"version":1,"unlocked":[],"equipped":""}',
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    // ── Client Login Codes ───────────────────────────────────────────────

    `CREATE TABLE IF NOT EXISTS client_login_codes (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        code_hash TEXT NOT NULL,
        expires_at TIMESTAMPTZ NOT NULL,
        used_at TIMESTAMPTZ,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        ip_address TEXT,
        user_agent TEXT
    )`,
    `CREATE INDEX IF NOT EXISTS client_login_codes_hash_idx ON client_login_codes(code_hash, expires_at)`,

    // ── Competitive MMR Migration ─────────────────────────────────────────
    // Fix existing profiles that have old default MMR=1000 and never played.
    // Safe: only affects profiles with wins=0, losses=0, games_played=0.
    `UPDATE game_stats
     SET current_mmr = 5000, highest_mmr = 5000
     WHERE current_mmr = 1000
       AND wins = 0
       AND losses = 0
       AND games_played = 0`,

    // ── Mini Game Scores (Aim Test, etc.) ────────────────────────────────

    `CREATE TABLE IF NOT EXISTS game_scores (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        game_id TEXT NOT NULL,
        score_value REAL NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        deleted_at TIMESTAMPTZ
    )`,
    `CREATE INDEX IF NOT EXISTS game_scores_user_game_idx ON game_scores(user_id, game_id, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS game_scores_leaderboard_idx ON game_scores(game_id, score_value ASC) WHERE deleted_at IS NULL`,

    // ── Email Campaign System ─────────────────────────────────────────

    `CREATE TABLE IF NOT EXISTS user_tags (
        id BIGSERIAL PRIMARY KEY,
        name TEXT NOT NULL UNIQUE,
        description TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `INSERT INTO user_tags (name, description) VALUES
        ('player', 'Game player'),
        ('tester', 'Beta tester'),
        ('admin', 'Administrator'),
        ('moderator', 'Moderator'),
        ('newsletter', 'Newsletter subscriber'),
        ('alpha', 'Alpha tester'),
        ('beta', 'Beta tester'),
        ('developer', 'Developer')
    ON CONFLICT (name) DO NOTHING`,

    `CREATE TABLE IF NOT EXISTS email_templates (
        id BIGSERIAL PRIMARY KEY,
        name TEXT NOT NULL,
        description TEXT NOT NULL DEFAULT '',
        html_body TEXT NOT NULL,
        created_by TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    `CREATE TABLE IF NOT EXISTS email_campaigns (
        id BIGSERIAL PRIMARY KEY,
        subject TEXT NOT NULL DEFAULT '',
        html_body TEXT NOT NULL DEFAULT '',
        template_id BIGINT REFERENCES email_templates(id) ON DELETE SET NULL,
        status TEXT NOT NULL DEFAULT 'draft'
            CHECK (status IN ('draft','sending','sent','failed','partial')),
        total_recipients INT NOT NULL DEFAULT 0,
        delivered_count INT NOT NULL DEFAULT 0,
        failed_count INT NOT NULL DEFAULT 0,
        skipped_count INT NOT NULL DEFAULT 0,
        rejected_count INT NOT NULL DEFAULT 0,
        invalid_email_count INT NOT NULL DEFAULT 0,
        smtp_error_count INT NOT NULL DEFAULT 0,
        db_error_count INT NOT NULL DEFAULT 0,
        sent_at TIMESTAMPTZ,
        created_by TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,

    `CREATE TABLE IF NOT EXISTS email_campaign_recipients (
        id BIGSERIAL PRIMARY KEY,
        campaign_id BIGINT NOT NULL REFERENCES email_campaigns(id) ON DELETE CASCADE,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        email TEXT NOT NULL,
        username TEXT NOT NULL DEFAULT '',
        status TEXT NOT NULL DEFAULT 'pending'
            CHECK (status IN ('pending','sending','delivered','failed','skipped','rejected','invalid_email','smtp_error','db_error')),
        error_message TEXT NOT NULL DEFAULT '',
        sent_at TIMESTAMPTZ,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS ecr_campaign_id_idx ON email_campaign_recipients(campaign_id)`,
    `CREATE INDEX IF NOT EXISTS ecr_status_idx ON email_campaign_recipients(campaign_id, status)`,

    // ── User-Tag junction table for future segmentation ──────────────
    `CREATE TABLE IF NOT EXISTS user_tag_assignments (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        tag_id BIGINT NOT NULL REFERENCES user_tags(id) ON DELETE CASCADE,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        UNIQUE(user_id, tag_id)
    )`,

    // ── Game Auth Session Columns ─────────────────────────────────
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS refresh_token_hash TEXT`,
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS device_id TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS device_name TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS platform TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS client_build TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE sessions ADD COLUMN IF NOT EXISTS last_used_at TIMESTAMPTZ`,
    `CREATE INDEX IF NOT EXISTS sessions_refresh_token_idx ON sessions(refresh_token_hash) WHERE refresh_token_hash IS NOT NULL`,
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

    // Print email campaign schema
    const emailTables = ["user_tags", "email_templates", "email_campaigns", "email_campaign_recipients", "user_tag_assignments"]
    for (const table of emailTables) {
        try {
            const schema = await pool.query(`
                SELECT column_name, data_type, is_nullable, column_default
                FROM information_schema.columns
                WHERE table_name = $1
                ORDER BY ordinal_position
            `, [table])
            console.log(`[DB] Schema for ${table}:`)
            for (const row of schema.rows) {
                console.log(`[DB]   ${row.column_name} (${row.data_type}) nullable=${row.is_nullable} default=${row.column_default || 'none'}`)
            }
            const count = await pool.query(`SELECT COUNT(*)::int AS cnt FROM ${table}`)
            console.log(`[DB]   ${table} row count: ${count.rows[0].cnt}`)
            if (count.rows[0].cnt > 0) {
                const sample = await pool.query(`SELECT * FROM ${table} LIMIT 5`)
                console.log(`[DB]   ${table} sample rows:`, JSON.stringify(sample.rows, null, 2))
            }
        } catch (e) {
            console.log(`[DB] Could not inspect ${table}: ${e.message}`)
        }
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
            "analytics_consent",
            "analytics_deletion_requests",
            "analytics_audit_log",
            "analytics_metrics",
            "feedback",
    "admin_sessions",
    "user_tags",
    "email_templates",
    "email_campaigns",
    "email_campaign_recipients"
]
    }
}
