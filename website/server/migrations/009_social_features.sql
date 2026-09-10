-- Migration 9: Social features (messaging, forum, friends, moderation, reactions, joins)
-- Applied: see schema_migrations

-- ── Last online indicator ─────────────────────────────────────────────
ALTER TABLE users ADD COLUMN IF NOT EXISTS last_seen_at TIMESTAMPTZ;

-- ── Messages (generalized messaging foundation) ────────────────────────
CREATE TABLE IF NOT EXISTS conversations (
    id BIGSERIAL PRIMARY KEY,
    type TEXT NOT NULL DEFAULT 'dm'
        CHECK (type IN ('dm', 'group', 'forum')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS conversation_members (
    id BIGSERIAL PRIMARY KEY,
    conversation_id BIGINT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    joined_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_read_at TIMESTAMPTZ,
    UNIQUE(conversation_id, user_id)
);
CREATE INDEX IF NOT EXISTS conv_members_user_idx ON conversation_members(user_id);

CREATE TABLE IF NOT EXISTS messages (
    id BIGSERIAL PRIMARY KEY,
    conversation_id BIGINT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    sender_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    body TEXT NOT NULL DEFAULT '',
    read_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS messages_conv_idx ON messages(conversation_id, created_at DESC);
CREATE INDEX IF NOT EXISTS messages_sender_idx ON messages(sender_id, created_at DESC);

-- ── Forum ─────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS forum_categories (
    id BIGSERIAL PRIMARY KEY,
    name TEXT NOT NULL DEFAULT '',
    slug TEXT NOT NULL UNIQUE,
    description TEXT NOT NULL DEFAULT '',
    sort_order INT NOT NULL DEFAULT 0,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS forum_threads (
    id BIGSERIAL PRIMARY KEY,
    category_id BIGINT NOT NULL REFERENCES forum_categories(id) ON DELETE CASCADE,
    author_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title TEXT NOT NULL DEFAULT '',
    reply_count INT NOT NULL DEFAULT 0,
    pinned BOOLEAN NOT NULL DEFAULT FALSE,
    locked BOOLEAN NOT NULL DEFAULT FALSE,
    last_reply_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS forum_threads_cat_idx ON forum_threads(category_id, pinned DESC, last_reply_at DESC);

CREATE TABLE IF NOT EXISTS forum_posts (
    id BIGSERIAL PRIMARY KEY,
    thread_id BIGINT NOT NULL REFERENCES forum_threads(id) ON DELETE CASCADE,
    author_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    body TEXT NOT NULL DEFAULT '',
    edited_at TIMESTAMPTZ,
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS forum_posts_thread_idx ON forum_posts(thread_id, created_at ASC);

CREATE TABLE IF NOT EXISTS forum_reactions (
    id BIGSERIAL PRIMARY KEY,
    post_id BIGINT NOT NULL REFERENCES forum_posts(id) ON DELETE CASCADE,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    emoji TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(post_id, user_id, emoji)
);
CREATE INDEX IF NOT EXISTS forum_reactions_post_idx ON forum_reactions(post_id);

-- ── Friends ───────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS friendships (
    id BIGSERIAL PRIMARY KEY,
    requester_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    addressee_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    status TEXT NOT NULL DEFAULT 'pending'
        CHECK (status IN ('pending', 'accepted', 'blocked')),
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    accepted_at TIMESTAMPTZ,
    UNIQUE(requester_id, addressee_id)
);
CREATE INDEX IF NOT EXISTS friendships_requester_idx ON friendships(requester_id, status);
CREATE INDEX IF NOT EXISTS friendships_addressee_idx ON friendships(addressee_id, status);

-- ── Moderation ────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS reports (
    id BIGSERIAL PRIMARY KEY,
    reporter_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reported_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    reason TEXT NOT NULL DEFAULT '',
    evidence JSONB NOT NULL DEFAULT '{}',
    status TEXT NOT NULL DEFAULT 'new'
        CHECK (status IN ('new', 'reviewing', 'action_taken', 'no_action', 'duplicate', 'escalated')),
    server_id TEXT NOT NULL DEFAULT '',
    match_id TEXT NOT NULL DEFAULT '',
    reviewed_by BIGINT REFERENCES users(id) ON DELETE SET NULL,
    reviewed_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS reports_status_idx ON reports(status, created_at DESC);
CREATE INDEX IF NOT EXISTS reports_reported_idx ON reports(reported_id, created_at DESC);

CREATE TABLE IF NOT EXISTS blocks (
    id BIGSERIAL PRIMARY KEY,
    blocker_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    blocked_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(blocker_id, blocked_id)
);
CREATE INDEX IF NOT EXISTS blocks_blocker_idx ON blocks(blocker_id);
CREATE INDEX IF NOT EXISTS blocks_blocked_idx ON blocks(blocked_id);

CREATE TABLE IF NOT EXISTS mutes (
    id BIGSERIAL PRIMARY KEY,
    muter_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    muted_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMPTZ,
    UNIQUE(muter_id, muted_id)
);
CREATE INDEX IF NOT EXISTS mutes_muter_idx ON mutes(muter_id);

-- ── Banner reactions & replies ────────────────────────────────────────
CREATE TABLE IF NOT EXISTS banner_reactions (
    id BIGSERIAL PRIMARY KEY,
    banner_id BIGINT NOT NULL REFERENCES site_banners(id) ON DELETE CASCADE,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    emoji TEXT NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(banner_id, user_id, emoji)
);
CREATE INDEX IF NOT EXISTS banner_reactions_banner_idx ON banner_reactions(banner_id);

CREATE TABLE IF NOT EXISTS banner_replies (
    id BIGSERIAL PRIMARY KEY,
    banner_id BIGINT NOT NULL REFERENCES site_banners(id) ON DELETE CASCADE,
    user_id BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    body TEXT NOT NULL DEFAULT '',
    deleted_at TIMESTAMPTZ,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS banner_replies_banner_idx ON banner_replies(banner_id, created_at ASC);

-- ── Join tracking ─────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS join_events (
    id BIGSERIAL PRIMARY KEY,
    user_id BIGINT REFERENCES users(id) ON DELETE SET NULL,
    source TEXT NOT NULL DEFAULT 'unknown'
        CHECK (source IN ('website_signup', 'game_client', 'client_login', 'link_code', 'unknown')),
    ip_address TEXT,
    user_agent TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT CURRENT_TIMESTAMP
);
CREATE INDEX IF NOT EXISTS join_events_source_idx ON join_events(source, created_at DESC);
CREATE INDEX IF NOT EXISTS join_events_created_idx ON join_events(created_at DESC);

-- Seed default forum categories
INSERT INTO forum_categories (name, slug, description, sort_order) VALUES
    ('General', 'general', 'General discussion about MiMITA', 0),
    ('Feedback', 'feedback', 'Share your thoughts and suggestions', 1),
    ('Bug Reports', 'bug-reports', 'Report bugs and issues', 2),
    ('Off-Topic', 'off-topic', 'Chat about anything', 3)
ON CONFLICT (slug) DO NOTHING;
