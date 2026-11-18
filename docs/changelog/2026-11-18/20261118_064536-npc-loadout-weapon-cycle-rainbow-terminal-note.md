# NPC loadout weapon cycle and rainbow terminal note

- Task ID: session-20261118-npc-loadout-weapon-cycle
- Session ID: session-20261118-npc-loadout-weapon-cycle
- Owner: Codex
- Focus: `src/npc/npc.cpp` weapon-cycle candidates; dev-loop terminal UI preference
- Summary: Prevent NPC profile weapon cycles from choosing weapons outside the resolved loadout; record request to restore the older rainbow terminal UI and make build activity clear without implying continuous builds.
- Status: SOURCE CHANGE SAVED; automatic game build observed in progress; runtime validation pending
- Date, time, timezone: 2026-11-18T06:45:36Z (2026-11-18 01:45:36 EST, America/New_York)
- Branch: `checkpoint/one-checkout-condensation-20261111`
- Commits: none

## Pre-existing changes

The shared checkout already contained unrelated edits across configuration,
dev-loop, NPC navigation profile, networking, UI, docs, and untracked
changelogs/regressions. `src/npc/npc.cpp` and the docs changed in this session
were clean before these edits. No unrelated file was changed or reverted.

## Findings

The live dev-loop run `20261118_062057` remains Payload (`server-01` PID 18992,
`client-01` PID 19804). A separate untracked server PID 14176 was launched by
the client process with `--mode duel --weapon-set 3`; it wrote its events into
the tracked client's journal. That Duel match resolved NPC 200000 to weapon
set 3 with `rocket_launcher` and `nothing`, then switched the same NPC to
`swordsword` and `chainsaw`.

`config/behavior-profiles.json` gives the shared `elite_squad` profile a cycle
containing both ranged and melee weapons and sets
`weapon_cycle_include_melee: true`. In `NpcSystem::updateOneNpc`,
`src/npc/npc.cpp` copied the profile cycle directly, then appended the
resolved loadout. It did not restrict profile entries to that loadout.
`npcSwitchWeapon` accepts any registered weapon, allowing the profile to
override the mode/role's resolved inventory. This disagrees with
`docs/specs/gamemodes/gamemodes.md`, which says the explicit host-selected set
is the shared effective loadout for human and NPC actors, and
`docs/specs/npc-behavior/npc-behavior.md`, which says the resolved role/mode
loadout determines which weapons exist.

The source of the Duel evidence is PID 14176. The normal dev-loop status is a
different Payload match, so it is not evidence that the Payload server changed
mode. The separately launched server was left running and untouched.

## Exact changes

### `src/npc/npc.cpp`, `NpcSystem::updateOneNpc`

- Before: `std::vector<std::string> order = npc.behavior.weaponCycle;`, then
  append any loadout weapons not already listed.
- After: begin with only profile cycle entries found in the resolved loadout,
  then append any remaining resolved loadout entries.
- Reason: a behavior profile can choose tactics for available weapons; it
  cannot grant weapons excluded by the effective mode/role loadout.
- Source ownership: `build/source-manifest.json` lists this file as
  `owner=game`, so this is eligible for the hot game-DLL path.

### `docs/specs/dev-loop/dev-loop.md`

Added the 2026-11-18 user preference under “Native dashboard and contributor
activity feed — desired”: restore the older rainbow terminal UI as the primary
visible interface, make current build activity easy to see, and clarify that
builds happen on source changes or explicit requests rather than continuously
while idle. Restoring the presentation still needs implementation and visual
review.

### Regression record

Added [duel-npc-weapon-cycle-REG-2026-11-18.md](../../regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md)
and Entry 42 in the gameplay regression index. The record marks the hot build
and runtime behavior as pending; it does not claim the behavior fix is proven.

## Validation and evidence

- Read the routed gamemode, actor preset/role, NPC behavior, weapon, runtime
  validation, hot-reload, source-ownership, regression, time-formatting, and
  task-completion documents; reviewed `docs/skills/spec-behavior-review-v1.md`
  and `docs/skills/documentation-checker-v1.md`.
- Read-only runtime checks: `python devscripts/dev-loop.py --status`,
  `python devscripts/dev-loop.py --list-instances`, process command lines, and
  parsed JSONL events in run `20261118_062057`.
- Source inspection and `git diff --check` passed for changed tracked files.
- No tests were added or run. No direct build request, compiler, or manual
  process operation was used. After the source save, the one observed dev-loop
  status reported `game-build status=building`; it did not yet report a
  generation or activation. No runtime evidence for the modified generation
  exists yet.
- Human visual and gameplay acceptance are pending.

## Changed paths

- `src/npc/npc.cpp`
- `docs/specs/dev-loop/dev-loop.md`
- `docs/regressions/gameplay/README.md`
- `docs/regressions/gameplay/duel-npc-weapon-cycle-REG-2026-11-18.md`
- `docs/changelog/2026-11-18/20261118_064536-npc-loadout-weapon-cycle-rainbow-terminal-note.md`

