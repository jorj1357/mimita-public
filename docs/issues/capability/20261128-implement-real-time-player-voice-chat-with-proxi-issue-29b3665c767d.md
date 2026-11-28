# issue-29b3665c767d: Implement real-time player voice chat with proximity, open-room, device and safety controls

## Human report (verbatim, immutable)

> how do we  make real time voice caht in the game  do we need external library or somethign? how to prove its working


## Agent suggestion

### 2026-11-28T18:08:21.203Z — voice-chat-design-review

Design recommendation (hypothesis, not implementation evidence): keep device capture/enumeration and output mixing in the existing EXE-owned miniaudio service; it already owns the single device/mixer and documents voice prioritization. Add a codec such as Opus for compressed speech packets, reusing the existing session/datagram transport rather than sending PCM or creating a second networking stack. Use bounded unreliable sequenced frames, a small jitter buffer, and concealment; server authorizes and routes by mode/team/proximity, while clients decode/spatialize through the mixer. Keep device names/IDs local and expose OS-enumerated endpoints/capabilities: wired/Bluetooth are OS driver routes, not separate game-level device classes, and availability/profile/sample-rate behavior varies by OS. For proximity, use server-validated actor origins and per-listener gain: 1 through 5m, linear fade to 0 at configured far distance (default 50m), then omit packets; document that 'global volume' is the receiver's local master voice gain. Implement test mode as local-monitor of the exact post-boost/filter/encode-decode path, with explicit echo protection, and never send those packets to peers. Keep boost bounded in dB, meter in dBFS, and use soft clipping/limiting; 999 dB cannot be physically represented and must not become unbounded gain. Phase 1 should be no extra effects beyond safe gain/limiter; EQ/muffle/reverb/vocoder and OTT-like processing can be opt-in client-side effects loaded only when selected, with a measured CPU budget and bypass under load. Proof should use two pinned clients plus server, correlation IDs and events for device open/capture/PTT, encoded sequence/bytes, server policy allow/drop/recipient, received/decode/playout/jitter/PLC, selected mode/distance/team/mute, end-to-end latency and loss, and bounded CPU/network/frame/tick cost; then do a human listening check. No runtime proof exists yet.

## Agent suggestion

### 2026-11-28T18:16:03.306Z — build-wait-reviewer

Duplicate-intake finding by build-wait-reviewer: issue-6e082da43cac has the identical title/summary/acceptance/paths from voice-chat-design-review and was created ~17s earlier without a linked context. This issue carries the linked user context ctx-756c4d7db51d, so it should be the canonical entry; consolidate scope/proof here and close issue-6e082da43cac as a duplicate. Leaving the decision to the reporter/owner to avoid clobbering active work.

## Agent suggestion

### 2026-11-28T18:18:01.870Z — voice-chat-spec-expansion

Spec expansion completed in docs/specs/ingame-chat/voice-chat.md per the user's follow-up. It now specifies device privacy and enumeration, server-authoritative voice modes and proximity recipient cutoff, local receiver volume, PTT/V and voice-activity modes, dBFS HUD meter, private post-processing test, bounded gain/limiter, lazy client-side effects, safe failure behavior, metadata-only reports with incident IDs, suggested player and moderator command surface, structured event chain, and pinned two-client-plus-server acceptance. Moderation reviewer permissions, retention, and enforcement remain owned by moderation.md and are explicitly left unsettled; no auto-scoring, audio recording, or automatic moderation verdict was introduced. Source/spec work only; no implementation or runtime proof.

## Agent suggestion

### 2026-11-28T18:19:45.834Z — voice-chat-spec-expansion

Duplicate-intake reconciliation: the older exact duplicate issue issue-6e082da43cac has now been closed with an explicit exemption limited to duplicate tracking, not feature proof. This canonical issue retains both linked user contexts, the design recommendation, and the new spec acceptance summary. docs/specs/ingame-chat/voice-chat.md links to this issue. The feature remains unimplemented and runtime-unproven.
