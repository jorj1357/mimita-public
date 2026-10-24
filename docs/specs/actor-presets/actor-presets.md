2026 10 20 1924 sadnbox  trying to maek it so 
like
in dansbox u can namke it so the actor rpeset
has movement presets 
so 
u change it to nervosubad and it has mvoemetn preset
we shoudl put that in here  C:\mimita-v9\config\behavior-profiles.json 
like what movemnt profile that role uses
or like 
idk how it works need to note hwo it works in this spec 

2026 10 20 1958
 in the sandbox game right now the npcstill is m oving super fast, needs to be the juggernuat movement mod 

C:\mimita-v9\config\npc-difficulty.json

when u set it in this file it wokrs, but, need it to not just work there, needs to work  per actor role 

<!--
Purpose: Define reusable actor presets and how roles and game modes select them.
Audience: Designers and developers authoring actors, teams, and mode rosters.
Does not: Replace movement, weapon, avatar, animation, NPC behavior, or
gamemode specifications; those own their respective data and runtime behavior.
-->

# Actor Presets, Roles, and Game Modes

## Movement presets and NPC behavior profiles are different

`behavior_profile` in `config/roles.json` and a mode's
`npc_behavior_profile` select NPC decision/combat tuning from
`config/behavior-profiles.json`. They control choices such as target selection,
reaction, and attack cadence; they do not select physical movement speed.

`movement_preset` selects the physical movement configuration from
`config/movement/`. For example, the `juggernaut` movement preset supplies the
slow movement values. The role is the current runtime owner of this selection:
set `movement_preset` on the NPC role, then assign that role to the NPC team.
The actor preset may also declare a movement preset as its reusable baseline,
but current role resolution does not yet merge every actor-preset field into a
referencing role. Until that merge is implemented, repeat the movement preset
on the role when it must take effect.

For an NPC-only Sandbox assignment, the gamemode team can set `players` to
`false`, `npcs` to `true`, and name an NPC role. A speed-focused role can look
like this:

```json
// config/roles.json
{
  "id": "sandbox_juggernaut_npc",
  "display_name": "Sandbox Juggernaut NPC",
  "team": 0,
  "team_id": "sandbox_npcs",
  "actor_preset": "juggernaut_arcade",
  "movement_preset": "juggernaut"
}
```

```json
// inside config/gamemodes/sandbox.json
"teams": [
  {
    "id": "sandbox_npcs",
    "display_name": "NPCs",
    "capacity": 0,
    "role": "sandbox_juggernaut_npc",
    "players": false,
    "npcs": true
  }
]
```

This keeps the mode's NPC assignment separate from human actors. Reusing a
full Juggernaut gameplay role also applies that role's health, weapons, and
other settings; create a narrower role when only the movement/presentation
preset should change. `behavior-profiles.json` should not gain a
`movement_preset` field because it has a separate responsibility.

- Status: Target specification. The repository has a partial, transitional implementation; current behavior is identified separately below.
- Scope: Explain which JSON owns an actor's reusable setup, how a role applies it, and how a game mode assigns that role to humans and NPCs.

## 1. The simple picture

```text
GAME MODE JSON
chooses the teams and which role belongs to each team
        ↓
ROLE in config/roles.json
says what this actor is doing on this team in this mode
        ↓
ACTOR PRESET in config/actor-presets/<id>.json
provides the reusable actor setup
        ↓
shared actor systems
apply movement, health, avatar, weapons, presentation, and AI as applicable
```

An actor preset is the reusable recipe for the actor. A role is the actor's job and team assignment in a particular match. A game mode chooses which roles its teams use. The human or NPC controller is separate: a human-controlled Normal Zombie and an NPC Normal Zombie can share the same actor preset while using different roles and NPC behavior.

## 2. Ownership: one job per JSON

