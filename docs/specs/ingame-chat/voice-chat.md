// 2026-11-28
/* purpose
* specify player-to-player real-time voice chat, its controls, routing, safety, and proof
* define one mode-driven voice system that uses the existing audio and network owners
* this file DOES NOT specify generated character dialogue or moderation penalties
* this file DOES NOT authorize recording or automated transcription of conversations
*/

# In-Game Voice Chat

**Status:** Desired behavior contract.
**Audience:** Audio, networking, game-mode, accessibility, moderation, and terminal-command contributors.
**Authority:** This document owns player voice-chat behavior and its user-facing commands. The existing audio service owns device access and mixing. The existing network session owns transport and identity. The moderation specification owns report adjudication and enforcement policy. Text chat remains independently available.

## Desired experience

Players can speak naturally in a multiplayer session using a selected microphone and hear the players allowed by that game mode. A mode selects voice off, proximity voice, or open-room voice. A player can disable their microphone, locally mute a speaker, adjust voice levels, test their own processed voice privately, and report abusive behavior. A player's ability to understand or complete a match MUST NOT depend only on hearing voice.

Voice is live communication. It is not generated character dialogue, an authored sound asset, or a second text-chat implementation. NPC voice generation follows [Live Voice Generation](../live-voice-generation/live-voice-generation.md).

## Ownership and audio path

- The stable EXE audio service owns OS device enumeration, microphone capture, output devices, and final mixing. Voice code MUST use that owner rather than open a second audio engine or device.
- The existing multiplayer session owns peer identity, transport security, connection lifecycle, and packet delivery. Voice MUST reuse that session and MUST NOT create a second account, room, or transport system.
- The voice policy owner decides whether capture is allowed, which recipients may hear a speaker, and the spatial gain for each speaker-listener pair.
- Capture callbacks MUST remain bounded, nonblocking, and allocation-free. Encoding, decoding, and network work MUST run outside the audio callback with bounded queues.
- Voice packets MUST be encoded with a low-latency speech codec. Opus is the preferred implementation candidate; codec choice does not change the packet, moderation, privacy, or proof requirements.
- Voice frames MUST use bounded, sequenced, lossy delivery. A late voice frame is discarded instead of retransmitted after it is useful. Receivers use a bounded jitter buffer and packet-loss concealment; voice backlog MUST NOT delay gameplay packets or fixed-tick simulation.
- The server MUST validate sender membership, voice mode, team policy, mute/block policy, packet size/rate, and authoritative speaker position before routing. A client-provided position MUST NOT determine who can hear whom.
- The server SHOULD relay encoded frames without decoding or recording them. Recipient selection and mode enforcement remain server-authoritative.
- Each receiver applies their own master voice volume and per-speaker volume. A receiver's volume settings MUST NOT alter another player's mix.

The intended path is:

```text
local device -> bounded capture -> optional sender processing -> speech codec
 -> existing session transport -> server validation and recipient routing
 -> bounded jitter buffer -> decode -> listener gain/spatialization -> shared mixer
```

## Device controls and privacy

The operating system is the device inventory authority. Device listing reports locally visible input and output endpoints, a session-local identifier, default status, and supported channel/sample-rate information where available. Wired and Bluetooth devices are not separate MiMITA device classes: the OS and drivers determine which endpoints and profiles are available. Device names, hardware IDs, and local device choices MUST NOT be sent to the server.

Microphone capture is disabled until the player enables it. Push-to-talk is the default pickup mode. The interface MUST clearly indicate when the microphone is open and when voice is being transmitted. Device removal, permission denial, initialization failure, and sample-format changes MUST fail closed and appear in local diagnostics.

The game MUST NOT automatically record, publish, transcribe, or retain voice conversations. A report MUST NOT silently attach an audio recording. A future recording feature requires a separate explicit, disclosed, and reviewed specification.

## Modes, recipients, and spatial gain

Each game mode selects one server-authoritative mode:

| `voice_mode` | Behavior |
|---|---|
| `0` | Voice disabled. No microphone frames are transmitted for the match. |
| `1` | Proximity voice. Server routes each speaker only to eligible listeners within the configured distance. |
| `2` | Open-room voice. Server routes each speaker to eligible listeners in the same room, subject to team, mute, block, and moderation restrictions. |

