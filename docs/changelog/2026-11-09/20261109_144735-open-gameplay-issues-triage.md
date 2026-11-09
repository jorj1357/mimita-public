# Open gameplay issue tracker cleanup

- Date: 2026-11-09T14:47:35Z
- Summary: Reorganized the open gameplay issue index around current status and next evidence needed.
- Status: Documentation updated; no gameplay or build-system behavior changed.

## Changes

- Replaced raw, untriaged notes at the top of `docs/specs/open-gameplay-issues.md` with the current P0/P1/P2 focus.
- Added separate entries for unexplained reconnects and the “Synchronizing map” loop; preserved the older freeze-then-reconnect report as a distinct issue.
- Recorded the user's report that reconnect begins after 2.5 seconds without authoritative snapshot traffic, with fresh journal verification as the next step.
- Marked networking hot-reload work P0 and called for verification of the recent modularization changes.
- Recorded the reported 17.8-minute cold-build measurement as unverified pending a matching machine-readable timing record.
- Reviewed the current multi-agent path requirements: lane worktrees are created beside the checkout, the shared scheduler defaults to `%LOCALAPPDATA%`, and dev-loop temporary files use the system temp directory. The requested repository-contained default is not implemented; the tracker calls for that review and redesign.
- Kept the requested header-size idea as an investigation lead rather than a fixed “10 symbols” rule, pending measured include fan-out.

## Evidence and validation

- Confirmed linked networking, map snapshot, build, cold-build, and visible-window documents exist.
- `git diff --check` passed for the tracker edit.
- No build, game launch, or runtime validation was performed. This change edits documentation only.