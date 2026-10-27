# Dry-fire HUD feedback

- Task ID: current Codex task `01a17b2f-1d44-7e73-be67-029877dd22fe`
- Summary: Add a configurable exclamation marker and temporary dry-fire crosshair color when a local weapon action is rejected by weapon readiness.
- Status: PASS_WITH_HUMAN_REVIEW
- Date, time, timezone: 2026-10-27T03:12:05Z (2026-10-26 23:12:05 EDT, America/New_York)
- Branch: `20261011zt`
- Base commit: `1cc507aa449b7aa6aae66bdf2749d62e3c84bc7f`
- Feature commit: `380396b046cb875f5cc14563c79edd75707a486d` (`exclaaiton poitn on  dry fire`). Two follow-up source edits remain uncommitted in `src/engine/engine-tick-ui-game-hud.cpp` (resolve local mode IDs) and `src/gamemode/gamemode.h` (format the new nullable-mode fields on separate lines).
- Final executable: `.dev/builds/3399/mimita.exe`

## Pre-existing changes

The workspace was already dirty at task start. The user committed the current worktree during this task as commit `380396b0`; that commit also contains unrelated pre-existing changes in account, analytics, audio, collision, video, hit effects, gamemode/NPC configuration and source, map assets, changelogs, regression notes, and the open-gameplay-issues document. Those unrelated changes were preserved and are not attributed to this feature. The user's existing crosshair appearance choices (including red/green/blue, alpha, size, dot, and arm visibility) were retained while adding the three `dryFire*` RGB fields. The pre-existing avatar allowlist edit in the gamemode masterlist was retained.

After that commit, the working tree also contained user changes in `config/accounts/default.json`, `config/analytics.json`, `config/audio/music-settings.json`, and `config/video-settings.json`. They were left untouched.

## Requested behavior

- On an actual local primary-action rejection due to an empty clip, reload, or weapon cooldown, show a typed `!` just to the right of the currently rendered crosshair.
- Fade marker alpha linearly from 1.0 to 0.0 across 30 client/UI ticks by default. A new rejected attempt restarts the timer at full opacity.
- Temporarily tint the crosshair with an RGB value configured in `config/crosshair.json`, then restore its ordinary configured RGB and alpha.
- Allow the shared HUD element to be edited and hot-reloaded, and let each mode inherit or override its visibility.
- Do not trigger on menu, pause, match-state, or no-weapon gates.

## Implementation and how to edit it

The weapon firing result now carries a typed readiness reason (`EmptyClip`, `Reloading`, or `Cooldown`) in `src/combat/weapon-types.h`. The owning firing paths in `src/combat/weapon-system.cpp` set that reason when rejecting a shot. `src/terminal/weapon-commands.cpp` uses it only after a local action was actually rejected during gameplay and resets the HUD feedback timer.

The crosshair state and fade calculation live in `src/crosshair/crosshair-render.cpp`. The fade uses elapsed UI ticks divided by the configured lifetime and clamps to zero at the lifetime. The HUD renderer in `src/engine/engine-tick-ui-game-hud.cpp` draws the marker with the existing typed text primitive, positions it from the crosshair's rendered coordinates (including physical aim), applies the current fade to its alpha, and uses the temporary crosshair tint. Bounded structured events `weapon.dry_fire_feedback` and `weapon.dry_fire_hud_drawn` record the rejected reason and the marker's mode, alpha, and position.

To customize the marker, edit the `dryFireIndicator` text object in `config/gui/hud.json`:

- `visible`: shared default on/off.
- `text`: glyph, default `!`.
- `fontSize`: typed-font scale, default `0.5`.
- `textColor`: normalized RGBA values from 0.0 to 1.0. Its alpha is the starting alpha multiplier; the fade scales it down to zero.
- `x`: extra horizontal gap after the crosshair, default `8`.
- `y`: vertical offset, default `-10`.
- `fadeTicks`: fade duration in client/UI ticks, default `30`.

To customize the crosshair tint, edit `dryFireRed`, `dryFireGreen`, and `dryFireBlue` in `config/crosshair.json`. They use integer RGB channels from 0 to 255 and are independent of the normal `red`, `green`, `blue`, and `alpha` settings.

To override visibility by mode, set `presentation.dry_fire_indicator` in that mode's `config/gamemodes/<mode>.json` file to `true` or `false`. Leave it `null` to inherit `dryFireIndicator.visible`. The masterlist schema defines this nullable Boolean and all 16 mode JSONs include its null default. HUD layout, crosshair, and gamemode configuration use their existing reload polling, so valid edits take effect without restarting; invalid configuration retains the last valid state.

## Specification alignment and changes

