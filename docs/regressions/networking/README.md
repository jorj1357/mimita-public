# Networking regressions

Domain regression index. Each linked record owns its chronology and evidence.

Entries: 15
---

## Evidence-backed recurring problems and fixes

| Recurring pattern | Evidence-backed response |
|---|---|
| Packets are rejected, misrouted, truncated, or tied to the wrong session. | Trace construction, transport, validation, and apply with full per-instance identity. |
| Snapshots stop while a client shows stale state or fails to reconnect. | Log snapshot age and session transitions; make snapshot starvation trigger reconnect independently. |

---

## Entry 1 — Repeating client/server live-generation mismatch

- Recorded: 2026-09-20T16:50:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md
- Detail: [generation-mismatch-repeats-REG-2026-09-20.md](generation-mismatch-repeats-REG-2026-09-20.md)

## Entry 2 — Avatar manifest packets silently rejected by server packet gate

- Recorded: 2026-10-15T20:17:04Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md docs/specs/griever-monster/griever-monster.md (7.1 avatar-change capability resolution)
- Detail: [avatar-manifest-packets-REG-2026-10-15.md](avatar-manifest-packets-REG-2026-10-15.md)

## Entry 3 — Map id truncation caused stale-world client spawn

- Recorded: 2026-10-17T00:40:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/gamemodes.md (2026-10-16 runtime evidence and acceptance), docs/specs/networking/networking.md (map agreement and join readiness).
- Detail: [map-id-truncation-REG-2026-10-17.md](map-id-truncation-REG-2026-10-17.md)

## Entry 4 — Duel mode presentation packet routing

- Recorded: 2026-10-21T08:47:00Z
- Status: FIXED IN SOURCE; LIVE CONFIG APPLICATION CONFIRMED
- Summary: In standard Duel, the JSON explicitly sets presentation.damagenumbers to false, but damage numbers remained visible.
- Detail: [duel-mode-presentation-REG-2026-10-21.md](duel-mode-presentation-REG-2026-10-21.md)

## Entry 5 — Dev-loop mode 1 reached an older server on a shared UDP port

- Recorded: 2026-10-24T14:29:58Z
- Status: FIXED IN LOCAL RUNTIME (one fresh run; repeated human play remains open)
- Summary: Mode 1 launched a new server and registered a room, but the client displayed “Server identity mismatch.” In logs/2026-10-24/20261024140417/events.jsonl, network.server-identity-mismatch recorded expected run 20261024140417 and actual run 20261024140320, with the same build ID.
- Detail: [mode-one-server-REG-2026-10-24.md](mode-one-server-REG-2026-10-24.md)

## Entry 6 — Client Prediction and Server Authority Disagreement

- Recorded: 2026-10-29T17:13:46Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/networking/contact-authority.md - docs/specs/networking/networking.md - docs/specs/weapons/melee/melee-weapons.md - docs/specs/debug-logging/debug-logging.md - docs/specs/dev-loop/quality-gates.md
- Detail: [client-prediction-server-REG-2026-10-29.md](client-prediction-server-REG-2026-10-29.md)

## Entry 7 — v2.0.7 Actor Matrix Coverage Gap

- Recorded: 2026-10-29T17:13:46Z
- Status: UNRESOLVED
- Summary: - docs/specs/dev-loop/quality-gates.md - docs/specs/networking/contact-authority.md - docs/workflows/runtime-scenario-validation.md
- Detail: [v2.0.7-actor-matrix-REG-2026-10-29.md](v2.0.7-actor-matrix-REG-2026-10-29.md)

## Entry 8 — Fun Counter Strike mode id truncated in DuelStatePacket

- Recorded: 2026-11-01T01:54:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/funcounterstrike.md, docs/specs/gamemodes/objective-hud.md
- Detail: [funcounterstrike-mode-id-REG-2026-11-01.md](funcounterstrike-mode-id-REG-2026-11-01.md)

