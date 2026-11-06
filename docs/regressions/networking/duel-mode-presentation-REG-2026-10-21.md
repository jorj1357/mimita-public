# Duel mode presentation packet routing

Time created: 2026-10-21T08:47:00Z
Time last updated: 2026-10-21T08:47:00Z

Status: FIXED IN SOURCE; LIVE CONFIG APPLICATION CONFIRMED

Related specification: `docs/specs/graphics/graphics-presentation.md`
Related changelog: `docs/changelog/2026-10-21/20261021_084700-duel-damage-numbers.md`

## Observed

In standard Duel, the JSON explicitly sets `presentation.damage_numbers` to
`false`, but damage numbers remained visible.

## Expected behavior

Every community mode's `DuelStatePacket` reaches the shared client mode
presentation owner. Explicit `presentation.damage_numbers: false` suppresses
damage numbers in both locally predicted and server-confirmed hit paths.

## Wrong code

`src/network/multiplayer-tick.cpp` routed `matchMode == "duel"` only to
`DuelQueue::onDuelState()`. The generic JSON mode presentation is applied by
`CommunityMatchClient::onState()`, so Duel never installed the explicit mode
override. The mode JSON parsed correctly; its client application boundary was
skipped.

## Fix

Pass every accepted community `DuelStatePacket` to
`CommunityMatchClient::onState()`. Also pass `duel` packets to
`DuelQueue::onDuelState()` to preserve queue-specific behavior.

## Proof

The live dev-loop run `20261021_084608` on build 2733 wrote
`client.gamemode.presentation.applied` from `CommunityMatchClient::onState`
with `mode: "duel"` and `damage_numbers: false`. The same event reported
the configured 0.1 blood amount/lifetime and 0.1 decal alpha/lifetime. This
confirms JSON-to-client-profile application; visual confirmation that no
number appears after an actual hit remains a human acceptance item.