- `docs/specs/gui/guiv2.md`, heading “Dry-fire crosshair feedback”: records trigger conditions, timer restart, placement, tint, mode override, and hot reload.
- `docs/specs/weapons/weapons.md`, heading “Local primary-action readiness feedback”: records the typed rejection result and separates client presentation from server attack authority.
- `config/gui/hud.json`, `dryFireIndicator`: adds the default `!` text object, size, color, placement, visibility, and 30-tick fade.
- `config/crosshair.json`, `dryFireRed/Green/Blue`: adds default tint RGB (255, 38, 38), preserving the user's prior ordinary crosshair settings.
- `config/gamemodes/masterlist/gamemode-masterlist.json`, `presentation.dry_fire_indicator`, and the 16 mode JSON files: add nullable per-mode visibility overrides.
- `src/gamemode/gamemode.h/.cpp`: load and distinguish an absent/null override from an explicit Boolean.
- `src/crosshair/crosshair-config.h/.cpp`: load, clamp, save, and hot-reload tint RGB.
- `src/crosshair/crosshair-render.h/.cpp`: track feedback lifetime, compute fade, tint crosshair RGB, and emit a rate-limited readiness diagnostic.
- `src/engine/engine-tick-ui-game-hud.cpp`: draw marker beside the actual crosshair and emit a bounded HUD submission event. If the community match has no mode ID, use `ACTIVE_GAME_MODE` so local modes such as sandbox receive their configured override too.
- `src/terminal/weapon-commands.cpp` and `src/combat/weapon-system.cpp/.h`: connect actual weapon readiness rejections to HUD feedback.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/specs/gui/guiv2.md`, `docs/specs/gui/gui-general.md` (the router's `docs/specs/gui/gui.md` is absent), `docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/architecture/time-and-formatting/time-and-formatting.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/operations/build-and-exe/build-and-exe.md`, `docs/operations/task-completion/task-completion.md`, `docs/specs/debug-logging/debug-logging.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/skills/logging-checker-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- Focused JSON masterlist synchronization check: `python devscripts/gamemode_json_sync.py --check` passed.
- Whitespace/diff validation: `git diff --check` passed.
- Full `python devscripts/config_selftest.py` was attempted earlier and failed on unrelated existing weapon/avatar configuration normalization errors, including malformed legacy weapon comment data and missing avatar fields. No errors in the new dry-fire configuration were identified by that run; it is not counted as a passing check.
- Canonical dev-loop build: build 3399 published successfully (`python devscripts/dev-loop.py --request build --wait-for-build`). This establishes compilation/linking only.
- Exact executable identity: `.dev/builds/3399/mimita.exe --versioninfo` returned `EXECUTABLE_PATH=C:\mimita-v9\.dev\builds\3399\mimita.exe`, run ID `20261026_231202`, and `EVENTS_JSONL_PATH=logs/10-26-2026/20261026_231202/events.jsonl`.
- Live runtime scenario: dev-loop launch mode 19, runtime build 3398, journal `logs/2026-10-27/20261027_030822_01/events.jsonl`. At journal lines 862–865, a revolver `empty_clip` rejection triggered `weapon.dry_fire_feedback`, and `weapon.dry_fire_hud_drawn` recorded mode `sandbox`, alpha `1.0`, crosshair position `(512,384)`, marker position `(526.27,376.89)`, and `crosshair_tinted=true`. This proves the rejection-to-HUD drawing path executed in the running client. Build 3399 includes only the subsequent header formatting change; the runtime feature path is identical.
- Human visual review is still needed to judge marker size, red tint, and readability while moving and aiming. The runtime event confirms that drawing was submitted, not how it looked to a player.
- Hot reload was confirmed from the existing HUD layout, crosshair config, and gamemode polling owners in source, but live file-edit/reload behavior was not exercised during this run.
- Focused skills: spec behavior review — PASS; logging checker — PASS (owner-side reason and draw diagnostics are bounded); documentation checker — PASS_WITH_NOTES (stale router target `docs/specs/gui/gui.md` is absent and existing TODOs remain).
- Existing TODO notes observed: `docs/specs/gui/gui-general.md:1` says “todo explainnnnnnn” (suggestion: state the specific GUI rule and link its owner); `docs/specs/gui/guiv2.md:23,25` ask to explain command-driven GUI architecture (suggestion: document commands as action owners and GUI controls as callers); `docs/specs/weapons/weapons.md:74` says “todo explain” (suggestion: replace it with the exact unresolved weapon rule and decision owner). These unrelated notes were left unchanged.

## Regression and acceptance

- Regression entry appended: no. No regression was confirmed; the reported behavior was implemented and observed in a live sandbox run.
- Source validation: passed focused checks and final canonical build 3399.
- Runtime validation: passed for an empty-clip rejection reaching marker/tint drawing in live runtime build 3398.
- Human acceptance: pending. No direct player-facing visual inspection was recorded.