A mode MAY further restrict recipients by team or role. Team voice is enabled for all teams by default. A mode may disable cross-team voice or define explicit team channels. The server applies the mode's policy to every frame; clients cannot opt themselves into a forbidden recipient group.

In proximity mode, distance is measured in world meters from the listener camera position to the speaker actor's authoritative body-origin position. The mode configuration owns `near_distance_m` (default 5 m) and `far_distance_m` (default 50 m), with `far_distance_m` strictly greater than `near_distance_m`.

- From zero through `near_distance_m`, spatial voice gain is 1.0 before the listener's local volume settings.
- Between near and far distance, gain decreases linearly to 0.0.
- At or beyond `far_distance_m`, the listener is excluded from that speaker's recipient set; the server does not send that speaker's frames to that listener.
- World occlusion MAY apply additional attenuation, but MUST NOT increase gain or bypass the mode's far-distance cutoff.

`voice_volume_global` is the local receiver's master gain for all other players. `voice_volume_player` is a local multiplier for one speaker. Both accept finite values from 0.0 (mute) through 1.0 (full configured level). Neither setting changes the sender's capture or other players' mix.

## Push-to-talk, meter, and private test

Pickup mode `1` is push-to-talk and is the default. The default key is V; players can rebind it through the normal input binding system. Pickup mode `2` is voice-activity activation and uses a configurable threshold. The voice mode and pickup mode are independent: pickup mode controls local capture gating; voice mode controls server routing.

While the player is transmitting, the HUD shows a microphone icon above the hotbar near the bottom center. It also shows an input meter based on digital full scale (dBFS), not calibrated acoustic dB. The meter MUST distinguish normal input, near-clipping, clipping, and overload. At and above digital full scale (0 dBFS), it shows a clear clipping warning. Color is not the only cue; use accessible text or shape indicators as well.

`voice_test 1` plays the local player's processed voice back only to that player. It MUST use the same gain, effects, codec, decode, and playback path used by remote listeners. It MUST NOT create network voice packets or be audible to another player. The HUD MUST show a prominent, blinking test-mode warning until `voice_test 0` disables it. Test playback MUST avoid feedback; headphones or an available echo-cancellation path SHOULD be recommended when speakers feed the microphone.

## Gain, dynamics, and effects

`voice_mic_boost` is a local sender-side digital gain applied after device capture and before encoding. Zero means no additional digital gain; it does not override the OS or hardware input level. The value MUST be finite and clamped to the configured safe maximum. Requests above that maximum are rejected with the accepted range shown; values such as 999 dB MUST NOT create unbounded amplification. A limiter or soft clipper MUST protect the codec input from overload. The player can still hear distortion from a poor source; processing does not guarantee a clean recording.

Voice effects have two explicit scopes:

- **Mic effects** process the sender's audio before encoding. Other players hear the processed result.
- **Local receive effects** process a speaker only in the choosing listener's mix. They do not change the speaker's transmitted audio or anyone else's mix.

EQ, muffling, reverb, vocoder, and OTT-style dynamics MAY be available as optional effects. Effects MUST be loaded only when selected, remain client-side, and have a measured CPU budget. If their cost exceeds that budget or frame/audio health degrades, the client MUST bypass or suspend optional effects while capture, decode, and basic mixing continue. Effects MUST NOT delay input, fixed-tick gameplay, or voice packet handling.

## Moderation and player safety

Local mute takes effect immediately on the local client and MUST stop rendering the selected speaker. It MUST NOT notify that speaker. Blocking uses the existing account/social block owner so voice, text, and future social surfaces do not create competing block lists. The voice spec does not define a second block database.

A voice report creates a traceable incident ID and enters the existing moderation report workflow. A report records the selected account, room/session, time, reporter-supplied category and optional note, and necessary delivery metadata. It does not contain raw audio or a transcript. The reported account and public users MUST NOT see the reporter's identity. The report workflow MAY offer a separate opt-in path for the reporter to provide contact information to authorized reviewers.

The command surface below describes proposed access boundaries. The moderation specification MUST define who may inspect and adjudicate reports, report retention, review status, and any enforcement action before those moderator commands are implemented. Voice chat MUST NOT introduce automatic social scores, public accusations, vote penalties, or automatic bans.

## Terminal command contract

