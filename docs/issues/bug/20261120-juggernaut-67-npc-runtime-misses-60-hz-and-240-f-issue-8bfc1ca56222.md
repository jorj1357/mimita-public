# issue-8bfc1ca56222: Juggernaut 67-NPC runtime misses 60 Hz and 240 FPS

## Human report (verbatim, immutable)

> record both do ur recommendation but get 240fps


## Triage

### 2026-11-20T03:15:34.343Z — coordination-v2

Summary: Pinned baseline run 20261120_031127 produced 67 NPCs and the expected arcade path, but server loop was 11-13 Hz with 100-570 ms stalls and client was 11-17 FPS; server NPC AI and client frame budget need separate owners.
Owner: coordination-v2
Priority: high

## Assignment

### 2026-11-20T03:15:34.834Z — coordination-v2

Owner: coordination-v2

## Task linked

### 2026-11-20T03:15:36.683Z — coordination-v2

Task: juggernaut-60hz-baseline (step 0)

## Task linked

### 2026-11-20T03:15:37.033Z — coordination-v2

Task: juggernaut-server-60hz (step 0)

## Task linked

### 2026-11-20T03:15:37.458Z — coordination-v2

Task: juggernaut-client-240fps (step 0)

## User quote

### 2026-11-20T03:33:16.111Z — coordination-v2

Via coordination-v2; supports issue-8bfc1ca56222:

> Pinned baseline 20261120_032618: 67 NPCs, 67 remote NPCs, physical_contact_ms=0; server 11-13 Hz with 100-570 ms stalls (npc_ms 30-220+; ai_ms~818-888/s); client 8-15 FPS, NetworkEntities up to 922 ms, Ragdoll::WorldCollision up to 810 ms; snapshots missed 697, interarrival ~1.0-1.4s. Source culling edit is pending build.

## Task linked

### 2026-11-20T03:33:16.506Z — coordination-v2

Task: remote-cull-240fps (step 0)

## Task linked

### 2026-11-20T03:44:50.364Z — runtime-60hz

Task: connection-stability (step 0)

## Agent suggestion

### 2026-11-22T14:35:31.676Z — codex-networking-stall-fix

Cross-issue evidence from the FCS runtime, relevant to NPC/tick budget: run 20261122_134637 (Fun Counter Strike, 19 NPCs, generation 495) recorded server npc.slow-update actor 100011 at 2026-11-22T13:59:45.064Z with total_ms=7035.3385, navigator_ms=7032.7936, decision_ms=7034.2276; npc.nav.plan-cost seq 95522 had recast_ms=7030.6396 and to_goal_m=0.0. The same tick stalled 8404.226 ms. This identifies a shared NPC navigation hotspot relevant to high-NPC server rate, but does not prove the 67-NPC scenario has the same cause. A guard was added in src/npc/npc-navigator.cpp and generation 499 published; fresh FCS runtime evidence is pending.
