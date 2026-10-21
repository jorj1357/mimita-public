# Actor preset movement ownership

- Request: Clarify how to make a Sandbox NPC use Juggernaut movement and explain the movement/behavior profile distinction in the actor-preset specification.
- Status: `PASS_WITH_HUMAN_REVIEW` for the documentation clarification. No Sandbox gameplay configuration was changed and no runtime behavior is claimed.
- Time: 2026-10-20T23:30:19Z (2026-10-20 19:30:19 EDT, America/New_York).
- Branch: `20261011zt`.
- HEAD at start: `0b5145b2`.
- Pre-existing working-tree changes: multiple unrelated user edits were present. `config/gamemodes/sandbox.json` already changed its `npc_behavior_profile` selection to `nervousbad`; the actor-preset specification already had the user's informal Sandbox note at lines 1-11. Neither was changed in this task.

## Finding

- Severity: medium.
- Type: unclear ownership / partial implementation.
- Specification: actor presets may select a movement preset; NPC behavior profiles tune NPC decisions and combat.
- Code path: `config/roles.json` → `GamemodeTeam.role` / `npc_role` in gamemode JSON → `serverResolveActorSpawnProfile()` → `ActorSpawnProfile.movementPreset` and `behaviorProfileId`.
- Actual behavior: role `movement_preset` selects physical movement. `behavior_profile` and mode `npc_behavior_profile` select NPC decision/combat tuning. Actor-preset references are not yet a complete field-by-field merge into a referencing role.
- Recommended action: use an NPC-specific role with `movement_preset: "juggernaut"`, reference `juggernaut_arcade` if its presentation is wanted, and assign it to the Sandbox NPC team. Use a narrower role if Juggernaut health and weapons are not wanted.

## Files and exact edits

- `docs/specs/actor-presets/actor-presets.md`, lines 21-68: added a formal explanation of movement preset versus NPC behavior profile, the current role-level movement requirement, and a JSON example using an NPC-only team and Juggernaut movement. Before this addition, the heading `# Actor Presets, Roles, and Game Modes` was followed by the status bullet. After it, the new heading is `## Movement presets and NPC behavior profiles are different`, followed by the role and gamemode examples. The user's informal note at the file top was preserved.
- This changelog is new; there was no prior text. It records this documentation-only update and its evidence boundary.

## Documents and review

- Read `docs/ROUTER.md`, `docs/specs/actor-presets/actor-presets.md`, `docs/specs/gamemodes/gamemodes.md`, `docs/specs/npc-behavior/npc-behavior.md`, `docs/architecture/player-npc-systems/npc-movement.md`, `docs/architecture/json-configuration/json-configuration.md`, `docs/workflows/runtime-scenario-validation.md`, `docs/skills/spec-behavior-review-v1.md`, `docs/operations/task-completion/task-completion.md`, and `docs/architecture/time-and-formatting/time-and-formatting.md`.
- `docs/doc-review-09-03-2026.md` was selected by the router but is absent from this checkout.
- Focused skill: `docs/skills/spec-behavior-review-v1.md` — applied; clarified the spec/code boundary without claiming runtime acceptance.

## Validation and remaining evidence

- `git diff --check -- docs/specs/actor-presets/actor-presets.md` reported trailing whitespace only on the pre-existing informal note at lines 1-11; the newly added section had no whitespace findings.
- No build, tests, or runtime scenario were run because this task changed documentation only.
- Human review is still needed to decide whether Sandbox should reuse the full Juggernaut role (including its health and weapons) or use a narrow movement-focused NPC role. The example documents the narrow-role option; it does not claim that Sandbox currently uses it.
