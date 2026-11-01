# Open gameplay issue tracker cleanup

Time created: 2026-11-01T19:15:30Z  
Display time: 2026-11-01 14:15:30 EST  
Time last updated: 2026-11-01T19:15:30Z  
Status: PASS — documentation cleanup complete; gameplay behavior remains unverified  
Branch: 20261011zt  
Base commit: f02843f6  
Session commits: none

## Goal

Reorganize the open gameplay issue file after user triage, distinguish current
work from verification and archive, and align active reports with their owner
specifications. The user also requested that NPC navigation, collision bounce,
render presence, bomb-site visibility, JSON validation, and event-driven
scenario completion be captured in their owner specs where intent was clear.

## Changes

### docs/specs/open-gameplay-issues.md

Replaced the chronological inbox and duplicate issue index with a 291-line
Markdown tracker. It has 19 stable OGI rows with priority, status, owner/spec
relation, and next action; detailed active issue notes; deferred owner-spec links;
a concise archive; and a reopen rule.

The original raw 2026-11-01 NPC report said “npc just goes down sinking into
the ground” and described corner trapping and entry into the CT spawn block.
It is now represented under OGI-003 with distinct actor-pair, world-containment,
navigation, and recovery checks. The user’s report that an NPC mesh disappears
while still damaging the player and that a Normal Zombie appears at the player’s
Fun Counter Strike spawn are separate OGI-005 and OGI-006.

The tracker records the user’s current decisions:
- Zombie Tower is the P0 complete-playable-mode goal; its current v1 spec
  requires at least 50 floors and a newer note asks whether the target should
  be 100, so this tracker leaves the count open.
- Multi-floor navigation is a shared NPC capability with Hospital and Zombie
  Tower acceptance cases; Griever work is grouped under Zombie Tower.
- Contact bounce, editor crash/dragging, bomb-site visibility, bomb-interaction
  acceptance, automatic recording, JSON validation, and dev-loop tooling remain
  open.
- NPC combat and authored spawn points are needs-recheck items; VS Jason is
  verify-only; melee, Godball/HAFS, and slot UX are deferred.
- The user-reported 240 FPS target is archived: it was not measured as achieved,
  while the user says the game felt fine. The user-selected old performance,
  visual, camera, position-sync, shot, ragdoll, and mode-start reports are
  archived without claiming they were fixed.

### Owner specifications

- docs/specs/movement/movement.md:1450 adds “Consistent bounce on physical
  impacts.” The prior contract used configured contact response and smoothed
  edge features but did not define a reliable bounce across the contact classes
  named by the user. The new text covers weapon/world, actor/world, actor-pair,
  turning/steering, and edge/corner impacts. Response magnitude and resting
  contact remain open tuning decisions.
- docs/architecture/player-npc-systems/npc-movement.md:3051 replaces the
  2026-10-16 Hospital note with “User-reported multi-floor pursuit and advanced
  traversal.” It records shared route planning, legal role-available jumps,
  dashes and rocket jumps, unreachable-goal reporting, and Hospital/Zombie
  Tower/corner acceptance scenarios. It leaves the 50-versus-100 floor choice
  to the Zombie Tower owner spec.
- docs/specs/npc-behavior/npc-behavior.md:626 adds “Live NPC render presence.”
  Active living NPC combatants must not silently lose their model while still
  acting; normal camera/world occlusion remains valid.
- docs/specs/entity-editor/entity-editor.md:765 adds the requirement that bomb
  sites display as labeled circles when Entity Visibility and the map-object
  category are enabled. At line 373, repaired the old link to the removed
  authored-surface intake heading so it points to the deferred-design section.
  The spec already says authored blocks become render and collision triangles
  on client and server.
- docs/architecture/json-configuration/json-configuration.md:428 adds a
  repository-wide field/reference inventory target, including detection of
  removed preset IDs and non-mutating validation aligned with runtime loaders.
- docs/specs/dev-loop/quality-gates.md:253 adds the event-driven scenario
  completion target with explicit success/failure evidence and a bounded timeout
  that is not treated as success.

## Reasoning and specification alignment

- The actor-pair no-pinning rule already exists in NPC behavior; the recent
  sinking report is an unaccepted runtime failure against that rule.
- Authored-block collision is already specified in Entity Editor. The report
  that an NPC enters a closed authored block is recorded as a spec violation.
- Draggable handles are already specified; the user report remains an
  unverified editor regression.
- Bomb-site visibility was absent from the visibility-category contract and was
  added as a default behavior.
- Multi-floor navigation and traversal abilities are already broadly specified;
  the Hospital report and desired Zombie Tower acceptance are now explicit.
- The Zombie Tower owner spec was already modified before this task and was not
  edited. Its current 50-floor minimum and newer question about 100 floors were
  inspected and reflected accurately.

## Validation

- git diff --check on all changed documentation files: PASS.
- Local Markdown file-link and heading-anchor scan on all changed documentation
  files: 0 broken links.
- Tracker inventory: 19 OGI rows; 291 lines.
- Documentation TODO-pattern scan found no actionable “todo: explain/clean/add
  an explanation” items; matches were router/skill instructions and historical
  changelog references.
- No build, code test, gameplay runtime, or human visual acceptance was run;
  this was a documentation-only task.

## Skills and documents

Used:
- docs/skills/documentation-checker-v1.md
- docs/skills/spec-behavior-review-v1.md

Also consulted AGENTS.md, docs/ROUTER.md, the NPC behavior and movement
specifications, collision architecture, movement, Entity Editor, Fun Counter
Strike, Zombie Tower, scenario recording, runtime validation, dev-loop quality
gates, JSON configuration, and task-completion guidance.

## Pre-existing and concurrent worktree changes

At task start, the worktree already contained edits in:
config/accounts/default.json, config/actor-collision.json, config/analytics.json,
config/audio/music-settings.json, config/hitfx.json,
config/maps/dust2cyberiav4.json, config/networkingconfig.json,
config/weapons/chainsaw/config.json, devscripts/blender-ai,
docs/specs/effects/effects.md, docs/specs/gamemodes/zombie-tower/zombie-tower.md,
docs/specs/gui/guiv2.md, docs/specs/networking/contact-authority.md,
docs/specs/weapons/weapons.md, src/combat/weapon-fire-damage.cpp,
src/config/networking-config.h, src/effects/effect-part-particles.cpp,
src/effects/effect-part-render.cpp, src/effects/hit-effects-config.cpp,
src/effects/hit-effects.h, src/engine/engine-tick-net.cpp,
src/input/input-poll.cpp, src/input/input-poll.h,
src/network/multiplayer-context.h, src/network/multiplayer-projectiles.cpp,
src/network/multiplayer-shots.cpp, src/network/server-attack.cpp,
src/network/server-npcs.cpp, src/npc/npc-combat.cpp, src/npc/npc.cpp,
src/npc/npc.h, src/physics/movement/actor-final-pose.cpp,
src/physics/movement/actor-pair-collision.cpp,
src/physics/movement/actor-pair-collision.h, src/sim/simulate-tick.cpp,
src/terminal/player-commands.cpp, and src/terminal/weapon-commands.cpp.
Pre-existing untracked items included
docs/changelog/2026-11-01/20261101_184453-shotgun-aggregate-prediction-and-damage-scale.md,
docs/specs/image-7.png, src/actor/player-ai.cpp, and src/actor/player-ai.h.

Later status checks also showed edits in devscripts/dev-loop.py,
src/debug/structured-log.cpp, src/debug/structured-log.h, and
src/devtools/dev-log-commands.cpp. These were not changed or claimed by this
session. No pre-existing edits were reverted.

