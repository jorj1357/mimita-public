# Replay export and frame pacing evidence intake

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-27T18:40:23Z (2026-10-27 14:40:23 EDT)
- Branch: `20261011zt`
- Base commit: `beb7f93a8c2041ea8308161cfc15c734126c6e9e`; no commit created.
- Pre-existing edits: existing changes to avatar JSON files (`assets/avatars/abusiveboy/avatar.json`, `abusivegirl/avatar.json`, `abusivegirl2/avatar.json`, `abusivegirl3/avatar.json`, `boiledboy/avatar.json`, `clown/avatar.json`, `jason/avatar.json`); animation JSON (`config/animations/library/weapons/chainsaw.json`, `mace.json`, `swordsword.json`, `config/animations/masterlist/animation-masterlist.json`) and untracked `config/animations/masterlist/animation-sets.json`, `config/animations/settings.json`; other modified config files (`config/accounts/default.json`, `analytics.json`, `audio/music-settings.json`, `command-aliases.json`, `devtimertips.json`, `gamemodes/masterlist/gamemode-masterlist.json`); modified animation, GUI, and network source files (`src/entities/player-animation-config.cpp`, `player-avatar-animations.cpp/.h`, `player.cpp`, `player.h`, `src/gui/gui-main.cpp`, `src/gui/menus/online-menu.cpp`, `src/network/community-match-client.cpp`, `community-server-config.cpp`, `server-avatar-network.cpp`, `server-gamemode.cpp`, `server-players.cpp`, `server.cpp`). Preserved. `docs/specs/open-gameplay-issues.md` was already modified from earlier intake work; this turn added the requested evidence without reverting it.

## Changed

- `docs/specs/open-gameplay-issues.md`, new subsection “New intake — 2026-10-27: replay export and frame pacing” near lines 60-80: added the exit-code-3 export failure and Freecam-versus-character-control performance report.
- `docs/specs/open-gameplay-issues.md`, issue summary table near lines 848 and 854: added current character-control frame-pacing report and updated replay export with the concrete screenshot failure.
- `docs/specs/open-gameplay-issues.md`, priority rank 8 near line 1212: noted the current displayed export failure and linked evidence.
- `docs/evidence/replay/20261027_183724-export-failed-frame-pacing.png`: copied the user-provided screenshot (SHA-256 `F646896BB3633A6EDDF30910D473E2F70094DE7ECE8236984048EC100AF46F35`). Its source file timestamp is 2026-10-27 14:37:24 EDT / 2026-10-27T18:37:24Z.

## Exact content changes

- Added the screenshot text `Export Failed: Export subprocess failed (exit code 3)`, with room `SID6XXBP`. Cause remains unknown because no export request details, subprocess output, or matching journal were supplied.
- Added the report that player-controlled gameplay is very choppy, Freecam feels faster, and returning to player control makes performance poor again. The user noticed choppiness after animations were added but is unsure whether it began around 09:00; this timing is approximate and recorded only as a correlation.
- Recorded visible HUD fields as a single unvalidated screenshot sample: FPS AVG 38, MAX 6503.84 ms, PHYS 48.94, CPU 1091.84 ms. The screenshot's subsystem timing rows show different values; this intake does not treat them as one coherent profile or infer a cause.
- Confirmed melee issues were already tracked in the 2026-10-27 intake, including airborne melee input and deliberate weapon collision coverage; no new melee symptom was visible in this screenshot.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/replay/replay.md`, `docs/specs/performance/performance.md`, `docs/operations/asset-management/asset-management.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/efficiency-checker-v1.md`, `docs/skills/documentation-checker-v1.md`, `docs/regressions/README.md`, and `docs/operations/task-completion/task-completion.md`.
- The router references `docs/doc-review-09-03-2026.md`; it is absent from this checkout.
- Documentation Checker v1: PASS. User report, visible screenshot text, historical melee entries, and unverified performance causes are distinguished.
- `git diff --check`: PASS for whitespace. Git printed line-ending notices for unrelated pre-existing changes; those files were not touched.
- No build, test, replay export retry, or controlled performance comparison was performed. Next evidence should include the exact build and export request/subprocess output/journal, plus matched Freecam and character-control frame-time windows on the same scene. Do not attribute the performance issue to animation changes until that comparison identifies the first cost difference.