Commands are a testable interface to the same state changes used by settings, HUD, input, and server policy. Local commands change only local state. Commands that change room policy go through the normal server-authorized command path. A client command MUST NOT bypass server authorization or directly mutate server-owned state.

| Command | Scope and behavior |
|---|---|
| `voice_status` | Local read-only report: capture state, selected local devices, pickup mode, test state, effects, and current room voice policy. Never includes raw audio. |
| `voice_mic_list` | Local list of OS-enumerated capture devices and capabilities; device names/IDs never go to the server. |
| `voice_mic_device <device-id>` | Select a local capture device from the current enumeration. Reject stale IDs and list devices again. |
| `voice_output_device <device-id>` | Select a local playback device from the current enumeration. |
| `voice_mic_enable 0|1` | Locally stop or enable capture. Disabling immediately closes the capture path and stops transmission. |
| `voice_volume_global <0.0..1.0>` | Set this listener's local master voice gain for all other players. |
| `voice_volume_player <player-id> <0.0..1.0>` | Set this listener's local gain for one player. |
| `voice_mute <player-id>` / `voice_unmute <player-id>` | Immediately mute or restore one speaker locally. |
| `voice_mute_list` | List locally muted players. |
| `player_block <account-id>` / `player_unblock <account-id>` | Use the shared social block owner. Blocking suppresses that account's voice and other covered social contact. |
| `player_block_list` | List accounts blocked by the local player. |
| `voice_pickup_mode 1|2` | Select push-to-talk (`1`, default key V) or voice-activity activation (`2`). |
| `voice_key <key>` | Set the local push-to-talk binding through the shared input-binding owner. |
| `voice_test 0|1` | Start or stop private local processed-voice monitoring. Test mode never transmits. |
| `voice_mic_boost <0..configured-max-dB>` | Set bounded sender-side digital gain; show the effective configured maximum and reject out-of-range/non-finite values. |
| `voice_mode [0|1|2]` | Without an argument, read current mode. Setting mode is a server-authorized room/mode operation; ordinary clients cannot change the mode. |
| `voice_proximity <near-meters> <far-meters>` | Read mode values as a client; set them only with server/mode-author permissions and validate finite distances with far greater than near. |
| `voice_team_all 0|1` | Read the current team-voice rule; changing it requires server/mode-author permission. Default is enabled. |
| `voice_fx_list [mic|receive]` | List available optional effects by scope and current enabled state. |
| `voice_fx_set <mic|receive> <effect> [parameters]` | Change local effect selection/parameters. Mic-scope processing is transmitted; receive-scope processing is local only. |
| `voice_report <player-id> <category> [note]` | Submit a privacy-safe report and return its incident ID. Categories include harassment, threats, slurs, sexual conduct, and other; category is a report label, not a verdict. |
| `voice_report_status <incident-id>` | Let the reporter check the state of their own report without exposing reviewer details or the reported player's private data. |
| `moderation_voice_reports [open|all]` | Proposed restricted reviewer command to list voice reports. Permission rules come from the moderation spec. |
| `moderation_voice_report_show <incident-id>` | Proposed restricted reviewer command to inspect report metadata. It never exposes an unconsented recording because reports contain no audio. |
| `moderation_voice_report_status <incident-id> <status>` | Proposed restricted reviewer command to update workflow status. Allowed states and audit rules are owned by the moderation spec. |

All commands MUST return structured accepted/rejected results with stable reason codes. Local device and volume commands MUST remain usable when disconnected; room-policy and report commands must explain unavailable server/workflow state and never silently claim success.

## Events and diagnostics

Use the shared structured event logger. Emit state transitions and failures, not a per-frame audio log. Correlate events with session, player, voice stream, packet sequence, and run IDs. Never log audio samples, device serial numbers, raw account tokens, or report notes in general telemetry.

Required event stages include:

