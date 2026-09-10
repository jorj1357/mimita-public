# DATABASE MAP — mimita_db (PostgreSQL 14)

## Schema

All tables are in the `public` schema.

---

### users

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| username | TEXT | — | NOT NULL |
| username_key | TEXT | — | NOT NULL, UNIQUE |
| email | TEXT | — | NOT NULL, UNIQUE |
| password_hash | TEXT | — | NOT NULL |
| bio | TEXT | '' | NOT NULL |
| avatar_url | TEXT | '' | NOT NULL |
| avatar_updated_at | TIMESTAMPTZ | — | Cache-busting timestamp |
| supporter_tier | TEXT | 'free' | NOT NULL, CHECK IN ('free','vip','super_vip','ultra_vip','moderator','admin','owner') |
| role | TEXT | 'user' | NOT NULL, CHECK IN ('owner','admin','moderator','user') |
| email_notifications_enabled | BOOLEAN | TRUE | NOT NULL |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| updated_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| deleted_at | TIMESTAMPTZ | — | Soft-delete |

**Indexes:** username_key (unique), email (unique)

---

### sessions

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| user_id | BIGINT | — | NOT NULL, FK → users(id) ON DELETE CASCADE |
| token_hash | TEXT | — | NOT NULL, UNIQUE |
| user_agent | TEXT | — | — |
| ip_address | TEXT | — | — |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| expires_at | TIMESTAMPTZ | — | NOT NULL |
| revoked_at | TIMESTAMPTZ | — | — |

**Indexes:**
- `sessions_user_id_idx` ON user_id
- `sessions_active_token_idx` ON (token_hash, expires_at) WHERE revoked_at IS NULL

---

### password_change_codes

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| user_id | BIGINT | — | NOT NULL, FK → users(id) ON DELETE CASCADE |
| code_hash | TEXT | — | NOT NULL |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| expires_at | TIMESTAMPTZ | — | NOT NULL |
| verified_at | TIMESTAMPTZ | — | — |
| used_at | TIMESTAMPTZ | — | — |
| request_ip | TEXT | — | — |
| request_user_agent | TEXT | — | — |

**Index:** `password_change_codes_user_id_idx` ON (user_id, created_at DESC)

---

### newsletter

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| email | TEXT | — | NOT NULL, UNIQUE |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

---

### analytics_events

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| event_name | TEXT | — | NOT NULL |
| event_data | JSONB | '{}' | — |
| user_id | BIGINT | — | FK → users(id) ON DELETE SET NULL |
| ip_address | TEXT | — | — |
| page_url | TEXT | — | — |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

**Indexes:**
- `analytics_events_name_idx` ON (event_name, created_at DESC)
- `analytics_events_created_idx` ON (created_at DESC)

---

### analytics_consent

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| anonymous_id | TEXT | — | NOT NULL, UNIQUE |
| user_id | BIGINT | — | FK → users(id) ON DELETE SET NULL |
| username | TEXT | '' | NOT NULL |
| analytics_enabled | BOOLEAN | TRUE | NOT NULL |
| permanently_disabled | BOOLEAN | FALSE | NOT NULL |
| source | TEXT | 'game' | NOT NULL |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| updated_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

**Index:** `analytics_consent_user_idx` ON user_id

---

### analytics_deletion_requests

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| user_id | BIGINT | — | FK → users(id) ON DELETE SET NULL |
| anonymous_id | TEXT | — | — |
| username | TEXT | '' | NOT NULL |
| email | TEXT | '' | NOT NULL |
| source | TEXT | 'game' | NOT NULL |
| status | TEXT | 'requested' | CHECK IN ('requested','reviewing','completed','rejected') |
| audit | JSONB | '{}' | NOT NULL |
| requested_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

**Index:** `analytics_deletion_requests_status_idx` ON (status, requested_at DESC)

---

### analytics_audit_log

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| action | TEXT | — | NOT NULL |
| user_id | BIGINT | — | FK → users(id) ON DELETE SET NULL |
| anonymous_id | TEXT | — | — |
| details | JSONB | '{}' | NOT NULL |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

