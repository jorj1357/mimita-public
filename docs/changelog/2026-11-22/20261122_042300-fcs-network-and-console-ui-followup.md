# FCS network and console UI follow-up

## Request

Continue investigating FCS join/reconnect and in-game GUI button issues. Use
dev-loop agent-console commands for GUI testing; do not use Computer Use for
MiMITA until the user changes that preference.

## Changes

- Added the user's console-only GUI testing preference and the scan-to-click
  verification requirement to the gold reference.
- Corrected agent mouse positioning in `src/main-systems.cpp` by removing the
  extra OS cursor reposition after the host cursor API.
- Changed NPC respawn fallback handling in `src/network/multiplayer-tick.cpp`
  to preserve an already-loaded replica model while an async replacement
  avatar is pending.
- Added a bounded `ui.mouse_edge_rearmed` diagnostic in `src/gui/ui-system.cpp`
  to identify a later interactive UI pass that advances the mouse history
  before the intended button consumes the press.
- Added a two-owning-frame click-edge grace budget, consumed by the first
  accepting button or text field, for input whose target UI is evaluated on the
  following frame.
- Updated `postAgentMousePosition` in `src/main-systems.cpp` to publish the
  agent cursor position directly to `InputRouter` after the host cursor move,
  before injecting the mouse button state.
- Changed the NPC respawn path in `src/network/multiplayer-tick.cpp` to avoid
  reapplying an avatar when the network avatar identity is unchanged.
- Regenerated/checked packet registry metadata; the generator reported current
  outputs unchanged after the earlier failed codegen check.
- Appended runtime findings to networking issue `issue-cf9cd77eb056`.

## Build evidence

- EXE build request completed with the same existing artifact; it did not
  include `src/main-systems.cpp`, which is game-DLL owned.
- Candidate game-DLL generation 451 was accepted by the no-op path for the
  then-current revision. It was pinned in the diagnostic run below.
- Candidate generation 454 included UI edge grace, generation 455 added direct
  InputRouter cursor synchronization, and generation 456 added the same-avatar
  NPC respawn guard.
- Generation 456 build attempt
  `game-dll-20261122T043601681032Z-fc1d05` published successfully in about 185
  seconds end to end. Its DLL path is `build/game/generation-456/game-g456.dll`.
- No manual compiler or second dev-loop was used.

## Runtime evidence

- Run `20261122_041358`, mode 45 / Fun Counter Strike on `dust2cyberiav4`,
  pinned EXE artifact
  `2026-11-22T02:37:06.972018Z:c94488e6bcdca96b94e61c2985a5ef3f30bb6b70250e32f1cb1bd36b8401e344`
  and candidate game DLL generation 451. Client journal:
  `logs/2026-11-22/20261122_041358/client-01/events.jsonl`.
- Over roughly three minutes, the client continued receiving/applying
  authoritative snapshots for 20 entities (19 remote NPCs). Sampled snapshot
  inter-arrival was about one second and the cumulative missed counter reached
  75. No reconnect-scheduled event was observed. The sample frame window showed
  `Network::MpTick` max 2.28 ms; this does not exclude stalls outside that
  window, and networking acceptance remains open.
- In-game console scan reported `resumeButton`; the agent-console press targeted
  its scanned center `(512.000, 231.822)`. `ui.mouse_transition` recorded a
  press edge, and `ui.button_input` saw the same `resumeButton` hovered, but
  rejected it with `mouse_click_edge_missing`. The pointer target mismatch is
  corrected; edge propagation is still failing. The test run was stopped via
  dev-loop after collecting evidence.
- Run `20261122_043041`, mode 45 / Fun Counter Strike on `dust2cyberiav4`,
  pinned to generation 455, verified the complete agent-console GUI action:
  `agent.gui_scan` found `resumeButton`; the down command targeted
  `(512.000, 231.822)`; and `ui.button_input` recorded
  `element_id=resumeButton`, `clicked=true`, `raw_hovered=true`. The
  `ui.mouse_edge_rearmed` event also confirmed the held press crossed an owning
  UI frame. A generation 456 GUI attempt did not count as a click proof because
  the pause menu had already closed before the down command; the resulting
  `agent.gui.button.failed` was preserved in its journal.
- Runtime comparison under a similar FCS NPC lifecycle load: generation 455
  run `20261122_043041` lasted about 343 seconds and recorded 65 server NPC
  spawn resolutions / 103 transform-epoch changes. It had three networking
  slices above 600 ms, including `Network::RemoteEntities` at 685.7 ms and
  `Network::SnapshotApply` at 623 ms with 304 NPC life-reset/avatar-apply calls
  in one captured window. Generation 456 run `20261122_043926` lasted about
  317 seconds and had 63 spawn resolutions / 101 epoch changes, with no
  networking frame slice above 250 ms. Its two large frame slices were
  startup state/replay initialization and `PlayerAnimation`, outside
  networking. Latest sampled snapshot gaps were about one second in both runs;
  cumulative missed counters were 103 and 31, respectively. Neither run
  scheduled a reconnect. This is promising runtime evidence that avoiding
  same-avatar reapplication reduced NPC snapshot stalls under a comparable FCS
  respawn load, but it does not prove all network paths or the health-rebase
  fix.

## Remaining proof

Repeat the successful same-widget console click on a fresh generation 456 run,
reproduce the health-clock rebase transition, and exercise same-room create,
leave, and rejoin paths. Keep the networking issue open until those paths have
repeated pinned-runtime proof.
