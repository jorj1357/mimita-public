// 2026-09-09T11:27:05Z
/* purpose
* record the spawn-velocity network integration fix and build evidence
* record the regression-process lesson about tracing linkage before blaming stale binaries
* preserve the exact validation and remaining human acceptance boundary
* does NOT claim live gameplay acceptance without a respawn trial
* does NOT overwrite unrelated pre-existing worktree edits
* does NOT treat a successful link as proof of visual movement behavior
*/

# Spawn velocity network linkage

## Session

- UTC timestamp: `2026-09-09T11:27:05Z`
- Local display time: `2026-09-09 07:27:05 America/New_York`
- Branch: `8292026stash`
- Commits: no commit created
- Pre-existing changes: the worktree already contained many unrelated modified and untracked files, including existing spawn-velocity files, `src/network/server-players.cpp`, and the prior spawn-velocity regression/build note. Those changes were preserved. This session changed only the files listed below.

## Request and finding

The request was to make `config/spawnvelocity.json` reliable on every respawn, record two recurring diagnosis lessons in regressions, and build until a new executable linked.

The exact failure was an incomplete network integration. `src/network/server-players.cpp:581-584` computed the configured spawn velocity and passed it to `beginAuthoritativeTransform`, but `PlayerRespawnedPacket` did not carry velocity. The client then reset velocity to zero in `src/network/multiplayer-tick.cpp:629` and `src/engine/engine-tick-net.cpp:366`. The reliable respawn event transport therefore reliably delivered a packet that omitted the value; this was not an unreliable-delivery problem.

The movement specification states: `base velocity = spawn velocity`, normally zero, while external impulse is zero. The correction carries the authoritative spawn velocity through the respawn boundary and applies it as the client's initial local/server velocity.

## Exact changes

### `src/network/packets.h`

- Old: `PlayerRespawnedPacket` ended with `posX`, `posY`, `posZ`, then `health`; its size assertion was `sizeof(PlayerRespawnedPacket) <= 576`.
- New: added `float velX`, `float velY`, and `float velZ` immediately after position, and changed the size assertion to `<= 584` because the packet is now 584 bytes.

### `src/network/server-players.cpp`

- Old: `completeAuthoritativeSpawn()` populated only `spawnSync.posX/Y/Z` before health and logged only position.
- New: it populates `spawnSync.velX/Y/Z` from `player.vel` and includes the authoritative velocity in the `DuelPacketSend` diagnostic.

### `src/network/multiplayer-tick.cpp`

- Old: `applyAuthoritativeSpawn()` assigned `ctx.localServerVelocity = glm::vec3(0.0f)`.
- New: it assigns `{spawn->velX, spawn->velY, spawn->velZ}` and the respawn receive diagnostic includes the packet velocity.

### `src/engine/engine-tick-net.cpp`

- Old: pending authoritative spawn application assigned `player.vel = glm::vec3(0.0f)`.
- New: it assigns `{spawn.velX, spawn.velY, spawn.velZ}` while continuing to clear only the separate external impulse.

### `src/network/multiplayer-packets.cpp`

- Old: an existing connection-notification ternary passed a possible `std::string` directly to `pushConnectionNotification(..., const char*)`, causing compilation to fail.
- New: the expression calls `.c_str()` on the complete selected string. This was required to obtain a successful build and is unrelated to spawn-velocity behavior.

### `docs/regressions/regressions-v1.md`

- Added an append-only process regression at line 40 recording that stale EXE is a hypothesis, not the default diagnosis, and that the more common failure is incomplete end-to-end linkage across related owners/consumers.
- It records the requested human experience: stale-binary explanations were rare compared with cases where code was not linked through all required paths.

## Skills and documents used

- `docs/ROUTER.md`
- `docs/regressions/regressions-v1.md`
- `docs/specs/networking/networking.md`
- `docs/specs/movement/movement.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/operations/build-and-exe/build-and-exe.md`
- `docs/operations/task-completion/task-completion.md`

Spec-behavior review result: `PASS_WITH_HUMAN_REVIEW`. The source now agrees with the spawn-state contract at the packet/application boundary. Live repeated respawn acceptance remains required.

## Validation

1. `git diff --check` ran. It reported only pre-existing trailing whitespace in unrelated modified documentation; no new whitespace error was introduced by the focused changes.
2. First build attempt failed at the packet-size static assertion (`584 <= 576`) and then exposed the pre-existing `std::string` to `const char*` error in `src/network/multiplayer-packets.cpp:1293-1296`.
3. After correcting those compile/link integration issues, `python build_agent.py` reported `Status: SUCCESS` at `2026-09-09 07:26:28`.
4. Because that successful run initially reported `Nothing changed` and the executable timestamp was older than the edits, the canonical `mimita.exe` was removed and `python build_agent.py` was run again.
5. The forced relink reported `BUILD SUCCESS`, `[LINK] mimita.exe`, `Status: SUCCESS`, and `Return Code: 0` at `2026-09-09 07:26:58`.
6. Expected output exists: `C:\mimita-priv-v8\mimita.exe`, size 104,752,287 bytes, timestamp `2026-09-09 07:26:58`.

## Remaining human review

Run a real client/server respawn trial with `config/spawnvelocity.json` enabled and verify every respawn. Confirm matching server `[DuelPacketSend] ... velocity=(...)`, client `[DuelPacketRecv] ... velocity=(...)`, and visible movement on every life. This build proves compilation and linking, not live gameplay acceptance under packet loss or all alternate duel/map spawn paths.
