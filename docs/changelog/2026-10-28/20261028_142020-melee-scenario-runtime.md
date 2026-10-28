# Melee scenario runtime investigation

- Time: 2026-10-28 14:20:20 EDT (2026-10-28T18:20:20Z)
- Branch: `20261011zt`
- HEAD: `933cfb48`
- Result: `PASS_WITH_HUMAN_REVIEW` — scenario executed on the matching map/set; action rejection and collision evidence recorded, while hit damage remains unverified.

## Runtime run

Reused the existing dev-loop, which was running build 3644 and had no pending
build. Its original run was on a different map and weapon set, so a temporary
dev-loop scenario descriptor requested the recording's map and weapon set.
The loop launched the matching run and replayed
`devscripts/scenarios/user/scenario-20261028_181131-meleeissue.jsonl` at 60 Hz.
The temporary descriptor
`devscripts/scenarios/user/scenario-run-meleeissue.json` was removed after the
run.

- Build: 3644, current published build; no build was needed or run.
- Mode/map/weapon set: Sandbox / `tdmtest3` / 29.
- Run ID: `20261028_181600`.
- Canonical journal: `logs/2026-10-28/20261028_181600/events.jsonl`.
- Dev-loop runner returned exit code 0 and reported 4 assertions passed. These
  assertions check playback lifecycle events, not whether combat succeeded.

## Evidence and limits

- `scenario.playback.start_ready` reports `start_restored=false` and
  `server_position_ready=false` after its 5-second wait. The first position
  divergence is already 15.9847 m at frame 0; playback later records 40 snaps.
  The run therefore did not faithfully reproduce the recorded trajectory.
- Client logs 55 `melee.client.action` starts. Server logs 55
  `weapon.attack.result` rejections, all `accepted=false`, `reason=8`, with
  matching request/server spawn generations. `src/network/server-attack.cpp`
  rejects attacks with reason 8 when `spawnState != Active` and also when the
  spawn generation is stale. Since generations match and the server's
  `server.actor.spawn_published` event reports `spawn_state=1`, which
  `src/network/server.h` defines as `AwaitingSpawnAck`, the evidence points to
  the server spawn-state gate rejecting these attacks before authoritative
  melee action acceptance. No server melee action start was found.
- One server actor spawn is present and no target appears in the run. There are
  no authoritative contact or damage outcome events, so this run cannot
  establish whether SwordSword or mace physically contacted a target, passed
  through a target, or disagreed with the server on damage. Weapon-versus-weapon
  ordering was also not exercised.
- Collision logs contain 407 `collision.bounce.applied` records for weapon
  parts contacting world geometry and 73 `collision.last_valid.rollback`
  events. One early rollback at tick 8 lists weapon contacts against static
  world triangle 1003 with penetrations around 1 m. This supports map collision
  interaction as a corner-snagging lead; mapping those triangle IDs to visible
  corners and judging the response still needs human review.
- `scenario.playback.completed` says `completed=true`, but also reports the
  frame-0 divergence and 40 snaps. It means input playback ended, not that the
  gameplay behavior passed.

## Issue tracker and review

Updated the existing Sandbox melee issue in
`docs/specs/open-gameplay-issues.md` (near line 1685) with the run identity,
playback fidelity failure, client/server attack-start disagreement, collision
evidence, and the lack of targets/contact/damage/weapon-pair evidence. The
overall damage/contact cause remains unconfirmed.

Read `docs/ROUTER.md`, `docs/specs/dev-loop/scenario-input-recording.md`,
`docs/workflows/runtime-scenario-validation.md`,
`docs/operations/build-and-exe/build-and-exe.md`,
`docs/specs/weapons/melee/melee-weapons.md`, and focused sources
`src/network/server-attack.cpp`, `src/network/server.h`, and
`src/network/server-packets.cpp`.

`git diff --check` is the documentation validation. No gameplay source or
configuration changed. Human visual review was not performed. A conclusive
damage run still needs a target and a faithful playback start; the attack
rejections and the position-restore failure should be resolved or accounted
for before treating this replay as a valid damage test.

## Pre-existing workspace changes

Preserved all existing edits, including earlier tracker/changelog entries;
modified `config/accounts/default.json`, `config/analytics.json`,
`config/audio/music-settings.json`, `config/collision.json`,
`config/weaponcollisions.json`, `config/weapons/mace/config.json`; deletion of
`docs/specs/open-gameplay-issues-human.md`; and untracked assets under
`assets/objects/things/effects-objects/` and
`assets/objects/weapons/weapon-objects/`, plus
`assets/textureshq/explosionmarkv1.png` and
`assets/textureshq/rocketsmokev1.png`.
