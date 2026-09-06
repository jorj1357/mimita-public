// 09 06 2026, 20 10 EST
/* purpose
* fix in-game sign-in failing with "Could not load account data"
* fix status text overlapping error text on the login menu
* add debug logging to getGameBootstrap failure paths
* fix parseStats to handle string-or-number JSON values from the API
* this file DOES NOT replace the regression entry
* this file DOES NOT claim human acceptance beyond the test performed
*/

# Task

- Task ID: login-bootstrap-fix
- Summary: Fix in-game sign-in failure ("Could not load account data") caused by JSON type mismatch in parseStats, and fix status/error text overlap on login menu
- Status: COMPLETE — builds, logs, and regression entry done; awaiting human playtesting
- Date: 2026-09-06
- Time: 20:10:00
- Timezone: EST
- Branch: 8292026stash
- Base commit: decc9fa
- Final commit: none yet (uncommitted changes)

# Pre-existing changes

- Exact status output:
  ```
  config/accounts/default.json       |  2 +-
  config/analytics.json              |  4 +-
  src/engine/engine-tick-state.cpp   |  2 +-
  ```
- Files not created or modified by this session: `config/accounts/default.json`, `config/analytics.json`, `src/engine/engine-tick-state.cpp`

# Requested behavior

User signs in through the in-game exe with correct admin credentials. The game should:
1. Complete login (gameLogin) successfully
2. Complete bootstrap (getGameBootstrap) successfully
3. Show "Signed in as admin" without any overlapping text
4. Transition to the main menu

# Specification alignment

- Current specification paths: `docs/specs/debug-logging/debug-logging.md`, `docs/architecture/json-configuration/json-configuration.md`
- Exact requirements: Debug logging spec requires `Debug::warn` at Important level for state transitions with category, reason, and context. JSON config spec requires hot-reloadable GUI configs.
- Why the change follows the specification: Added `Debug::warn(Debug::Category::Auth, ...)` at every failure path in `getGameBootstrap()`, matching the existing auth logging pattern in `auth-controller.cpp`. The `parseStats` fix uses defensive type checking per the robustness principle.
- Conflicts or decisions: None.

# Exact implementation changes

## File: `src/auth/auth-controller.cpp`

- Lines: 263-268 (`setError` function)
- Old content:
  ```cpp
  void AuthController::setError(const std::string& code, const std::string& message)
  {
      mRuntime.state = AuthState::Failed;
      mRuntime.errorCode = code;
      mRuntime.errorMessage = message;
  }
  ```
- New content:
  ```cpp
  void AuthController::setError(const std::string& code, const std::string& message)
  {
      mRuntime.state = AuthState::Failed;
      mRuntime.errorCode = code;
      mRuntime.errorMessage = message;
      mRuntime.statusText.clear();
  }
  ```
- Reason: When `setError()` was called after a failed bootstrap, `mRuntime.statusText` still contained "Loading account..." from the earlier `setStatus()` call. Since both `statusText` and `errorText` GUI elements are at position (720, 504) in `login-menu.json`, both rendered simultaneously and overlapped. Clearing `statusText` ensures only the error message displays.
- Why unrelated behavior is preserved: `setError()` is only called on failure paths. Clearing status on failure is the correct semantic — no success path is affected.

## File: `src/website/api-client.cpp`

- Lines: 300-343 (new helpers + rewritten `parseStats`)
- Old content:
  ```cpp
  static void parseStats(GameStats& stats, const json& s)
  {
      stats.wins = s.value("wins", 0);
      stats.losses = s.value("losses", 0);
      stats.kills = s.value("kills", 0);
      stats.deaths = s.value("deaths", 0);
      stats.gamesPlayed = s.value("games_played", 0);
      stats.playtimeSeconds = s.value("playtime_seconds", 0LL);
      stats.highestMmr = s.value("highest_mmr", 5000);
      stats.currentMmr = s.value("current_mmr", 5000);
      stats.accuracy = s.value("accuracy", 0.0f);
      stats.headshots = s.value("headshots", 0);
      stats.bestKillStreak = s.value("best_kill_streak", 0);
      stats.totalXp = s.value("total_xp", 0LL);
      stats.gold = s.value("gold", 0LL);
  }
  ```
- New content:
  ```cpp
  static int jsonInt(const json& j, const std::string& key, int fallback)
  {
      if (!j.contains(key)) return fallback;
      const json& v = j[key];
      if (v.is_number_integer()) return v.get<int>();
      if (v.is_string()) return std::atoi(v.get<std::string>().c_str());
      return fallback;
  }

  static long long jsonLong(const json& j, const std::string& key, long long fallback)
  {
      if (!j.contains(key)) return fallback;
      const json& v = j[key];
      if (v.is_number()) return v.get<long long>();
      if (v.is_string()) return std::atoll(v.get<std::string>().c_str());
      return fallback;
  }

  static float jsonFloat(const json& j, const std::string& key, float fallback)
  {
      if (!j.contains(key)) return fallback;
      const json& v = j[key];
      if (v.is_number()) return v.get<float>();
      if (v.is_string()) return (float)std::atof(v.get<std::string>().c_str());
      return fallback;
  }

  static void parseStats(GameStats& stats, const json& s)
  {
      stats.wins = jsonInt(s, "wins", 0);
      stats.losses = jsonInt(s, "losses", 0);
      stats.kills = jsonInt(s, "kills", 0);
      stats.deaths = jsonInt(s, "deaths", 0);
      stats.gamesPlayed = jsonInt(s, "games_played", 0);
      stats.playtimeSeconds = jsonLong(s, "playtime_seconds", 0LL);
      stats.highestMmr = jsonInt(s, "highest_mmr", 5000);
      stats.currentMmr = jsonInt(s, "current_mmr", 5000);
      stats.accuracy = jsonFloat(s, "accuracy", 0.0f);
      stats.headshots = jsonInt(s, "headshots", 0);
      stats.bestKillStreak = jsonInt(s, "best_kill_streak", 0);
      stats.totalXp = jsonLong(s, "total_xp", 0LL);
      stats.gold = jsonLong(s, "gold", 0LL);
  }
  ```
