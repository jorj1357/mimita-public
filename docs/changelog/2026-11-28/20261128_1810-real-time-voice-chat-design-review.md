# Real-time voice chat design review

## Summary

Reviewed the user proposal against the existing voice, audio ownership, networking, moderation, performance, and scheduling documents. Created capability issue `issue-29b3665c767d` and appended a transport/device/effects recommendation and proof plan.

## Findings

- Existing `docs/specs/ingame-chat/voice-chat.md` already establishes open-room/proximity voice, mute controls, room disable, moderation/reporting, captions where available, and multiplayer/human listening acceptance. It does not define capture devices, codec, packet behavior, or detailed gain policy.
- `docs/architecture/live-development/hot-audio-contract.md` assigns device and mixer ownership to the stable EXE service and includes audio voice priorities. Recommend retaining that owner and adding voice capture/mixing through it.
- `docs/specs/moving-physical-objects/moving-physical-objects.md` describes a priority order and deferred-work budgets as a target scheduler direction. This does not establish that a general scheduler is implemented.
- No voice chat implementation or runtime evidence was examined. Build or runtime acceptance is not claimed.

## Design recommendation

Use the existing miniaudio service for OS device enumeration, capture, and playback; add Opus for compressed interactive voice; route bounded sequenced datagrams through the current networking stack, with a small jitter buffer and concealment. The server should authorize and route streams by mode, team, mute and proximity policy. Clients should decode, apply local listener gain and spatialization, and mix through the existing mixer.

Treat wired and Bluetooth endpoints as OS-enumerated devices. Device/profile capabilities vary with OS and drivers. The terminal proposal should report local input devices and capabilities without sending device identity to the server. Use local mic settings, a receiver-side voice master gain, bounded dB boost, a dBFS meter, and a limiter/soft clipper. Keep voice test on a private local-monitor route through the same processing path, never to peers. Ship no expensive effects initially; add opt-in client-side filters only after measuring CPU cost and implementing graceful bypass under load.

## Proof plan

Use a pinned server and two clients. Capture events for device open, capture/PTT, encoded sequence and bytes, server policy decision and recipient set, receiver decode/playout/jitter/loss, selected mode/distance/team/mute, local test routing, and CPU/network/frame/tick cost. Exercise devices, disconnects, packet loss, all routing modes, and proximity boundaries. Separate machine-readable transport/routing evidence from human listening review.

## External references

- Opus describes in-game chat as a target use and supports packet-loss concealment: https://www.opus-codec.org/
- miniaudio provides device enumeration and capture/playback APIs: https://github.com/mackron/miniaudio/blob/master/examples/engine_advanced.c

## Validation

Documentation and issue review only. No source code, build, test, or runtime behavior was changed or verified. Issue discussion and user-facing response contain the proposed acceptance criteria.

## Separate scheduler question

Created capability issue `issue-a23380d89b39` for checking and proving a general frame-time work priority scheduler. The existing physics document gives conceptual priority and deferred-work guidance; this review did not inspect source/runtime to determine whether a general scheduler is implemented.
