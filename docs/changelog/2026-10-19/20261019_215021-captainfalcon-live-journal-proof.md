# Gold example for live journal evidence

- Local time: 2026-10-19 17:50 EDT (UTC-04:00)
- UTC: 2026-10-19 21:50 UTC
- Branch/starting commit: `20261011zt` / `35c9b12a`
- Request: preserve the Falcon animation investigation as a gold example of discovering and proving a runtime bug through the live journal.

## Changes

- Added `docs/gold/2026-10-19-captainfalcon-walk-live-journal-proof.md`.
- The new document includes the user's quoted response, live-run path and event sequence evidence before/after the config edit, the smallest fix, a reusable investigation pattern, and a clear statement that journal state does not prove visual acceptance.
- Before: there was no gold document for this investigation.
- After: the new document opens with `# Gold example: find an animation mismatch in the live journal` and explains the observable decision, its cause, fix, and runtime confirmation.

## Review and validation

- Followed `docs/ROUTER.md` to `docs/gold/` and read the related examples `docs/gold/2026-10-06-ragdoll-events-jsonl-live-debugging-reference.md` and `docs/gold/2026-09-12-live-jsonl-ai-observability.md`.
- Applied `docs/skills/documentation-checker-v1.md`: the document declares purpose, audience, and limits; it links only to the real run journal and source/config paths discussed.
- Checked `docs/architecture/time-and-formatting/time-and-formatting.md` and used the required UTC timestamp in the filename and changelog, with EDT recorded for the user-provided time.
- Rechecked the supporting run journal `logs/2026-10-19/20261019_213626/events.jsonl`: sequence 102449 selected `captainfalcon_run`; sequence 102519 reloaded avatar animation profile revision 3; sequence 102529 selected `captainfalcon_walk` while moving.
- `docs/doc-review-09-03-2026.md` is absent in this checkout; it was not needed to add an example under the existing `docs/gold/` route.
- Documentation-only addition. No build or runtime action was needed; the source run is cited as historical evidence, and human visual acceptance remains unclaimed.
