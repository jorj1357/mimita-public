# Juggernaut mode plan

Date file created: 2026-10-06

Status: Phase 1 configuration implemented; Juggernaut health, arcade presentation,
team-colored outlines, endless round cycling, and the Fighter weapon-choice UI
are implemented. Connected-client gameplay acceptance is still pending.

## Implemented first slice

The first playable configuration now exists in the active registries:

- `juggernaut` is available through `config/onlinemodes.json`.
- The roster is 16 Fighters versus 2 Juggernauts.
- Fighters use fast movement, 100 health, and choose one weapon per life from
  the visible four-choice popup with keys 1-4.
- Juggernauts use heavy movement, 5,000 health, a big shotgun/hitscan-rifle
  role loadout, and a limited-information NPC behavior profile.
- The mode uses the shared round lifecycle with one life and team elimination.
- The presentation-only juggernaut_arcade actor preset enables damage numbers,
  hitmarkers, hit effects, hit sounds, blood, muzzle flashes, and world impacts.
- Juggernaut presentation uses green friendly outlines and red enemy outlines.
- The round lifecycle is endless: intermission, 3-2-1-GO, numbered round,
  round-over result, and the next countdown repeat without a match cap.

This configuration has not yet been accepted through a connected human client.

## End goal

Create a JSON-controlled, round-based elimination mode in which a small team
of slow, powerful Juggernauts fights a larger team of fast, fragile Fighters.
The mode should reuse the shared actor lifecycle, movement, weapon, damage,
physics, NPC, networking, and spectator systems. Adding this mode must not
create a second player implementation, a second weapon implementation, or a
Juggernaut-only NPC system.

The intended first balance target is:

- Fighters: the larger team, `100` health, fast movement, limited weapon choice.
- Juggernauts: two or four players by configuration, `5,000` health initially,
  slow movement, narrow field of view, and unusually powerful weapons.
- Both teams: no respawns during a round.
- Round victory: eliminate every living member of the opposing team.

All important values are tuning data. The numbers above are the starting
proposal, not hardcoded engine constants.

## Player experience

### Fighters

Fighters should feel close to the current sandbox player, except that the mode
limits their life to one weapon and gives them a faster movement profile.

- Health: `100` initially.
- Movement: fast Fighter movement preset.
- Camera/FOV: normal Fighter values.
- Respawn: disabled during the active round.
- Weapon count: exactly one weapon per life.
- Weapon choices: revolver, shotgun, spy knife, or rocket launcher.
- Weapon selection: an explicit pre-round choice, never an unannounced random
  assignment.

The server validates and locks the choice before the Fighter spawns. The
player cannot switch to another weapon during that life. After death, the
player spectates until the next round and receives a new choice opportunity.

The selection UI should show the four legal choices and enough information to
make the tradeoff understandable. The final authority remains the server:

```text
intermission/team selection
    -> Fighter chooses one weapon
    -> server validates team, phase, and weapon choice
    -> Fighter spawns with one locked weapon
    -> death creates a spectator state
    -> next round permits a new choice
```

### Juggernauts

Juggernauts should be rare, intimidating, and dangerous rather than merely
being Fighters with more health.

- Team size: configurable; initial target is two or four Juggernauts.
- Health: `5,000` initially, JSON-controlled.
- Movement: slow Juggernaut movement preset.
- Turning/handling: may be slower while using heavy weapons.
- Field of view: narrow, JSON-controlled.
- Respawn: disabled during the active round.
- Weapons: a small Juggernaut weapon set with powerful variants.

Their weakness is limited mobility and awareness. Their strength is their
health, damage, knockback, range, and weapon pressure. The mode should not
silently compensate for the narrow FOV with perfect radar, omniscient AI, or
automatic target knowledge.

## Mode lifecycle

The mode uses the shared round lifecycle:

```text
INTERMISSION
    -> team assignment and roster lock
    -> Fighter weapon selection
    -> COUNTDOWN
    -> GO / ACTIVE ROUND
    -> elimination or configured timeout
    -> RESULTS
    -> next round or match results
```

Round rules:

- No player or NPC respawns during an active round.
- A dead participant becomes a spectator for the remainder of the round.
- A team is eliminated when it has no living participants.
- Fighters win by eliminating every Juggernaut.
- Juggernauts win by eliminating every Fighter.
- Match length and rounds-to-win are configurable.
- Team capacity and the number of Juggernauts are configurable.
- The mode must define what happens when a participant disconnects, leaves, or
  never completes weapon selection; it must not leave an invisible living
  participant blocking round completion.

