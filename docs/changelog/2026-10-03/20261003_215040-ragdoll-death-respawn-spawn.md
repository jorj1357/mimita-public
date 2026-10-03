# Ragdoll death no longer respawns at the death spot

Date: 2026-10-03
EST timestamp: 2026-10-03 17:50:40 ET
UTC timestamp: 2026-10-03T21:50:40Z
Branch: `8ac195e7`

## Result

`PASS_WITH_HUMAN_REVIEW`

Source and build evidence are proven (3 translation units recompiled, `mimita.exe`
relinked, `BUILD SUCCESS`). No live gameplay acceptance was performed. The
human must still die while holding `G` (live ragdoll) and confirm the next life
spawns at an authoritative spawn point, not the death spot.

## Scope

Human report: "when I die in ragdoll mode, I respawn just like normal, I don't
just respawn in the spot I was at when I died while in ragdoll mode."

This is the single changelog for this session.

## Specification

`docs/specs/movement/movement.md` section 15 (Death, respawn, teleport, and
reconnect):

> Respawn creates a clean movement state:
> position = authoritative spawn position

The live ragdoll re-asserted its pre-death simulated pose over that authoritative
spawn position, so the implementation disagreed with the spec.

## Root cause

Live ragdoll mode (`G`) makes `RagdollModeSystem::syncToPlayer` write
`player.pos`/`player.vel` from the simulated torso every tick
(`src/ragdoll/ragdoll-mode.cpp:1593`). `DeathSystem::kill` marked the actor dead
but never ended ragdoll mode, so on the next non-dead tick
(`src/sim/simulate-tick.cpp` ragdoll branch) the still-active body wrote the old
death-location pose back over the freshly applied spawn transform. Respawn ended
up at the death spot.

`DeathSystem::respawn` already picked the correct authoritative spawn
(`src/combat/death-system.cpp:251`); the ragdoll body overwrote it afterward.

## Files changed

### `RagdollModeSystem::deactivate` gains a transform-preserving mode

`src/ragdoll/ragdoll-mode.h`, `src/ragdoll/ragdoll-mode.cpp`:

```cpp
// old
void deactivate(Player& player);
// new
void deactivate(Player& player, bool movePlayerToBody = true);
```

`movePlayerToBody == true` keeps the existing toggle-off behavior (return the
authoritative root to the body's last pose). `false` tears the body down without
writing `player.pos`/`player.vel`, for lifecycle resets where the authoritative
transform must stand.

### Death ends live ragdoll

`src/combat/death-system.cpp` (`DeathSystem::kill`, after the momentum capture):

```cpp
if (&victim == gpPlayer && RagdollModeSystem::instance().isActive()) {
    RagdollModeSystem::instance().deactivate(victim, /*movePlayerToBody=*/false);
    victim.ragdollModeActive = false;
}
```

This is the local death owner for every local player death (HP, void, weapon,
terminal). It runs before the same-tick instant-respawn path, so jump-to-respawn
is covered too. The guard excludes NPC deaths and remote-player deaths so only
the local player's own ragdoll is affected.

### Authoritative lifecycle reset also ends live ragdoll

`src/sim/simulate-tick.cpp` (the existing lifecycle-identity block):

```cpp
const bool lifecycleChanged = lifeId != 0 && lifeId != s_aimLifecycleId;
if (lifecycleChanged && ragdoll.isActive()) {
    ragdoll.deactivate(*sim.player, /*movePlayerToBody=*/false);
    sim.player->ragdollModeActive = false;
}
```

A live ragdoll must not survive a respawn, teleport, map change, or reconnect
either; the same writeback would drag the player to the pre-discontinuity pose.
The aim-body rebind (`rebindAimToAuthoritativePlayer`) already used this same
lifecycle identity and is unchanged.

## Validation

Deleted the three stale objects (a background live-build watcher had already
recompiled them, so the first `build_agent.py` reported `NOTHING_CHANGED` and did
not relink) and rebuilt:

```text
[CXX ] src\combat\death-system.cpp
[CXX ] src\ragdoll\ragdoll-mode.cpp
[CXX ] src\sim\simulate-tick.cpp
[LINK] mimita.exe
 BUILD SUCCESS
Compiled: 3
Skipped : 514
```

Header-only `src/ragdoll/ragdoll-mode.h` change is source-compatible (added
default parameter); affected includers were already newer than the header.

## Evidence separation

- Source: this file and the diff (`git diff --stat`: 4 files, +28/-5).
- Build: `BUILD SUCCESS`; `mimita.exe` relinked 2026-10-03 17:50 local.
- Runtime: NOT performed.
- Human acceptance: pending (die in ragdoll, confirm next spawn is at a spawn
  point and normal movement resumes).

## Documents read

- `AGENTS.md`, `docs/ROUTER.md`.
- `docs/specs/movement/movement.md` section 15 (death/respawn contract).
- `docs/specs/ragdoll-retrograd/ragdoll-retrograd.md` section 4 (player root /
  respawn authority).
- `docs/operations/build-and-exe/build-and-exe.md`.
- `docs/architecture/live-development/live-development.md`.
- `docs/regressions/README.md` (2026-10-01 policy: no new cold-build occurrence
  required under the current cold-build-allowed policy; no behavior regression
  file — the reported behavior had not previously worked).

## Known limitations / follow-ups

- Multiplayer local death goes through `DeathSystem::kill` via
  `DeathSystem::update`, so it is covered by the same fix; a live two-client
  ragdoll-death test was not run.
- No commit was made; nothing was pushed or deployed.
