# Hot-reload regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 1

---

| Recurring pattern | Evidence-backed response |
|---|---|
| “Cutover complete” is inferred from a successful build, link, packet path, or one swap. | Resolve every behavior owner, prove the same PID/session, and keep only named primitives cold. |

## Entry 1 — Cutover-1 treated as complete with networking policy still resident

- Recorded: 2026-11-17
- Status: CONFIRMED — specifications corrected; network source split and live proof pending
- Summary: Packet handlers and ordinary swaps were hot, but `server-ice.cpp`, `server.cpp` session policy, and `ice-agent.cpp` policy were still resident. ICE negotiation/listener bugs and some join/reconnect roots therefore require an EXE restart.
- Detail: [cutover-claimed-complete-REG-2026-11-17.md](cutover-claimed-complete-REG-2026-11-17.md)
