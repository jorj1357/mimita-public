# Reconnect, Juggernaut Horde server consistency, and NPC target investigation

# Task

- Wake: `wake-564b18d1a287`, context `ctx-7b69ff7b24ef`
- Session agent: live-build-gui-timer-resource-audit
- Owner for findings: issue-cf9cd77eb056 (root-connection-reliability) and issue-b95574b63f29 (fcs-npc-live-fix)
- Summary: read-only trace of why the user's in-game Juggernaut Horde server can show one NPC with no Fighter/Juggernaut teams, why reconnect/lifecycle identity is inconsistent, and whether NPCs stop targeting the human. No source, build, or process was changed.
- Status: EVIDENCE_RECORDED, OPEN
- Date, time, timezone: 2026-11-21T23:03:27Z

# Pre-existing changes

- The shared checkout contains extensive pre-existing modifications from other agents. This session edited no source and reverted nothing.
- Files created by this session: this changelog only. Append-only issue discussion and coordination tasks were written through `agent-coord.py`.

# Requested behavior

Trace the whole path of creating and joining a Juggernaut Horde game, explain why reconnecting and server creation are inconsistent, and determine why NPCs reportedly stop choosing the human as a target. Record the first divergence with correlated evidence and route it to the existing canonical issues.

# Findings

- In-game Juggernaut Horde launch at 2026-11-21T22:41:48Z (client PID 24192 -> child server PID 20908, map `badhouse`, mode `jugger_horde`):
  - `network.gui-server.launch` recorded empty `client_events_jsonl` and `server_events_jsonl`; the child server wrote its events into `client-01/events.jsonl`.
  - `server.policy.startup` logged `gamemode="duel"` and `npc_count=1`.
  - Source: `src/gui/gui-main.cpp:390-411` passes `--gamemode` only when `settings.duelMode` and always passes `--npcs settings.startupNpcCount`; the binding default is 1 (`gui-main.cpp:266,304-307`, `online-menu.cpp:369-370`).
  - Community mode still took effect: `gamemode.match.configured` showed fighters capacity 64 and juggernauts capacity 4.
- The "only 1 NPC" symptom is the WAITING state before a team roster exists.
  - Tick 60: first mode packet `participant_count=0`, `client.gamemode.local-actor-identity` `found=false`, `team=-1`.
  - Tick 180: the roster created 63 fighter NPCs and 4 juggernaut NPCs (`gamemode.team-roster-npc.created`), and removed startup NPC 1000 as unrelated.
  - Separate run `20261121_215237`, server PID 29912, stayed at `network.mode.start` `stage=waiting`, `active_players=0`, `participants=0`, `npc_count=1` at tick 107694 (22:40:59Z); `npc.trace` showed id 1000 with empty role, team -1, PATROL, stuck.
  - Therefore a `jugger_horde` server with no active human client cannot show Fighters/Juggernauts; it keeps only the single generic startup NPC. The visible inconsistency tracks which server actually receives the client.
- Missing team identity has a concrete packet limit.
  - `src/network/schema/packet-gamemode.h:120-127` fixes the participant identity arrays at 64.
  - `src/network/server-gamemode.cpp:2055` clamps `participantCount=min(64,d.participants.size())`; `d.participants` is sorted at `:3628`.
  - A full horde is 1 human + 63 Fighter NPCs + 4 Juggernaut NPCs = 68, so the four highest-sorted Juggernaut ids fall outside the identity window; recipient fallback patches only the local actor into slot 63 (`:2120-2158`).
  - The body snapshot path is separate and did send 67 NPCs (client applied 66 `remote_npcs`). Server-side targeting is not gated by this packet cap.
- Target loss is state-dependent, not universal.
  - Before `team_pick 2`, no `npc.target-changed` had `target_actor=1`; juggernaut NPCs targeted fighter NPCs.
  - At tick 438 the host ran `team_pick 2`; `actor.team-assigned` set player 1 to team 1, role `juggernaut_mode_juggernaut`.
  - After that, 14 `npc.target-changed` events targeted actor 1 at 1.6-19.1 m between ticks 625 and 1130, all `module_scored`.
  - Candidate code includes players and NPCs at `src/network/server-npcs.cpp:2221-2244` and `:2287-2315`; stale players are excluded at `:2225/2291/2353`.
- Lifecycle identity remains split.
  - `run.json` names server-01 PID 27816 in `payload1/payload`, while child server PID 20908 ran `jugger_horde` in the same run.
  - `run.json` marks client-01 stale with `event_age_ms=442030` while the server was online.
  - Retained server PIDs 34328 and 29912 also appeared in reconciliation.

# Specification alignment

- Followed `docs/ROUTER.md` routing for issue intake, proof and verification, dev-loop, networking, and NPC behavior.
- Kept source, build, runtime, and visual claims separate. No build or runtime behavior change was claimed.

# Exact implementation changes

- None. Append-only issue evidence and two owner-review tasks only.

# Validation

- Build status: no build requested by this session; no compiler, worker, or second dev-loop was invoked.
- Runtime evidence: read existing run `20261121_224058` client/server journals, `20261121_215237`, and `20261121_011815`; no process was started or stopped.
- Visual evidence: none. The HUD/scoreboard was not captured, so the exact RED/BLUE label source remains unproven.
- Issue records updated: `issue-cf9cd77eb056` and `issue-b95574b63f29`.
- Tasks added to `issue-cf9cd77eb056`: `horde-effective-mode-launch`, `horde-participant-identity-window`.

# Measured evidence

- Child server built snapshots with 68 entities / 67 NPCs at ticks 181 and 247; client applied 66 `remote_npcs`; `client.npc.viewport-window` reported `in_view` 2-5.
- Client identity packets reported `participant_count=64` while 68 actors existed.
- `network.mode.start` waiting sample: tick 107694, `active_players=0`, `participants=0`, `npc_count=1`.
- `team_pick 2` at tick 438; 14 `target_actor=1` target changes from tick 625 to 1130.
- Confidence: high for launch-argument, WAITING-state, and 64-slot truncation findings; medium for the visual RED/BLUE label cause; high that targeting works after the team change in this run.

# Regression review

- Regression entry appended: no.
- Why: no source change and no behavior fix. The findings are routed as open issue evidence and tasks.

# Human acceptance

- No human visual or multiplayer acceptance was performed. Recommended next check: capture the live HUD/scoreboard during a full 68-participant horde, and capture player id/team, `connectionStale`, hp/downed state, server phase, and the next `npc.target-sense`/`npc.target-changed` at the moment the user reports target loss.
