# Creation, Sharing, and Distribution

**Purpose:** define how people can create, save, share, and discover MiMITA
experiences without requiring a central service or pressuring people to post.
**Audience:** game, creator-tool, networking, and community contributors.
**Status:** product direction and staged specification.
**Does not:** promise a social network, automatic posting, monetization, or a
federated storage system in the first release.

## Product intent

MiMITA should be enjoyable alone and become more social when other people join.
Sharing should follow from making something a person likes, not from login
streaks, view-count rewards, referrals, or pressure to publish. The project
should remain usable when its central website or database is unavailable.

The game should eventually let a player host or join a session using the
networking already inside MiMITA, then make a replay or video from that
experience. A newcomer should be able to get from opening the game to a
shareable creation quickly; the initial usability target is a five-minute path
for a simple replay clip, subject to measurement with new users.

## Near-term creation and sharing

- Keep replay capture, editing, and export inside the MiMITA application. See
  [Replay](../replay/replay.md) and the future
  [Video Editor](../creator-tools/video-editor.md) specification.
- Let users save and export locally without signing in, connecting to a
  website, or uploading anything.
- Make external sharing an explicit user action. Show what will be exported
  and where it will go before opening an external site or upload flow.
- Keep publishing optional. Do not add automatic room-code posts, automatic
  livestreams, recurring upload reminders, view-count rewards, or login
  rewards as default behavior.
- Make costs, licenses, required services, and export limitations visible
  before a user commits to an effect or export.

## Longer-term discovery and distribution

Possible future work includes user-published replay or game pages, searchable
community-made content, portable room invitations, federated discovery, and
volunteer-hosted sessions or storage. These are separate capabilities with
their own privacy, abuse, moderation, availability, and licensing
requirements. They are not prerequisites for local creation or direct
connection by room code.

No cryptocurrency, tradable hosting reward, or real-world-value token is
approved by this document. Any such proposal requires a separate public
decision covering legal, financial, abuse, and sustainability consequences.

## Ownership and dependencies

- Replay recording and export own replay data and render fidelity.
- Creator tools own local editing projects and export settings.
- Networking owns sessions and room discovery.
- Moderation owns conduct reporting and enforcement for public spaces.
- Distribution owns user-controlled publication and content discovery.

Do not couple a basic local export to a central account, database, or social
network. Do not move credentials, browser profiles, or platform tokens into
game configuration or shared content.

## Accessibility and quality

The sharing flow must be usable with keyboard and controller navigation where
the platform allows it. Important export status must be readable without
sound, color alone, or a high frame rate. Export runs must not freeze gameplay
or editing input.

Measure the time and errors for a first-time user to select a replay, make a
simple edit, export it, and understand the resulting file. Treat missed
controls, unclear export failures, and unexplained costs as usability defects.
Do not optimize the flow for views or posting frequency.

## Future sync and storage

Local files are the first durable copy of personal creations. Multi-device
sync, federated storage, volunteer hosting, and content discovery need a
separate design for identity, ownership, moderation, integrity, and recovery.
Until then, local scores and edits are not globally verified or ranked.

## Related specifications

- [MiMITA vision and principles](../../architecture/vision-mentality/vision.md)
- [Accessibility and portability](../../architecture/accessibility/accessibility.md)
- [Replay](../replay/replay.md)
- [Video Editor](../creator-tools/video-editor.md)
- [Live voice generation](../live-voice-generation/live-voice-generation.md)
- [Persistent data and rewards](../data-saving/data-saving.md)
