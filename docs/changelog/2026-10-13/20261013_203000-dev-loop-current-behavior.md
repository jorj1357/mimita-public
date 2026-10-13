# Dev-loop current behavior specification

- Timestamp: 2026-10-13T20:30:00Z
- Branch: 20261011zt
- Result: PASS_WITH_HUMAN_REVIEW
- Runtime processes were not stopped or restarted.

## Change

Updated docs/specs/dev-loop/dev-loop.md to accurately distinguish the current
devscripts/dev-loop.py source behavior from the desired contract.

Added a current implementation snapshot covering:

- source-change/build/publish behavior;
- [1] selection behavior;
- .dev/server-owner.json creation and Windows adoption;
- Popen versus adopted-server health tracking;
- replacement and stop-result events;
- current status fields;
- loopback firewall policy and emitted result values;
- persistent server behavior;
- current limitations around pre-readiness crashes, Windows-only adoption,
  marker validation, Firewall rule inspection, and reserved result states;
- the fact that an already-running daemon does not hot-reload dev-loop.py;
- the observed current daemon/build/status boundary.

Updated acceptance wording to include FIREWALL_POLICY_BLOCKED and
EXTERNAL_BIND_REQUIRES_FIREWALL_REVIEW.

## Evidence

- Read-only status showed profile npc-navigation, mode 10, server build 2202,
  builds_match=false, and a fresh journal path.
- The new status fields were null in that snapshot, consistent with the active
  daemon still using an older script version.
- git diff --check passed with only the normal LF/CRLF warning.
- No build or runtime restart was performed.

## Preserved unrelated edits

The following worktree changes were not made by this documentation update and
were preserved:

- config/analytics.json
- config/audio/music-settings.json
- config/gamemodes/zombie_tower.json
- devscripts/dev-loop.py
- src/gui/ui-system.cpp
- src/network/server-gamemode.cpp

The devscripts/dev-loop.py entry is included because it was an existing
implementation change being documented, not changed during this specification
update.