The first slice should support uneven rosters, such as six Fighters versus two
Juggernauts or ten Fighters versus four Juggernauts. Health and damage should
remain explicit tuning values first; later balance work may scale Juggernaut
health or weapon power by roster size if playtesting requires it.

## Reusable architecture

The mode should compose existing owners instead of introducing specialized
actor classes:

```text
Juggernaut gamemode
  -> team membership and round/elimination rules
  -> Fighter role + Fighter actor preset + weapon-choice policy
  -> Juggernaut role + Juggernaut actor preset + weapon set
  -> shared actor lifecycle
  -> human input or NPC ActorIntent
  -> shared movement, weapon, damage, physics, and networking
```

### Gamemode owns

- Team definitions and capacity.
- Round phases, timers, countdown, results, and match victory.
- No-respawn/elimination rules.
- Team assignment and roster lock.
- Fighter weapon-selection window and lock policy.
- Victory conditions and timeout behavior.
- Mode-specific HUD metadata and scoreboard presentation.

### Roles own

- Team identity.
- Actor-preset reference.
- Behavior-preset reference.
- Spawn group.
- Role-level capability restrictions.
- Initial loadout or weapon-selection policy reference.

### Actor presets own

- Health and life baseline.
- Movement preset.
- Camera/FOV policy.
- Avatar and presentation policy.
- Weapon restrictions or in-memory weapon overlays.

The Juggernaut mode must not duplicate health, movement, FOV, or presentation
ownership in several JSON files. The mode selects the role; the role resolves
the actor preset; the actor preset supplies the actor configuration.

### Behavior presets own

Behavior presets apply to NPCs using the same roles as human players. They may
control:

- Perception range and FOV.
- Preferred engagement distance.
- Aggression and target priority.
- Whether the actor pursues, holds, retreats, or searches.
- Weapon preference within the role's legal weapon set.

Behavior presets must not grant weapons or override the team rules. A
Juggernaut behavior profile can prefer close-range shotgun pressure, but it
must still consume the Juggernaut role's legal weapon set.

## Weapon plan

Juggernaut weapons are configurations of the shared weapon definition and
execution systems. They are not separate `JuggernautWeapon` code paths.

### Juggernaut big shotgun

Start as a configured shotgun variant:

- More pellets or larger pellet traces.
- Higher damage.
- Stronger knockback.
- Large tracer and muzzle presentation.
- Deliberate range/spread tradeoff.
- Fire delay tuned separately from the Fighter shotgun.

The server continues to own pellet traces, damage, and knockback. Client
prediction may show immediate effects, but confirmation and health remain
authoritative.

### Juggernaut large machine gun

Start as a configured automatic hitscan variant using the shared rifle/hitscan
path:

- High fire rate.
- Large tracer thickness and heavy firing presentation.
- Moderate per-hit damage balanced against sustained fire.
- Victim knockback.
- Weapon-local wall-penetration budget.
- Potentially reduced turn/movement handling while firing.

Wall penetration must be a property of this weapon variant, not a global rule.
The intended trace is:

```text
trace toward target
    -> hit a penetrable wall
    -> measure or classify wall thickness/material
    -> spend penetration budget
    -> continue only if budget remains
    -> reduce damage after penetration
```

The first implementation may use a bounded penetration budget and simple
material classes. It must not permit unlimited wall shots or make every weapon
penetrate by accident.

### Juggernaut heavy revolver

This is optional for the first playable slice, but should fit the same system:

- Slow fire rate.
- Small magazine.
- Very high damage.
- Large projectile/tracer presentation.
- Strong knockback.
- Deliberate recovery time.

It should be a weapon-definition variant, not a separate combat subsystem.

### Juggernaut chainsaw

The spy knife is a valid reuse point for melee validation and contact
authority, but a chainsaw should be represented as a continuous active melee
window rather than one enormous knife hit.

Conceptual behavior:

```text
attack begins
    -> chainsaw active window
    -> bounded melee overlap checks at fixed ticks
    -> damage at a controlled interval
    -> continuous push/knockback while contact persists
    -> attack ends
```

Required safeguards:

- Damage interval prevents damage every render frame.
- Server validates active window, range, target, and life generation.
- Contact work has bounded targets and reusable scratch storage.
- Audio/effects are event-driven and do not create duplicate confirmation FX.