| Data | Owner | It answers |
|---|---|---|
| Mode and team rules | `config/gamemodes/<mode>.json` | Which teams exist, their capacity, phase/timing rules, and which role each team assigns. |
| Mode discovery | `config/onlinemodes.json` | Which mode is shown in the online/community mode picker. |
| Role | `config/roles.json` | Which team/job an actor is assigned, which actor preset it uses, and any explicit role-specific overrides such as behavior profile or spawn group. |
| Actor preset | `config/actor-presets/<preset-id>.json` | Reusable health, movement selection, weapon overlay, avatar/camera, presentation, and supported NPC policy settings. |
| Movement values | `config/movement/*.json` | The physics values selected by a `movement_preset`; an actor preset refers to an ID instead of copying them. |
| Weapons and ammo sets | `config/weapons.json`, `config/weaponsets.json` | Weapon behavior and available inventory. A role or preset references these definitions. |
| NPC decision profile | `config/behavior-profiles.json` and supported actor-preset NPC policy | How an NPC chooses actions. It does not decide the mode's objective or team membership. |
| Avatar shape and clips | `config/avatar.json` or the selected avatar's files, plus `assets/avatars/<avatar>/animations.json` | Which model parts exist and how they animate. An actor preset selects an avatar; it does not copy the rig or animation clips. |
| Map encounters | Map configuration / map entities | Where a role or monster group spawns, what activates it, and how many actors it creates. A monster preset does not decide where the map places it. |

`config/onlinemodes.json` makes a mode selectable; its matching file under `config/gamemodes/` supplies the mode's actual rules. A mode must not duplicate every actor's health, loadout, movement, and appearance inline when a role and actor preset can be referenced.

## 3. How a mode assigns a role

The target team entry names a role ID from `config/roles.json`:

```json
{
  "id": "humans",
  "display_name": "Humans",
  "capacity": 4,
  "role": "zombie_tower_human",
  "spawn_group": "human_spawn"
}
```

The role then references its reusable actor preset:

```json
{
  "id": "zombie_tower_human",
  "display_name": "Zombie Tower Human",
  "team": 0,
  "team_id": "humans",
  "actor_preset": "zombie_tower_human",
  "weapon_set": "zombie_tower_humans",
  "starting_weapon": "revolver",
  "spawn_group": "human_spawn"
}
```

These snippets illustrate the intended relationship; the referenced Zombie Tower role, weapon set, and preset are not all present yet. `GamemodeTeam.role` is parsed today, but Zombie Tower's current `config/gamemodes/zombie_tower.json` does not assign team roles.

A team role answers “what role does this team receive?” It does not mean every actor on that team must be the same controller. The match roster can fill human slots with humans and missing slots with NPCs while applying the appropriate player or companion role.

## 4. Actor preset contents

An actor preset may select or tune only settings with an existing owner. Its reusable fields can include:

- **Identity:** stable `id`, schema version, and display name.
- **Health:** spawn and maximum health, where the spawn lifecycle supports them.
- **Movement:** a named `movement_preset` and supported movement/navigation policy. Movement formulas and physical values stay in the movement system/configuration.
- **Avatar and camera:** selected avatar, whether it is forced, field of view, and perspective policy.
- **Avatar policy:** optional `avatar.allowed` and `avatar.denied` pattern lists.
  Patterns are case-insensitive and support `*`; deny patterns take precedence.
  An empty allow list permits any avatar not denied. Mode clients fall back to
  the preset's safe `avatar.name` when the selected avatar is disallowed, and
  the server rejects disallowed avatar manifests for the active role.
- **Loadout and weapon tuning:** a named weapon set plus per-preset weapon overrides for fields supported by `ActorPresetWeapons`. Base weapon definitions remain in `weapons.json`.
- **Presentation:** damage numbers, hit feedback, muzzle flashes, blood, and other supported visual/audio gates.
- **NPC policy:** supported movement-executor, navigation, and movement-decision settings. The role or mode may separately select an NPC combat `behavior_profile`.

The avatar owns mesh nodes, body-part roles, collision declarations, and animation clips. A preset selects that avatar. A weapon or attack definition owns its damage/cooldown/contact behavior. A map encounter owns spawn position, activation, count, and respawn policy. Presets should reference those owners rather than grow into a second avatar editor, weapon system, AI objective system, or map spawner.

## 5. Resolution and override rules

The desired resolution order is:

