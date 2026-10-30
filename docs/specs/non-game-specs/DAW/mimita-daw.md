# MiMITA Music Creation and DAW Direction

**Purpose:** preserve the long-term direction for making and sharing music
inside MiMITA.
**Audience:** audio, creator-tool, collaboration, and accessibility
contributors.
**Status:** long-term product direction; not part of the first video-editor
implementation.
**Does not:** define a shippable DAW, promise real-time collaboration, or
replace the game's music playback specification.

## Vision

MiMITA should eventually let people compose, arrange, record, and edit music
inside the same application they use to play and create videos. The interface
should make a first musical idea easy to produce, while leaving room for
detailed editing and experimentation.

Live co-creation is a desired future capability: participants should be able
to work in one shared project rather than repeatedly sending whole files back
and forth. This depends on a reliable local project format, audio timing,
conflict handling, permissions, and network recovery.

## Staged direction

1. Make game music playback, local music selection, and replay/video audio
   dependable.
2. Define a portable local music project and simple editing workflow.
3. Prototype live collaboration with explicit project ownership, participant
   permissions, revision history, and recovery after disconnects.
4. Consider public music discovery or distributed sharing only after licensing,
   moderation, spam, and storage responsibilities are defined.

Do not make music sharing depend on Soulseek or any external service. It may be
studied as inspiration for peer discovery, but it is not an adopted protocol or
product dependency.

## Product principles

- Creation and local saving work without account rewards or pressure to publish.
- People can export their work in documented, portable formats where practical.
- Collaboration is opt-in and shows who can edit, listen, or export.
- Samples, presets, and voices expose their licenses and provenance.
- Resource use and latency are measured on low-capability devices.

## Related specifications

- [In-App Video Editor](../../creator-tools/video-editor.md)
- [Distribution](../../distribution/distribution.md)
- [Music playback](../../music/music.md)
- [Accessibility and portability](../../../architecture/accessibility/accessibility.md)
