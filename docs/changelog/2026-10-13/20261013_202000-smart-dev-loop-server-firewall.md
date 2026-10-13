# Smart dev-loop server and firewall policy

- Timestamp: 2026-10-13T20:20:00Z
- Branch: 20261011zt
- Result: PASS_WITH_HUMAN_REVIEW
- Pre-existing active runtime preserved; no process was stopped or restarted.
- Pre-existing/unrelated worktree edits preserved: config/gamemodes/zombie_tower.json,
  src/gui/ui-system.cpp, and src/network/server-gamemode.cpp.

## Requested outcome

Align devscripts/dev-loop.py with docs/specs/dev-loop/dev-loop.md so builds do not unnecessarily interrupt runtime, stale servers are replaced safely, and ordinary local dev launches do not request public Windows Firewall access.

## Source changes

Updated devscripts/dev-loop.py:

- Added .dev/server-owner.json persistence containing checkout root, PID, executable, arguments, build/run identity, journal, and readiness.
- Added Windows process identity lookup using executable path and command line.
- Added safe adoption of a previous server only when the persisted marker and live process identity match.
- Added PID/executable/server-command verification before terminating an adopted server.
- Added explicit replacement and stop-result structured events.
- Added status fields for server_adopted, build_mismatch_reason, and firewall_result.
- Added mismatch reasons such as server_uses_older_published_build, server_executable_build_number_or_hash, and server_launch_configuration.
- Added loopback-only firewall preflight for ordinary dev profiles.
- Non-loopback binds are rejected unless allow_external_bind is explicitly set. Explicit external binds remain marked EXTERNAL_BIND_REQUIRES_FIREWALL_REVIEW and cannot claim FIREWALL_CLEAR.
- Loopback launches become FIREWALL_CLEAR only after the bounded launch monitor observes no firewall prompt; a detected prompt remains an explicit result state.
- Existing builds are not stopped during compilation. Replacement occurs only during explicit launch reconciliation after a verified build is available.

Updated docs/specs/dev-loop/dev-loop.md to make the ownership marker path and ordinary-profile loopback requirement exact.

## Validation

- python -m py_compile devscripts/dev-loop.py: passed.
- python devscripts/dev-loop.py --help: passed.
- git diff --check: passed; only the repository normal LF/CRLF warning remains.
- Read-only status of the existing daemon showed an older live server at build 2195 and a newer current published build; no replacement was attempted.
- No real fresh-daemon launch, server replacement, firewall observation, or mode-11 runtime acceptance was claimed because the active user-owned daemon still runs the previous script version.

## Human/runtime review required

Restart the dev-loop through its documented lifecycle, then perform a fresh explicit launch and verify:

1. the new daemon writes or adopts .dev/server-owner.json;
2. status reports the first mismatch reason;
3. an old owned server is stopped only immediately before launching the new verified build;
4. the new server/client identities match;
5. the fresh journal contains firewall preflight, stop-result, launch, and readiness events;
6. no Windows Firewall popup appears during a headless loopback launch.

Do not use this source/build evidence as proof that live behavior has already changed; the active daemon must be refreshed first.

