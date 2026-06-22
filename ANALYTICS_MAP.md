# Mimita Analytics Map

Analytics is opt-out only after the first-launch education popup is answered. Until then the game stores only local consent/config state and does not upload gameplay events.

## Privacy Controls

- Config file: `config/analytics.json`
- Disable permanently: first-launch popup, settings menu, or `analytics_disable`
- Data deletion request: settings menu or `analytics_request_delete`
- Privacy page: `https://mimita.fun/terms/privacy`

The game does not collect chat contents, passwords, session cookies, auth tokens, or sensitive free-form text. The backend rejects those keys if a future event tries to send them.

## Event Schema

Events post to `POST /api/game/analytics/events` as a batch:

- `event_name`: registered event id
- `event_category`: account, gameplay, movement, engagement, ui, retention, errors
- `anonymous_id`: local random id from `config/analytics.json`
- `session_id`: random id for the current game run
- `client_event_id`: session-local de-dupe/debug id
- `username`: local profile username when available
- `account_id`: only when a local profile/config provides it
- `occurred_at`: UTC timestamp from the game
- `properties`: event-specific fields

The backend stores the flexible fields in `analytics_events.event_data` JSONB so new events do not require a migration.

## Events

| Event | Fields | Why It Exists | Used For |
|---|---|---|---|
| `account_created` | username, source | Account growth | Accounts / new players |
| `login` | username, source | Return activity | Returning players |
| `logout` | username, source | Session lifecycle | Session/account debugging |
| `failed_login` | source, identifier_type | Auth quality | Failed login/error card |
| `session_start` | launch_count | Game open | Sessions, retention |
| `session_end` | duration_seconds | Quit point/session close | Session completion |
| `session_duration` | duration_seconds | Session length | Avg session, duration buckets |
| `jump` | none | Movement use | Movement stats |
| `dash` | none | Movement use | Movement stats |
| `air_jump` | none | Movement use | Movement stats |
| `wall_jump` | none | Movement use | Movement stats |
| `map_loaded` | map, map_path, triangles, spawns | Map interest/stability | Most played maps |
| `map_completed` | map | Future completion tracking | Map completion |
| `weapon_used` | weapon | Weapon interest | Most used weapons |
| `settings_opened` | none | Feature use | Most used features |
| `outfit_editor_opened` | none | Feature use | Most used features |
| `replay_viewed` | none | Feature use | Most used features |
| `first_launch` | first_launch_date | Onboarding cohort | New players |
| `day_1_return` | none | Retention | Retention |
| `day_7_return` | none | Retention | Retention |
| `day_30_return` | none | Retention | Retention |
| `crash_detected` | reason | Previous run did not shut down cleanly | Quality |
| `disconnect` | reason | Multiplayer/network exits | Quality |

## Deletion

`POST /api/game/analytics/deletion-request` receives `account_id`, `username`, `email`, and `anonymous_id` when available. The backend:

- writes `analytics_deletion_requests`
- writes `analytics_audit_log`
- emails `hello@mimita.fun` with account id, username, email, anonymous id, and timestamp

Deletion is fulfilled by removing rows matching the user/account/anonymous id from analytics tables and marking the request completed.

## Future Workflow

1. Add the event name and category in `src/analytics/analytics-events.cpp`.
2. Add the same event in `website/server/gameAnalytics.js`.
3. Emit the event through `AnalyticsManager::track(...)` from the subsystem that owns the action.
4. Add a dashboard query/card only if the event needs a first-class card.

No DB migration is needed for ordinary new events because event-specific fields live in JSONB.
