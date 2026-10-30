# In-Game Voice Chat

**Purpose:** describe player-to-player voice communication inside MiMITA.
**Audience:** networking, audio, moderation, accessibility, and game-mode
contributors.
**Status:** desired behavior; platform, moderation, and runtime acceptance
remain open.
**Does not:** specify generated character dialogue or promise voice support on
every platform.

## Desired experience

Voice chat should let players talk naturally in a shared room. Modes may choose
open room voice or proximity voice, where distance and world occlusion affect
what a player hears. A player's ability to understand a match must not depend
only on hearing voice.

## Controls and user choice

- Provide clear mute, unmute, and per-player mute controls.
- Show who is speaking and whether their voice can be heard locally.
- Provide an obvious room-level voice disable setting.
- Allow a mode to select open-room or proximity behavior without creating a
  separate chat system.
- Keep text chat available independently.

## Safety and accessibility

Public voice requires moderation controls for reports, muting, and enforcement
under the moderation specification. Do not automatically record or publish
voice conversations. Any future recording must be explicit and disclosed to
participants.

Provide captions or text equivalents where technically available, independent
voice volume, and low-bandwidth behavior. Support push-to-talk and configurable
input where the platform allows it.

## Distinction from generated voices

Player voice chat is live communication. Authored text-to-speech is a
character or creator asset governed by
[Live Voice Generation](../live-voice-generation/live-voice-generation.md).
Generated dialogue does not enter a voice-chat room unless the user explicitly
uses a supported share action.

## Acceptance

Test open-room and proximity modes, player mute/unmute, room disable, report
and moderation flow, text chat alongside voice, and behavior with captions
enabled or voice unavailable. Multiplayer and human listening review are
required before claiming acceptance.
