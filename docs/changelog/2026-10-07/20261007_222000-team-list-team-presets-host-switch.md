# Team listing, role-specific presets, and host team switching

Date: 2026-10-07

## Change

- `team_list` now enumerates the active gamemode's configured team objects,
  including display name, stable id, and capacity.
- Juggernaut Fighters use a dedicated actor preset with FOV 100 and `source`
  movement. Juggernauts retain FOV 50 and explicitly use `juggernaut`
  movement. Both role definitions reference their actor preset.
- A host may use team switching while a round is active or while the target
  team is full. A roster NPC is evicted first; if no NPC is available, another
  human is moved to spectator. Non-hosts retain the existing round lock and
  full-team rejection.

## Evidence

- Source and configuration changes are present in the working tree.
- Build and runtime evidence will be recorded separately after validation.
