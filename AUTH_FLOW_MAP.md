# Auth Flow Map

## Overview

Two parallel auth systems with different route paths but a **shared session table** (`sessions`).

---

## System 1: Public Auth (`/api/auth/*`)

| Route | Method | File | Expects | Sets Cookie | Middleware |
|---|---|---|---|---|---|
| `/api/auth/signup` | POST | `server.js:215` | `{ username, email, password }` | Yes | `authRateLimit` |
| `/api/auth/signin` | POST | `server.js:296` | `{ identifier, password }` | Yes | `authRateLimit` |
| `/api/auth/logout` | POST | `server.js:362` | (cookie) | Clears | `authenticate` |
| `/api/auth/me` | GET | `server.js:391` | (cookie) | No | `authenticate` |
| `/api/auth/forgot-password` | POST | `server.js:500` | `{ email }` | No | `authRateLimit` |
| `/api/auth/reset-password` | POST | `server.js:597` | `{ code, password, confirmPassword }` | No | `authRateLimit` |
| `/api/auth/change-password` | POST | `server.js:631` | `{ oldPassword, newPassword, confirmNewPassword }` | No | `authenticate` |
| `/api/auth/delete-account` | POST | `server.js:741` | `{ password }` | No | `authenticate` |

**Frontend callers:** `Auth.jsx` (signin/signup page for regular users)

## System 2: Admin Auth (`/api/admin/*`)

| Route | Method | File | Expects | Sets Cookie | Middleware |
|---|---|---|---|---|---|
| `/api/admin/login` | POST | `admin.js:124` | `{ username, password }` | Yes | `adminRateLimit` |
| `/api/admin/logout` | POST | `admin.js:179` | (cookie) | Clears | `requireAdmin` |
| `/api/admin/me` | GET | `admin.js:198` | (cookie) | No | `requireAdmin` |
| `/api/admin/dashboard` | GET | `admin.js:211` | (cookie) | No | `requireAdmin` |
| `/api/admin/dashboard/refresh` | POST | `admin.js:221` | (cookie) | No | `requireAdmin` |
| `/api/admin/users` | GET | `admin.js:232` | (cookie) | No | `requireAdmin` |
| `/api/admin/feedback` | GET | `admin.js:266` | (cookie) | No | `requireAdmin` |

**Frontend callers:** `AdminLogin.jsx` (admin login), `AdminDashboard.jsx` (dashboard/refresh/logout)

---

## Shared Infrastructure

### Session Table
Both systems use the **same `sessions` table** (`db.js` schema). The `admin_sessions` table exists in the schema but is **never used**.

### Cookie Name
Both use `mimita_session` (from `SESSION_COOKIE_NAME` env var).
- `server.js:111` — `setSessionCookie()` for public auth
- `admin.js:31` — `setAdminCookie()` for admin auth

### Cookie Settings
```js
{ httpOnly: true, secure: NODE_ENV==="production", sameSite: "lax", maxAge: 30 days, path: "/" }
```

### Session Secret
Both use `SESSION_SECRET` env var (fallback: `"development-only-change-me"`).

---

## Field Name Mismatch (FIXED)

| System | Frontend sends | Backend reads | Status |
|---|---|---|---|
| `/api/auth/signin` | `{ identifier, password }` | `req.body.identifier` | ✓ Match |
| `/api/admin/login` | `{ username, password }` | `req.body.username` | ✓ Match (was `identifier`, fixed in this session) |

The admin login previously expected `req.body.identifier` but the frontend sends `{ username, password }`. This caused `identifier` to be `undefined`, triggering the "identifier and password required" 400 error.

---

## Session Creation

### Public signin (`server.js:338`)
```js
await createSession(user.id, req, res)
// INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
```

### Admin login (`admin.js:161`)
```js
await createAdminSession(user.id, req, res)
// INSERT INTO sessions (user_id, token_hash, user_agent, ip_address, expires_at)
```

Both insert into the identical `sessions` table with the same columns. The functions are separate copies of the same logic.

---

## Session Verification

### Public (`authenticate` at `server.js:167`)
```sql
SELECT u.id, u.username, u.email, u.bio, u.email_notifications_enabled
FROM sessions s JOIN users u ON u.id = s.user_id
WHERE s.token_hash = $1 AND s.revoked_at IS NULL
  AND s.expires_at > NOW() AND u.deleted_at IS NULL
```

### Admin (`requireAdmin` at `admin.js:73`)
```sql
SELECT u.id, u.username, u.email, u.role, u.bio
FROM sessions s JOIN users u ON u.id = s.user_id
WHERE s.token_hash = $1 AND s.revoked_at IS NULL
  AND s.expires_at > NOW() AND u.deleted_at IS NULL
```

With additional role check:
```js
if (!ADMIN_ROLES.includes(user.role)) → 403
```

---

## Environment Differences

| Setting | Development | Production |
|---|---|---|
| Frontend URL | `http://localhost:5173` | `https://mimita.fun` |
| API URL | `/api` (Vite proxy → `localhost:3002`) | `/api` |
| CORS origin | `http://localhost:5173` | `https://mimita.fun` |
| Cookie `secure` | `false` | `true` |
| `NODE_ENV` | not set (falsy) | `production` |
| `SESSION_SECRET` | `dev-session-secret-...` | production secret |

---

## Summary

- **Two auth systems** with different route paths but shared session table and cookie
- **Field name mismatch fixed** — admin login now reads `req.body.username` to match frontend
- **Port mismatch fixed** — Vite proxy now targets `localhost:3002`
- **SESSION_SECRET added** to `.env`
