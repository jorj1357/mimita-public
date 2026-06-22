# API MAP — mimita-api (Port 3002)

## Base URL

Production: `https://mimita.fun/api`  
Local dev: `http://localhost:3002/api`

---

## Auth Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /api/auth/signup | No | Create account. Rate limited (10/min). |
| POST | /api/auth/signin | No | Sign in via username or email. Rate limited. |
| POST | /api/auth/signout | Yes | Revoke current session cookie. |
| GET | /api/auth/me | Yes | Return current user profile. |

**signup body:** `{ username, email, password }`  
**signin body:** `{ identifier, password }` — identifier can be username or email

---

## Account Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| PATCH | /api/account/profile | Yes | Update bio (max 500 chars) |
| PATCH | /api/account/notification-preferences | Yes | Toggle email_notifications_enabled |
| DELETE | /api/account | Yes | Soft-delete account (requires password confirmation) |
| GET | /api/users/:username | No | Public profile lookup (username + bio only) |

---

## Password Change Endpoints

All require authentication.

| Method | Path | Description |
|--------|------|-------------|
| POST | /api/auth/password-change/request | Sends 6-digit code via email |
| POST | /api/auth/password-change/verify | Verifies the code |
| POST | /api/auth/password-change/finalize | Sets new password (requires old password + verified code) |

**Flow:** Request → Verify → Finalize. Codes expire in 10 minutes.

---

## Newsletter

| Method | Path | Auth | Rate Limit |
|--------|------|------|------------|
| POST | /api/newsletter | No | 5/min |

**body:** `{ email }`

---

## Admin Endpoints

All require admin session cookie. Rate limited (20/min).

| Method | Path | Description |
|--------|------|-------------|
| POST | /api/admin/login | Authenticate via username/email + password. Checks role IN ('admin','owner'). |
| POST | /api/admin/logout | Revoke admin session. |
| GET | /api/admin/me | Return admin user profile. |
| GET | /api/admin/dashboard | Return all dashboard metrics. |
| POST | /api/admin/dashboard/refresh | Recalculate and return all metrics. |
| GET | /api/admin/users?limit=50&offset=0 | List all users (paginated). |
| GET | /api/admin/feedback?limit=20&offset=0 | List all feedback submissions. |
| GET | /api/admin/feedback/presets | Return available feedback presets. |

**Admin login body:** `{ identifier, password }`

---

## Debug Endpoints

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| GET | /api/debug/health | No | Health check — server uptime, DB status, environment |
| GET | /api/debug/error-catalog | No | Expected errors debugging reference |
| GET | /api/debug/routes | No | List all registered routes (Express 5 compatible) |

---

## Feedback (Public)

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /api/admin/feedback | No | Submit feedback (used by FeedbackBox component) |

**body:** `{ selectedPresets, customFeedback, contactInfo, pageUrl, userId }`

---

## Authentication System

### Session-based auth
- Cookie name: `mimita_session` (configurable via `SESSION_COOKIE_NAME`)
- httpOnly, Secure in production, SameSite=Lax
- Default expiry: 30 days (configurable via `SESSION_DAYS`)
- Tokens: 32 random bytes, base64url encoded
- Token hash: SHA-256(sessionSecret + ":" + token)
- Sessions stored in `sessions` table with expiry + revoke support

### Password hashing
- Algorithm: Node.js crypto.scrypt with random 16-byte salt
- Format: `scrypt:salt:hex` (64-byte derived key)
- Verification uses `crypto.timingSafeEqual` to prevent timing attacks

### Rate limiting
- Auth endpoints: 10 requests/minute/IP
- Admin endpoints: 20 requests/minute/IP
- Newsletter: 5 requests/minute/IP
- In-memory store, resets after window

### Admin authorization
- No hardcoded credentials
- Admin role stored in `users.role` column
- Valid roles: `owner` (superadmin), `admin`, `moderator`, `user`
- `requireAdmin` middleware checks session → user → role IN ('admin','owner')

---

## Analytics System

### Event tracking (`analytics_events` table)
Events are inserted via `trackEvent(eventName, data)`:
- `page_visit` — page views
- `download` — file downloads
- registered game events from `/api/game/analytics/events`

### Game Analytics

| Method | Path | Auth | Description |
|--------|------|------|-------------|
| POST | /api/game/analytics/events | No | Accept up to 64 registered game analytics events per request. |
| POST | /api/game/analytics/consent | No | Upsert anonymous analytics consent/opt-out state. |
| POST | /api/game/analytics/deletion-request | No | Create audit log, email hello@mimita.fun, and store deletion request. |

Game analytics events use `analytics_events.event_data` JSONB for flexible fields. New events are registered in `server/gameAnalytics.js`; normal event additions do not need schema changes.

The backend rejects password/token/chat/message-style fields from gameplay analytics.

### Metric aggregation (`analytics_metrics` table)
Metrics are stored daily. Refresh via:
- POST `/api/admin/dashboard/refresh`
- Automatically on dashboard GET

Tracked metrics:
- site_visitors (today, 7d, 30d, all)
- downloads (today, all)
- accounts_created (today, all)
- game sessions, session duration buckets, crashes, disconnects, failed logins
- live dashboard lists for maps, weapons, features, movement, opt-outs, deletion requests
- Plus computed: DAU, WAU, MAU, retention, etc.

### Google Analytics
- Property ID: G-ZB5BEVX9MR
- Loaded in `/dist/index.html`
- Metricool tracker also present
