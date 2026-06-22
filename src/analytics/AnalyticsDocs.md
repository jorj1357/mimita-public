# Game Analytics Subsystem

Owner: `src/analytics/`

Files:

- `analytics-manager.*`: session lifecycle, event creation, consent actions, terminal commands
- `analytics-events.*`: registered game event names and categories
- `analytics-config.*`: `config/analytics.json` load/save
- `analytics-uploader.*`: batched HTTPS POST uploads
- `analytics-consent.*`: first-launch popup and settings panel UI

Rules:

- Gameplay systems should only call `AnalyticsManager::track...` helpers after an action already succeeded.
- Do not read passwords, chat text, auth tokens, or arbitrary user text into analytics.
- Do not change movement, weapon, or physics constants for analytics.
- Keep new event fields small and specific.

Runtime controls:

- First launch popup: Continue, Read More, Disable
- Settings menu: Analytics Enabled, Disable Analytics, Request Data Deletion, Privacy Policy
- Terminal: `analytics_status`, `analytics_disable`, `analytics_request_delete`, `analytics_flush`

Backend endpoints:

- `POST https://mimita.fun/api/game/analytics/events`
- `POST https://mimita.fun/api/game/analytics/consent`
- `POST https://mimita.fun/api/game/analytics/deletion-request`
