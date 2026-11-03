# Networking regressions

Append-only master ledger. Each entry links to the detailed dated record that owns the full evidence.

Entries: 12

---

## Entry 1 — Repeating client/server live-generation mismatch

- Recorded: 2026-09-20T16:50:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md
- Detail: `docs/regressions/2026-09-20/generation-mismatch-repeats-REG.md`

## Entry 2 — Avatar manifest packets silently rejected by server packet gate

- Recorded: 2026-10-15T20:17:04Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/networking/networking.md docs/specs/griever-monster/griever-monster.md (7.1 avatar-change capability resolution)
- Detail: `docs/regressions/2026-10-15/avatar-manifest-packets-rejected-REG.md`

## Entry 3 — Map id truncation caused stale-world client spawn

- Recorded: 2026-10-17T00:40:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/gamemodes.md (2026-10-16 runtime evidence and acceptance), docs/specs/networking/networking.md (map agreement and join readiness).
- Detail: `docs/regressions/2026-10-17/map-id-truncation-stale-ready-REG.md`

## Entry 4 — Duel mode presentation packet routing

- Recorded: 2026-10-21T08:47:00Z
- Status: FIXED IN SOURCE; LIVE CONFIG APPLICATION CONFIRMED
- Summary: In standard Duel, the JSON explicitly sets presentation.damagenumbers to false, but damage numbers remained visible.
- Detail: `docs/regressions/2026-10-21/duel-mode-presentation-packet-routing-REG.md`

## Entry 5 — Dev-loop mode 1 reached an older server on a shared UDP port

- Recorded: 2026-10-24T14:29:58Z
- Status: FIXED IN LOCAL RUNTIME (one fresh run; repeated human play remains open)
- Summary: Mode 1 launched a new server and registered a room, but the client displayed “Server identity mismatch.” In logs/2026-10-24/20261024140417/events.jsonl, network.server-identity-mismatch recorded expected run 20261024140417 and actual run 20261024140320, with the same build ID.
- Detail: `docs/regressions/2026-10-24/mode-one-server-port-REG.md`

## Entry 6 — Client Prediction and Server Authority Disagreement

- Recorded: 2026-10-29T17:13:46Z
- Status: ATTEMPTED FIX (1)
- Summary: - docs/specs/networking/contact-authority.md - docs/specs/networking/networking.md - docs/specs/weapons/melee/melee-weapons.md - docs/specs/debug-logging/debug-logging.md - docs/specs/dev-loop/quality-gates.md
- Detail: `docs/regressions/2026-10-29/client-prediction-server-authority-REG.md`

## Entry 7 — v2.0.7 Actor Matrix Coverage Gap

- Recorded: 2026-10-29T17:13:46Z
- Status: UNRESOLVED
- Summary: - docs/specs/dev-loop/quality-gates.md - docs/specs/networking/contact-authority.md - docs/workflows/runtime-scenario-validation.md
- Detail: `docs/regressions/2026-10-29/v2.0.7-actor-matrix-coverage-REG.md`

## Entry 8 — Fun Counter Strike mode id truncated in DuelStatePacket

- Recorded: 2026-11-01T01:54:00Z
- Status: ATTEMPTED FIX (1)
- Summary: docs/specs/gamemodes/funcounterstrike.md, docs/specs/gamemodes/objective-hud.md
- Detail: `docs/regressions/2026-11-01/funcounterstrike-mode-id-truncation-REG.md`

## Entry 9 — Regression: server did not track the player; forward/back rubberband (FIX DIRECTION CONFIRMED, TUNING REMAINS)

- Recorded: unknown
- Status: FIX WORKING — TUNING REMAINS
- Summary: // 2026-09-16T18:10:00Z / purpose record the movement-authority fix that eliminated the spawn lock and the forward/back rubberband, and confirm the direction is the right one capture what remains as tuning, not a redesign this file DOES NOT replace docs/regressions/regressions-v1.md
- Detail: `docs/regressions/2026-09-16/2026-09-16-movement-authority-adopt-fixed-rubberband.md`

## Entry 10 — Regression: server never tracks the player / stuck near spawn (client/server hot generation mismatch)

- Recorded: unknown
- Status: ROOT CAUSE CONFIRMED
- Summary: // 2026-09-16T17:40:00Z / purpose record the confirmed regression where the server never tracked the player (stuck near spawn, server position error never returned to 0) capture the exact evidence, cause, and permanent prevention
- Detail: `docs/regressions/2026-09-16/2026-09-16-server-client-generation-mismatch-spawn-lock.md`

## Entry 11 — Regression TODO: ICE host stops fetching join requests after first client (OPEN)

- Recorded: unknown
- Status: OPEN / UNRESOLVED TODO
- Summary: // 2026-09-17T03:40:00Z / purpose TODO regression record: after the first client joins, the ICE host stops fetching later join requests and the room expires, so further joins fail. Records the evidence and the working assumption that this is NOT the
- Detail: `docs/regressions/2026-09-17/2026-09-17-ice-host-poll-not-fetched-after-first-join.md`

## Entry 12 — Payload roster NPCs did not replicate and both teams used one spawn

- Recorded: unknown
- Status: FIXED IN SOURCE; RUNTIME AND HUMAN VISIBILITY CONFIRMED FOR PAYLOAD 1
- Summary: - Recorded: 2026-10-25T15:29:08Z - Status: FIXED IN SOURCE; RUNTIME AND HUMAN VISIBILITY CONFIRMED FOR PAYLOAD 1 - Changelog: [../../changelog/2026-10-25/20261025152908-payload-npc-spawn-authoring.md](../../changelog/2026-10-25/20261025152908-payload-npc-spawn-authoring.md)
- Detail: `docs/regressions/2026-10-25/payload-npc-replication-and-spawn-collapse-REG.md`
