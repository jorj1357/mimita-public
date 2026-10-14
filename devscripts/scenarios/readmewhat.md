# Avatar monster spawn, movement, animation, and offset scenario

This is the repository scenario document referenced by
`C:\mimita-v9\docs\specs\dev-loop\dev-loop.md`.

The JSON below tests both paths involved in the reported bug: a monster avatar
used immediately after spawn, and the same monster reloaded after Jason. The
fresh `events.jsonl` must be inspected for command delivery, avatar
application, model policy, pose samples, and assertions; launch arguments alone
are not a passing result.

```json
{
  "id": "monster_avatar_spawn_walk_offsets_v1",
  "description": "Compare Stabber and Boomer monster avatars at initial spawn and after a Jason-to-monster rebind",
  "launch": {
    "profile": "npc-navigation",
    "launch_mode": "10",
    "visibility": "visible",
    "fresh_build_required": true,
    "fresh_journal_required": true
  },
  "setup": [
    {"command": "avatar.load", "args": ["monsterstabber"], "label": "stabber_initial"},
    {"command": "agent.snapshot", "args": ["stabber_spawn_initial"], "label": "record_initial_stabber_offsets"}
  ],
  "steps": [
    {"id": "load_jason_control", "command": "avatar.load", "args": ["jason"], "label": "load_known_working_control_avatar"},
    {"id": "jason_control_snapshot", "command": "agent.snapshot", "args": ["jason_control"], "label": "record_control_bind_and_pose"},
    {"id": "reload_stabber_after_jason", "command": "avatar.load", "args": ["monsterstabber"], "label": "rebind_stabber_after_jason"},
    {"id": "stabber_rebind_snapshot", "command": "agent.snapshot", "args": ["stabber_after_jason"], "label": "record_rebound_stabber_offsets"},
    {"id": "stabber_walk_forward", "command": "agent.move", "args": [1, 0, 120], "delay_ms_after": 500, "label": "walk_forward_after_stabber_rebind"},
    {"id": "stabber_after_walk", "command": "agent.snapshot", "args": ["stabber_after_walk"], "label": "record_rebound_stabber_animation_and_offsets"},
    {"id": "load_boomer_control", "command": "avatar.load", "args": ["monstertest1"], "label": "load_boomer_control_avatar"},
    {"id": "boomer_control_snapshot", "command": "agent.snapshot", "args": ["boomer_control"], "label": "record_boomer_bind_and_pose"},
    {"id": "boomer_walk_forward", "command": "agent.move", "args": [1, 0, 120], "delay_ms_after": 500, "label": "walk_forward_with_boomer"},
    {"id": "boomer_after_walk", "command": "agent.snapshot", "args": ["boomer_after_walk"], "label": "record_boomer_animation_and_offsets"}
  ],
  "assertions": [
    {
      "type": "command_delivery_complete",
      "for_each_step": true,
      "required_events": ["dev-loop.command.request", "terminal.command.received", "terminal.command.resolved", "terminal.command.completed"]
    },
    {"type": "avatar_policy", "step": "stabber_spawn_initial", "avatar": "monsterstabber", "body_frame_policy": "player_canonical", "body_transform_source": "model"},
    {"type": "avatar_policy", "step": "jason_control", "avatar": "jason", "body_frame_policy": "authored"},
    {"type": "avatar_policy", "step": "stabber_after_jason", "avatar": "monsterstabber", "body_frame_policy": "player_canonical", "body_transform_source": "model"},
    {"type": "avatar_policy", "step": "boomer_control", "avatar": "monstertest1", "body_frame_policy": "player_canonical", "body_transform_source": "model"},
    {"type": "position_delta_nonzero", "from": "stabber_after_jason", "to": "stabber_after_walk", "minimum_distance": 0.01},
    {
      "type": "pose_sample_present",
      "steps": ["stabber_spawn_initial", "jason_control", "stabber_after_jason", "stabber_after_walk", "boomer_control", "boomer_after_walk"],
      "required_fields": ["fixed_tick", "active_animation", "camera_forward", "physicalBody.parts", "world_matrix"]
    },
    {
      "type": "offset_rebind_comparison",
      "before": "stabber_spawn_initial",
      "control": "jason_control",
      "after": "stabber_after_jason",
      "compare_parts": ["head", "torso", "leftArm", "rightArm", "leftLeg", "rightLeg"],
      "report_only": true,
      "reason": "identify whether initial spawn and avatar rebind use different bind frames"
    },
    {"type": "avatar_command_assert", "step": "stabber_after_walk", "assert": "Stabber animation and offsets remain stable after rebind", "tolerance": 0.01},
    {"type": "avatar_command_assert", "step": "boomer_after_walk", "assert": "Boomer animation and offsets remain stable during movement", "tolerance": 0.01}
  ],
  "journal_review": {
    "path_source": "dev-loop status events_path",
    "never_guess_path": true,
    "required_event_order": ["dev-loop.command.request", "terminal.command.received", "terminal.command.resolved", "avatar.model.transform-source", "avatar.model.raw-node", "avatar.model.normalized-node", "avatar.pose.sample", "terminal.command.completed", "avatar.command.assert"],
    "first_divergence_rule": "report the first missing or incorrect event before changing offsets or animation math",
    "compare": ["avatar name and model path", "body_frame_policy and body_transform_source", "raw and normalized node matrices", "physicalBody.parts world matrices", "active animation and fixed tick", "weapon grip and muzzle if equipped", "spawn_initial versus after_jason offset deltas"]
  }
}
```

## How to run and inspect it

The intended future command is:

```text
python devscripts/dev-loop.py --scenario-file C:\mimita-v9\devscripts\scenarios\readmewhat.md
```

Until the scenario-file control request is implemented, this document is a
test contract and must be executed through the currently supported startup or
terminal command path. Do not call it passed until the fresh journal contains
the required event chain and the initial-monster versus Jason-rebind offsets
have been compared.

The screenshot symptom is recorded as a hypothesis, not a conclusion: the
initial monster spawn appears offset incorrectly, while loading Jason and then
reloading the monster appears to repair placement. The first owners to check
are initial avatar application, physical-body bind initialization, and avatar
rebind cleanup. Do not compensate with a permanent per-avatar offset until the
journal identifies which owner differs between those paths.