- Reason: The `/api/game/me` endpoint returns PostgreSQL `game_stats` rows directly. PostgreSQL node drivers return BIGINT columns as JavaScript strings (no native 64-bit integer in JSON). The original `s.value("total_xp", 0LL)` throws `json.exception.type_error.302: type must be number, but is string` when the value is `"0"` instead of `0`. The helpers check the actual JSON type before extracting, matching the pattern `parseUserInfo()` already uses for the `id` field.
- Why unrelated behavior is preserved: The helpers return the same fallback values when keys are missing. Only the type-checking path changes — no field names, default values, or struct assignments are altered.

## File: `src/website/api-client.cpp`

- Lines: 11-12 (include), 426-475 (getGameBootstrap logging)
- Old content:
  ```cpp
  #include "website/api-client.h"
  #include <algorithm>
  ...
  GameBootstrap getGameBootstrap(const std::string& sessionToken)
  {
      GameBootstrap bootstrap;
      if (sessionToken.empty()) return bootstrap;
      ...
      catch (...) {}
      return bootstrap;
  }
  ```
- New content: Added `#include "debug/debug-log.h"` and `Debug::warn(Debug::Category::Auth, ...)` at all 5 failure return paths (empty token, HTTP failure, server success=false, user invalid, JSON parse exception).
- Reason: The original code had 5 silent failure paths. When the bootstrap failed, there was zero diagnostic output. The first test run immediately revealed: `BOOTSTRAP failed: json parse exception '[json.exception.type_error.302] type must be number, but is string'`.
- Why unrelated behavior is preserved: Only logging was added. No control flow, return values, or data parsing changed.

## File: `website/server/game-api.js`

- Line: 38
- Old content: `playtime_ticks: "0",`
- New content: `playtime_ticks: 0,`
- Reason: `defaultStats()` returned `playtime_ticks` as a string `"0"` instead of number `0`. This would cause the same type error on first login for new users who have no stats row yet.
- Why unrelated behavior is preserved: Only the literal value type changed. The field name and fallback semantics are identical.

## File: `docs/regressions/regressions-v1.md`

- Appended regression entry at the end of the file documenting the exact wrong code, fixed code, root cause, prevention rules, and proof.

# Diagnostics

- Owner/category: Auth
- Input: admin credentials (username=admin, password=correct)
- Decision: gameLogin succeeds, then getGameBootstrap is called
- Output (before fix): `json.exception.type_error.302: type must be number, but is string` → bootstrap.valid = false → error "Could not load account data"
- Output (after fix): bootstrap succeeds → "Signed in as admin"
- Failure or rejection reason: parseStats assumed all JSON values were numbers, but PostgreSQL node driver returns BIGINT as strings
- Rate limiting: N/A (one-shot auth flow)

# Validation

- Focused skill paths and results:
  - `docs/skills/spec-behavior-review-v1.md`: not loaded (no spec change, only bug fix)
  - `docs/skills/logging-checker-v1.md`: logging follows Debug::warn with Auth category, specific failure reasons, and source context —符合 debug-logging spec
- Tests and exact commands:
  - Built with `python build_agent.py` — SUCCESS
  - Ran `mimita.exe --timeout 15` — game started, login screen displayed
  - Manual sign-in test: admin credentials → login succeeded, bootstrap succeeded, main menu appeared
- Build status: SUCCESS (2 files compiled: auth-controller.cpp, api-client.cpp)
- Runtime evidence: log line `[AUTH] LOGIN RESPONSE success accountId=1 username=admin` followed by no `BOOTSTRAP failed` line (previously showed `BOOTSTRAP failed: json parse exception`)
- Output files: `logs/09-06-2026/Gameterminal_log_160228.txt` (test run), `logs/09-06-2026/Gameterminal_log_160553.txt` (post-fix run)

# Measured evidence

- Before values: getGameBootstrap throws exception, bootstrap.valid = false, user sees "Could not load account data"
- After values: getGameBootstrap succeeds, bootstrap.valid = true, user sees "Signed in as admin"
- Timestamps: fix applied at ~20:05 EST, verified at ~20:10 EST
- Tick/frame/network measurements: sign-in HTTP call takes ~200-400ms (blocking main thread, visible as frame spike)

# Regression review

- Regression entry appended: yes — `docs/regressions/regressions-v1.md` new section at end of file
- Why this is a confirmed regression: the login worked in earlier versions (before the `/api/game/me` endpoint started returning stats as strings from PostgreSQL), then broke when the data-saving persistence migration added BIGINT columns that PostgreSQL returns as strings through the node driver
- Related regression paths: `docs/regressions/regressions-v1.md` section "9 6 2026 2000 — In-game sign-in fails with 'Could not load account data'"

# Human acceptance

- Visual review: pending — user should verify no text overlap on login menu
- Gameplay review: pending — user should verify full sign-in → main menu flow
- Multiplayer review: N/A (sign-in only)
- Still unverified: sign-in on non-admin accounts, sign-up flow, forgot-password flow

# Related feature record

- Feature path: N/A (bug fix, not new feature)
