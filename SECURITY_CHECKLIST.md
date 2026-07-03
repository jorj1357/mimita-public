# Mimita Website Security Checklist

Manual security tests to verify the admin system is properly secured.

---

## Authentication

- [ ] Dashboard without login → 401
- [ ] Dashboard after logout → 401
- [ ] Refresh after login stays logged in
- [ ] Invalid password rejected (401)
- [ ] Invalid username rejected (401)
- [ ] Empty password rejected
- [ ] Rate limiting kicks in after 10 failed attempts

## Authorization

- [ ] Normal user cannot access `/admin/dashboard` → 401 or 403
- [ ] Normal user cannot call `/api/admin/dashboard` → 401 or 403
- [ ] Normal user cannot call `/api/admin/users` → 401 or 403
- [ ] Normal user cannot call `/api/admin/feedback` (GET) → 401 or 403
- [ ] Normal user cannot call `/api/admin/email-campaigns/*` → 401 or 403
- [ ] Normal user cannot call `/api/admin/admins` → 401 or 403
- [ ] Hidden URLs still blocked (direct curl requests)
- [ ] Direct API requests blocked without cookie
- [ ] Browser refresh of admin page still requires auth
- [ ] Private routes blocked for normal users

## Session

- [ ] Logout destroys session (cannot re-use same cookie)
- [ ] Expired session returns 401
- [ ] Cookie removed after logout (check browser DevTools)
- [ ] Old session cookie rejected after logout

## Browser Attack Tests

Attempt each of the following:

- [ ] Typing `/admin/dashboard` URL manually → 401 or redirect
- [ ] Opening multiple tabs with admin session → all work
- [ ] Incognito window → not logged in
- [ ] Refreshing rapidly on admin page → stays on page
- [ ] Spam clicking admin dashboard link → never bypasses auth
- [ ] Deleting cookies → logged out
- [ ] Editing localStorage/sessionStorage → no effect on auth
- [ ] Editing frontend JavaScript → no effect (server is authoritative)
- [ ] Modifying requests in DevTools → server validates
- [ ] Calling admin APIs via `curl` without cookie → 401
- [ ] Calling admin APIs via `curl` with non-admin user cookie → 403

## API Endpoint Security

- [ ] `GET /api/admin/check` returns only `{ success, isAdmin }` (no role leak)
- [ ] `GET /api/debug/health` → 404 in production
- [ ] `GET /api/debug/routes` → 404 in production
- [ ] `GET /api/debug/error-catalog` → 404 in production
- [ ] `POST /api/auth/signout` → CSRF bypass works, auth still required
- [ ] `POST /api/admin/feedback` → public (intentional, rate-limited)

## CSRF Protection

- [ ] Non-GET requests to auth endpoints require `X-CSRF-Token` header
- [ ] Game API endpoints bypass CSRF (intentional, uses Bearer tokens)
- [ ] Signout bypasses CSRF (intentional)

## Rate Limiting

- [ ] Signin: 10/min/IP
- [ ] Signup: 10/min/IP
- [ ] Feedback: 5/min/IP
- [ ] Avatar upload: 3/min/IP
- [ ] Admin API: 20/min/IP
- [ ] Link endpoints: 10/10s/IP

## Security Headers

- [ ] `X-Content-Type-Options: nosniff`
- [ ] `X-Frame-Options: DENY`
- [ ] `Referrer-Policy: strict-origin-when-cross-origin`
- [ ] `Permissions-Policy: camera=(), microphone=(), geolocation=(), interest-cohort=()`
- [ ] `Content-Security-Policy: default-src 'self' ...`
- [ ] `Strict-Transport-Security` (production only)

## Expected Results

| Test | Expected HTTP Status |
|------|---------------------|
| Unauthenticated GET to any `/api/admin/*` | 401 |
| Non-admin authenticated GET to any `/api/admin/*` | 403 |
| Public GET to `/api/users` | 200 (public data only) |
| Public GET to `/api/debug/health` (production) | 404 |
| Public GET to `/api/admin/check` | 200 (`isAdmin: false`) |
| POST to `/api/admin/login` with wrong password | 401 |
| POST to `/api/admin/login` with admin creds | 200 + Set-Cookie |
| POST to `/api/auth/signout` (authenticated) | 200 |
| POST to `/api/auth/signout` (unauthenticated) | 401 |
