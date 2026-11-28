# Expanded voice chat specification

## Summary

Expanded `docs/specs/ingame-chat/voice-chat.md` into a behavior contract for real-time player voice chat. The specification now covers device discovery and privacy, server-authoritative modes and team routing, proximity gain and recipient cutoffs, push-to-talk and voice-activity pickup, dBFS HUD metering, private processed-voice testing, bounded mic gain, optional client-side effects, moderation/report mechanics, command contracts, structured events, failures, and multiplayer acceptance.

## Moderation boundary

Added privacy-safe incident IDs and report status lookup. Local voice mute is immediate; account blocking uses the shared social block owner. Proposed reviewer commands remain subject to the unresolved moderation policy for reviewer permissions, retention, and adjudication. The specification does not introduce automatic scores, voice recordings, transcripts, or automatic penalties.

## Issue coordination

The canonical capability record is `issue-29b3665c767d`, linked from the specification. A duplicate identical intake, `issue-6e082da43cac`, was closed with an exemption that applies only to duplicate tracking; it makes no implementation or runtime-proof claim. Both user requests are linked to the canonical issue.

## Validation

Documentation review only. Relative links resolved and `git diff --check` passed after removing the extra trailing blank line. No code was changed; no build, software test, or runtime behavior was run. The feature remains specified but unimplemented and unproven.