**Index:** `analytics_audit_log_action_idx` ON (action, created_at DESC)

---

### analytics_metrics

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| metric_date | DATE | CURRENT_DATE | NOT NULL |
| metric_name | TEXT | — | NOT NULL |
| metric_value | BIGINT | 0 | NOT NULL |
| UNIQUE(metric_date, metric_name) | — | — | — |

**Index:** `analytics_metrics_date_idx` ON (metric_date DESC)

---

### feedback

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| selected_presets | TEXT[] | '{}' | NOT NULL |
| custom_feedback | TEXT | '' | NOT NULL |
| contact_info | TEXT | '' | NOT NULL |
| page_url | TEXT | '' | NOT NULL |
| user_id | BIGINT | — | FK → users(id) ON DELETE SET NULL |
| status | TEXT | 'new' | NOT NULL, CHECK IN ('new','reviewed','completed','ignored') |
| category | TEXT | 'general' | NOT NULL |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |

**Indexes:**
- `feedback_status_idx` ON (status, created_at DESC)
- `feedback_created_idx` ON (created_at DESC)

---

### admin_sessions

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| token_hash | TEXT | — | NOT NULL, UNIQUE |
| created_at | TIMESTAMPTZ | CURRENT_TIMESTAMP | NOT NULL |
| expires_at | TIMESTAMPTZ | — | NOT NULL |

**Index:** `admin_sessions_token_idx` ON (token_hash)

---

## Entity Relationships

```
users ──1:N──→ sessions
users ──1:N──→ password_change_codes
users ──1:N──→ analytics_events (SET NULL on delete)
users ──1:N──→ analytics_consent (SET NULL on delete)
users ──1:N──→ analytics_deletion_requests (SET NULL on delete)
users ──1:N──→ analytics_audit_log (SET NULL on delete)
users ──1:N──→ feedback (SET NULL on delete)
users ──1:N──→ messages
users ──1:N──→ conversation_members
users ──1:N──→ forum_threads
users ──1:N──→ forum_posts
users ──1:N──→ forum_reactions
users ──1:N──→ friendships (as requester or addressee)
users ──1:N──→ reports (as reporter or reported)
users ──1:N──→ blocks (as blocker or blocked)
users ──1:N──→ mutes (as muter or muted)
users ──1:N──→ banner_reactions
users ──1:N──→ banner_replies
users ──1:N──→ join_events
conversations ──1:N──→ conversation_members
conversations ──1:N──→ messages
forum_categories ──1:N──→ forum_threads
forum_threads ──1:N──→ forum_posts
forum_posts ──1:N──→ forum_reactions
site_banners ──1:N──→ banner_reactions
site_banners ──1:N──→ banner_replies
```

---

## New Tables (Migration 9)

### users — new column

| Column | Type | Default | Description |
|--------|------|---------|-------------|
| last_seen_at | TIMESTAMPTZ | NULL | Last authenticated request (throttled 60s) |

---

### conversations

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| type | TEXT | 'dm' | CHECK IN ('dm','group','forum') |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

### conversation_members

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| conversation_id | BIGINT | — | FK → conversations ON DELETE CASCADE |
| user_id | BIGINT | — | FK → users ON DELETE CASCADE |
| joined_at | TIMESTAMPTZ | NOW() | NOT NULL |
| last_read_at | TIMESTAMPTZ | NULL | — |

**Unique:** (conversation_id, user_id)

### messages

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| conversation_id | BIGINT | — | FK → conversations ON DELETE CASCADE |
| sender_id | BIGINT | — | FK → users ON DELETE CASCADE |
| body | TEXT | '' | NOT NULL |
| read_at | TIMESTAMPTZ | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

---

### forum_categories

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| name | TEXT | '' | NOT NULL |
| slug | TEXT | — | NOT NULL, UNIQUE |
| description | TEXT | '' | NOT NULL |
| sort_order | INT | 0 | NOT NULL |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