1. The active game mode assigns an actor to a team and a role.
2. The role explicitly names one actor preset.
3. The actor preset supplies the reusable baseline for values it defines.
4. Explicit values on the role override the preset only for that role assignment. Missing role fields inherit the preset value, then the shared system default.
5. A mode-level override may supersede a role only when the mode schema explicitly declares it. A server operator may also make a temporary per-actor override with `actor_preset_actor`; the server applies it authoritatively through that actor's spawn profile.

All actors use the normal spawn and respawn lifecycle to apply the resolved setup. The server remains authoritative for team, health, inventory, attacks, and damage. Clients may apply the same selected preset for camera and presentation, but may not change server-owned gameplay by editing local JSON.

The actor preset is not a second controller type. The same actor setup can be used by a player, an NPC, a remote replica, or a replay actor; input source and role determine who controls it. An NPC `behavior_profile` does not make a human use NPC decision logic.

## 6. Monster presets use the same actor model

A monster is not a separate kind of actor preset. It uses the same actor, avatar, movement, weapon/attack, and spawn owners as other actors. Use two role assignments when the same monster can be player-controlled or NPC-controlled:

```text
Normal Zombie actor preset
    ├── Normal Zombie player role (monster team; human input)
    └── Normal Zombie NPC role (monster team; NPC behavior profile)
```

Both roles point to the same Normal Zombie actor preset. The NPC role selects the zombie's AI behavior; the player role does not. Monster spawn volumes select the NPC role or a pool of monster role IDs and own count, activation, maximum alive, and reactivation timing. This separation lets Zombie Tower spawn an NPC Normal Zombie while `zt_team zombie` assigns a human player to the player-controlled Normal Zombie role.

For the first grounded-monster set, the actor preset defines the shared body/health/movement/avatar/loadout selections; the role defines monster-team membership and NPC behavior; the avatar files define body and animation; weapon/attack definitions specify how it damages actors; and the map's monster volume decides where and when it appears. Do not invent a new movement implementation for each walking monster. Use a shared grounded movement preset and give each monster its own behavior/attack choices where the shared systems support them.

## 7. Current repository behavior (partial)

These are observed source/config facts, not claims that every combination works in a live match:

- `MatchRoleRegistry` loads `config/roles.json` and scans `config/actor-presets/`. Both are currently stored in the transitional `MatchRoleDefinition` type.
- `roles.json` roles currently parse team, health, movement preset, weapon set, starting weapon, NPC `behavior_profile`, `actor_preset` ID, spawn group, avatar, and objective permissions.
- Actor-preset files currently parse camera, movement preset, weapon set/overrides, health, avatar, allowed team IDs, presentation gates, NPC movement policy, navigation, and movement-executor selection.
- A gamemode team can reference a role using its `role` field. Gamemode JSON can also name a mode-level `actor_preset`, but that legacy fallback is used only for modes without explicit team roles/role counts.
- `serverResolveActorSpawnProfile()` takes health, movement preset, weapon set, starting weapon, and NPC behavior from the resolved role. It carries the actor-preset ID for preset-owned policies and overlays. A role's `actor_preset` reference is not yet a complete, general field-by-field merge of every preset value into the role.
- Actor preset scanning contains a compatibility merge for an actor preset and gameplay role with the same ID. This same-ID behavior is transitional and must not become the desired authoring rule; use explicit IDs and a documented resolution order.
- `config/gamemodes/zombie_tower.json` currently has Humans and NPC teams but no role references or spectator/monster role. It uses a five-second intermission and a persistent NPC spawn cap of one. It does not implement the first-playable roster and phase plan by itself.
- Current actor presets do not define a general monster archetype schema, arbitrary attack sets, or a mode's monster encounter manifest. Existing specs separately own avatar animation, NPC behavior, weapons, and map entities.

The desired contract above is broader and clearer than the current loader/runtime. Implementation work must add missing role resolution and roster behavior to existing owners instead of treating this document as proof those features already work.

## 8. Hot reload and safe application

