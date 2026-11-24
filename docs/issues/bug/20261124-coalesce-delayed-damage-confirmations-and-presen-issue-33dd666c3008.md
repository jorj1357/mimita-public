# issue-33dd666c3008: Coalesce delayed damage confirmations and present a server-wide lightning event

## Human report (verbatim, immutable)

> queued  damage events like the hit presentation i amgetting hit from liek a backlog of damage, so i tihnk we need to liek at a certain point batch it and apply it all at once
> 
> and  since we  do the game engine and its damage it shoudl be like a huge server wide event liek lightingin strike
> 
> so we can solve  the networking issue  where its a lot of littel evnets being tramsitted and all redone  over and over individually long after the actual even thappened, and its batching that  and i tlooks cool
> and applies all the damage at once in a way that makes sense in teh game with lighitng and like a sever wide sound regardless of gamemode i might have  the sound idk where it is tho  it might be in the  repo alread


## Agent discussion and suggestions

### 2026-11-24T00:10:27.528Z — damage-lightning-backlog

Message: `msg-2f27849b13e9` (question)

Subject: Ownership check for dirty damage paths

These three files are already modified in the shared checkout. My trace finds queueServerDamageConfirmedEvent builds one reliable DamageConfirmedEventPacket per applied result; mpProcessDamageConfirmedEventPacket applies state then calls presentConfirmedDamage per received event. Who currently owns these edits, and is there a live run or known queue-delay evidence tied to them? I have not edited these paths.

Sender: damage-lightning-backlog
Paths: src/network/server-damage.cpp, src/network/multiplayer-projectiles.cpp, src/network/confirmed-damage-presentation.cpp

This is a cross-agent signal. Please reply with whether this work is yours, the exact owner if known, and the first missing or failing step. Do not assume the sender will stop or overwrite your work.

## Agent suggestion

### 2026-11-24T00:13:43.734Z — damage-lightning-backlog

New evidence narrows the first incorrect stage to the client local-victim replay queue, not yet the network transport. In logs/2026-11-23/20261123_235443 (Juggernaut Horde; EXE build 2026-11-23T19:22:15.804284Z; server/client PIDs 30384/19504), client receives DamageConfirmed records and immediately emits presentation.local_victim_damage, then enqueues one PendingVictimHealth per event at src/network/multiplayer-projectiles.cpp:1730-1755. mpDrainPendingVictimHealth iterates and applies each saved healthAfter/knockback at src/network/multiplayer-reconcile.cpp:475-538. Journal lines 27, 32, 35, 36 show server healthAfter 999881 at tick 14277 then 999824 at tick 14284; drain applies 999881 after local health is already 999824 (wall_time 00:02:20.186Z), then reapplies 999824 in the same millisecond. This causes stale health/knockback replay; it does not prove delayed packet delivery. Keep the transport aggregation as a separate measured question. Proposed first patch: keep authoritative latest local health monotonic by tick/generation and coalesce queued presentation/knockback safely; preserve lethal transitions. A server-wide cue requires a batch identity that all clients receive.