- `voice.device.enumerated`, `voice.device.selected`, `voice.capture.started`, `voice.capture.stopped`, and `voice.device.failed`;
- `voice.transmit.started`, `voice.transmit.stopped`, and sampled `voice.packet.sent` events with sequence, encoded bytes, duration, and queue depth;
- `voice.server.packet.accepted` or `voice.server.packet.rejected` with reason code and recipient count, plus sampled recipient decisions with mode and distance band;
- `voice.packet.received`, `voice.decode.completed`, `voice.playout.started`, `voice.packet.lost`, and `voice.playout.concealed`;
- `voice.policy.loaded` / `voice.policy.reloaded`, `voice.test.started` / `voice.test.stopped`, `voice.local_mute.changed`, and `voice.report.created` / `voice.report.failed`;
- one-second bounded metrics for active speakers, encoded/received bytes, packet loss, jitter-buffer depth, end-to-end latency estimate, codec/effect CPU time, and audio underruns.

Events MUST not include audio content. Sampling and rate limits MUST prevent diagnostics from becoming the voice bottleneck.

## Failure behavior

- If the input device is unavailable or capture permission is denied, capture remains off and the player receives an actionable local error.
- If the network is congested, old voice frames are dropped before gameplay traffic is delayed. The client reports degraded voice quality and continues gameplay.
- If a speaker is muted, blocked, outside proximity range, or disallowed by mode/team policy, their audio is not played locally; the server omits ineligible proximity recipients where possible.
- If encoding, decoding, or an optional effect fails, optional effects are bypassed first. The client stops or degrades voice safely without crashing, blocking the audio callback, or affecting gameplay correctness.
- If the session disconnects or mode disables voice, all voice streams and queues are stopped and released; no stale audio plays after leaving the room.
- Failed configuration reload retains the last valid voice policy and reports the rejected fields.

## Accessibility

Text chat remains independent. Provide captions only when a player explicitly enables a supported transcription feature under a separate privacy review; captions MUST be visibly labeled as machine-generated and MUST NOT be stored as chat history by default. Voice volume, mute, and mode indicators must be available to keyboard/controller users and have non-color cues. Voice-unavailable clients must still be able to participate in the mode through its existing non-voice information channels.

## Acceptance

Acceptance requires pinned multiplayer runtime proof and human listening review. It is not satisfied by a successful build, command response, local loopback alone, or synthetic codec test.

- A local device scenario lists multiple connected input/output devices, selects each, captures only after enable, responds to device unplug/permission failure, and records local state transitions.
- A private voice-test scenario proves the local processed signal is monitored and no voice packet or remote playback occurs on the second client.
- A two-client-plus-server scenario proves capture, push-to-talk or voice-activity gating, encode/send, server validation/routing, receive/decode/playout, local mute/block, and disconnect cleanup in the matching `events.jsonl` files.
- Mode `0` produces no transmitted frames; mode `2` reaches allowed room peers; mode `1` reaches eligible listeners at full volume through 5 m, fades linearly to zero at the configured far distance, and sends no frames at or beyond the cutoff. Include known positions on both sides of each boundary.
- A team scenario proves default cross-team voice and a configured restricted-team rule. A local mute and shared block scenario prove independent listener controls and consistent account block behavior.
- A report scenario returns a traceable incident ID, preserves reporter anonymity from the target/public, exposes the report only through authorized workflow, and includes no recording or transcript.
- Under controlled loss, jitter, congestion, and many simultaneous speakers, report latency, loss, jitter, per-speaker bitrate, CPU, audio underruns, client frame-time distribution, and authoritative tick cost with sample count, build/generation, run IDs, and journal paths. Voice must not violate existing gameplay tick and frame budgets.
- Human listening review confirms intelligibility, clipping protection, spatial attenuation, test-mode equivalence, and understandable status indicators. Record human acceptance separately from machine-readable runtime proof.

## Non-goals

- Generated NPC dialogue, synthetic player voices, or voice impersonation.
- Automatic voice recording, public voice archives, background transcription, or automated moderation verdicts.
- A new social scoring, vote-penalty, or enforcement policy.
- A second mixer, device registry, chat system, account block list, or network transport.
- Claiming voice chat is implemented or available on a platform before pinned runtime and human listening acceptance.

## Related specifications

- [In-game chat](ingame-chat.md)
- [Networking](../networking/networking.md)
- [Moderation](../moderation/moderation.md)
- [Terminal commands](../../architecture/terminal-commands/terminal-commands.md)
- [Hot audio contract](../../architecture/live-development/hot-audio-contract.md)
- [Voice chat capability issue](../../issues/capability/20261128-implement-real-time-player-voice-chat-with-proxi-issue-29b3665c767d.md)
