# Hot-reload Cutover-1 checklist (2026-11-14)

Temporary coordination scratchpad. This is not a specification. The current
specification is `docs/specs/hotreload/hotreload.md`.

## Gate list

1. **Hot client accepted.** Non-black, stable, same-PID swap.
2. **Server hot accepted.** Join/chat/lag-comp through the DLL.
3. **State migration proven.** Add/remove fields survives a swap.
4. **Safe unload proven.** Repeated swaps without leaks.
5. **EXE cutover.** The EXE links only bootstrap/host/loader/barrier/renderer/
   logging/`glad.c`.
6. **Link gate.** Fail the build if a `game`-owned source is in the EXE link.
7. **Delete `MIMITA_GAME_DISABLE` and the legacy files; document the deletion.**
8. **Same-PID proof.** Edit -> build -> swap with state preserved.

## Source-checked blockers

| Blocker | Code |
| --- | --- |
| Hot client is opt-in | `src/main.cpp:528` `const bool hotClient = std::getenv("MIMITA_GAME_HOT_CLIENT") != nullptr;` |
| Resident client fallback | `src/main.cpp:558` `_putenv_s("MIMITA_GAME_DISABLE", "1");` |
| Hot server still has a resident `else` | `src/network/server.cpp:1575` `const bool hotServer = ...serverApiAvailable();` |
| Hot server gated by the probe flag | `src/host/host-runtime.cpp:2558` `return probeEnabled_ && ...serverApi != nullptr;` |
| `MIMITA_GAME_DISABLE` turns the host off | `src/host/host-runtime.cpp:2187` |
| EXE links game sources | `config/source-owners.json:108` `"owner": "game", "include": ["src/**/*.cpp"]` |
| Legacy renderer | `src/render/render-command-legacy.cpp:13`, `src/host/game-gl-legacy.cpp:9` |
| State fails closed on size change | `src/host/host-runtime.cpp:2067` `if (record.size != size || record.alignment != alignment) return nullptr;` |

## Refreshed status (2026-11-16)

- Gate 3, state migration: passed. Live same-PID `player_core` v1→v3 and
  v2→v3 commits, plus poisoned-v2 rejection with old-state retention, are in
  run `20261116_180054`, client PID `25896`.
- Gates 5-6, EXE cutover/link gate: current generated reports show
  `exe=46`, `cold=45`, `game_in_exe=0`, `cold_undefined=0`, and
  `linked_game_sources=0`.
- Gate 1 still needs a fresh connected hot-client gate run. The migration run
  disconnected after repeated swaps, so it is not final gameplay acceptance.
- Gate 4 safe unload needs regeneration against the current generation set.
- Gates 7-8 remain open.

The blocker table below is historical code-location context. Re-check current
code before using it.

## Suggested order

1. B checkpoints the packet split and resident pump.
2. A updates tooling/ABI; C starts on disjoint client networking files.
3. Fix the hot client presentation -> gate 1.
4. Prove server join/chat/lag-comp -> gate 2; delete the server fallback.
5. State migration -> gate 3.
6. Safe unload -> gate 4.
7. EXE cutover + link gate -> gates 5-6.
8. Delete fallback/legacy; document -> gate 7.
9. Same-PID proof -> gate 8.

## Ownership note

The current active focus is in `docs/handoffs/hot-reload-agent-focus.md`.
