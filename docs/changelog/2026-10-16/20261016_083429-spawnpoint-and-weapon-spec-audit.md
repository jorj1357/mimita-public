# Spawn-point and weapon specification audit

Time created: 2026-10-16T08:34:29Z
Time last updated: 2026-10-16T08:34:29Z

## Scope

Investigated authored spawn-point consumption and the existing Godball,
SwordSword, Spy Knife, and HAFS specifications. Updated specification and
issue-index documentation only. No source code changed. This follows
`docs/skills/spec-behavior-review-v1.md` and the routed
`docs/workflows/runtime-scenario-validation.md` evidence boundary.

## Findings

- Spawnpoint authoring lives in the Entity Editor and `MapConfigRegistry`.
  `gamemodeSpawnPoint()` already consumes authored points in some server
  gamemode spawn paths. Initial player join still selects GLB spawn nodes;
  `npc_spawn` currently chooses a local/GLB point and includes that position in
  its request. The ownership/design gap is inconsistent consumption, not zero
  consumers.
- Proposed spawn contract: one server-owned selector for initial join,
  respawn/map changes, team/role placement, and NPC spawn requests/waves. Keep
  existing lifecycle precedence, prefer eligible enabled authored points,
  safely fall back to GLB points, and retain the documented safe fallback.
  Remote editor changes must be accepted by the server; local JSON save alone
  cannot update another machine.
- Godball already has a detailed definition in `docs/specs/weapons/weapons.md`
  §8: persistent sphere, continuous sweep, non-blocking world response, and
  player overlap damage/force. SwordSword is §9 there and has
  `docs/specs/weapons/swordsword.md` for open user-reported issues. Spy Knife
  behavior/networking is in `docs/specs/weapons/melee-weapons.md` §§4–9.
- Source review found Spy Knife remote candidate selection excludes players
  when an NPC replica map is populated, and predicted damage is always routed
  to the NPC prediction store. Server historical collision uses a root sphere
  while the client uses replicated body parts; that may disagree, but runtime
  cause is unproven. The handler also does not select its entity namespace from
  the packet target-kind bit; ID namespace overlap needs verification.
- HAFS has no focused spec and was incorrectly easy to conflate with
  Godball/SwordSword. A draft audit records that HAFS local slash hit code is
  separate from server contact, and the input trace indicates neither slash
  nor lunge is sent through the currently observed generic server attack path:
  slash returns no fired marker; the lunge sender only sends for SwordSword.
  Confirm with live request/timeline logs before treating this as reproduced.

## Documentation changes

- `docs/specs/entity-editor/entity-editor.md`: correct the claim that authored
  spawnpoints have no gameplay consumer; document existing consumers, missing
  paths, design, remote-edit requirement, and acceptance scenarios.
- `docs/specs/gamemodes/gamemodes.md`: add the proposed authoritative shared
  spawn-selection contract.
- `docs/specs/open-gameplay-issues-2026-10-16.md`: correct the spawnpoint
  status and link its actual design owners.
- `docs/specs/weapons/melee-weapons.md`: record code-level Spy Knife findings,
  candidate fix sequence, and real-server acceptance cases without claiming a
  runtime root cause.
- `docs/specs/weapons/weapons.md`: add a weapon document map; preserve existing
  Godball and SwordSword rules.
- `docs/specs/weapons/hafs.md`: add a draft source audit and requirements
  decision list for user review before implementation.

## Validation and limitations

- `git diff --check` was run. It reported existing trailing whitespace in
  `config/behavior-profiles.json` and `docs/specs/networking/networking.md`;
  neither file was edited in this task. No whitespace error was reported for
  the edited/new documentation.
- Source paths and related documents were inspected. The router-referenced
  `docs/doc-review-09-03-2026.md` is absent in this checkout; regression index
  exists at `docs/regressions/README.md` (the router's `regressions-v1.md` path
  is absent except under `archive-ignore`).
- No build, runtime scenario, server session, or live journal was run because
  this task stops at spec review pending the user's design approval. Therefore
  no gameplay fix is claimed.
- Spawn precedence, behavior of checkpoints/teleports, editor server
  authorization, HAFS move/target/damage policy, and final melee historical
  body representation need human review before code implementation.
