# 2026-10-16 weapon identity and spawn handoff specifications

- Canonical time: 2026-10-16T22:48:57Z
- Display time: 2026-10-16 18:48:57 EDT (America/New_York)
- Branch: `20261011zt`
- HEAD at start/end: `3a34520009d91d10a571dfd08ca3ee90c9c56639`
- Result: `PASS_WITH_HUMAN_REVIEW`

## Request

Clarify whether priorities 1 and 2 in the open gameplay issue index are ready
for an implementation handoff; explain how weapon hotbar/native slots and IDs
currently work; specify the desired stable-identity and data-driven
component-based item/tool architecture; make the two issue entries detailed
enough for an agent to trace and repair. Scope was specification and evidence
review only.

## Existing workspace state

`docs/specs/weapons/weapons.md`, `docs/specs/networking/networking.md`, and
`docs/specs/gamemodes/gamemodes.md` already contained unrelated pre-existing
modified content. `docs/specs/open-gameplay-issues-2026-10-16.md` is an existing
untracked file. All prior content was preserved. The router points to
`docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md`; neither
exists in this checkout. No source or configuration files were changed.

## Changes

- `docs/specs/weapons/weapons.md:194` — added the target architecture for
  data-defined item/tool definitions with reusable behavior components,
  distinct definition/instance/hotbar identities, server-owned equip identity,
  staged migration, and six acceptance cases. Before: “The weapon definition
  selects which systems are used and supplies their configuration.” After:
  the added contract says adding content must not require a per-weapon
  `create...Definition()` function or packet, and a hotbar index is never
  weapon identity. Updated the conceptual `WeaponDefinition::id` comment at
  line 263 from `// 1,2,3,4,5,6..` to `// stable identity, e.g. "revolver";
  never a hotbar slot`.
- `docs/specs/weapons/weapons.md:265` — added a current implementation audit.
  Before: the spec's opening note already described JSON overlaying
  C++-created definitions, but did not describe the present equip mapping and
  attack validation together. After: it names
  `registerBuiltinWeapons()`, `registerWeaponFromJson()`, `weaponsets.json`
  array order, per-definition native `slot`, client/server translation owners,
  and the fact that AttackRequest already carries `weaponDefNetworkId` while
  server authorization still compares equipped native slot to definition
  native slot.
- `docs/specs/networking/networking.md:464` — added the authoritative equip
  and attack identity contract: validate definition/instance identity at
  server-validated tick and spawn generation; slots remain UI mappings; equip
  revision, respawn publication, and bounded diagnostics are required. Before:
  the earlier respawn addition mentioned logical/native slot agreement but did
  not establish the desired replacement identity model. After: stable item
  identity is authoritative and protocol changes retain one deliberate
  compatibility boundary.
- `docs/specs/gamemodes/gamemodes.md:126` — strengthened the map/spawn contract
  with canonical map/content identity, load readiness before join/spawn,
  visible failure if no safe spawn exists, and linked diagnostics. Before:
  the runtime evidence section ended with acceptance covering selected map,
  chosen spawnpoint, actor generation, and transform. After: server/client map
  agreement and load readiness are prerequisites, and fallback to stale-world
  coordinates is explicitly prohibited.
- `docs/specs/open-gameplay-issues-2026-10-16.md:42` — added a handoff section
  for priorities 1 and 2. Before: these were summaries with journal evidence
  and owner links. After: each has known evidence, first trace, desired repair,
  real-runtime acceptance scenario, and explicit handoff readiness. Spawn is
  ready for investigation, not a guessed one-line fix; weapon respawn/equip is
  ready for trace and repair, while component migration is staged separately.

## Source and runtime evidence reviewed

Current source/config confirms a mixed slot and identity model:

- `config/weaponsets.json`: a set's ordered `weapons` list maps positions to
  weapon IDs (logical hotbar positions).
- `config/weapons.json`: each definition separately has a native `slot`; the
  Rocket Launcher is native slot 7 in this checkout.
- `src/combat/weapon-data.cpp::registerBuiltinWeapons()` invokes individual
  constructors. `src/combat/weapon-json-config.cpp::registerWeaponFromJson()`
  applies JSON values to an existing definition and registers it; arbitrary
  JSON-only weapon creation is not yet the current path.
- `src/terminal/weapon-commands.cpp::nativeSlotForCommunitySlot()` and server
  helpers translate logical position -> weapon ID -> native slot.
- `src/network/server-attack.cpp` resolves `weaponDefNetworkId`, then compares
  server equipped native slot with the definition's native slot. Thus the
  attack packet already identifies the weapon by network ID, while equip state
  still relies on native slot representation.
- Journal `logs/2026-10-16/20261016_134002/events.jsonl` previously inspected:
  26 `weapon_not_equipped` rejections (21 Force Punch, 5 Rocket Launcher),
  rocket requests showed client logical slot 3 / definition native slot 7 /
  server equipped slot 0, and a later accepted equip resolved 3 to 7. The
  rejection event did not carry actor spawn generation or equipped definition
  ID, so it does not establish which respawn transition failed.
- The same journal records different map loads in server PID 22216 and client
  PID 32808, repeated failure to load a non-v4 map asset, later map loads by
  PIDs 32276 and 32808, and an actor/corpse-presentation position around
  Z=-501. It supports a map/session mismatch and out-of-bounds position but
  does not identify the exact spawn writer or PID 32276 launch owner.

These two issues are ready to give an agent as trace-first investigation and
repair tasks with specified acceptance. The existing evidence alone is not
sufficient to safely prescribe a particular code edit. The weapon architecture
rewrite should not be folded into the immediate slot/lifecycle repair unless
tracing proves a safe narrow repair is impossible.

## Review and validation

Read `AGENTS.md`, `docs/ROUTER.md`,
`docs/skills/spec-behavior-review-v1.md`,
`docs/skills/documentation-checker-v1.md`,
`docs/architecture/time-and-formatting/time-and-formatting.md`,
`docs/workflows/runtime-scenario-validation.md`, and
`docs/operations/task-completion/task-completion.md`. The router-referenced
regression and doc-review files are absent. `git diff --check` on the changed
files found one pre-existing trailing-whitespace line in
`docs/specs/networking/networking.md:3405`; it was not edited. No build, tests,
new runtime scenario, or live game action was run because this work only
changed specifications.

Human/runtime acceptance remains required. The next agent must use the
canonical dev-loop and fresh `EVENTS_JSONL_PATH`, add or improve bounded
owner-level diagnostics where needed, reproduce the two scenarios, identify
first divergence and exact root cause, then repair and rerun. Keep source,
build, runtime, and visual acceptance claims separate.
