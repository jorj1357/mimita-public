# Local restart, room code, and lazy NPC setup

- Status: `RUNTIME_VERIFIED_WITH_FIREWALL_LIMITATION`
- Recorded: `2026-10-24T13:33:13-04:00` (America/New_York)
- Branch: `20261011zt`
- HEAD: `0472121baef8d96342508d82aef5598de97daff7`
- Request: enter the local server immediately, keep its registered room code visible, make DevLoop restart commands work after leaving, defer expensive NPC setup, keep mode 4 as NPC-free Sandbox on funworld3, announce each public room once, and suppress the Windows firewall prompt by allowing the server.

## Source changes

### `src/main.cpp`

`processAgentCommandQueue()` now retains gameplay-dependent requests that are not ready, while continuing to deliver server lifecycle commands. It preserves request order within a scenario and records a deferred event once per request.

Before, queue processing stopped as soon as gameplay was unavailable:

```cpp
if (queuePath.empty() || !gameplayReady)
    return;
```

After, it reads requests into a retained queue and permits server start/leave commands while gameplay-only work waits:

```cpp
const bool gameplayIndependent = startsLocalServer || command == "server_leave";
...
else if (!gameplayReady && !gameplayIndependent)
    deferredReason = "gameplay_not_ready";
```

This fixed the observed sequence where `agent.snapshot restart-blocker` blocked a later `server_start_preset` after leaving a room.

### `src/gui/gui-main.cpp`

Local preset start queues a direct loopback join immediately. If the room file arrives while that join is pending, it allows 1.5 seconds for UDP retries before falling back through the coordinator. When the room file contains a registered code, the GUI writes that code into `MP_CONTEXT.currentRoomCode`, the gameplay HUD's source, and records `network.room-code.hud-source-updated`.

Before, the room-file poll updated the server settings and online-menu code only:

```cpp
gServerLaunchSettings.serverCode = content;
onlineMenuSetServerCode(content);
```

After, it also updates the HUD source and logs the value:

```cpp
if (MP_CONTEXT.currentRoomCode != content)
{
    MP_CONTEXT.currentRoomCode = content;
    StructuredLogger::instance().writeEvent(
        StructuredCategory::Network, StructuredLevel::Important,
        "network.room-code.hud-source-updated", content,
```

### `src/network/server.cpp`

The server now skips NPC collision/navigation mesh construction until NPCs exist. The measured build duration is logged at that point. An empty Sandbox server therefore reaches its UDP gameplay loop without waiting for NPC navigation setup.

Before, collision/navigation setup ran before the UDP socket was ready:

```cpp
buildNpcWorldCollision(npcWorld, world);
```

After, startup records the work as pending:

```cpp
bool npcWorldCollisionReady = false;
```

and performs it only once the NPC list is nonempty:

```cpp
if (!npcWorldCollisionReady && !npcs.empty())
{
    const auto npcWorldBuildStart = std::chrono::steady_clock::now();
    buildNpcWorldCollision(npcWorld, world);
    npcWorldCollisionReady = true;
```

### `src/engine/engine-tick-state.cpp`

Removed trailing spaces from the two existing comments while validating whitespace. No runtime behavior changed in this cleanup.

## Build and runtime evidence

- DevLoop published current build `3085` from source generation `19`; status reported `latest_build_stale=false`, matching server/client build IDs, and a live server and client.
- In `logs/2026-10-24/20261024_172949/events.jsonl`, the base no-NPC Sandbox server reported `network.server.local-gameplay-ready` at `t=0.712s` after process start.
- Restarting `sandbox_1` after `server_leave` succeeded despite the deferred snapshot request. The new local server reported gameplay-ready at `t=0.839s`; the client connected directly to `127.0.0.1:56298` with matching build identity.
- The restarted server's room code `AMJ35WD` updated the gameplay HUD source once. The same journal contains exactly one `network.server-announcement.queued` event for `AMJ35WD` and zero identity-mismatch events.
- Mode 4 was run on build `3083` with `funworld3`, `sandbox`, and `--no-npcs`; the DevLoop config names it `funworld3nothing`. Mode 4's behavior is runtime-verified from the preceding run, not from the currently active mode 1 session.
- NPC navigation was deferred for the one-NPC preset; its first build took about `4023 ms` after the server became gameplay-ready. This keeps local play responsive but does not make NPC activation instantaneous.
- `git diff --check` reported only line-ending normalization warnings; no whitespace errors remained.
- No synthetic tests were added or run.

## Firewall limitation

The requested inbound allow rule could not be created because the current Windows account is not elevated; the firewall command returned `Access is denied`. DevLoop reports `FIREWALL_PROMPT_DETECTED`. The app cannot silently add a Windows Defender Firewall rule without administrator rights. No firewall was disabled and no popup automation was added. A one-time administrator-approved allow rule is still required for inbound hosting; this remains unverified and incomplete.

## Preserved work

The checkout contained other staged and unstaged user changes before this continuation. They were preserved, including unrelated physics/collision edits and the existing changelog `20261024_151214-triangle-sweep-snag.md`.