The chainsaw should be added after the core round and firearm slice is stable.

## Automatic-fire batching and performance

Fast fire must remain compatible with the fixed 60 Hz gameplay rule. Batching
means grouping legal work, not silently dropping valid shots.

For each fixed simulation tick:

1. Validate fire intents, life generation, team state, ammo, and cooldown.
2. Accept only the number of shots legally permitted by the weapon's timing.
3. Process related traces and damage in a bounded batch.
4. Accumulate knockback/damage results deterministically.
5. Emit compact confirmed events for replication and presentation.

The implementation must include:

- Per-weapon cooldown validation.
- Per-connection attack-request rate limiting.
- A bounded number of accepted shots per actor per tick.
- Reused scratch buffers in hot paths.
- Bounded wall-penetration steps.
- No unbounded per-shot allocation in fixed-tick combat.
- Deterministic ordering when multiple attacks affect the same target.

The client may predict automatic fire and tracer presentation, but the server
must own final hit, health, death, knockback, score, and round-end decisions.

## Proposed configuration shape

The exact schema should follow the active gamemode/role/actor-preset registries,
but the ownership should be equivalent to:

```json
{
  "id": "juggernaut",
  "name": "Juggernaut",
  "teams": [
    {
      "id": "fighters",
      "role": "juggernaut_fighter",
      "capacity": 10,
      "weapon_selection": {
        "mode": "one_per_life",
        "choices": ["revolver", "shotgun", "spy_knife", "rocket_launcher"]
      }
    },
    {
      "id": "juggernauts",
      "role": "juggernaut",
      "capacity": 4
    }
  ],
  "rounds": {
    "respawns": false,
    "victory": "team_elimination",
    "juggernaut_health": 5000
  }
}
```

This is illustrative only. Do not add a second schema if the active registry
already has equivalent fields. Existing role, actor-preset, behavior-profile,
and weapon-set owners should be extended only where necessary.

## Implementation phases

### Phase 1: data model and ownership

- Confirm the active gamemode, role, actor-preset, behavior-profile, and
  weapon-set schemas.
- Define Fighter and Juggernaut roles.
- Define actor presets for fast/fragile and slow/heavy behavior.
- Define the mode's team capacities, round rules, and weapon-selection policy.
- Add pure validation for unknown references, invalid team sizes, and illegal
  weapon choices.

### Phase 2: round lifecycle

- Reuse the shared intermission/countdown/active/results lifecycle.
- Lock teams and Fighter weapon choices at the correct phase boundary.
- Disable respawn during the active round.
- Transition dead participants to spectators.
- End rounds on team elimination and replicate the result state.

### Phase 3: actor-preset application

- Apply health, movement, FOV, camera, loadout, and behavior through existing
  spawn/reset owners.
- Verify initial spawn and every new round reset use the same role/preset path.
- Ensure NPCs and human players receive equivalent gameplay configuration.

### Phase 4: weapon restrictions and selection

- Present the four Fighter choices.
- Validate and replicate the selected weapon.
- Lock the selection for the life.
- Reject equip, fire, or swap requests for weapons outside the role policy.

### Phase 5: Juggernaut firearms

- Add configured big shotgun and LMG variants.
- Add weapon-local knockback and tracer presentation.
- Add bounded, weapon-local wall penetration for the LMG.
- Add automatic-fire batching and focused performance diagnostics.

### Phase 6: AI behavior and balancing

- Add Juggernaut and Fighter behavior profiles.
- Enforce perception FOV and range for NPC target acquisition.
- Confirm no omniscient targeting bypasses the Juggernaut's intended weakness.
- Tune health, damage, fire rate, movement, FOV, and team-size balance through
  live JSON values.

#### Fighter unit behavior (general, not Juggernaut-specific)

The intended Fighter experience is a **coordinated swarm**: Fighters leave spawn
together, move as one unit toward the nearest Juggernaut, spread out instead of
stacking, and keep shooting while they close. This is composed entirely from the
general owners defined in:

- `docs/architecture/player-npc-systems/npc-group-behavior.md` (TeamFocus +
  SquadCoordinator);
- `docs/architecture/player-npc-systems/npc-local-avoidance.md` (spacing);
- the Recast/Detour navigation backend (routing).

The mode owns no Fighter AI. The Fighter role references a behavior profile whose
group fields (see the group-behavior contract section 6) enable focus toward the
hostile team and swarm slots. The Juggernaut role uses a profile without focus
(`focus_enabled: false`) unless a future revision says otherwise.

