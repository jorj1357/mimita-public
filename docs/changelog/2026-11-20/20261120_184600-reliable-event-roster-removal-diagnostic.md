# Reliable-event roster removal diagnostic

## Change

Added a bounded structured cause event and the shared player-session removal record when reliable gameplay-event delivery evicts a player connection. This is diagnostic only; connection eviction behavior is unchanged. The change is in `src/network/reliable-gameplay-events.cpp` and has build note `2b6c3ae7f791`.

## Evidence

- Historical TDM run `20261120_152743` showed all 10 reconnect attempts arriving at the server and being rejected with `token_not_found`. Server snapshots moved from one player to zero between snapshots without a structured removal event. This establishes transport receipt before rejection, but not the cause of the initial roster loss.
- The current root-owned DLL attempt `game-dll-20261120T182726149737Z-df8100` builds source generation 29 (generation 409); its timing log had completed 494/642 translation units at the latest inspection. It started before this diagnostic source save, so it does not contain this change.
- The source tree is at generation 31 with one pending game-DLL request at the latest status inspection. A pinned runtime proof has not been run.
- Existing dev-loop run `20261120_170259` remains on its prior EXE identity and running sandbox server; it was not replaced.

## Limits and next step

No build, runtime, visual, or human acceptance claim is made for this change. Build the current source through the root dev-loop, then repeat a pinned TDM join/disconnect/reconnect scenario and correlate the structured removal cause with the client and server journals. Continue the broader lifecycle and Payload cart-grounding investigation under `issue-cf9cd77eb056`.
