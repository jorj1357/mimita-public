CREATE TABLE IF NOT EXISTS analytics_consent (
    id BIGSERIAL PRIMARY KEY,
    anonymous_id TEXT NOT NULL UNIQUE,
    user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    username TEXT NOT NULL DEFAULT '',
    analytics_enabled BOOLEAN NOT NULL DEFAULT TRUE,
    permanently_disabled BOOLEAN NOT NULL DEFAULT FALSE,
    source TEXT NOT NULL DEFAULT 'game',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS analytics_consent_user_idx
    ON analytics_consent(user_id);

CREATE TABLE IF NOT EXISTS analytics_deletion_requests (
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
);

CREATE INDEX IF NOT EXISTS analytics_deletion_requests_status_idx
    ON analytics_deletion_requests(status, requested_at DESC);

CREATE TABLE IF NOT EXISTS analytics_audit_log (
    id BIGSERIAL PRIMARY KEY,
    action TEXT NOT NULL,
    user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    anonymous_id TEXT,
    details JSONB NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS analytics_audit_log_action_idx
    ON analytics_audit_log(action, created_at DESC);