## Entry 9 — Regression: server did not track the player; forward/back rubberband (FIX DIRECTION CONFIRMED, TUNING REMAINS)

- Recorded: unknown
- Status: FIX WORKING — TUNING REMAINS
- Summary: // 2026-09-16T18:10:00Z / purpose record the movement-authority fix that eliminated the spawn lock and the forward/back rubberband, and confirm the direction is the right one capture what remains as tuning, not a redesign this file DOES NOT replace docs/regressions/regressions-v1.md
- Detail: [movement-authority-adopt-REG-2026-09-16.md](movement-authority-adopt-REG-2026-09-16.md)

## Entry 10 — Regression: server never tracks the player / stuck near spawn (client/server hot generation mismatch)

- Recorded: unknown
- Status: ROOT CAUSE CONFIRMED
- Summary: // 2026-09-16T17:40:00Z / purpose record the confirmed regression where the server never tracked the player (stuck near spawn, server position error never returned to 0) capture the exact evidence, cause, and permanent prevention
- Detail: [server-client-generation-REG-2026-09-16.md](server-client-generation-REG-2026-09-16.md)

## Entry 11 — Regression TODO: ICE host stops fetching join requests after first client (OPEN)

- Recorded: unknown
- Status: OPEN / UNRESOLVED TODO
- Summary: // 2026-09-17T03:40:00Z / purpose TODO regression record: after the first client joins, the ICE host stops fetching later join requests and the room expires, so further joins fail. Records the evidence and the working assumption that this is NOT the
- Detail: [ice-host-poll-REG-2026-09-17.md](ice-host-poll-REG-2026-09-17.md)

## Entry 12 — Payload roster NPCs did not replicate and both teams used one spawn

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME AND HUMAN VISIBILITY CONFIRMED FOR PAYLOAD 1
- Summary: - Recorded: 2026-10-25T15:29:08Z - Status: FIXED IN SOURCE; RUNTIME AND HUMAN VISIBILITY CONFIRMED FOR PAYLOAD 1 - Changelog: [../../changelog/2026-10-25/20261025152908-payload-npc-spawn-authoring.md](../../changelog/2026-10-25/20261025152908-payload-npc-spawn-authoring.md)
- Detail: [payload-npc-replication-REG-2026-10-25.md](payload-npc-replication-REG-2026-10-25.md)

## Entry 13 — Client appeared frozen while a different server kept reporting 60 Hz

- Recorded: 2026-11-04T14:30:00Z
- Status: FIXED IN SOURCE; live journal proof recorded; dashboard visual review pending
- Summary: One run had two servers and a shared journal; the client's server died, the client kept replaying its last snapshot, and a playerless 60 Hz server made the run look healthy.
- Detail: [frozen-client-dead-REG-2026-11-04.md](frozen-client-dead-REG-2026-11-04.md)

## Entry 14 — Fun Counter Strike late-join client received unknown team identity

- Recorded: 2026-11-05 (captured run started at 2026-11-05T20:06:15Z)
- Status: ATTEMPTED FIX — build passed; fresh FCS runtime verification pending
- Summary: FCS's empty legacy `victory_condition` field prevented the active-player identity repair from assigning the late-joining client a team and role.
- Detail: [funcounterstrike-late-join-identity-REG-2026-11-05.md](funcounterstrike-late-join-identity-REG-2026-11-05.md)

## Entry 15 — Stale remote NPCs survived a client map switch

- Recorded: 2026-11-06T05:56:43Z
- Status: FIXED IN SOURCE; initial-load journal proof recorded; non-zero map-switch runtime proof pending
- Summary: The client loaded skyhouse while 9 badhouse NPCs from the previous session remained visible at old-map coordinates until teardown. New map loads now clear remote actors/projectiles/ragdolls before the new world is shown.
- Detail: [stale-remote-npcs-map-switch-REG-2026-11-06.md](stale-remote-npcs-map-switch-REG-2026-11-06.md)