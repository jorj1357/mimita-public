# Competitive no-headless avatar group and gameplay issue updates

- Recorded: 2026-10-26T13:31:20Z (2026-10-26 09:31:20 EDT, America/New_York)
- Branch: `20261011zt`
- HEAD before this change: `4d96b119f54c3e08cfc840c7f41ae77709ee4fdd`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Create a reusable `competitive_no_headless` avatar group that permits all
non-headless, non-monster avatars; apply it to Team Deathmatch (TDM); and add
the current bullet-hole appearance and NPC avatar-scale world-collision reports
to the gameplay issue records.

## Changes

- `config/gamemodes/masterlist/gamemode-masterlist.json`, avatar group enum at
  line 1289 and group definition at line 1345:
  - Added `competitive_no_headless` to the enum.
  - Added `allowed: ["*"]` and `denied: ["*monster*", "*headless*"]`.
  - The code review of `src/gamemode/match-roles.cpp`'s avatar policy matcher
    confirms case-insensitive wildcard matching and deny-before-allow
    evaluation. The loader in `src/gamemode/gamemode.cpp` appends this shared
    policy to the mode policy.
- `config/gamemodes/tdm.json`, avatar policy at line 250:
  - Replaced the former explicit allowlist (which included headless variants)
    and empty group name with `allowed: []`, `denied: []`, and
    `group: "competitive_no_headless"`.
  - Preserved the pre-existing working-tree changes to TDM goal value 100,
    weapon set 29, and team capacity 24.
- `docs/specs/open-gameplay-issues.md`:
  - Updated the detailed bullet/world-impact issue at line 397 and priority
    item at line 761 with the 2026-10-26 report that bullet-hole visuals still
    do not match the intended appearance. Earlier shape work is recorded as
    implemented, but user visual acceptance remains open.
  - Updated the TDM invisible-enemy issue at line 411 to record the current
    working-tree team capacity of 24 while preserving the report of 99 per
    team and the absence of runtime evidence.
  - Updated NPC collision issue at line 474 to describe large avatars
    colliding as normal-sized and the desired size-scaled collision behavior;
    cause remains unverified.
- `docs/specs/open-gameplay-issues-human.md`:
  - Mirrored the bullet-hole update at line 208 and large-NPC collision report
    at lines 242 and 284.

## Requirements and review

- Consulted `docs/ROUTER.md`, `docs/specs/actor-presets/actor-presets.md`,
  `docs/specs/gamemodes/gamemodes.md`,
  `docs/specs/gamemodes/teamdeathmatch.md`,
  `docs/architecture/json-configuration/json-configuration.md`,
  `docs/architecture/time-and-formatting/time-and-formatting.md`,
  `docs/operations/task-completion/task-completion.md`,
  `docs/skills/spec-behavior-review-v1.md`, and
  `docs/skills/documentation-checker-v1.md`.
- `docs/doc-review-09-03-2026.md`, routed by `docs/ROUTER.md`, is absent from
  this checkout. No replacement review document was inferred.
- The behavior review found no contradiction: the existing wildcard policy
  matcher checks denied patterns before allowed patterns, so the deny rules
  exclude monster/headless IDs even though the group allows `*`.
- Documentation keyword scan found only examples/instructions in
  `docs/ROUTER.md:120` and `docs/skills/documentation-checker-v1.md:22`, plus
  historical changelog mentions; no actionable TODO comment was found.

## Validation and evidence

- `python devscripts/gamemode_json_sync.py --check`: `PASS` (`[GAMEMODE SYNC]
  PASS`).
- `git diff --check`: `PASS`.
- No build was run because this change only updates JSON configuration and
  issue documentation.
- No live TDM scenario or journal was inspected for this change. Configuration
  acceptance by a running game, actual selected avatars, and in-game visual
  acceptance have not been established.
- Bullet-hole appearance and scaled NPC/world collision remain user-reported
  issues, not verified regressions or completed fixes.

## Pre-existing working-tree changes preserved

- Modified before this task: `config/accounts/default.json`,
  `config/gamemodes/tdm.json`, `config/movement.json`, and
  `docs/specs/open-gameplay-issues.md`.
- Existing edits in `config/gamemodes/tdm.json` set goal value 100, team
  capacities 24, and weapon set 29; all were retained.
- Existing gameplay issue text included the TDM invisible enemy report; it was
  retained and clarified with the current capacity.
- Existing untracked file
  `docs/changelog/2026-10-26/20261026_042100-tdm-invisible-players.md` was
  left untouched.
- No files were committed.

## Human review still needed

Load TDM and confirm the mode policy is applied to both teams, normal avatars
remain selectable, and headless/monster avatars are rejected. Separately
reproduce the bullet-hole visuals against the intended reference and verify
NPC collision dimensions against large and small avatar models. Source/config
validation does not prove those live results.
