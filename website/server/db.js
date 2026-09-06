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
import { readFile } from "node:fs/promises"

const { Pool } = pg

pg.types.setTypeParser(20, (val) => parseInt(val, 10))

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

export const MIGRATION_STATEMENTS = [
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
    `CREATE TABLE IF NOT EXISTS password_reset_codes (
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
    `CREATE INDEX IF NOT EXISTS password_reset_codes_user_id_idx ON password_reset_codes(user_id, created_at DESC)`,
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
    `ALTER TABLE users ADD COLUMN IF NOT EXISTS stripe_customer_id TEXT NOT NULL DEFAULT ''`,
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
    `ALTER TABLE match_participants ADD COLUMN IF NOT EXISTS created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP`,
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

    // ── VIP subscriptions, payments, styles, and join tickets ───────────

    `CREATE TABLE IF NOT EXISTS vip_orders (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        tier TEXT NOT NULL CHECK (tier IN ('vip','super_vip','ultra_vip')),
        purchase_type TEXT NOT NULL CHECK (purchase_type IN ('one_month','monthly_subscription','twelve_month')),
        amount_cents INT NOT NULL CHECK (amount_cents > 0),
        currency TEXT NOT NULL DEFAULT 'usd',
        status TEXT NOT NULL DEFAULT 'pending'
            CHECK (status IN ('pending','paid','failed','cancelled','refunded','disputed')),
        stripe_price_id TEXT NOT NULL DEFAULT '',
        stripe_checkout_session_id TEXT NOT NULL DEFAULT '',
        stripe_payment_intent_id TEXT NOT NULL DEFAULT '',
        stripe_subscription_id TEXT NOT NULL DEFAULT '',
        stripe_customer_id TEXT NOT NULL DEFAULT '',
        stripe_event_id TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        paid_at TIMESTAMPTZ
    )`,
    `CREATE INDEX IF NOT EXISTS vip_orders_user_idx ON vip_orders(user_id, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS vip_orders_status_idx ON vip_orders(status, created_at DESC)`,
    `CREATE UNIQUE INDEX IF NOT EXISTS vip_orders_checkout_session_uidx
        ON vip_orders(stripe_checkout_session_id)
        WHERE stripe_checkout_session_id <> ''`,
    `CREATE INDEX IF NOT EXISTS vip_orders_subscription_idx
        ON vip_orders(stripe_subscription_id)
        WHERE stripe_subscription_id <> ''`,

    `CREATE TABLE IF NOT EXISTS vip_entitlements (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        order_id BIGINT REFERENCES vip_orders(id) ON DELETE SET NULL,
        tier TEXT NOT NULL CHECK (tier IN ('vip','super_vip','ultra_vip')),
        source TEXT NOT NULL DEFAULT 'stripe'
            CHECK (source IN ('stripe','subscription','admin','test','refund_adjustment')),
        status TEXT NOT NULL DEFAULT 'active'
            CHECK (status IN ('active','expired','cancelled','refunded','disputed')),
        starts_at TIMESTAMPTZ NOT NULL,
        expires_at TIMESTAMPTZ NOT NULL,
        stripe_checkout_session_id TEXT NOT NULL DEFAULT '',
        stripe_payment_intent_id TEXT NOT NULL DEFAULT '',
        stripe_subscription_id TEXT NOT NULL DEFAULT '',
        stripe_customer_id TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        CHECK (expires_at > starts_at)
    )`,
    `CREATE INDEX IF NOT EXISTS vip_entitlements_user_active_idx
        ON vip_entitlements(user_id, tier, expires_at DESC)
        WHERE status = 'active'`,
    `CREATE INDEX IF NOT EXISTS vip_entitlements_expiry_idx ON vip_entitlements(status, expires_at)`,
    `CREATE INDEX IF NOT EXISTS vip_entitlements_order_idx
        ON vip_entitlements(order_id)
        WHERE order_id IS NOT NULL`,
    `CREATE INDEX IF NOT EXISTS vip_entitlements_subscription_idx
        ON vip_entitlements(stripe_subscription_id)
        WHERE stripe_subscription_id <> ''`,
    `CREATE UNIQUE INDEX IF NOT EXISTS vip_entitlements_subscription_uidx
        ON vip_entitlements(stripe_subscription_id)
        WHERE stripe_subscription_id <> ''`,

    `CREATE TABLE IF NOT EXISTS vip_subscriptions (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        tier TEXT NOT NULL CHECK (tier IN ('vip','super_vip','ultra_vip')),
        stripe_customer_id TEXT NOT NULL DEFAULT '',
        stripe_subscription_id TEXT NOT NULL UNIQUE,
        status TEXT NOT NULL DEFAULT 'incomplete',
        current_period_start TIMESTAMPTZ,
        current_period_end TIMESTAMPTZ,
        cancel_at_period_end BOOLEAN NOT NULL DEFAULT FALSE,
        canceled_at TIMESTAMPTZ,
        latest_invoice_id TEXT NOT NULL DEFAULT '',
        latest_payment_intent_id TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS vip_subscriptions_user_idx
        ON vip_subscriptions(user_id, current_period_end DESC)`,
    `CREATE INDEX IF NOT EXISTS vip_subscriptions_status_idx
        ON vip_subscriptions(status, current_period_end DESC)`,

    `CREATE TABLE IF NOT EXISTS vip_name_styles (
        user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
        style_json JSONB NOT NULL DEFAULT '{"version":1,"kind":"none"}',
        style_revision INT NOT NULL DEFAULT 1,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `ALTER TABLE vip_name_styles ADD COLUMN IF NOT EXISTS style_revision INT NOT NULL DEFAULT 1`,

    `CREATE TABLE IF NOT EXISTS vip_name_presets (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        name TEXT NOT NULL DEFAULT '',
        style_json JSONB NOT NULL,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS vip_name_presets_user_idx ON vip_name_presets(user_id, created_at DESC)`,

    `CREATE TABLE IF NOT EXISTS vip_notifications (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        notification_type TEXT NOT NULL
            CHECK (notification_type IN ('expires_7d','expires_3d','expires_1d','expired')),
        entitlement_key TEXT NOT NULL DEFAULT '',
        sent_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        channel TEXT NOT NULL DEFAULT 'website',
        UNIQUE(user_id, notification_type, entitlement_key, channel)
    )`,
    `CREATE INDEX IF NOT EXISTS vip_notifications_user_idx ON vip_notifications(user_id, sent_at DESC)`,

    `CREATE TABLE IF NOT EXISTS vip_stripe_events (
        event_id TEXT PRIMARY KEY,
        event_type TEXT NOT NULL,
        livemode BOOLEAN NOT NULL DEFAULT FALSE,
        status TEXT NOT NULL DEFAULT 'processing'
            CHECK (status IN ('processing','processed','ignored','failed')),
        source TEXT NOT NULL DEFAULT 'mimita_vip',
        checkout_session_id TEXT NOT NULL DEFAULT '',
        subscription_id TEXT NOT NULL DEFAULT '',
        payment_intent_id TEXT NOT NULL DEFAULT '',
        error_message TEXT NOT NULL DEFAULT '',
        received_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        processed_at TIMESTAMPTZ
    )`,
    `CREATE INDEX IF NOT EXISTS vip_stripe_events_status_idx ON vip_stripe_events(status, received_at DESC)`,

    `CREATE TABLE IF NOT EXISTS vip_join_tickets (
        token_hash TEXT PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        room_code TEXT NOT NULL DEFAULT '',
        issued_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        expires_at TIMESTAMPTZ NOT NULL,
        used_at TIMESTAMPTZ,
        last_result JSONB NOT NULL DEFAULT '{}'
    )`,
    `CREATE INDEX IF NOT EXISTS vip_join_tickets_user_idx ON vip_join_tickets(user_id, issued_at DESC)`,
    `CREATE INDEX IF NOT EXISTS vip_join_tickets_expiry_idx ON vip_join_tickets(expires_at)`,
    // ── Banner Payment Orders (Stripe sandbox pipeline) ─────────────────

    `CREATE TABLE IF NOT EXISTS banner_payment_orders (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        duration_days INT NOT NULL CHECK (duration_days BETWEEN 1 AND 7),
        amount_cents INT NOT NULL CHECK (amount_cents > 0),
        currency TEXT NOT NULL DEFAULT 'usd',
        status TEXT NOT NULL DEFAULT 'pending'
            CHECK (status IN ('pending','paid','failed','cancelled')),
        stripe_checkout_session_id TEXT NOT NULL DEFAULT '',
        stripe_event_id TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        paid_at TIMESTAMPTZ
    )`,
    `CREATE INDEX IF NOT EXISTS banner_payment_orders_status_idx ON banner_payment_orders(status, created_at DESC)`,
    `ALTER TABLE banner_payment_orders ADD COLUMN IF NOT EXISTS updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP`,
    `CREATE INDEX IF NOT EXISTS banner_payment_orders_session_idx ON banner_payment_orders(stripe_checkout_session_id) WHERE stripe_checkout_session_id <> ''`,
    `CREATE INDEX IF NOT EXISTS banner_payment_orders_event_idx ON banner_payment_orders(stripe_event_id) WHERE stripe_event_id <> ''`,

    // ── Site Banners (community banner system) ──────────────────────────

    `CREATE TABLE IF NOT EXISTS site_banners (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        payment_order_id BIGINT UNIQUE REFERENCES banner_payment_orders(id),
        kind TEXT NOT NULL DEFAULT 'free'
            CHECK (kind IN ('free','paid','admin')),
        days INT NOT NULL DEFAULT 1
            CHECK (days BETWEEN 1 AND 365),
        message TEXT NOT NULL DEFAULT '',
        target_url TEXT NOT NULL DEFAULT '',
        background_color TEXT NOT NULL DEFAULT '#000000',
        text_color TEXT NOT NULL DEFAULT '#ffffff',
        status TEXT NOT NULL DEFAULT 'draft'
            CHECK (status IN ('draft','pending_payment','queued','active','expired','disabled','deleted','replaced')),
        starts_at TIMESTAMPTZ,
        expires_at TIMESTAMPTZ,
        disabled_by_admin_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        disabled_reason TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS site_banners_active_idx ON site_banners(status, starts_at)`,
    `CREATE INDEX IF NOT EXISTS site_banners_queue_idx ON site_banners(status, kind, created_at)`,
    `CREATE INDEX IF NOT EXISTS site_banners_order_idx ON site_banners(payment_order_id) WHERE payment_order_id IS NOT NULL`,

    `CREATE TABLE IF NOT EXISTS banner_reports (
        id BIGSERIAL PRIMARY KEY,
        banner_id BIGINT NOT NULL REFERENCES site_banners(id) ON DELETE CASCADE,
        reporter_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        status TEXT NOT NULL DEFAULT 'new'
            CHECK (status IN ('new','reviewed','resolved')),
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS banner_reports_banner_idx ON banner_reports(banner_id, created_at DESC)`,

    // ── Banner schema additions (v2) ─────────────────────────────────────

    `ALTER TABLE banner_payment_orders ADD COLUMN IF NOT EXISTS payment_intent_id TEXT NOT NULL DEFAULT ''`,
    `ALTER TABLE site_banners ADD COLUMN IF NOT EXISTS remaining_days DOUBLE PRECISION`,
    `ALTER TABLE site_banners ADD COLUMN IF NOT EXISTS moderation_state TEXT NOT NULL DEFAULT 'ok'
        CHECK (moderation_state IN ('ok','flagged','removed'))`,
    `ALTER TABLE site_banners ADD COLUMN IF NOT EXISTS refund_status TEXT NOT NULL DEFAULT ''
        CHECK (refund_status IN ('','refund_requested','refunded'))`,

    `CREATE TABLE IF NOT EXISTS support_requests (
        id BIGSERIAL PRIMARY KEY,
        user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        email TEXT NOT NULL DEFAULT '',
        topic TEXT NOT NULL DEFAULT 'other'
            CHECK (topic IN ('user_issue','game_issue','payment_finance','security','other')),
        subject TEXT NOT NULL DEFAULT '',
        message TEXT NOT NULL DEFAULT '',
        url TEXT NOT NULL DEFAULT '',
        banner_order_id BIGINT REFERENCES banner_payment_orders(id) ON DELETE SET NULL,
        status TEXT NOT NULL DEFAULT 'new'
            CHECK (status IN ('new','open','in_progress','resolved')),
        priority TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS support_requests_created_idx ON support_requests(created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS support_requests_topic_idx ON support_requests(topic, created_at DESC)`,
    `ALTER TABLE support_requests DROP CONSTRAINT IF EXISTS support_requests_topic_check`,
    `ALTER TABLE support_requests ADD CONSTRAINT support_requests_topic_check
        CHECK (topic IN ('user_issue','game_issue','payment_finance','vip_purchase','security','other'))`,

    `CREATE TABLE IF NOT EXISTS admin_actions (
        id BIGSERIAL PRIMARY KEY,
        admin_user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
        action TEXT NOT NULL DEFAULT '',
        banner_id BIGINT,
        previous_state TEXT NOT NULL DEFAULT '',
        new_state TEXT NOT NULL DEFAULT '',
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS admin_actions_created_idx ON admin_actions(created_at DESC)`,

    // ── Plan 2: Persistent Progression, XP, Gold, PvP History ───────────

    // Extend game_stats with progression columns
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS total_xp BIGINT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS gold BIGINT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS lifetime_player_kills INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS lifetime_npc_kills INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS lifetime_deaths INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS draws INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS matches_played INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS matches_won INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS matches_lost INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS matches_drawn INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS ffa_matches_played INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS ffa_wins INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS tdm_matches_played INT NOT NULL DEFAULT 0`,
    `ALTER TABLE game_stats ADD COLUMN IF NOT EXISTS tdm_wins INT NOT NULL DEFAULT 0`,

    // Processed events (idempotency dedup table)
    `CREATE TABLE IF NOT EXISTS processed_events (
        event_id TEXT PRIMARY KEY,
        event_type TEXT NOT NULL,
        processed_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        result JSONB NOT NULL DEFAULT '{}'
    )`,

    // PvP kill relationships (who killed whom, how many times)
    `CREATE TABLE IF NOT EXISTS player_kill_relationships (
        id BIGSERIAL PRIMARY KEY,
        attacker_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        victim_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        kill_count INT NOT NULL DEFAULT 0,
        first_kill_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        last_kill_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
        UNIQUE(attacker_id, victim_id)
    )`,
    `CREATE INDEX IF NOT EXISTS pkr_attacker_idx ON player_kill_relationships(attacker_id, kill_count DESC)`,
    `CREATE INDEX IF NOT EXISTS pkr_victim_idx ON player_kill_relationships(victim_id, kill_count DESC)`,

    // Raw kill event history
    `CREATE TABLE IF NOT EXISTS kill_events (
        id BIGSERIAL PRIMARY KEY,
        event_id TEXT NOT NULL UNIQUE,
        match_id TEXT,
        server_tick INT NOT NULL DEFAULT 0,
        attacker_type TEXT NOT NULL DEFAULT 'player',
        attacker_id BIGINT NOT NULL DEFAULT 0,
        victim_type TEXT NOT NULL DEFAULT 'player',
        victim_id BIGINT NOT NULL DEFAULT 0,
        weapon_id TEXT NOT NULL DEFAULT '',
        distance_meters REAL NOT NULL DEFAULT 0.0,
        created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
    )`,
    `CREATE INDEX IF NOT EXISTS kill_events_attacker_idx ON kill_events(attacker_id, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS kill_events_victim_idx ON kill_events(victim_id, created_at DESC)`,
    `CREATE INDEX IF NOT EXISTS kill_events_match_idx ON kill_events(match_id)`,

    // Per-player weapon kill stats
    `CREATE TABLE IF NOT EXISTS player_weapon_stats (
        id BIGSERIAL PRIMARY KEY,
        player_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
        weapon_id TEXT NOT NULL DEFAULT '',
        kills INT NOT NULL DEFAULT 0,
        UNIQUE(player_id, weapon_id)
    )`,
    `CREATE INDEX IF NOT EXISTS pws_player_idx ON player_weapon_stats(player_id, kills DESC)`,

    // Extend match_history with victory type and team scores
    `ALTER TABLE match_history ADD COLUMN IF NOT EXISTS victory_type TEXT NOT NULL DEFAULT 'unknown'`,
    `ALTER TABLE match_history ADD COLUMN IF NOT EXISTS red_score INT NOT NULL DEFAULT 0`,
    `ALTER TABLE match_history ADD COLUMN IF NOT EXISTS blue_score INT NOT NULL DEFAULT 0`,
]

export async function runMigrations(database = pool) {
    const client = await database.connect()
    let version = 0
    try {
        await client.query("BEGIN")
        await client.query("SELECT pg_advisory_xact_lock(1835627636)")
        await client.query(`CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY, applied_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
        )`)
        const applied = await client.query("SELECT version FROM schema_migrations")
        const versions = new Set(applied.rows.map(row => Number(row.version)))
        // Version 1 adopts the existing bootstrap, including installations without a ledger.
        // Version 1 adopts db.js's complete historical bootstrap (001 through 004).
        // Version 5 matches the next SQL filename; versions 2-4 are not rerun separately.
        for (version of [1, 5]) {
            if (versions.has(version)) continue
            const statements = version === 1 ? MIGRATION_STATEMENTS : [
                await readFile(new URL("./migrations/005_progression.sql", import.meta.url), "utf8")
            ]
            for (const sql of statements) await client.query(sql)
            await client.query("INSERT INTO schema_migrations(version) VALUES ($1)", [version])
        }
        await client.query("COMMIT")
        console.log("[DB] Versioned migrations committed")
    } catch (error) {
        await client.query("ROLLBACK")
        console.error(`[DB] Migration ${version} rolled back; code=${error.code || "unknown"}`)
        throw error
    } finally {
        client.release()
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
            "password_reset_codes",
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
    "email_campaign_recipients",
    "vip_orders",
    "vip_entitlements",
    "vip_subscriptions",
    "vip_name_styles",
    "vip_name_presets",
    "vip_notifications",
    "vip_stripe_events",
    "vip_join_tickets",
    "banner_payment_orders",
    "site_banners",
    "banner_reports",
    "support_requests",
    "admin_actions"
]
    }
}
