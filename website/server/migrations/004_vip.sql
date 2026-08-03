CREATE TABLE IF NOT EXISTS vip_orders (
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
);

CREATE INDEX IF NOT EXISTS vip_orders_user_idx
    ON vip_orders(user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS vip_orders_status_idx
    ON vip_orders(status, created_at DESC);
CREATE UNIQUE INDEX IF NOT EXISTS vip_orders_checkout_session_uidx
    ON vip_orders(stripe_checkout_session_id)
    WHERE stripe_checkout_session_id <> '';
CREATE INDEX IF NOT EXISTS vip_orders_subscription_idx
    ON vip_orders(stripe_subscription_id)
    WHERE stripe_subscription_id <> '';

CREATE TABLE IF NOT EXISTS vip_entitlements (
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
);

CREATE INDEX IF NOT EXISTS vip_entitlements_user_active_idx
    ON vip_entitlements(user_id, tier, expires_at DESC)
    WHERE status = 'active';
CREATE INDEX IF NOT EXISTS vip_entitlements_expiry_idx
    ON vip_entitlements(status, expires_at);
CREATE INDEX IF NOT EXISTS vip_entitlements_order_idx
    ON vip_entitlements(order_id)
    WHERE order_id IS NOT NULL;
CREATE INDEX IF NOT EXISTS vip_entitlements_subscription_idx
    ON vip_entitlements(stripe_subscription_id)
    WHERE stripe_subscription_id <> '';

CREATE TABLE IF NOT EXISTS vip_subscriptions (
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
);

CREATE INDEX IF NOT EXISTS vip_subscriptions_user_idx
    ON vip_subscriptions(user_id, current_period_end DESC);
CREATE INDEX IF NOT EXISTS vip_subscriptions_status_idx
    ON vip_subscriptions(status, current_period_end DESC);

CREATE TABLE IF NOT EXISTS vip_name_styles (
    user_id BIGINT PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    style_json JSONB NOT NULL DEFAULT '{"version":1,"kind":"none"}',
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS vip_name_presets (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name TEXT NOT NULL DEFAULT '',
    style_json JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX IF NOT EXISTS vip_name_presets_user_idx
    ON vip_name_presets(user_id, created_at DESC);

CREATE TABLE IF NOT EXISTS vip_notifications (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    notification_type TEXT NOT NULL
        CHECK (notification_type IN ('expires_7d','expires_3d','expires_1d','expired')),
    entitlement_key TEXT NOT NULL DEFAULT '',
    sent_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    channel TEXT NOT NULL DEFAULT 'website',
    UNIQUE(user_id, notification_type, entitlement_key, channel)
);

CREATE INDEX IF NOT EXISTS vip_notifications_user_idx
    ON vip_notifications(user_id, sent_at DESC);

CREATE TABLE IF NOT EXISTS vip_stripe_events (
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
);

CREATE INDEX IF NOT EXISTS vip_stripe_events_status_idx
    ON vip_stripe_events(status, received_at DESC);

CREATE TABLE IF NOT EXISTS vip_join_tickets (
    token_hash TEXT PRIMARY KEY,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    room_code TEXT NOT NULL DEFAULT '',
    issued_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMPTZ NOT NULL,
    used_at TIMESTAMPTZ,
    last_result JSONB NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS vip_join_tickets_user_idx
    ON vip_join_tickets(user_id, issued_at DESC);
CREATE INDEX IF NOT EXISTS vip_join_tickets_expiry_idx
    ON vip_join_tickets(expires_at);