Configuration reload behavior must be stated per field. Camera and presentation may be applied to clients through their presentation owner. Match-critical team, health, movement, loadout, and monster identity must be applied by the server at a documented safe boundary, normally a spawn, respawn, or next run start. A reload must validate the whole candidate, retain the last valid version on error, and avoid half-applying a mixed actor setup. Do not claim every current actor-preset field hot-reloads merely because the registry watches files.

### Temporary server assignment for one actor

The server operator may list current match actors with `actor_list` and assign
one loaded actor preset using
`actor_preset_actor <actor-id> <preset-id|index>`. Numeric preset indices use
the alphabetical order shown by `actor_preset_list`; stable preset IDs are
preferred for saved instructions. The assignment is server-authoritative and
overlays the selected actor's role at its next spawn or respawn. It remains
attached to that actor through deaths and match-roster rebuilds, then is
discarded when the actor is removed or disconnects. It does not write JSON or
carry over to a later connection. Other actors retain their own role and
preset resolution.

## 9. First monster design pass (behavior targets, not balance numbers)

These are behavior identities drawn from the Zombie Tower design. Exact health, damage, size, speed, and attack cadence remain unset unless selected in the Zombie Tower monster sections.

| Monster | Ground role | Readable threat and proposed distinct behavior |
|---|---|---|
| Boomer | Grounded area pressure | Approaches on foot, spits acid that can leave a damaging pool, and can swing at close range. Its danger is making floor space unsafe. |
| Stabber | Fast ground melee | Pursues the team and reaches with a visible arm/blade stab. Distinguish it from the Charger by sustained pursuit rather than one committed burst. |
| Crawler | Low grounded pressure | Moves close to the floor, is easy to miss in a crowd, and threatens the player's lower body. It stays grounded; it does not crawl on walls/ceilings in this first pass. |
| Brute | Slow heavy ground pressure | Larger and tougher than a normal zombie; uses a heavy punch and may use a ground slam that damages and displaces nearby actors. |
| Chainsaw Boss | Grounded boss placeholder | Fast, low-friction pursuit with chainsaw contact pressure. In the initial playable version this is the temporary boss encounter, not the final boss. |
| Charger | Grounded burst attacker | Lines up and commits to a fast forward charge, then attacks at close range. Its clear burst/recovery should make it different from the Stabber's continued chase. |

These six are all intended to use grounded locomotion initially. “Grounded” describes their movement mode, not identical stats, body shapes, attack ranges, or decision-making. Preserve the existing Zombie Tower section for each monster as the source of prior design notes; unresolved numeric values stay `NEEDS_SPEC_DECISION` instead of being guessed.

### First authoring steps

1. Define one simple grounded Normal Zombie actor preset and one human-controlled role plus one NPC role that reference it.
2. Add one shared human-team preset/role and one allied NPC role that shares its loadout but selects a companion behavior profile.
3. Add the other five monster presets/roles one at a time, reusing grounded movement and shared attack owners; add only the specific attack difference that makes each monster readable.
4. Reference monster role IDs from authored map volumes. Keep the mode's encounter and boss progress in gamemode/map data, not inside actor presets.
5. After each monster's behavior identity is decided, select its health, body scale, movement preset, attack definition, avatar/animations, and sound references in their owning files. Then play it to tune numbers.

### Decisions to make before authoring the six presets

- Which avatar/model and rig should each monster use? A name in the monster table does not mean an avatar asset already exists.
- Do all six use one shared grounded movement preset initially, or should the Stabber/Charger/Boss start from different named movement presets?
- Should the Boomer's acid spit and pool be a required first-slice attack, or should the first grounded slice start with contact/melee attacks and add acid afterward?
- Should the Chainsaw Boss placeholder be available as a player-controlled monster, or NPC-only?
- Exact health/damage/speed and attack cadence should be tuned after their behavior and models are authored; no values are invented here.

---

### Historical raw design note (retained verbatim)

2026 10 19 1834 jroj todo explain this stuff is so cool like

its related to json hot reload and stuff 
so 

juggernaut preset is a whol epreset
but

u can do like
load preset juggernaut
instnatly
ur fov = 50
health = like 3500
ur weaponset changed
ur movement mode changes like u get the heavy slower movement 
u should be bable to chang a lot of things about an actor withthe preset, like everthing should be able to change 