### forum_threads

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| category_id | BIGINT | — | FK → forum_categories ON DELETE CASCADE |
| author_id | BIGINT | — | FK → users ON DELETE CASCADE |
| title | TEXT | '' | NOT NULL |
| reply_count | INT | 0 | NOT NULL |
| pinned | BOOLEAN | FALSE | NOT NULL |
| locked | BOOLEAN | FALSE | NOT NULL |
| last_reply_at | TIMESTAMPTZ | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

### forum_posts

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| thread_id | BIGINT | — | FK → forum_threads ON DELETE CASCADE |
| author_id | BIGINT | — | FK → users ON DELETE CASCADE |
| body | TEXT | '' | NOT NULL |
| edited_at | TIMESTAMPTZ | NULL | — |
| deleted_at | TIMESTAMPTZ | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

### forum_reactions

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| post_id | BIGINT | — | FK → forum_posts ON DELETE CASCADE |
| user_id | BIGINT | — | FK → users ON DELETE CASCADE |
| emoji | TEXT | '' | NOT NULL |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

**Unique:** (post_id, user_id, emoji)

---

### friendships

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| requester_id | BIGINT | — | FK → users ON DELETE CASCADE |
| addressee_id | BIGINT | — | FK → users ON DELETE CASCADE |
| status | TEXT | 'pending' | CHECK IN ('pending','accepted','blocked') |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |
| accepted_at | TIMESTAMPTZ | NULL | — |

**Unique:** (requester_id, addressee_id)

---

### reports

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| reporter_id | BIGINT | — | FK → users ON DELETE CASCADE |
| reported_id | BIGINT | — | FK → users ON DELETE CASCADE |
| reason | TEXT | '' | NOT NULL |
| evidence | JSONB | '{}' | NOT NULL |
| status | TEXT | 'new' | CHECK IN ('new','reviewing','action_taken','no_action','duplicate','escalated') |
| server_id | TEXT | '' | NOT NULL |
| match_id | TEXT | '' | NOT NULL |
| reviewed_by | BIGINT | NULL | FK → users ON DELETE SET NULL |
| reviewed_at | TIMESTAMPTZ | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

### blocks

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| blocker_id | BIGINT | — | FK → users ON DELETE CASCADE |
| blocked_id | BIGINT | — | FK → users ON DELETE CASCADE |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

**Unique:** (blocker_id, blocked_id)

### mutes

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| muter_id | BIGINT | — | FK → users ON DELETE CASCADE |
| muted_id | BIGINT | — | FK → users ON DELETE CASCADE |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |
| expires_at | TIMESTAMPTZ | NULL | — |

**Unique:** (muter_id, muted_id)

---

### banner_reactions

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| banner_id | BIGINT | — | FK → site_banners ON DELETE CASCADE |
| user_id | BIGINT | — | FK → users ON DELETE CASCADE |
| emoji | TEXT | '' | NOT NULL |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

**Unique:** (banner_id, user_id, emoji)

### banner_replies

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| banner_id | BIGINT | — | FK → site_banners ON DELETE CASCADE |
| user_id | BIGINT | — | FK → users ON DELETE CASCADE |
| body | TEXT | '' | NOT NULL |
| deleted_at | TIMESTAMPTZ | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

---

### join_events

| Column | Type | Default | Constraints |
|--------|------|---------|-------------|
| id | BIGSERIAL | — | PRIMARY KEY |
| user_id | BIGINT | NULL | FK → users ON DELETE SET NULL |
| source | TEXT | 'unknown' | CHECK IN ('website_signup','game_client','client_login','link_code','unknown') |
| ip_address | TEXT | NULL | — |
| user_agent | TEXT | NULL | — |
| created_at | TIMESTAMPTZ | NOW() | NOT NULL |

## Admin User

Created via migration script. Default credentials are for setup only:

- **username:** admin
- **password:** AdminPass1!
- **role:** owner
- **email:** admin@mimita.fun

Change these immediately in production. Database has no hardcoded credentials — admin role is checked against the `role` column at runtime.