Required Fighter properties:

- leave spawn toward the focus instead of patrolling or wall-grinding;
- converge as a unit on the focus with bounded cohesion;
- occupy distributed firing slots rather than one line or a stack;
- cap how many members commit to a single focus actor (`max_attackers_per_target`);
- re-rally and re-slot after deaths or when the focus moves;
- never aim through walls; a focus actor is a travel target, not an aim target.

#### First acceptance map

The first acceptance map for this behavior is `dust2cyberiav4`. The mode's map
list should include it so the round/scenario pipeline can exercise the swarm on
the same geometry used by the navigation contract.

#### Juggernaut behavior

Juggernauts advance or hold with limited awareness. Their narrow FOV and range
limits are respected by the perception owner; they must not receive focus-style
shared omniscience that bypasses their intended weakness.

#### Acceptance metrics (initial, dust2cyberiav4, fixed seed)

- 100% of Fighter routes have a result or an explicit hold/fallback reason;
- Fighters make positive net progress toward the focus until engagement;
- zero unrecovered wall-intersection or collision-loop failures;
- Fighters arrive distributed (measured slot spread) rather than single-file;
- at least one coordinated push reaches interaction range with a Juggernaut
  before the round's first engagement window ends;
- no aim or damage event originates without a perception-valid visible target;
- human review confirms the Fighters look like a unit rather than loose
  individuals.

### Phase 7: optional heavy weapons

- Add the heavy revolver.
- Add the chainsaw using continuous melee-window logic.
- Validate damage interval, knockback, replication, and presentation.

## Ownership targets

Likely owners to inspect before implementation:

- Gamemode rules/lifecycle: `src/gamemode/` and
  `src/network/server-gamemode.*`.
- Roles and actor-preset composition: `src/gamemode/match-roles.*` and the
  existing role/preset registries.
- Shared weapon definitions/execution: `src/combat/` and the active weapon
  configuration registries.
- Authoritative attacks/damage/knockback: existing server attack and damage
  owners; do not create a Juggernaut-only network path.
- NPC behavior/perception: existing behavior-profile, perception, intent, and
  navigation owners.
- Spectating and life generations: existing shared actor lifecycle and
  spectator replication owners.
- HUD and selection UI: generalized gamemode GUI/selection owners.
- Diagnostics: the canonical structured JSONL event logger.

These paths are ownership targets for investigation, not permission to assume
that every historical implementation is active. The active checkout and
runtime-loaded configuration must be verified before edits.

## Acceptance criteria

### Data and source

- The mode loads from the active gamemode registry.
- Fighter and Juggernaut role references resolve to valid actor and behavior
  presets.
- The four Fighter weapons are legal choices and no other Fighter weapon is
  equipable.
- Juggernaut health, movement, FOV, team capacity, and weapon values are
  configuration-owned.
- No duplicate mode-specific actor or weapon subsystem is introduced.

### Round behavior

- Teams lock before the countdown.
- Fighters select one weapon before spawning.
- A Fighter cannot change weapon during the life.
- Death removes a participant from the active roster and creates spectator
  state without respawning them.
- The round ends when one team has no living participants.
- Next-round reset applies the correct role, preset, health, weapon policy, and
  life generation to every participant.

### Combat and performance

- Juggernaut weapons use shared validation, tracing, damage, knockback, and
  replication owners.
- LMG wall penetration is bounded, weapon-local, and reduces damage according
  to the configured rule.
- Automatic fire remains fixed-tick and rate-limited under many simultaneous
  shooters.
- No render-frame collision or damage path is introduced.
- Server health, death, knockback, score, and round outcome remain authoritative.

### Human acceptance still required

- A real round visibly communicates team, health, weapon selection, countdown,
  death, spectating, and victory.
- Fighters feel fast and vulnerable.
- Juggernauts feel powerful but meaningfully slow and unaware outside their FOV.
- Two- and four-Juggernaut rosters are both playable.
- Heavy weapon audio, tracer size, knockback, and penetration feel correct.
- Multiplayer behavior remains stable with simultaneous automatic fire.

## Related documents

- `docs/specs/gamemodes/gamemodes.md`
- `docs/specs/weapons/weapons.md`
- `docs/specs/networking/networking.md`
- `docs/specs/performance/performance.md`
- `docs/skills/spec-behavior-review-v1.md`
- `docs/features/gamemodes/counterstrike.md`
