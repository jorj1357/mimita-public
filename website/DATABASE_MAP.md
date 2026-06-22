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
users ──1:N──→ feedback (SET NULL on delete)
```

## Admin User

Created via migration script. Default credentials are for setup only:

- **username:** admin
- **password:** AdminPass1!
- **role:** owner
- **email:** admin@mimita.fun

Change these immediately in production. Database has no hardcoded credentials — admin role is checked against the `role` column at runtime.
