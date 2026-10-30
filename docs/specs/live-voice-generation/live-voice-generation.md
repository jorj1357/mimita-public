# Live Voice Generation

**Purpose:** specify editable text-to-speech voices for character dialogue,
gameplay lines, and creator projects.
**Audience:** audio, animation, dialogue, accessibility, replay, and moderation
contributors.
**Status:** proposed capability; runtime and voice-provider choices require
implementation review.
**Does not:** replace live voice chat, grant permission to imitate a real
person, or require a network service for ordinary gameplay.

## Product goal

Players and creators can write text and have an in-game character speak it.
The generated line can play as spatial character audio and can optionally
appear as readable text above the character. Dialogue is editable and reusable
in replay and video projects.

## Authoring and playback

- Enter text in the in-game dialogue or creator interface.
- Choose an available voice preset and supported delivery controls such as
  speed, pitch, pause, emphasis, whisper, and emotion.
- Preview the line before adding it to an authored sequence or playing it.
- Choose whether the line is audible, displayed above the speaker, included in
  subtitles, or some combination of those outputs.
- Bind authored lines to an existing actor or animation event where needed.
- Record enough identity and timing information for replay and export to
  reproduce the line through the normal audio path.

Voice presets may describe qualities such as calm, nervous, angry, fearful,
soldier-like, or whispering. They must not imply endorsement, ownership, or
permission to impersonate a real person. Voice sources and licenses must be
documented before distribution.

## Runtime and privacy

The voice-generation interface is provider-neutral so platform-supported
on-device voices can be used where available. Generated text and audio stay
local by default. A future remote provider requires explicit user selection,
clear disclosure of what text is sent, and a separate privacy review.

If no compatible voice is available, keep the authored text usable as
subtitles and report that speech generation is unavailable. Do not block
gameplay or replay editing.

Generated lines use the shared audio routing and replay behavior. They do not
enter proximity voice chat unless the user deliberately chooses a supported
share action.

## Accessibility

Generated speech must have a text equivalent. Above-head text is optional,
readable at adjustable scale, and can be disabled independently of audio.
Provide volume and mute controls through the shared audio categories.

## Acceptance scenarios

1. Write a line, select a preset, preview it, and play it through an actor.
2. Disable speech but retain captions, then hide above-head text while keeping
   subtitles in the creator preview.
3. Save a line in a replay or video project and confirm timing and audio survive
   reopening and export.
4. Run without network access and verify that local text and any available
   local voice remain usable.

## Related specifications

- [Video Editor](../creator-tools/video-editor.md)
- [Replay](../replay/replay.md)
- [Voice chat](../ingame-chat/voice-chat.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
