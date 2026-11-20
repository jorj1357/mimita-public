# Hot-reload regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 2

---

| Recurring pattern | Evidence-backed response |
|---|---|
| “Cutover complete” is inferred from a successful build, link, packet path, or one swap. | Resolve every behavior owner, prove the same PID/session, and keep only named primitives cold. |
| New actors after a DLL generation change ignore active mode spawn rules. | Route joins through the active gamemode spawn owner; log startup fallback; validate a join and respawn after same-PID activation. |

## Entry 1 — Cutover-1 treated as complete with networking policy still resident

- Recorded: 2026-11-17
- Status: CONFIRMED — specifications corrected; network source split and live proof pending
- Summary: Packet handlers and ordinary swaps were hot, but `server-ice.cpp`, `server.cpp` session policy, and `ice-agent.cpp` policy were still resident. ICE negotiation/listener bugs and some join/reconnect roots therefore require an EXE restart.
- Detail: [cutover-claimed-complete-REG-2026-11-17.md](cutover-claimed-complete-REG-2026-11-17.md)

## Entry 2 — New joins bypass active mode spawns after DLL reload

- Recorded: 2026-11-19 (user report; exact report time unavailable)
- Status: SOURCE FIX SAVED — build, runtime, and human validation pending
- Summary: New player join handlers used the generic resident world spawn picker while a mode was active, bypassing mode/team spawn rules.
- Detail: [mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md](mode-spawn-bypassed-after-dll-reload-REG-2026-11-19.md)
