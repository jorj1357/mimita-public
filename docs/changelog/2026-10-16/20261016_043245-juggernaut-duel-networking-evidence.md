# Juggernaut Duel Dev Loop and attack rejection evidence

- Timestamp: 2026-10-16T04:32:45Z (2026-10-16 00:32:45 America/New_York, EDT)
- Branch/HEAD: `20261011zt` / `3a345200`
- Result: `SOURCE_AND_LOG_ROOT_CAUSE_IDENTIFIED; FIX_PENDING`

## Outcome

Updated `docs/specs/gamemodes/juggernaut-duel.md` with a persistent Dev Loop
launch path, the Jorj-reported 2026-10-16 00:24 implementation attempt, and a
networking TODO grounded in the supplied live journal. Added launch mode 13,
`juggernaut_duel`, to `devscripts/dev-launch-modes.json`.

The documented start paths are:

- Fresh loop: `python devscripts/dev-loop.py --profile npc-navigation --launch-mode 13 --auto-restart`
- Existing loop: `python devscripts/dev-loop.py --request launch --launch-mode 13`
- Interactive loop: press `2`, enter `13`, then press `1` to launch.

## Attack rejection finding

Inspected `logs/2026-10-16/20261016_042126/events.jsonl` (87,796,860 bytes).
It contains 321 `weapon.attack.slot_mismatch` records for Juggernaut Duel role
weapons: 287 `large_machine_gun`, 26 `heavy_shotgun`, and 8 `force_punch`.
Example request 61 reports requested logical slot 2 versus expected slot 9 for
the Large Machine Gun, with `actor_weapon_set: 11` and `mode_weapon_set: 1`.
Heavy Shotgun requests report requested slot 1 and expected slot 24.

Source explains the first divergence: Duel roles use weapon set 11, while the
mode state uses set 1. `objectiveRounds` is true only for `victoryCondition ==
"rounds"` (`server-gamemode.cpp`, `serverCommunityStartMatch`); this first-to-
score mode is not an objective-round mode. Consequently
`serverCommunityWeaponLogicalSlot(const ServerPlayer&, ...)` falls back to the
mode-level set instead of the participant role set. `handleAttackRequest`
rejects mismatched slots with result reason 4. This establishes the cause of
the role-weapon requests recorded in this journal; the spec leaves the shared
set-ownership correction as a networking TODO pending implementation and a
repeat live check.

The same journal contains 27 hitscan-to-NPC `server.damage.applied` records
from a separate server process running sandbox behavior. They are not evidence
that Juggernaut Duel attacks succeeded. The log mixes process records, so the
spec calls out that distinction.

## Live process check and limits

Queued the read-only `versioninfo` command to the active client. Its
`versioninfo.executed` record confirms PID 2900, executable
`.dev/builds/2415/mimita.exe`, server tick 16441, and the supplied journal path.
Current Dev Loop status is launch mode 1 (`current`, map `cramped`); its tracked
server PID 10504 is no longer alive, and a separate sandbox server PID 33072 is
active. No mode switch or process replacement was performed. This does not
provide a fresh live Duel replay or verify a fix.

## Documents and review

Read `docs/ROUTER.md`, `docs/specs/gamemodes/gamemodes.md`,
`docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/operations/task-completion/task-completion.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/architecture/terminal-commands/terminal-commands.md`,
`docs/architecture/collision/collision.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/regressions/README.md`,
`docs/skills/spec-behavior-review-v1.md`, and
`docs/skills/documentation-checker-v1.md`.

- Spec/behavior review: `PASS_WITH_HUMAN_REVIEW`. Concrete log/source
  disagreement found; source root cause identified; code correction and
  accepted damage/replication proof remain outstanding.
- Documentation checker: `PASS_WITH_WARNINGS`. The new cross-links resolve.
  The router's `docs/doc-review-09-03-2026.md` target is absent in this
  checkout. Whole-docs TODO scan found existing notes; relevant entry:
  `docs/specs/gamemodes/gamemodes.md:3`, “todo this not good but move tothis
  version more more general things for gamemodes”. Suggested replacement:
  make it a structured declarative gamemode scalability section covering stable
  IDs, JSON-owned rules/lifecycle/UI, and shared runtime consumers.
- `git diff --check` passed. Launch-mode JSON parsed and mode 13 was verified.
  No code build or tests were run for this documentation/config task.
- Current live Duel acceptance and visible/multiplayer gameplay remain open.

## Scope and preservation

This turn changed the Juggernaut Duel spec and added launch mode 13. Earlier
Juggernaut implementation edits and existing working-tree edits to
`assets/avatars/abusivegirlheadless4/avatar.json`, its backup, account,
analytics, and music settings were preserved. No other process was stopped.
