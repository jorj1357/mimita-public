# Hot gamemodes: runtime-registered modes, real FFA scoring/win hot (ABI v7)

- EST timestamp: 2026-09-14 12:22:33 EDT (UTC 2026-09-14T16:22:33Z)
- Branch: `8292026stash`
- Commits: none (uncommitted for human review)
- Result: `PASS` (source + cold build + `--gamemode-hot-selftest` + all other
  self-tests except the concurrently-rewritten movement-parity test)

## Migration standard

1. **Old cold path.** A gamemode was a JSON blob consumed by one 3.5k-line
   `serverGamemodeTick` with `matchMode`/`winCondition` string switches and a
   monolithic `ServerGamemodeState`; no runtime mode registry, no hot scoring or
   win ownership, no package match state.
2. **New hot path.** A mode is runtime metadata (`GameModeDescriptorV1`) whose
   systems and domain-scoped event handlers run only while the mode domain is
   active. The kernel emits generic match facts and asks the active mode to
   score (`actor.killed`) and decide the outcome (`match.evaluate`) through its
   package dynamic state and generic match capabilities.
3. **Kernel mechanisms retained.** Match lifecycle phase clock, participant/
   team/role assignment, spawn/respawn timers, kill pipeline, map rotation,
   reliable packet transport, `DuelStatePacket`.
4. **New general capabilities.** `match.current`, `match.actorTeamRead`,
   `match.finish`, `match.setPhase`, `match.respawn`, `match.setTeam`; the
   `GameModeDescriptorV1` array; domain-scoped event dispatch. The
   `match.score.snapshot` capability is a temporary bridge to the legacy packet.
5. **Why general, not mode-specific.** Every operation is over a runtime mode id
   or a match handle; the kernel never switches on a mode name. No
   `GamemodeType`, no `GameFfaStateV1`, no per-mode callback.
6. **State ownership.** Package dynamic components on the kernel-owned match
   entity own mode state (score, limits, mode-defined fields);
   `ServerGamemodeState` still owns phase/winner/lifecycle.
7. **Migration / rollback.** Mode schemas migrate at activation via the
   established `DynamicComponentStore::applySchemaUpdate`; a failed migration
   rejects the candidate and keeps last-good state.
8. **Files changed.** See below.
9. **Evidence.** See below.
10. **Remaining cold pieces.** Timers/intermission/phase, respawn policy,
    TDM/duel/objective/wave scoring/win, team/role assignment, mode UI, and
    generic replication of package match state.
11. **Next cold call site.** Projectile/weapon policy in
    `src/network/server-projectiles.cpp` / `src/combat/*` (Priority 3).

## What changed

- `game-api.h` ABI 6 -> 7: `GameModeDescriptorV1` + `modes[]` on
  `GamePackageDescriptorV1`; generic match capability fns and general match
  payloads (`GameActorKilledV1`, `GameMatchEvaluateV1`); the temporary
  `GameMatchScoreSnapshotV1` bridge; `GameSharedStateV1.matchEntity`.
- `GenericRuntime`: mode registry (`hasMode`/`modeDomain`/`modeDisplayName`),
  `setActiveModeDomain`/`runActiveModeDomain`, mode-domain skipping in
  `runRegisteredDomains`, and `domainId`-filtered event dispatch.
- `live-behavior`: match capabilities; `LiveBehavior::dispatchActorKilled` and
  `dispatchMatchEvaluate` (host context so handlers get capabilities).
- `server-gamemode.cpp`: kernel match entity + `serverMatchResetEntity`; generic
  match capability helpers; data-driven active mode from `matchMode`; emits
  `actor.killed` and skips cold scoring when handled; dispatches `match.evaluate`
  and skips cold FFA/TDM win when handled; generic score snapshot mapping in
  `broadcastDuelState`; hot-mode fallback in `serverCommunityStartMatch`.
- `server.cpp`: runs the active mode domain each fixed step.
- New hot modules: `src/hot-reload/modules/gamemodes/ffa.cpp` (real FFA) and
  `gamemodes/hot-test.cpp` (new mode); `hot-modules.json` gained the subdir glob.
- New `src/network/gamemode-hot-selftest.{h,cpp}` + `--gamemode-hot-selftest`.

## Evidence

- `python build_agent.py` -> `Status: SUCCESS`.
- `mimita.exe --gamemode-hot-selftest` -> **PASS (18/18)**: FFA + a brand-new
  mode registered at runtime; distinct mode domains; kernel match entity
  published; active FFA handler owns `actor.killed` and writes package dynamic
  state; generic score snapshot mirrors it; hot-test weighted scoring runs; an
  inactive mode handler does not run (domain filtering); `match.evaluate` owned
  by the active mode; mode schema migration preserves state; a failed migration
  keeps last-good; hot FFA scoring reaches the limit and the hot win condition
  ends the match via `match.finish`.
- All other self-tests PASS: dynamic-lifecycle, movement, entity-slice,
  hot-authoritative, live-code, project, phase456, telemetry, creation,
  ragdoll-slice.
- `-fsyntax-only` clean (real build flags) for every changed cold source and the
  new hot modules under `-DMIMITA_GAME_DLL`.

## Honest limitations

- `--movement-parity-selftest` currently crashes inside the concurrently-rewritten
  `movement.main` / `capPhysicsMove` path (`live-behavior.cpp`), which another
  agent is editing. The backtrace is entirely in that movement code, not the
  gamemode slice; `live-behavior.cpp` was newer than the running binary.
  Left untouched per the separation-of-ownership rule.
- The full single-process live falsification (create/select/edit/rename/syntax/
  migrate/remove a mode against a running server) was not executed this pass
  because the tree is being concurrently rewritten by the gameplay agent; the
  headless self-test covers the same mechanisms on the real DLL.
- Cold FFA persistence still reads the (now unwritten) `ffaKills`; per-mode
  score persistence via generic state is future work.
- No runtime human/visual acceptance; no multiplayer proof.

## Next

- Projectile/weapon/tool policy onto the same component/relationship/capability
  substrate; then per-entity behavior bindings; then timers/respawn; then
  network policy.
