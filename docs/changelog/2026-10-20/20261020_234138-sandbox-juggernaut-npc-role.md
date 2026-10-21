# Sandbox Juggernaut movement role

- Request: Assign Sandbox NPCs to an NPC-only team whose members use the Juggernaut movement preset.
- Status: `PASS_WITH_HUMAN_REVIEW`. Configuration and role resolution were updated. The running dev session accepted the Sandbox mode command but did not produce evidence that the match transitioned or assigned the new role; runtime behavior is therefore not claimed as confirmed.
- Time: 2026-10-20T23:41:38Z (2026-10-20 19:41:38 EDT, America/New_York).
- Branch: `20261011zt`.
- HEAD at start: `0b5145b2`.
- Pre-existing working-tree edits: unrelated user changes were present across configuration, source, and documentation files. The earlier edit in `config/gamemodes/sandbox.json` selecting `npc_behavior_profile: "nervousbad"` and the informal note at lines 1-11 of `docs/specs/actor-presets/actor-presets.md` were preserved. Other pre-existing edits were not touched.

## Implementation

- Added role `sandbox_juggernaut_npc` in `config/roles.json` with `actor_preset: "juggernaut_arcade"` and `movement_preset: "juggernaut"`. It intentionally omits health and weapon overrides so Sandbox defaults remain in effect.
- Replaced Sandbox's empty `team_names` value with an NPC-only team: `players: false`, `npcs: true`, unlimited capacity (`0`), and role `sandbox_juggernaut_npc`.
- Kept the role's movement preset explicit because current runtime resolution applies role movement directly; actor-preset references are not yet a complete field-by-field role merge.

## Exact file edits

- `config/roles.json`, lines 3-10: new role; there was no prior text. Exact addition:

  ```json
  {
    "id": "sandbox_juggernaut_npc",
    "display_name": "Sandbox Juggernaut NPC",
    "team": 0,
    "team_id": "sandbox_npcs",
    "actor_preset": "juggernaut_arcade",
    "movement_preset": "juggernaut"
  },
  ```

- `config/gamemodes/sandbox.json`, lines 13-22: replaced `"team_names": [],` with the team block below. The prior user edit to `npc_behavior_profile` remains unchanged.

  ```json
  "teams": [
    {
      "id": "sandbox_npcs",
      "display_name": "NPCs",
      "capacity": 0,
      "role": "sandbox_juggernaut_npc",
      "players": false,
      "npcs": true
    }
  ],
  ```

- `docs/specs/actor-presets/actor-presets.md`, lines 43-50: updated the prior role example to include the team identity and display name, and removed the example-only `behavior_profile` so the example matches the movement-only role. Before, it contained `"id": "sandbox_juggernaut_npc"`, `"actor_preset": "juggernaut_arcade"`, `"movement_preset": "juggernaut"`, and `"behavior_profile": "juggernaut_limited"`. After, it contains the exact role shown above.
- This changelog is new; there was no prior text.

## Documents and focused review

- Read the actor-preset, gamemode, movement, NPC behavior, JSON configuration, networking, debug logging, runtime validation, terminal-command, build/EXE, live-development, and task-completion guidance, plus the focused `docs/skills/spec-behavior-review-v1.md` and `docs/skills/logging-checker-v1.md`.
- The router-selected `docs/doc-review-09-03-2026.md` and `docs/regressions/regressions-v1.md` are absent from this checkout.
- `spec-behavior-review-v1`: `PASS_WITH_HUMAN_REVIEW`; role and team ownership match the current resolver, but runtime roster assignment was not proven.
- `logging-checker-v1`: existing `network.server-command.result-sent` and `npc.roster-profile-resolved` diagnostics provide command and actor-role evidence; the run contained no Sandbox role event after the switch request.

## Validation evidence

- Configuration parse: PowerShell `ConvertFrom-Json` parsed `roles.json` and the comment-stripped `sandbox.json`; resolved values were `sandbox_npcs` → `sandbox_juggernaut_npc` → `actor_preset=juggernaut_arcade`, `movement_preset=juggernaut`, with players disabled and NPCs enabled.
- Dev-loop build: published build 2695 successfully. Build 2694, used by the already-running server, and build 2695 have the same SHA-256 `65632E84AEE818B9C41A96D9814B6BDED4351F936364E1285CB76BC53189D3DB`; no C++ source changed.
- Exact executable identity: `.dev/builds/2695/mimita.exe --versioninfo` reported `EVENTS_JSONL_PATH=logs/10-20-2026/20261020_193822/events.jsonl`.
- Runtime session: reused the existing dev-loop run `20261020_233803`, journal `logs/2026-10-20/20261020_233803/events.jsonl`. `modestartnow 1` was accepted twice with status `started now: sandbox (3-2-1)`, but the server's `active_mode` remained `juggernaut`; no `gamemode.match.configured` event for Sandbox and no `npc.roster-profile-resolved` event for `sandbox_juggernaut_npc` appeared. The running server was left alive.
- `git diff --check` found trailing whitespace only in the pre-existing informal note at lines 1-11 of the actor-preset spec; the new configuration and example produced no whitespace findings.
- No tests were added or run. Human confirmation of slow movement remains open because the live session never reached a logged Sandbox roster assignment.
