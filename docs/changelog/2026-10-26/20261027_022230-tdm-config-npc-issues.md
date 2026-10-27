# TDM configuration and NPC issue intake

- Result: PASS_WITH_HUMAN_REVIEW
- Timestamp: 2026-10-27T02:22:30Z (2026-10-26 22:22:30 EDT)
- Branch: `20261011zt`
- Base commit: `1cc507aa449b7aa6aae66bdf2749d62e3c84bc7f`; no commit created.
- Pre-existing edits: configuration changes including `config/camconfig.json`, `config/accounts/default.json`, `config/analytics.json`, `config/collision.json`, `config/crosshair.json`, `config/devtimertips.json`, `config/gamemodes/masterlist/gamemode-masterlist.json`, and `config/hitfx.json`; `config/video-settings.json`; `src/network/server-gamemode.cpp/.h`; the open gameplay issue document; an untracked map asset; prior changelog, gold, regression, and Clown-spec files. Preserved them. In particular, did not edit `config/camconfig.json`.

## Changed

- `docs/specs/open-gameplay-issues.md`, New intake near lines 25-68: expanded the TDM NPC non-response report, added the camera configuration persistence/override report, and clarified the request for nicer authored NPC animations.
- `docs/specs/open-gameplay-issues.md`, gameplay issue table near lines 753-756: updated NPC combat summary and added a camera override item.

## Exact content changes

- NPC report: expanded from close-pass failure to acknowledge or damage a nearby player to include cases where NPCs reportedly do not attack anyone; requested evidence includes NPC state, target/team/visibility eligibility, accepted shots, and nearby actors. Cause and relationship to the post-death shooting issue remain unknown.
- Camera report: records the player's desired layers: persistent `config/camconfig.json` baseline, temporary match-scoped gamemode overrides that restore afterward, and the most temporary console changes. Precedence and whether a mode can block a console command remain open and should be visible per setting.
- Source finding: `SettingsBackup::saveBackups()` copies `config/camconfig.json` to `.bak` before a community-match presentation override; `CommunityMatchClient::reset()` restores the backup over the JSON file. The backup path is triggered for any nonzero FOV/ragdoll/blood override, and TDM has explicit `presentation.ragdolls: false` (encoded as an explicit disabled override). This confirms a potential TDM-related disk restore mechanism, but not that the JSON file changes at match start or that the user reproduced this exact timing.
- Console lead: the `scm` and `settings_camera_smoothness` command handlers in `src/camera/camera-commands.cpp` print the supplied value without applying it to `CamConfig`; the failing command was not identified, so this is recorded as a lead rather than a confirmed cause.
- NPC animation request: changed the existing intake title to ask for nicer, more polished authored NPC animations while retaining the previously recorded shared-state examples and AimBody/hitbox questions.

## Review and validation

- Read `AGENTS.md`, `docs/ROUTER.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/gamemodes/teamdeathmatch.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/specs/animations/animations.md`, `docs/features/camsway-realisticish/camsway.md`, `docs/regressions/README.md`, `docs/skills/spec-behavior-review-v1.md`, and `docs/skills/documentation-checker-v1.md`.
- Source trace: `src/config/settings-backup.cpp::SettingsBackup::saveBackups()` and `restoreBackups()`; `src/network/community-match-client.cpp::CommunityMatchClient::reset()` and the gamemode presentation-override block; `src/network/server-gamemode.cpp` encodes explicit ragdoll false as packet value 1; `src/network/packets.h` defines 1 as disabled and 0 as no override; `src/camera/camera-commands.cpp::registerCameraCommands()` contains the no-op smoothness handlers.
- Documentation Checker v1: PASS. User-reported timing and NPC behavior are attributed; code findings and unresolved runtime questions are distinguished.
- `docs/doc-review-09-03-2026.md`, referenced by the router, is absent from this checkout.
- `git diff --check`: PASS for whitespace. Git emitted line-ending notices for pre-existing `config/collision.json` and `config/gamemodes/masterlist/gamemode-masterlist.json`; neither was changed in this session.
- No build or runtime TDM/NPC validation was performed. To resolve timing and precedence, compare camconfig bytes/hash and runtime camera values before, during, and after TDM, including backup/restore events; identify the exact console command and whether it applies. Capture the non-attacking NPC's mode/run and target/attack state before assigning a cause. Human review remains needed for authored animation quality.
